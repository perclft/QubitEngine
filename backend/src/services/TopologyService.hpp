#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace services {

class QUBIT_ENGINE_EXPORT TopologyService {
public:
  TopologyService() = default;

  grpc::Status GetHardwareTopology(
      grpc::ServerContext *context,
      const qubit_engine::HardwareTopologyRequest *request,
      qubit_engine::HardwareTopologyResponse *response);
};

} // namespace services
} // namespace qubit_engine
