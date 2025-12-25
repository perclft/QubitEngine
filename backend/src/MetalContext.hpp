#pragma once

#include <cstddef>
#include <vector>

class MetalContext {
public:
  static MetalContext &getInstance();

  bool isAvailable() const;
  void initialize();

  // Persistent State Management
  void uploadState(double *cpuData, size_t num_qubits);
  void downloadState(double *cpuData, size_t num_qubits);

  // Runs Hadamard on the resident GPU buffer
  void runHadamard(size_t num_qubits, size_t target_qubit);

  // Runs Hadamard on the given buffer (must be compatible with Complex dict)
  void runHadamard(void *buffer, size_t num_qubits, size_t target_qubit);

private:
  MetalContext();
  ~MetalContext();

  // Pimpl idiom using void* to hide Obj-C types from C++ headers
  void *device_ = nullptr;
  void *commandQueue_ = nullptr;
  void *library_ = nullptr;
  void *hadamardPipeline_ = nullptr;
  void *paulixPipeline_ = nullptr;

  // Persistent Buffer
  void *gpuBuffer_ = nullptr;
  size_t bufferCapacity_ = 0; // In number of Complex<float> elements

  bool initialized_ = false;
};
