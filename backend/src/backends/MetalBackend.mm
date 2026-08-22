#include "MetalBackend.hpp"
#include "Types.hpp"
#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <string>
#include <vector>
#include <memory>
#include <complex>
#include <spdlog/spdlog.h>
#include <random>
#include <cmath>

#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <filesystem>

// Metal GPU struct uses float; C++ Complex uses double.
// We must convert explicitly during upload/download.
struct MetalComplex {
  float real;
  float imag;
};

namespace qubit_engine {

// --- Lifecycle ---

MetalBackend::MetalBackend(size_t num_qubits)
    : num_qubits_(num_qubits), device_(nullptr), commandQueue_(nullptr),
      gpuBuffer_(nullptr) {
  initializeMetal();

  // Initialize State |0...0>
  size_t dim = 1ULL << num_qubits;
  std::vector<Complex> initial(dim, {0.0, 0.0});
  initial[0] = {1.0, 0.0};
  uploadState(initial);
}

MetalBackend::~MetalBackend() {
  if (lastCommandBuffer_)
    CFRelease(lastCommandBuffer_);
  if (hadamardPipeline_) CFRelease(hadamardPipeline_);
  if (paulixPipeline_) CFRelease(paulixPipeline_);
  if (pauliyPipeline_) CFRelease(pauliyPipeline_);
  if (paulizPipeline_) CFRelease(paulizPipeline_);
  if (rxPipeline_) CFRelease(rxPipeline_);
  if (ryPipeline_) CFRelease(ryPipeline_);
  if (rzPipeline_) CFRelease(rzPipeline_);
  if (phaseSPipeline_) CFRelease(phaseSPipeline_);
  if (phaseTPipeline_) CFRelease(phaseTPipeline_);
  if (cnotPipeline_) CFRelease(cnotPipeline_);
  if (toffoliPipeline_) CFRelease(toffoliPipeline_);
  if (measureProb0Pipeline_) CFRelease(measureProb0Pipeline_);
  if (projectStatePipeline_) CFRelease(projectStatePipeline_);
  if (expectationZPipeline_) CFRelease(expectationZPipeline_);
  if (computeProbabilitiesPipeline_) CFRelease(computeProbabilitiesPipeline_);
  if (diagonalExpectationPipeline_) CFRelease(diagonalExpectationPipeline_);
  if (denseUnitary1qPipeline_) CFRelease(denseUnitary1qPipeline_);
  if (denseUnitary2qPipeline_) CFRelease(denseUnitary2qPipeline_);
  if (kraus1qPipeline_) CFRelease(kraus1qPipeline_);
  if (swapPipeline_) CFRelease(swapPipeline_);
  if (czPipeline_) CFRelease(czPipeline_);
  if (kraus2qPipeline_) CFRelease(kraus2qPipeline_);
  if (gpuBuffer_)
    CFRelease(gpuBuffer_);
  if (device_)
    CFRelease(device_);
  if (commandQueue_)
    CFRelease(commandQueue_);
}

void MetalBackend::initializeMetal() {
  spdlog::info("MetalBackend::initializeMetal - Start");
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    throw std::runtime_error("Metal is not supported on this device.");
  }
  spdlog::info("MetalBackend::initializeMetal - Device Created: {}",
      [[device name] UTF8String]);
  device_ = (void *)CFBridgingRetain(device);

  id<MTLCommandQueue> queue = [device newCommandQueue];
  commandQueue_ = (void *)CFBridgingRetain(queue);
  spdlog::info("MetalBackend::initializeMetal - CommandQueue Created");

  // 1. Collect candidate search directories
  NSMutableArray<NSString *> *searchDirs = [NSMutableArray array];
  
  if (const char* envPath = std::getenv("QUBIT_SHADERS_PATH")) {
    [searchDirs addObject:[NSString stringWithUTF8String:envPath]];
  }
  if (const char* envRoot = std::getenv("QUBIT_ENGINE_ROOT")) {
    [searchDirs addObject:[NSString stringWithFormat:@"%s", envRoot]];
    [searchDirs addObject:[NSString stringWithFormat:@"%s/backend/src", envRoot]];
    [searchDirs addObject:[NSString stringWithFormat:@"%s/bin", envRoot]];
  }

  // Executable directory & parent directories
  char exeBuf[1024];
  uint32_t exeBufSize = sizeof(exeBuf);
  if (_NSGetExecutablePath(exeBuf, &exeBufSize) == 0) {
    try {
      std::filesystem::path exePath = std::filesystem::canonical(exeBuf);
      std::filesystem::path exeDir = exePath.parent_path();
      [searchDirs addObject:[NSString stringWithUTF8String:exeDir.string().c_str()]];
      [searchDirs addObject:[NSString stringWithUTF8String:(exeDir / ".." / "backend" / "src").lexically_normal().string().c_str()]];
      [searchDirs addObject:[NSString stringWithUTF8String:(exeDir / ".." / "src").lexically_normal().string().c_str()]];
      [searchDirs addObject:[NSString stringWithUTF8String:(exeDir / "backend" / "src").lexically_normal().string().c_str()]];
    } catch (...) {}
  }

