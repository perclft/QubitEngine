#include "TopologyService.hpp"
#include "../HardwareConfig.hpp"
#include "../ConfigManager.hpp"
#include <spdlog/spdlog.h>

namespace qubit_engine {
namespace services {

grpc::Status TopologyService::GetHardwareTopology(
    grpc::ServerContext *context,
    const qubit_engine::HardwareTopologyRequest *request,
    qubit_engine::HardwareTopologyResponse *response) {

  spdlog::info("TopologyService: Serving GetHardwareTopology request...");

  qubit_engine::HardwareConfig config;
  auto topoPath = qubit_engine::ConfigManager::Instance().getTopologyPath();
  
  bool loaded = false;
  if (topoPath.has_value()) {
      loaded = config.loadFromFile(topoPath.value());
  } else {
      loaded = config.loadFromFile("topology.json");
  }

  if (!loaded) {
      config.loadDefaultHeavyHex();
  }

  for (const auto &n : config.getNodes()) {
    auto *node = response->add_nodes();
    node->set_id(n.id);
    node->set_x(n.x);
    node->set_y(n.y);
  }

  for (const auto &e : config.getEdges()) {
    auto *edge = response->add_edges();
    edge->set_node1(e.node1);
    edge->set_node2(e.node2);
  }

  return grpc::Status::OK;
}

} // namespace services
} // namespace qubit_engine
