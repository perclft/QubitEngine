#pragma once

#include <cstddef>
#include <string>

// Pure C++ header. No Metal/Objective-C frameworks here.
// This allows cross-platform compilation of files that include GPUContext.hpp

class MetalContext {
public:
  static MetalContext &getInstance();

  // Prevent copying
  MetalContext(const MetalContext &) = delete;
  void operator=(const MetalContext &) = delete;

  // Initialization
  int getDeviceCount();
  void initializeDevice(int deviceId);
  bool isInitialized() const;

  // Memory Management
  int allocate(void **ptr, size_t size);
  void free(void *ptr);

  // Data Transfer
  int copyToDevice(void *dst, const void *src, size_t size);
  int copyToHost(void *dst, const void *src, size_t size);

  // Compute Pipeline Initialization
  void loadLibrary(const std::string &path);
  void dispatchCompute(const std::string &kernelName, void *buffer,
                       size_t bufferSize, size_t gridDim, size_t blockDim);

private:
  MetalContext();
  ~MetalContext();

  // Opaque pointer to the Objective-C++ implementation
  class Impl;
  Impl *pImpl;
};
