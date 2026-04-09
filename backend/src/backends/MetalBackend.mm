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

  // Load Library
  NSError *error = nil;
  id<MTLLibrary> library =
      [device newDefaultLibraryWithBundle:[NSBundle mainBundle] error:&error];
  if (!library) {
    spdlog::info("MetalBackend::initializeMetal - Main Bundle Library not "
                 "found, trying file...");
    // Try to load from "default.metallib" in multiple locs
    NSArray<NSString *> *paths = @[
      @"default.metallib", @"bin/default.metallib", @"../bin/default.metallib",
      @"backend/build/default.metallib"
    ];
    for (NSString *path in paths) {
      NSURL *url = [NSURL fileURLWithPath:path];
      library = [device newLibraryWithURL:url error:&error];
      if (library) {
        spdlog::info("MetalBackend::initializeMetal - Loaded from {}",
                  [path UTF8String]);
        break;
      }
    }
  }
  if (!library) {
    spdlog::error("MetalBackend::initializeMetal - Failed to load library: {}",
        [[error localizedDescription] UTF8String]);
    throw std::runtime_error(std::string("Could not load Metal library: ") +
                             [[error localizedDescription] UTF8String]);
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
  applyCNOT(qubit1, qubit2);
  applyCNOT(qubit2, qubit1);
  applyCNOT(qubit1, qubit2);
}

void MetalBackend::applyCZ(size_t control, size_t target) {
  applyHadamard(target);
  applyCNOT(control, target);
  applyHadamard(target);
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
  if (targets.size() == 1) {
    uint32_t stride = 1 << targets[0];
    std::vector<MetalComplex> m(4);
    for (int i = 0; i < 4; ++i) {
        m[i].real = (float)matrix[i].real();
        m[i].imag = (float)matrix[i].imag();
    }
    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    id<MTLBuffer> mBuf = [device newBufferWithBytes:m.data() length:4*sizeof(MetalComplex) options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
    id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)denseUnitary1qPipeline_;
    [enc setComputePipelineState:pso];
    [enc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
    [enc setBytes:&stride length:sizeof(uint32_t) atIndex:1];
    [enc setBuffer:mBuf offset:0 atIndex:2];
    [enc dispatchThreads:MTLSizeMake(dim/2, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
    [enc endEncoding];
    [cmdBuf commit];
    if (lastCommandBuffer_) CFRelease(lastCommandBuffer_);
    lastCommandBuffer_ = (void*)CFBridgingRetain(cmdBuf);
  } else if (targets.size() == 2) {
    uint32_t s_low = (uint32_t)targets[0];
    uint32_t s_high = (uint32_t)targets[1];
    if (s_low > s_high) std::swap(s_low, s_high);
    std::vector<MetalComplex> m(16);
    for (int i = 0; i < 16; ++i) {
        m[i].real = (float)matrix[i].real();
        m[i].imag = (float)matrix[i].imag();
    }
    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    id<MTLBuffer> mBuf = [device newBufferWithBytes:m.data() length:16*sizeof(MetalComplex) options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
    id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)denseUnitary2qPipeline_;
    [enc setComputePipelineState:pso];
    [enc setBuffer:(__bridge id<MTLBuffer>)gpuBuffer_ offset:0 atIndex:0];
    [enc setBytes:&s_low length:sizeof(uint32_t) atIndex:1];
    [enc setBytes:&s_high length:sizeof(uint32_t) atIndex:2];
    [enc setBuffer:mBuf offset:0 atIndex:3];
    [enc dispatchThreads:MTLSizeMake(dim/4, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
    [enc endEncoding];
    [cmdBuf commit];
    if (lastCommandBuffer_) CFRelease(lastCommandBuffer_);
    lastCommandBuffer_ = (void*)CFBridgingRetain(cmdBuf);
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

std::vector<double> MetalBackend::getProbabilities() {
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
  bool is_diagonal = true;
  for (size_t i = 0; i < pauli.length(); ++i) {
    if (pauli[i] == 'Z') {
      z_mask |= (1ULL << i);
    } else if (pauli[i] != 'I') {
      is_diagonal = false;
      break;
    }
  }

  if (!is_diagonal) {
    // For non-diagonal, we could perform basis rotation on GPU, 
    // but for now we fallback or return 0 for unsupported non-diagonal on Metal
    spdlog::warn("MetalBackend: Non-diagonal expectation values not yet supported on GPU.");
    return 0.0;
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
int MetalBackend::measure(size_t t) { return 0; }
std::vector<double> MetalBackend::getProbabilities() { return {}; }
double MetalBackend::expectationValue(const std::string &p) const { return 0.0; }
std::vector<Complex> MetalBackend::getStateVector() const { return {}; }
} // namespace qubit_engine

#endif
