#pragma once
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace qubit_engine {
namespace services {

class TopologyService {
public:
  TopologyService() = default;

  grpc::Status GetHardwareTopology(
      grpc::ServerContext *context,
      const qubit_engine::HardwareTopologyRequest *request,
      qubit_engine::HardwareTopologyResponse *response);
};

} // namespace services
} // namespace qubit_engine
