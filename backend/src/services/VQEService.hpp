#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <vector>
#include <string>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace services {

class QUBIT_ENGINE_EXPORT VQEService {
public:
  VQEService() = default;

  grpc::Status RunVQE(grpc::ServerContext *context,
                      const qubit_engine::VQERequest *request,
                      grpc::ServerWriter<qubit_engine::VQEResponse> *writer);
};

} // namespace services
} // namespace qubit_engine