  // Dynamic library location (when loaded as python module or dylib)
  Dl_info dlInfo;
  static int s_anchorSymbol = 0;
  if (dladdr((const void*)&s_anchorSymbol, &dlInfo) && dlInfo.dli_fname) {
    try {
      std::filesystem::path libPath = std::filesystem::canonical(dlInfo.dli_fname);
      std::filesystem::path libDir = libPath.parent_path();
      [searchDirs addObject:[NSString stringWithUTF8String:libDir.string().c_str()]];
      [searchDirs addObject:[NSString stringWithUTF8String:(libDir / ".." / "backend" / "src").lexically_normal().string().c_str()]];
      [searchDirs addObject:[NSString stringWithUTF8String:(libDir / ".." / "src").lexically_normal().string().c_str()]];
      [searchDirs addObject:[NSString stringWithUTF8String:(libDir / "backend" / "src").lexically_normal().string().c_str()]];
    } catch (...) {}
  }

  // Working directory relative paths
  [searchDirs addObject:@"."];
  [searchDirs addObject:@"backend/src"];
  [searchDirs addObject:@"src"];
  [searchDirs addObject:@"../backend/src"];
  [searchDirs addObject:@"../src"];
  [searchDirs addObject:@"../../backend/src"];
  [searchDirs addObject:@"../../../backend/src"];

  // 2. Try loading precompiled default.metallib
  NSError *error = nil;
  id<MTLLibrary> library = [device newDefaultLibraryWithBundle:[NSBundle mainBundle] error:&error];
  if (!library) {
    NSArray<NSString *> *metallibNames = @[
      @"default.metallib", @"bin/default.metallib",
      @"build/default.metallib", @"build_macos/default.metallib"
    ];
    for (NSString *dir in searchDirs) {
      for (NSString *rel in metallibNames) {
        NSString *fullPath = [dir stringByAppendingPathComponent:rel];
        if ([[NSFileManager defaultManager] fileExistsAtPath:fullPath]) {
          NSURL *url = [NSURL fileURLWithPath:fullPath];
          library = [device newLibraryWithURL:url error:&error];
          if (library) {
            spdlog::info("MetalBackend::initializeMetal - Loaded precompiled metallib from {}", [fullPath UTF8String]);
            break;
          }
        }
      }
      if (library) break;
    }
  }

  // 3. Fallback: JIT compile shaders.metal source
  if (!library) {
    spdlog::info("MetalBackend::initializeMetal - metallib file not found, loading shaders.metal source...");
    NSArray<NSString *> *shaderFiles = @[
      @"shaders.metal", @"gate_kernels.metal",
      @"src/shaders.metal", @"backend/src/shaders.metal",
      @"src/kernels/metal/gate_kernels.metal"
    ];
    NSMutableArray<NSString *> *checkedPaths = [NSMutableArray array];
    for (NSString *dir in searchDirs) {
      for (NSString *file in shaderFiles) {
        NSString *fullPath = [dir stringByAppendingPathComponent:file];
        [checkedPaths addObject:fullPath];
        if ([[NSFileManager defaultManager] fileExistsAtPath:fullPath]) {
          NSString *srcContent = [NSString stringWithContentsOfFile:fullPath encoding:NSUTF8StringEncoding error:&error];
          if (srcContent) {
            library = [device newLibraryWithSource:srcContent options:nil error:&error];
            if (library) {
              spdlog::info("MetalBackend::initializeMetal - Compiled library from source: {}", [fullPath UTF8String]);
              break;
            }
          }
        }
      }
      if (library) break;
    }
    if (!library) {
      std::string checkedStr;
      for (NSString *p in checkedPaths) {
        checkedStr += "\n  - " + std::string([p UTF8String]);
      }
      spdlog::error("MetalBackend::initializeMetal - Failed to find or compile shaders.metal. Searched candidate paths:{}", checkedStr);
    }
  }

  if (!library) {
    spdlog::error("MetalBackend::initializeMetal - Failed to load library: {}",
        error ? [[error localizedDescription] UTF8String] : "No valid shader file found");
    throw std::runtime_error(std::string("Could not load Metal library: ") +
                             (error ? [[error localizedDescription] UTF8String] : "No valid shader file found"));
  }
  spdlog::info("MetalBackend::initializeMetal - Library Loaded");

  buildPipelines((void *)CFBridgingRetain(library));
  spdlog::info("MetalBackend::initializeMetal - Pipelines Built");
}

