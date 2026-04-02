#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <vector>
#include <string>

namespace qubit_engine {
namespace services {

class VQEService {
public:
  VQEService() = default;

  grpc::Status RunVQE(grpc::ServerContext *context,
                      const qubit_engine::VQERequest *request,
                      grpc::ServerWriter<qubit_engine::VQEResponse> *writer);
};

} // namespace services
} // namespace qubit_engine
