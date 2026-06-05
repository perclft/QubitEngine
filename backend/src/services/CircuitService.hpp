#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include "qubit_engine_export.h"

namespace qubit_engine {
class QuantumRegister;

namespace services {

class QUBIT_ENGINE_EXPORT CircuitService {
public:
  CircuitService() = default;

  grpc::Status RunCircuit(grpc::ServerContext *context,
                          const qubit_engine::CircuitRequest *request,
                          qubit_engine::StateResponse *response);

  grpc::Status StreamGates(
      grpc::ServerContext *context,
      grpc::ServerReaderWriter<qubit_engine::StateResponse,
                               qubit_engine::GateStreamRequest> *stream);

  grpc::Status VisualizeCircuit(
      grpc::ServerContext *context, const qubit_engine::CircuitRequest *request,
      grpc::ServerWriter<qubit_engine::StateResponse> *writer);

  static bool hasEnoughMemory(int num_qubits);

private:
  void applyGate(qubit_engine::QuantumRegister &qreg,
                 const qubit_engine::GateOperation &op,
                 qubit_engine::StateResponse *response);

  void serializeState(const qubit_engine::QuantumRegister &qreg,
                      qubit_engine::StateResponse *response,
                      qubit_engine::CircuitRequest::MeasurementStrategy
                          strategy = qubit_engine::CircuitRequest::FULL_STATE,
                      bool use_shm = false);
};

} // namespace services
} // namespace qubit_engine