void MetalBackend::buildPipelines(void *libPtr) {
  id<MTLLibrary> library = (__bridge id<MTLLibrary>)libPtr;
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  NSError *error = nil;

  auto createPipe = [&](NSString *name) -> void * {
    id<MTLFunction> func = [library newFunctionWithName:name];
    if (!func) {
      spdlog::error("Error: Function {} not found.", [name UTF8String]);
      return nullptr;
    }
    id<MTLComputePipelineState> pso =
        [device newComputePipelineStateWithFunction:func error:&error];
    if (!pso) {
      spdlog::error("Error creating pipeline for {}", [name UTF8String]);
      return nullptr;
    }
    return (void *)CFBridgingRetain(pso);
  };

  hadamardPipeline_ = createPipe(@"hadamard_kernel");
  paulixPipeline_ = createPipe(@"paulix_kernel");
  pauliyPipeline_ = createPipe(@"pauliy_kernel");
  paulizPipeline_ = createPipe(@"pauliz_kernel");
  rxPipeline_ = createPipe(@"rx_kernel");
  phaseSPipeline_ = createPipe(@"phases_kernel");
  phaseTPipeline_ = createPipe(@"phaset_kernel");
  ryPipeline_ = createPipe(@"ry_kernel");
  rzPipeline_ = createPipe(@"rz_kernel");
  cnotPipeline_ = createPipe(@"cnot_kernel");
  toffoliPipeline_ = createPipe(@"toffoli_kernel");
  measureProb0Pipeline_ = createPipe(@"measure_prob0_kernel");
  projectStatePipeline_ = createPipe(@"project_state_kernel");
  expectationZPipeline_ = createPipe(@"expectation_z_kernel");
  computeProbabilitiesPipeline_ = createPipe(@"compute_probabilities_kernel");
  diagonalExpectationPipeline_ = createPipe(@"diagonal_expectation_kernel");
  denseUnitary1qPipeline_ = createPipe(@"dense_unitary_1q_kernel");
  denseUnitary2qPipeline_ = createPipe(@"dense_unitary_2q_kernel");
  kraus1qPipeline_ = createPipe(@"apply_kraus_kernel");
  swapPipeline_ = createPipe(@"swap_kernel");
  czPipeline_ = createPipe(@"cz_kernel");
  kraus2qPipeline_ = createPipe(@"kraus_2q_kernel");
  computeNormSqPipeline_ = createPipe(@"compute_norm_sq_kernel");
  scaleStatePipeline_ = createPipe(@"scale_state_kernel");
}

// --- Memory Management ---

void MetalBackend::initializeBuffer(const std::vector<Complex> &initialState) {
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  size_t size = initialState.size() * sizeof(Complex);

  id<MTLBuffer> mtlBuf =
      [device newBufferWithBytes:initialState.data()
                          length:size
                         options:MTLResourceStorageModeShared];
  gpuBuffer_ = (void *)CFBridgingRetain(mtlBuf);
}

void MetalBackend::uploadState(const std::vector<Complex> &cpuState) {
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  size_t num_elements = cpuState.size();
  // Metal buffer uses float pairs (MetalComplex), not double pairs
  size_t bufferSize = num_elements * sizeof(MetalComplex);

  if (gpuBuffer_)
    CFRelease(gpuBuffer_);

  id<MTLBuffer> mtlBuf =
      [device newBufferWithLength:bufferSize
                          options:MTLResourceStorageModeShared];
  gpuBuffer_ = (void *)CFBridgingRetain(mtlBuf);
  capacity_ = num_elements;

  // Convert double -> float for Metal
  MetalComplex *ptr = (MetalComplex *)[mtlBuf contents];
  for (size_t i = 0; i < num_elements; ++i) {
    ptr[i].real = static_cast<float>(cpuState[i].real());
    ptr[i].imag = static_cast<float>(cpuState[i].imag());
  }
}

void MetalBackend::downloadState(std::vector<Complex> &cpuState) const {
  // SYNC: Wait for the last committed GPU command buffer to finish
  if (lastCommandBuffer_) {
    id<MTLCommandBuffer> lastBuf =
        (__bridge id<MTLCommandBuffer>)lastCommandBuffer_;
    [lastBuf waitUntilCompleted];
  }

  id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)gpuBuffer_;
  MetalComplex *ptr = (MetalComplex *)[mtlBuf contents];
  size_t num_elements = 1ULL << num_qubits_;

  // Convert float -> double for C++
  cpuState.resize(num_elements);
  for (size_t i = 0; i < num_elements; ++i) {
    cpuState[i] = Complex(static_cast<double>(ptr[i].real),
                          static_cast<double>(ptr[i].imag));
  }
}

std::vector<Complex> MetalBackend::getStateVector() const {
  std::vector<Complex> state;
  downloadState(state);
  return state;
}

// --- Dispatch Helper ---

