#pragma once

#include <iostream>
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
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM)
    // Check for device count
    int deviceCount = 0;
#ifdef ENABLE_CUDA
    cudaGetDeviceCount(&deviceCount);
#else
    hipGetDeviceCount(&deviceCount);
#endif
    if (deviceCount == 0) {
      std::cerr << "Warning: No GPU devices found." << std::endl;
      return;
    }

    // Select device 0
#ifdef ENABLE_CUDA
    cudaSetDevice(0);
#else
    hipSetDevice(0);
#endif
    std::cout << "GPU Context Initialized. Devices: " << deviceCount
              << std::endl;
    initialized = true;
#else
    std::cerr << "Warning: GPU compilation disabled." << std::endl;
#endif
  }

  void *allocate(size_t size) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM)
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
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM)
    if (ptr)
      gpuFree(ptr);
#endif
  }

  void copyToDevice(void *dst, const void *src, size_t size) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM)
    gpuMemcpy(dst, src, size, gpuMemcpyHostToDevice);
#endif
  }

  void copyToHost(void *dst, const void *src, size_t size) {
#if defined(ENABLE_CUDA) || defined(ENABLE_ROCM)
    gpuMemcpy(dst, src, size, gpuMemcpyDeviceToHost);
#endif
  }

private:
  GPUContext() = default;
  ~GPUContext() = default;
  bool initialized = false;
};
