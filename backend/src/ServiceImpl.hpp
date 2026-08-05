#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace qubit_engine {
namespace auth { class AuthInterceptor; }
namespace services {
class CircuitService;
class VQEService;
class TopologyService;
}
}

class QubitEngineServiceImpl final
    : public qubit_engine::QuantumCompute::Service {
public:
  QubitEngineServiceImpl();
  ~QubitEngineServiceImpl() override;

  grpc::Status RunCircuit(grpc::ServerContext *context,
                          const qubit_engine::CircuitRequest *request,
                          qubit_engine::StateResponse *response) override;

  grpc::Status StreamGates(
      grpc::ServerContext *context,
      grpc::ServerReaderWriter<qubit_engine::StateResponse,
                               qubit_engine::GateStreamRequest> *stream) override;

  grpc::Status VisualizeCircuit(
      grpc::ServerContext *context, const qubit_engine::CircuitRequest *request,
      grpc::ServerWriter<qubit_engine::StateResponse> *writer) override;

  grpc::Status
  RunVQE(grpc::ServerContext *context, const qubit_engine::VQERequest *request,
         grpc::ServerWriter<qubit_engine::VQEResponse> *writer) override;

  grpc::Status GetHardwareTopology(
      grpc::ServerContext *context,
      const qubit_engine::HardwareTopologyRequest *request,
      qubit_engine::HardwareTopologyResponse *response) override;

  grpc::Status AcknowledgeShmRead(
      grpc::ServerContext *context,
      const qubit_engine::ShmAckRequest *request,
      qubit_engine::ShmAckResponse *response) override;

private:
  std::unique_ptr<qubit_engine::auth::AuthInterceptor> auth_interceptor_;
  std::unique_ptr<qubit_engine::services::CircuitService> circuit_service_;
  std::unique_ptr<qubit_engine::services::VQEService> vqe_service_;
  std::unique_ptr<qubit_engine::services::TopologyService> topology_service_;
};