void MetalBackend::dispatchHelper(void *queuePtr, void *psoPtr, void *bufPtr,
                                  size_t dim, std::vector<void *> args,
                                  std::vector<size_t> sizes) {
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)queuePtr;
  id<MTLComputePipelineState> pso =
      (__bridge id<MTLComputePipelineState>)psoPtr;
  id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)bufPtr;

  if (!pso)
    return;

  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];

  [enc setComputePipelineState:pso];
  [enc setBuffer:buffer offset:0 atIndex:0];

  for (size_t i = 0; i < args.size(); ++i) {
    [enc setBytes:args[i] length:sizes[i] atIndex:(i + 1)];
  }

  NSUInteger totalThreads = dim / 2;
  NSUInteger maxThreads = pso.maxTotalThreadsPerThreadgroup;
  if (maxThreads > 256)
    maxThreads = 256;

  MTLSize threadsPerGroup = MTLSizeMake(maxThreads, 1, 1);
  MTLSize gridSize = MTLSizeMake(totalThreads, 1, 1);

  [enc dispatchThreads:gridSize threadsPerThreadgroup:threadsPerGroup];
  [enc endEncoding];
  [cmdBuf commit];

  // Track the last command buffer so downloadState can wait on it
  if (lastCommandBuffer_) {
    CFRelease(lastCommandBuffer_);
  }
  lastCommandBuffer_ = (void *)CFBridgingRetain(cmdBuf);
}

void MetalBackend::dispatchWithBuffers(void *psoPtr, size_t threadCount,
                                        std::vector<std::pair<int, void*>> bufferArgs,
                                        std::vector<std::tuple<int, void*, size_t>> bytesArgs) {
  id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)psoPtr;
  if (!pso) return;

  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];

  [enc setComputePipelineState:pso];

  // Bind MTLBuffer arguments
  for (const auto& [index, bufPtr] : bufferArgs) {
    [enc setBuffer:(__bridge id<MTLBuffer>)bufPtr offset:0 atIndex:index];
  }

  // Bind inline bytes arguments
  for (const auto& [index, dataPtr, dataSize] : bytesArgs) {
    [enc setBytes:dataPtr length:dataSize atIndex:index];
  }

  NSUInteger maxThreads = pso.maxTotalThreadsPerThreadgroup;
  if (maxThreads > 256) maxThreads = 256;

  [enc dispatchThreads:MTLSizeMake(threadCount, 1, 1)
      threadsPerThreadgroup:MTLSizeMake(maxThreads, 1, 1)];
  [enc endEncoding];
  [cmdBuf commit];

  if (lastCommandBuffer_) CFRelease(lastCommandBuffer_);
  lastCommandBuffer_ = (void *)CFBridgingRetain(cmdBuf);
}

// --- Gate Implementations ---

void MetalBackend::applyHadamard(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, hadamardPipeline_, gpuBuffer_, dim, {&stride},
                 {sizeof(uint32_t)});
}

void MetalBackend::applyX(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, paulixPipeline_, gpuBuffer_, dim, {&stride},
                 {sizeof(uint32_t)});
}

void MetalBackend::applyY(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, pauliyPipeline_, gpuBuffer_, dim, {&stride},
                 {sizeof(uint32_t)});
}

void MetalBackend::applyZ(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, paulizPipeline_, gpuBuffer_, dim, {&stride},
                 {sizeof(uint32_t)});
}

void MetalBackend::applyCNOT(size_t control, size_t target) {
  if (control == target)
    throw std::invalid_argument(
        "Control and target qubits cannot be the same.");
  uint32_t t_stride = 1 << target;
  uint32_t c_stride = 1 << control;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, cnotPipeline_, gpuBuffer_, dim,
                 {&t_stride, &c_stride}, {4, 4});
}

void MetalBackend::applyRotationY(size_t target, Precision angle) {
  uint32_t stride = 1 << target;
  float theta = (float)angle;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, ryPipeline_, gpuBuffer_, dim, {&stride, &theta},
                 {4, 4});
}

void MetalBackend::applyRotationZ(size_t target, Precision angle) {
  uint32_t stride = 1 << target;
  float theta = (float)angle;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, rzPipeline_, gpuBuffer_, dim, {&stride, &theta},
                 {4, 4});
}

void MetalBackend::applyRotationX(size_t target, Precision angle) {
  uint32_t stride = 1 << target;
  float theta = (float)angle;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, rxPipeline_, gpuBuffer_, dim, {&stride, &theta},
                 {4, 4});
}

void MetalBackend::applySWAP(size_t qubit1, size_t qubit2) {
  uint32_t s_low = static_cast<uint32_t>(std::min(qubit1, qubit2));
  uint32_t s_high = static_cast<uint32_t>(std::max(qubit1, qubit2));
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, swapPipeline_, gpuBuffer_, dim / 2,
                 {&s_low, &s_high}, {sizeof(uint32_t), sizeof(uint32_t)});
}

void MetalBackend::applyCZ(size_t control, size_t target) {
  uint32_t s_low = static_cast<uint32_t>(std::min(control, target));
  uint32_t s_high = static_cast<uint32_t>(std::max(control, target));
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, czPipeline_, gpuBuffer_, dim / 2,
                 {&s_low, &s_high}, {sizeof(uint32_t), sizeof(uint32_t)});
}

void MetalBackend::applyPhaseS(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, phaseSPipeline_, gpuBuffer_, dim, {&stride},
                 {sizeof(uint32_t)});
}

