#include "MetalContext.h"
#include <iostream>
#include <stdexcept>


#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>


class MetalContext::Impl {
public:
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> commandQueue = nil;
  id<MTLLibrary> defaultLibrary = nil;

  Impl() {}
  ~Impl() {}
};

MetalContext::MetalContext() : pImpl(new Impl()) {}
MetalContext::~MetalContext() { delete pImpl; }

MetalContext &MetalContext::getInstance() {
  static MetalContext instance;
  return instance;
}

int MetalContext::getDeviceCount() {
  // Metal inherently supports 1 default device on Apple Silicon
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  return device ? 1 : 0;
}

void MetalContext::initializeDevice(int deviceId) {
  pImpl->device = MTLCreateSystemDefaultDevice();
  if (!pImpl->device) {
    throw std::runtime_error("Metal is not supported on this device.");
  }
  pImpl->commandQueue = [pImpl->device newCommandQueue];
}

bool MetalContext::isInitialized() const { return pImpl->device != nil; }

int MetalContext::allocate(void **ptr, size_t size) {
  if (!pImpl->device)
    return 1;
  // For Metal, we allocate a shared buffer
  id<MTLBuffer> buffer =
      [pImpl->device newBufferWithLength:size
                                 options:MTLResourceStorageModeShared];
  if (!buffer)
    return 1;

  *ptr = (__bridge_retained void *)buffer;
  return 0; // Success
}

void MetalContext::free(void *ptr) {
  if (ptr) {
    // Transfer ownership back to ARC to release
    id<MTLBuffer> buffer = (__bridge_transfer id<MTLBuffer>)ptr;
    buffer = nil;
  }
}

int MetalContext::copyToDevice(void *dst, const void *src, size_t size) {
  id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)dst;
  memcpy([buffer contents], src, size);
  return 0;
}

int MetalContext::copyToHost(void *dst, const void *src, size_t size) {
  id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)src;
  memcpy(dst, [buffer contents], size);
  return 0;
}

void MetalContext::loadLibrary(const std::string &path) {
  if (!pImpl->device)
    return;

  @autoreleasepool {
    NSString *libPath = [NSString stringWithUTF8String:path.c_str()];
    NSError *error = nil;
    NSURL *url = [NSURL fileURLWithPath:libPath];
    pImpl->defaultLibrary = [pImpl->device newLibraryWithURL:url error:&error];

    if (!pImpl->defaultLibrary) {
      std::cerr << "Failed to load Metal library: " <<
          [[error localizedDescription] UTF8String] << std::endl;
    }
  }
}

void MetalContext::dispatchCompute(const std::string &kernelName,
                                   void *bufferPtr, size_t bufferSize,
                                   size_t gridDim, size_t blockDim) {
  if (!pImpl->device || !pImpl->defaultLibrary)
    return;

  @autoreleasepool {
    NSString *name = [NSString stringWithUTF8String:kernelName.c_str()];
    id<MTLFunction> function = [pImpl->defaultLibrary newFunctionWithName:name];
    if (!function) {
      std::cerr << "Failed to find Metal function: " << kernelName << std::endl;
      return;
    }

    NSError *error = nil;
    id<MTLComputePipelineState> pipelineState =
        [pImpl->device newComputePipelineStateWithFunction:function
                                                     error:&error];
    if (!pipelineState) {
      std::cerr << "Failed to create pipeline state: " <<
          [[error localizedDescription] UTF8String] << std::endl;
      return;
    }

    id<MTLCommandBuffer> commandBuffer = [pImpl->commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> computeEncoder =
        [commandBuffer computeCommandEncoder];

    [computeEncoder setComputePipelineState:pipelineState];

    id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)bufferPtr;
    [computeEncoder setBuffer:buffer offset:0 atIndex:0];

    MTLSize gridSize = MTLSizeMake(gridDim * blockDim, 1, 1);
    MTLSize threadGroupSize = MTLSizeMake(blockDim, 1, 1);

    [computeEncoder dispatchThreads:gridSize
              threadsPerThreadgroup:threadGroupSize];
    [computeEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
  }
}

#else

// Windows / Linux Fallback dummy implementations if they compile this somehow
class MetalContext::Impl {};
MetalContext::MetalContext() : pImpl(nullptr) {}
MetalContext::~MetalContext() {}
MetalContext &MetalContext::getInstance() {
  static MetalContext instance;
  return instance;
}
int MetalContext::getDeviceCount() { return 0; }
void MetalContext::initializeDevice(int) {}
bool MetalContext::isInitialized() const { return false; }
int MetalContext::allocate(void **, size_t) { return 1; }
void MetalContext::free(void *) {}
int MetalContext::copyToDevice(void *, const void *, size_t) { return 1; }
int MetalContext::copyToHost(void *, const void *, size_t) { return 1; }
void MetalContext::loadLibrary(const std::string &) {}
void MetalContext::dispatchCompute(const std::string &, void *, size_t, size_t,
                                   size_t) {}

#endif
