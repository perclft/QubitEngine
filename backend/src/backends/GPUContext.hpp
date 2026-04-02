#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <stdexcept>
#include <string>

// Conditional Includes
#ifdef ENABLE_CUDA
#include <cuda_runtime.h>
#define GPU_SUCCESS cudaSuccess
#define GPU_Error_t cudaError_t
#define gpuGetErrorString cudaGetErrorString
#define gpuMalloc cudaMalloc
#define gpuFree cudaFree
#define gpuMemcpy cudaMemcpy
#define gpuMemcpyHostToDevice cudaMemcpyHostToDevice
#define gpuMemcpyDeviceToHost cudaMemcpyDeviceToHost
#elif defined(ENABLE_ROCM)
#include <hip/hip_runtime.h>
#define GPU_SUCCESS hipSuccess
#define GPU_Error_t hipError_t
#define gpuGetErrorString hipGetErrorString
#define gpuMalloc hipMalloc
#define gpuFree hipFree
#define gpuMemcpy hipMemcpy
#define gpuMemcpyHostToDevice hipMemcpyHostToDevice
#define gpuMemcpyDeviceToHost hipMemcpyDeviceToHost
#elif defined(ENABLE_METAL)
// Metal specifics are hidden behind the pure C++ context interface
// to prevent Objective-C pollution in core C++ files.
#include "metal/MetalContext.h"
#define GPU_SUCCESS 0
#define GPU_Error_t int
#define gpuGetErrorString(err) "Metal Error"
#define gpuMalloc(ptr, size) MetalContext::getInstance().allocate(ptr, size)
#define gpuFree(ptr) MetalContext::getInstance().free(ptr)
#define gpuMemcpyHostToDevice 1
#define gpuMemcpyDeviceToHost 2
#define gpuMemcpy(dst, src, size, dir)                                         \
  ((dir == gpuMemcpyHostToDevice)                                              \
       ? MetalContext::getInstance().copyToDevice(dst, src, size)              \
       : MetalContext::getInstance().copyToHost(dst, src, size))
#endif

class GPUContext {
public:
  static GPUContext &getInstance() {
    static GPUContext instance;
    return instance;
  }

  void initialize() {
    if (initialized)
      return;
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM) || defined(ENABLE_METAL)
    // Check for device count
    int deviceCount = 0;
#ifdef ENABLE_CUDA
    cudaGetDeviceCount(&deviceCount);
#elif defined(ENABLE_ROCM)
    hipGetDeviceCount(&deviceCount);
#elif defined(ENABLE_METAL)
    deviceCount = MetalContext::getInstance().getDeviceCount();
#endif

    if (deviceCount == 0) {
      spdlog::warn("No GPU devices found.");
      return;
    }

    // Select device 0
#ifdef ENABLE_CUDA
    cudaSetDevice(0);
#elif defined(ENABLE_ROCM)
    hipSetDevice(0);
#elif defined(ENABLE_METAL)
    MetalContext::getInstance().initializeDevice(0);
#endif
    spdlog::info("GPU Context Initialized. Devices: {}", deviceCount);
    initialized = true;
#else
    spdlog::warn("GPU compilation disabled.");
#endif
  }

  void *allocate(size_t size) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM) || defined(ENABLE_METAL)
    void *ptr = nullptr;
    GPU_Error_t err = gpuMalloc(&ptr, size);
    if (err != GPU_SUCCESS) {
      throw std::runtime_error("GPU Malloc failed: " +
                               std::string(gpuGetErrorString(err)));
    }
    return ptr;
#else
    throw std::runtime_error("GPU not enabled.");
#endif
  }

  void free(void *ptr) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM) || defined(ENABLE_METAL)
    if (ptr)
      gpuFree(ptr);
#endif
  }

  void copyToDevice(void *dst, const void *src, size_t size) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM) || defined(ENABLE_METAL)
    gpuMemcpy(dst, src, size, gpuMemcpyHostToDevice);
#endif
  }

  void copyToHost(void *dst, const void *src, size_t size) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM) || defined(ENABLE_METAL)
    gpuMemcpy(dst, src, size, gpuMemcpyDeviceToHost);
#endif
  }

private:
  GPUContext() = default;
  ~GPUContext() = default;
  bool initialized = false;
};