void MetalBackend::applyPhaseT(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  dispatchHelper(commandQueue_, phaseTPipeline_, gpuBuffer_, dim, {&stride},
                 {sizeof(uint32_t)});
}

void MetalBackend::applyToffoli(size_t c1, size_t c2, size_t target) {
  if (c1 == target || c2 == target || c1 == c2)
    throw std::invalid_argument("Control and target qubits must be distinct.");
    
  uint32_t t_stride = 1 << target;
  uint32_t c1_stride = 1 << c1;
  uint32_t c2_stride = 1 << c2;
  size_t dim = 1ULL << num_qubits_;
  
  dispatchHelper(commandQueue_, toffoliPipeline_, gpuBuffer_, dim,
                 {&t_stride, &c1_stride, &c2_stride}, {4, 4, 4});
}

void MetalBackend::applyDenseUnitary(const std::vector<size_t> &targets,
                                   const std::vector<Complex> &matrix) {
  size_t dim = 1ULL << num_qubits_;
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;

  if (targets.size() == 1) {
    uint32_t stride = 1 << targets[0];
    std::vector<MetalComplex> m(4);
    for (int i = 0; i < 4; ++i) {
        m[i].real = (float)matrix[i].real();
        m[i].imag = (float)matrix[i].imag();
    }
    dispatchWithBuffers(denseUnitary1qPipeline_, dim / 2,
                        {{0, gpuBuffer_}},
                        {{1, &stride, sizeof(uint32_t)}, {2, m.data(), 4*sizeof(MetalComplex)}});
  } else if (targets.size() == 2) {
    uint32_t s_low = (uint32_t)targets[0];
    uint32_t s_high = (uint32_t)targets[1];
    if (s_low > s_high) std::swap(s_low, s_high);
    std::vector<MetalComplex> m(16);
    for (int i = 0; i < 16; ++i) {
        m[i].real = (float)matrix[i].real();
        m[i].imag = (float)matrix[i].imag();
    }
    dispatchWithBuffers(denseUnitary2qPipeline_, dim / 4,
                        {{0, gpuBuffer_}},
                        {{1, &s_low, sizeof(uint32_t)}, {2, &s_high, sizeof(uint32_t)}, {3, m.data(), 16*sizeof(MetalComplex)}});
  }
}

void MetalBackend::applyDepolarizingNoise(Precision p) {
  if (p <= 0.0) return;
  
  static std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dist(0.0, 1.0);
  
  // Stochastic noise: For each qubit, with probability p, apply a random Pauli gate
  for (size_t target = 0; target < num_qubits_; ++target) {
    if (dist(gen) < p) {
      float r = dist(gen);
      if (r < 0.3333f) applyX(target);
      else if (r < 0.6666f) applyY(target);
      else applyZ(target);
    }
  }
}

