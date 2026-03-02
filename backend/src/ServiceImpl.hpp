#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace qubit_engine {
class QuantumRegister;
}

class QubitEngineServiceImpl final
    : public qubit_engine::QuantumCompute::Service {
public:
  grpc::Status RunCircuit(grpc::ServerContext *context,
                          const qubit_engine::CircuitRequest *request,
                          qubit_engine::StateResponse *response) override;

  grpc::Status StreamGates(
      grpc::ServerContext *context,
      grpc::ServerReaderWriter<qubit_engine::StateResponse,
                               qubit_engine::GateOperation> *stream) override;

  grpc::Status VisualizeCircuit(
      grpc::ServerContext *context, const qubit_engine::CircuitRequest *request,
      grpc::ServerWriter<qubit_engine::StateResponse> *writer) override;

  grpc::Status
  RunVQE(grpc::ServerContext *context, const qubit_engine::VQERequest *request,
         grpc::ServerWriter<qubit_engine::VQEResponse> *writer) override;

private:
  void applyGate(qubit_engine::QuantumRegister &qreg,
                 const qubit_engine::GateOperation &op,
                 qubit_engine::StateResponse *response);
  void serializeState(const qubit_engine::QuantumRegister &qreg,
                      qubit_engine::StateResponse *response);
};
