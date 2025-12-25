#include "MetalContext.hpp"
#include "Shaders.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <iostream>

MetalContext &MetalContext::getInstance() {
  static MetalContext instance;
  return instance;
}

MetalContext::MetalContext() {}
MetalContext::~MetalContext() {
  // ARC handles definition but void* cast requires care if bridging retained.
  // Assuming simple globals/static lifecycle for MVP.
}

bool MetalContext::isAvailable() const { return initialized_; }

void MetalContext::initialize() {
  if (initialized_)
    return;

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    std::cerr << "Metal: No device found!" << std::endl;
    return;
  }

  std::cout << "Metal: Using device " << [[device name] UTF8String]
            << std::endl;

  id<MTLCommandQueue> queue = [device newCommandQueue];

  // Compile Shaders
  NSError *error = nil;
  NSString *source = [NSString stringWithUTF8String:METAL_SHADERS_SOURCE];
  id<MTLLibrary> library = [device newLibraryWithSource:source
                                                options:nil
                                                  error:&error];

  if (!library) {
    std::cerr << "Metal: Library compilation failed: " <<
        [[error localizedDescription] UTF8String] << std::endl;
    return;
  }

  id<MTLFunction> hadamardFunc =
      [library newFunctionWithName:@"hadamard_kernel"];
  id<MTLComputePipelineState> hadamardPSO =
      [device newComputePipelineStateWithFunction:hadamardFunc error:&error];

  if (!hadamardPSO) {
    std::cerr << "Metal: Hadamard PSO failed" << std::endl;
    return;
  }

  // Assign to void* properties (bridged retain)
  device_ = (__bridge_retained void *)device;
  commandQueue_ = (__bridge_retained void *)queue;
  library_ = (__bridge_retained void *)library;
  hadamardPipeline_ = (__bridge_retained void *)hadamardPSO;

  initialized_ = true;
}

void MetalContext::uploadState(double *cpuData, size_t num_qubits) {
  if (!initialized_)
    return;

  size_t num_elements =
      (1ULL << num_qubits); // Number of Complex numbers (pairs of doubles)
  // Note: cpuData has 2 * num_elements doubles.

  // Allocate GPU buffer if needed
  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  size_t floatSize = num_elements * sizeof(float) * 2; // Complex<float>

  // Re-allocate if size changed or not exists
  id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)gpuBuffer_;
  if (!mtlBuf || bufferCapacity_ < num_elements) {
    if (gpuBuffer_) {
      CFRelease(gpuBuffer_);
    }
    mtlBuf = [device newBufferWithLength:floatSize
                                 options:MTLResourceStorageModeShared];
    gpuBuffer_ = (__bridge_retained void *)mtlBuf;
    bufferCapacity_ = num_elements;
  }

  // Convert and Copy
  float *gpuPtr = static_cast<float *>([mtlBuf contents]);
  for (size_t i = 0; i < num_elements * 2; ++i) {
    gpuPtr[i] = static_cast<float>(cpuData[i]);
  }
}

void MetalContext::downloadState(double *cpuData, size_t num_qubits) {
  if (!initialized_ || !gpuBuffer_)
    return;

  id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)gpuBuffer_;
  float *gpuPtr = static_cast<float *>([mtlBuf contents]);

  size_t num_elements = (1ULL << num_qubits);
  for (size_t i = 0; i < num_elements * 2; ++i) {
    cpuData[i] = static_cast<double>(gpuPtr[i]);
  }
}

void MetalContext::runHadamard(size_t num_qubits, size_t target_qubit) {
  if (!initialized_ || !gpuBuffer_)
    return;

  // Uses resident buffer
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLComputePipelineState> pso =
      (__bridge id<MTLComputePipelineState>)hadamardPipeline_;
  id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)gpuBuffer_;

  id<MTLCommandBuffer> bufferCmd = [queue commandBuffer];
  id<MTLComputeCommandEncoder> encoder = [bufferCmd computeCommandEncoder];

  [encoder setComputePipelineState:pso];
  [encoder setBuffer:mtlBuf offset:0 atIndex:0];

  uint32_t stride = (1 << target_qubit);
  [encoder setBytes:&stride length:sizeof(uint32_t) atIndex:1];

  NSUInteger totalThreads = (1ULL << (num_qubits - 1));
  NSUInteger w = pso.threadExecutionWidth;
  NSUInteger h = pso.maxTotalThreadsPerThreadgroup;

  MTLSize threadsPerGroup = MTLSizeMake(h, 1, 1);
  MTLSize gridSize = MTLSizeMake(totalThreads, 1, 1);

  [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadsPerGroup];
  [encoder endEncoding];

  [bufferCmd commit];
  [bufferCmd waitUntilCompleted];
}

void MetalContext::runHadamard(void *buffer, size_t num_qubits,
                               size_t target_qubit) {
  if (!initialized_)
    return;

  id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue_;
  id<MTLComputePipelineState> pso =
      (__bridge id<MTLComputePipelineState>)hadamardPipeline_;

  size_t dataSize = (1ULL << num_qubits) * sizeof(double) * 2; // Complex

  // Zero-Copy Strategy:
  // We assume 'buffer' is page-aligned (std::vector might not be).
  // If we use newBufferWithBytesNoCopy, it MUST be aligned.
  // For MVP safety, let's use a temporary staging buffer or newBufferWithBytes
  // (copy). Copy is slow (host->device), but good for MVP functionality logic
  // check.

  // To truly benefit, we should reuse a persistent MTLBuffer?
  // For this one-shot call:
  id<MTLBuffer> mtlBuf =
      [device newBufferWithBytes:buffer
                          length:dataSize
                         options:MTLResourceStorageModeShared];

  id<MTLCommandBuffer> bufferCmd = [queue commandBuffer];
  id<MTLComputeCommandEncoder> encoder = [bufferCmd computeCommandEncoder];

  [encoder setComputePipelineState:pso];
  [encoder setBuffer:mtlBuf offset:0 atIndex:0];

  // Stride = 2^target
  uint32_t stride = (1 << target_qubit);
  [encoder setBytes:&stride length:sizeof(uint32_t) atIndex:1];

  // Dispatch
  // Total items = 2^N.
  // Each thread handles 2 items (a pair).
  // Total threads = 2^N / 2 = 2^(N-1).
  NSUInteger totalThreads = (1ULL << (num_qubits - 1));

  // Max threads per group usually 512 or 1024
  NSUInteger w = pso.threadExecutionWidth;
  NSUInteger h = pso.maxTotalThreadsPerThreadgroup;

  MTLSize threadsPerGroup = MTLSizeMake(h, 1, 1);
  MTLSize gridSize = MTLSizeMake(totalThreads, 1, 1);

  [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadsPerGroup];
  [encoder endEncoding];

  [bufferCmd commit];
  [bufferCmd waitUntilCompleted];

  // Copy back
  memcpy(buffer, [mtlBuf contents], dataSize);
}