void MetalBackend::renormalizeStateVector() {
  size_t dim = 1ULL << num_qubits_;
  size_t totalThreads = dim;
  size_t maxThreads = 256;
  size_t numGroups = (totalThreads + maxThreads - 1) / maxThreads;

  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  id<MTLBuffer> partialSumsBuf = [device newBufferWithLength:numGroups * sizeof(float)
                                                     options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
  id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)computeNormSqPipeline_;

  if (!pso) return;

  uint32_t tot_threads = static_cast<uint32_t>(totalThreads);
  [enc setComputePipelineState:pso];
  [enc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
  [enc setBuffer:partialSumsBuf offset:0 atIndex:1];
  [enc setBytes:&tot_threads length:sizeof(uint32_t) atIndex:2];
  [enc dispatchThreads:MTLSizeMake(totalThreads, 1, 1)
      threadsPerThreadgroup:MTLSizeMake(maxThreads, 1, 1)];
  [enc endEncoding];
  [cmdBuf commit];
  [cmdBuf waitUntilCompleted];

  float *sums = (float *)[partialSumsBuf contents];
  float norm_sq = 0.0f;
  for (size_t i = 0; i < numGroups; ++i) {
    norm_sq += sums[i];
  }

  float norm = std::sqrt(norm_sq);
  if (norm > 1e-12f) {
    float scale = 1.0f / norm;
    id<MTLCommandBuffer> scaleCmdBuf = [queue commandBuffer];
    id<MTLComputeCommandEncoder> scaleEnc = [scaleCmdBuf computeCommandEncoder];
    id<MTLComputePipelineState> scalePso = (__bridge id<MTLComputePipelineState>)scaleStatePipeline_;

    if (scalePso) {
      [scaleEnc setComputePipelineState:scalePso];
      [scaleEnc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
      [scaleEnc setBytes:&scale length:sizeof(float) atIndex:1];
      [scaleEnc dispatchThreads:MTLSizeMake(totalThreads, 1, 1)
          threadsPerThreadgroup:MTLSizeMake(maxThreads, 1, 1)];
      [scaleEnc endEncoding];
      [scaleCmdBuf commit];

      if (lastCommandBuffer_) CFRelease(lastCommandBuffer_);
      lastCommandBuffer_ = (void *)CFBridgingRetain(scaleCmdBuf);
    }
  }
}

static bool isScaledUnitary1Q(const std::array<Complex, 4>& m, float prob) {
  float d0 = std::norm(m[0]) + std::norm(m[2]);
  float d1 = std::norm(m[1]) + std::norm(m[3]);
  Complex off = m[0] * std::conj(m[1]) + m[2] * std::conj(m[3]);
  return (std::abs(d0 - prob) < 1e-4f && std::abs(d1 - prob) < 1e-4f && std::abs(off) < 1e-4f);
}

static bool isScaledUnitary2Q(const std::array<Complex, 16>& m, float prob) {
  for (int c = 0; c < 4; ++c) {
    float col_norm = 0.0f;
    for (int r = 0; r < 4; ++r) {
      col_norm += static_cast<float>(std::norm(m[r * 4 + c]));
    }
    if (std::abs(col_norm - prob) > 1e-4f) return false;
  }
  return true;
}

void MetalBackend::applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target) {
  if (channel.operators.empty()) return;

  bool all_scaled_unitary = true;
  for (const auto& op : channel.operators) {
    if (!isScaledUnitary1Q(op.matrix, static_cast<float>(op.probability))) {
      all_scaled_unitary = false;
      break;
    }
  }

  std::vector<float> probabilities;
  probabilities.reserve(channel.operators.size());
  float total_p = 0.0f;

  if (all_scaled_unitary) {
    for (const auto& op : channel.operators) {
      float p = static_cast<float>(op.probability);
      probabilities.push_back(p);
      total_p += p;
    }
  } else {
    // Compute 1Q RDM to get state-dependent trajectory jump probabilities: P(i) = Tr(K_i† K_i ρ)
    auto sv = getStateVector();
    size_t stride = 1ULL << target;
    Complex r00(0, 0), r01(0, 0), r11(0, 0);
    for (size_t i = 0; i < sv.size(); i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex c0 = sv[j];
        Complex c1 = sv[j + stride];
        r00 += c0 * std::conj(c0);
        r11 += c1 * std::conj(c1);
        r01 += c0 * std::conj(c1);
      }
    }
    std::array<Complex, 4> rdm = {r00, r01, std::conj(r01), r11};

    for (const auto& op : channel.operators) {
      const auto& M = op.matrix_dag_self;
      Complex tr = M[0] * rdm[0] + M[1] * rdm[2] + M[2] * rdm[1] + M[3] * rdm[3];
      float p = std::abs(static_cast<float>(tr.real()));
      probabilities.push_back(p);
      total_p += p;
    }
  }

  if (total_p < 1e-20f) return;

  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dis(0.0f, total_p);
  float r = dis(gen);

  const KrausOperator1Q* selected = &channel.operators.back();
  float selected_prob = probabilities.back();
  float cumulative = 0.0f;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cumulative += probabilities[i];
    if (r <= cumulative) {
      selected = &channel.operators[i];
      selected_prob = probabilities[i];
      break;
    }
  }

  if (selected_prob < 1e-20f) return;

  bool selected_is_unitary = isScaledUnitary1Q(selected->matrix, static_cast<float>(selected->probability));
  float inv_norm = (all_scaled_unitary && selected_is_unitary) ? (1.0f / std::sqrt(static_cast<float>(selected->probability))) : 1.0f;

  uint32_t stride = 1 << target;
  std::vector<MetalComplex> m(4);
  for (int i = 0; i < 4; ++i) {
      m[i].real = static_cast<float>(selected->matrix[i].real());
      m[i].imag = static_cast<float>(selected->matrix[i].imag());
  }

  size_t dim = 1ULL << num_qubits_;
  dispatchWithBuffers(kraus1qPipeline_, dim / 2,
                      {{0, gpuBuffer_}},
                      {{1, &stride, sizeof(uint32_t)}, {2, m.data(), 4*sizeof(MetalComplex)}, {3, &inv_norm, sizeof(float)}});

  if (!all_scaled_unitary) {
    renormalizeStateVector();
  }
}

