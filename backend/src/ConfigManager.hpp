#pragma once

#include <string>
#include <optional>

namespace qubit_engine {

class ConfigManager {
public:
  static ConfigManager& Instance();

  std::optional<std::string> getCloudUrl() const;
  bool forceLocalExecution() const;
  std::optional<std::string> getTopologyPath() const;

private:
  ConfigManager() = default;
  ~ConfigManager() = default;

  // Prevent copying and assignment
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;
};

} // namespace qubit_engine
