#include "ConfigManager.hpp"
#include <cstdlib>

namespace qubit_engine {

ConfigManager& ConfigManager::Instance() {
  static ConfigManager instance;
  return instance;
}

std::optional<std::string> ConfigManager::getCloudUrl() const {
  if (const char* env_p = std::getenv("QUBIT_CLOUD_URL")) {
    return std::string(env_p);
  }
  return std::nullopt;
}

bool ConfigManager::forceLocalExecution() const {
  if (const char* env_p = std::getenv("QUBIT_FORCE_LOCAL")) {
    return std::string(env_p) == "1" || std::string(env_p) == "true";
  }
  return false;
}

std::optional<std::string> ConfigManager::getTopologyPath() const {
  if (const char* env_p = std::getenv("QUBIT_TOPOLOGY_PATH")) {
    return std::string(env_p);
  }
  return std::nullopt;
}

} // namespace qubit_engine