void MetalBackend::applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2) {
  if (channel.operators.empty()) return;

  bool all_scaled_unitary = true;
  for (const auto& op : channel.operators) {
    if (!isScaledUnitary2Q(op.matrix, static_cast<float>(op.probability))) {
      all_scaled_unitary = false;
      break;
    }
  }

  std::vector<float> probabilities;
  probabilities.reserve(channel.operators.size());
  float total_p = 0.0f;

  if (all_scaled_unitary) {
    for (const auto& op : channel.operators) {
      float p = static_cast<float>(op.probability);
      probabilities.push_back(p);
      total_p += p;
    }
  } else {
    // Compute 2Q RDM to get state-dependent trajectory jump probabilities: P(i) = Tr(K_i† K_i ρ)
    auto sv = getStateVector();
    size_t stride1 = 1ULL << q1;
    size_t stride2 = 1ULL << q2;
    std::array<Complex, 16> rdm{};
    rdm.fill(Complex(0, 0));

    for (size_t j = 0; j < sv.size(); ++j) {
      if (!((j & stride1) || (j & stride2))) {
        size_t idx[4] = {j, j | stride2, j | stride1, j | stride1 | stride2};
        Complex amps[4] = {sv[idx[0]], sv[idx[1]], sv[idx[2]], sv[idx[3]]};
        for (int r = 0; r < 4; ++r) {
          for (int c = 0; c < 4; ++c) {
            rdm[r * 4 + c] += amps[r] * std::conj(amps[c]);
          }
        }
      }
    }

    for (const auto& op : channel.operators) {
      const auto& M = op.matrix_dag_self;
      Complex tr(0, 0);
      for (int k = 0; k < 16; ++k) tr += M[k] * rdm[k];
      float p = std::abs(static_cast<float>(tr.real()));
      probabilities.push_back(p);
      total_p += p;
    }
  }

  if (total_p < 1e-20f) return;

  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dis(0.0f, total_p);
  float r = dis(gen);

  const KrausOperator2Q* selected = &channel.operators.back();
  float selected_prob = probabilities.back();
  float cumulative = 0.0f;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cumulative += probabilities[i];
    if (r <= cumulative) {
      selected = &channel.operators[i];
      selected_prob = probabilities[i];
      break;
    }
  }

  if (selected_prob < 1e-20f) return;

  bool selected_is_unitary = isScaledUnitary2Q(selected->matrix, static_cast<float>(selected->probability));
  float inv_norm = (all_scaled_unitary && selected_is_unitary) ? (1.0f / std::sqrt(static_cast<float>(selected->probability))) : 1.0f;

  // 2. Convert 4×4 Kraus matrix to Metal float format
  std::vector<MetalComplex> m(16);
  for (int i = 0; i < 16; ++i) {
      m[i].real = static_cast<float>(selected->matrix[i].real());
      m[i].imag = static_cast<float>(selected->matrix[i].imag());
  }

  // 3. Dispatch the 2Q Kraus kernel
  uint32_t s_low = static_cast<uint32_t>(std::min(q1, q2));
  uint32_t s_high = static_cast<uint32_t>(std::max(q1, q2));
  size_t dim = 1ULL << num_qubits_;

  dispatchWithBuffers(kraus2qPipeline_, dim / 4,
                      {{0, gpuBuffer_}},
                      {{1, &s_low, sizeof(uint32_t)}, {2, &s_high, sizeof(uint32_t)}, {3, m.data(), 16*sizeof(MetalComplex)}, {4, &inv_norm, sizeof(float)}});

  if (!all_scaled_unitary) {
    renormalizeStateVector();
  }
}

