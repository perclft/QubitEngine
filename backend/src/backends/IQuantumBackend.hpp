#pragma once
#include <api/proto/quantum.grpc.pb.h>

class IQuantumBackend {
public:
  virtual ~IQuantumBackend() = default;

  // Apply a single gate operation
  virtual void applyGate(const qubit_engine::GateOperation &op) = 0;

  // Retrieve the current state (or measurement results)
  // For Simulator: Returns full state vector
  // For Hardware: Returns measurement counts (StateVector might be empty or
  // approximated)
  virtual void getResult(qubit_engine::StateResponse *response) = 0;
};
