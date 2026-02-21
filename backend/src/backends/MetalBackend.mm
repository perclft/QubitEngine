#include "MetalBackend.hpp"

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <iostream>

namespace qubit_engine {

// --- Lifecycle ---

MetalBackend::MetalBackend(size_t num_qubits)
    : num_qubits_(num_qubits), device_(nullptr), commandQueue_(nullptr),
      gpuBuffer_(nullptr) {
  initializeMetal();

  // Initialize State |0...0>
  size_t dim = 1ULL << num_qubits;
  std::vector<Complex> initial(dim, {0.0f, 0.0f});
  initial[0] = {1.0f, 0.0f};
  uploadState(initial);
}

MetalBackend::~MetalBackend() {
  if (gpuBuffer_)
    CFRelease(gpuBuffer_);
  if (device_)
    CFRelease(device_);
  if (commandQueue_)
    CFRelease(commandQueue_);
}

void MetalBackend::initializeMetal() {
  std::cout << "MetalBackend::initializeMetal - Start" << std::endl;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    throw std::runtime_error("Metal is not supported on this device.");
  }
  std::cout << "MetalBackend::initializeMetal - Device Created: " <<
      [[device name] UTF8String] << std::endl;
  device_ = (void *)CFBridgingRetain(device);

  id<MTLCommandQueue> queue = [device newCommandQueue];
  commandQueue_ = (void *)CFBridgingRetain(queue);
  std::cout << "MetalBackend::initializeMetal - CommandQueue Created"
            << std::endl;

  // Load Library
  NSError *error = nil;
  id<MTLLibrary> library =
      [device newDefaultLibraryWithBundle:[NSBundle mainBundle] error:&error];
  if (!library) {
    std::cout << "MetalBackend::initializeMetal - Main Bundle Library not "
                 "found, trying file..."
              << std::endl;
    // Try to load from "default.metallib" in current dir
    NSString *path = @"default.metallib";
    NSURL *url = [NSURL fileURLWithPath:path];
    library = [device newLibraryWithURL:url error:&error];
  }
  if (!library) {
    std::cerr << "MetalBackend::initializeMetal - Failed to load library: " <<
        [[error localizedDescription] UTF8String] << std::endl;
    throw std::runtime_error(std::string("Could not load Metal library: ") +
                             [[error localizedDescription] UTF8String]);
  }
  std::cout << "MetalBackend::initializeMetal - Library Loaded" << std::endl;

  buildPipelines((void *)CFBridgingRetain(library));
  std::cout << "MetalBackend::initializeMetal - Pipelines Built" << std::endl;
}

void MetalBackend::buildPipelines(void *libPtr) {
  id<MTLLibrary> library = (__bridge id<MTLLibrary>)libPtr;
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  NSError *error = nil;

  auto createPipe = [&](NSString *name) -> void * {
    id<MTLFunction> func = [library newFunctionWithName:name];
    if (!func) {
      std::cerr << "Error: Function " << [name UTF8String] << " not found."
                << std::endl;
      return nullptr;
    }
    id<MTLComputePipelineState> pso =
        [device newComputePipelineStateWithFunction:func error:&error];
    if (!pso) {
      std::cerr << "Error creating pipeline for " << [name UTF8String]
                << std::endl;
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
  size_t bufferSize = num_elements * sizeof(Complex);

  if (gpuBuffer_)
    CFRelease(gpuBuffer_);

  id<MTLBuffer> mtlBuf =
      [device newBufferWithLength:bufferSize
                          options:MTLResourceStorageModeShared];
  gpuBuffer_ = (void *)CFBridgingRetain(mtlBuf);
  capacity_ = num_elements;

  void *ptr = [mtlBuf contents];
  std::memcpy(ptr, cpuState.data(), bufferSize);
}

void MetalBackend::downloadState(std::vector<Complex> &cpuState) const {
  id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)gpuBuffer_;

  // SYNC: Wait for all pending GPU work to finish
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
  [cmdBuf commit];
  [cmdBuf waitUntilCompleted];

  void *ptr = [mtlBuf contents];
  size_t num_elements = 1ULL << num_qubits_;

  cpuState.resize(num_elements);
  std::memcpy(cpuState.data(), ptr, num_elements * sizeof(Complex));
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
  // Removed [cmdBuf waitUntilCompleted] for Async Execution!
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
  // Stilling stubbed for now until logic verified
}
void MetalBackend::applyDepolarizingNoise(Precision p) {}

int MetalBackend::measure(size_t target) {
  // SYNC is implicit here because getStateVector calls downloadState which we
  // just synced.
  auto state = getStateVector();

  double prob0 = 0.0;
  size_t stride = 1ULL << target;
  for (size_t i = 0; i < state.size(); ++i) {
    if (!(i & stride))
      prob0 += std::norm(state[i]);
  }

  int outcome = (static_cast<double>(rand()) / RAND_MAX > prob0) ? 1 : 0;

  double norm = 0.0;
  if (outcome == 0) {
    for (size_t i = 0; i < state.size(); ++i) {
      if (i & stride)
        state[i] = 0.0f;
      else
        norm += std::norm(state[i]);
    }
  } else {
    for (size_t i = 0; i < state.size(); ++i) {
      if (!(i & stride))
        state[i] = 0.0f;
      else
        norm += std::norm(state[i]);
    }
  }

  norm = std::sqrt(norm);
  if (norm > 1e-9) {
    for (auto &val : state)
      val /= (Precision)norm;
  }
  uploadState(state);

  return outcome;
}

std::vector<double> MetalBackend::getProbabilities() { return {}; }

double MetalBackend::expectationValue(const std::string &pauli) { return 0.0; }

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
double MetalBackend::expectationValue(const std::string &p) { return 0.0; }
std::vector<Complex> MetalBackend::getStateVector() const { return {}; }
} // namespace qubit_engine

#endif