int MetalBackend::measure(size_t target) {
  uint32_t stride = 1 << target;
  size_t dim = 1ULL << num_qubits_;
  size_t totalThreads = dim;
  size_t maxThreads = 256;
  size_t numGroups = (totalThreads + maxThreads - 1) / maxThreads;
  
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  id<MTLBuffer> partialSumsBuf = [device newBufferWithLength:numGroups * sizeof(float) options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
  id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)measureProb0Pipeline_;

  [enc setComputePipelineState:pso];
  [enc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
  [enc setBytes:&stride length:sizeof(uint32_t) atIndex:1];
  [enc setBuffer:partialSumsBuf offset:0 atIndex:2];
  [enc dispatchThreads:MTLSizeMake(totalThreads, 1, 1) threadsPerThreadgroup:MTLSizeMake(maxThreads, 1, 1)];
  [enc endEncoding];
  [cmdBuf commit];
  [cmdBuf waitUntilCompleted];

  float* sums = (float*)[partialSumsBuf contents];
  float prob0 = 0.0;
  for (size_t i = 0; i < numGroups; ++i) prob0 += sums[i];
  
  static std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dist(0.0, 1.0);
  int outcome = (dist(gen) > prob0) ? 1 : 0;
  float norm = (outcome == 0) ? std::sqrt(prob0) : std::sqrt(1.0 - prob0);
  if (norm < 1e-9) norm = 1.0;
  
  uint32_t out_val = outcome;
  id<MTLCommandBuffer> projCmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> projEnc = [projCmdBuf computeCommandEncoder];
  id<MTLComputePipelineState> projPso = (__bridge id<MTLComputePipelineState>)projectStatePipeline_;
  [projEnc setComputePipelineState:projPso];
  [projEnc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
  [projEnc setBytes:&stride length:sizeof(uint32_t) atIndex:1];
  [projEnc setBytes:&out_val length:sizeof(uint32_t) atIndex:2];
  [projEnc setBytes:&norm length:sizeof(float) atIndex:3];
  [projEnc dispatchThreads:MTLSizeMake(totalThreads, 1, 1) threadsPerThreadgroup:MTLSizeMake(maxThreads, 1, 1)];
  [projEnc endEncoding];
  [projCmdBuf commit];
  if (lastCommandBuffer_) CFRelease(lastCommandBuffer_);
  lastCommandBuffer_ = (void*)CFBridgingRetain(projCmdBuf);

  return outcome;
}

std::vector<double> MetalBackend::getProbabilities() const {
  size_t dim = 1ULL << num_qubits_;
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  id<MTLBuffer> probBuf = [device newBufferWithLength:dim * sizeof(float) options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
  id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)computeProbabilitiesPipeline_;
  [enc setComputePipelineState:pso];
  [enc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
  [enc setBuffer:probBuf offset:0 atIndex:1];
  [enc dispatchThreads:MTLSizeMake(dim, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
  [enc endEncoding];
  [cmdBuf commit];
  [cmdBuf waitUntilCompleted];
  float* probsPtr = (float*)[probBuf contents];
  std::vector<double> results(dim);
  for (size_t i = 0; i < dim; ++i) results[i] = (double)probsPtr[i];
  return results;
}

double MetalBackend::expectationValue(const std::string &pauli) const {
  uint64_t z_mask = 0;
  for (size_t i = 0; i < pauli.length(); ++i) {
    if (pauli[i] == 'Z') {
      z_mask |= (1ULL << i);
    } else if (pauli[i] == 'X') {
      z_mask |= (1ULL << i);
      const_cast<MetalBackend*>(this)->applyHadamard(i);
    } else if (pauli[i] == 'Y') {
      z_mask |= (1ULL << i);
      const_cast<MetalBackend*>(this)->applyRotationX(i, -M_PI / 2.0);
    }
  }

  size_t dim = 1ULL << num_qubits_;
  size_t totalThreads = dim;
  size_t maxThreads = 256;
  size_t numGroups = (totalThreads + maxThreads - 1) / maxThreads;
  
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  id<MTLBuffer> partialSumsBuf = [device newBufferWithLength:numGroups * sizeof(float)
                                                     options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
  id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)diagonalExpectationPipeline_;

  [enc setComputePipelineState:pso];
  [enc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
  [enc setBytes:&z_mask length:sizeof(uint64_t) atIndex:1];
  [enc setBuffer:partialSumsBuf offset:0 atIndex:2];
  [enc setBytes:&totalThreads length:sizeof(uint32_t) atIndex:3];
  [enc dispatchThreads:MTLSizeMake(totalThreads, 1, 1)
      threadsPerThreadgroup:MTLSizeMake(maxThreads, 1, 1)];
  [enc endEncoding];
  [cmdBuf commit];
  [cmdBuf waitUntilCompleted];

  float *sums = (float *)[partialSumsBuf contents];
  float expected_val = 0.0;
  for (size_t i = 0; i < numGroups; ++i) {
    expected_val += sums[i];
  }

  // Restore state vector basis
  for (size_t i = 0; i < pauli.length(); ++i) {
    if (pauli[i] == 'X') {
      const_cast<MetalBackend*>(this)->applyHadamard(i);
    } else if (pauli[i] == 'Y') {
      const_cast<MetalBackend*>(this)->applyRotationX(i, M_PI / 2.0);
    }
  }

  return (double)expected_val;
}

} // namespace qubit_engine

#else // Not Apple

namespace qubit_engine {
MetalBackend::MetalBackend(size_t n) {}
MetalBackend::~MetalBackend() {}
void MetalBackend::initializeMetal() {}
void MetalBackend::applyHadamard(size_t t) {}
void MetalBackend::applyX(size_t t) {}
void MetalBackend::applyY(size_t t) {}
void MetalBackend::applyZ(size_t t) {}
void MetalBackend::applyCNOT(size_t c, size_t t) {}
void MetalBackend::applyToffoli(size_t c1, size_t c2, size_t t) {}
void MetalBackend::applyPhaseS(size_t t) {}
void MetalBackend::applyPhaseT(size_t t) {}
void MetalBackend::applyRotationY(size_t t, Precision a) {}
void MetalBackend::applyRotationZ(size_t t, Precision a) {}
void MetalBackend::applyRotationX(size_t t, Precision a) {}
void MetalBackend::applySWAP(size_t q1, size_t q2) {}
void MetalBackend::applyCZ(size_t c, size_t t) {}
void MetalBackend::applyDepolarizingNoise(Precision p) {}
void MetalBackend::applyNoiseChannel1Q(const NoiseChannel1Q&, size_t) {}
void MetalBackend::applyNoiseChannel2Q(const NoiseChannel2Q&, size_t, size_t) {}
int MetalBackend::measure(size_t t) { return 0; }
std::vector<double> MetalBackend::getProbabilities() const { return {}; }
double MetalBackend::expectationValue(const std::string &p) const { return 0.0; }
std::vector<Complex> MetalBackend::getStateVector() const { return {}; }
void MetalBackend::applyDenseUnitary(const std::vector<size_t>&, const std::vector<Complex>&) {}
} // namespace qubit_engine

#endif
