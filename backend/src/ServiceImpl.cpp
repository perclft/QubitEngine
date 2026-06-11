#include "ServiceImpl.hpp"
#include "auth/AuthInterceptor.hpp"
#include "services/CircuitService.hpp"
#include "services/VQEService.hpp"
#include "services/TopologyService.hpp"
#include <spdlog/spdlog.h>

QubitEngineServiceImpl::QubitEngineServiceImpl()
    : auth_interceptor_(std::make_unique<qubit_engine::auth::AuthInterceptor>()),
      circuit_service_(std::make_unique<qubit_engine::services::CircuitService>()),
      vqe_service_(std::make_unique<qubit_engine::services::VQEService>()),
      topology_service_(std::make_unique<qubit_engine::services::TopologyService>()) {
  spdlog::info("QubitEngineServiceImpl initialized with specialized domain services.");
}

QubitEngineServiceImpl::~QubitEngineServiceImpl() = default;

grpc::Status QubitEngineServiceImpl::RunCircuit(grpc::ServerContext *context,
                                                const qubit_engine::CircuitRequest *request,
                                                qubit_engine::StateResponse *response) {
  bool authorized = auth_interceptor_->ValidateAuth(context);
  return circuit_service_->RunCircuit(context, request, response, authorized);
}

grpc::Status QubitEngineServiceImpl::StreamGates(
    grpc::ServerContext *context,
    grpc::ServerReaderWriter<qubit_engine::StateResponse,
                             qubit_engine::GateStreamRequest> *stream) {
  bool authorized = auth_interceptor_->ValidateAuth(context);
  return circuit_service_->StreamGates(context, stream, authorized);
}

grpc::Status QubitEngineServiceImpl::VisualizeCircuit(
    grpc::ServerContext *context, const qubit_engine::CircuitRequest *request,
    grpc::ServerWriter<qubit_engine::StateResponse> *writer) {
  bool authorized = auth_interceptor_->ValidateAuth(context);
  return circuit_service_->VisualizeCircuit(context, request, writer, authorized);
}

grpc::Status QubitEngineServiceImpl::RunVQE(
    grpc::ServerContext *context, const qubit_engine::VQERequest *request,
    grpc::ServerWriter<qubit_engine::VQEResponse> *writer) {
  if (!auth_interceptor_->ValidateAuth(context)) {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or missing authorization token");
  }
  return vqe_service_->RunVQE(context, request, writer);
}

grpc::Status QubitEngineServiceImpl::GetHardwareTopology(
    grpc::ServerContext *context,
    const qubit_engine::HardwareTopologyRequest *request,
    qubit_engine::HardwareTopologyResponse *response) {
  if (!auth_interceptor_->ValidateAuth(context)) {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or missing authorization token");
  }
  return topology_service_->GetHardwareTopology(context, request, response);
}
