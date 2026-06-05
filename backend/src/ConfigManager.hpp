#pragma once

#include <string>
#include <optional>
#include "qubit_engine_export.h"

namespace qubit_engine {

class QUBIT_ENGINE_EXPORT ConfigManager {
public:
  static ConfigManager& Instance();

  std::optional<std::string> getCloudUrl() const;
  bool forceLocalExecution() const;
  std::optional<std::string> getTopologyPath() const;
  int getMpsThreshold() const;
  int getMpsBondDimension() const;

private:
  ConfigManager() = default;
  ~ConfigManager() = default;

  // Prevent copying and assignment
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;
};

} // namespace qubit_engine
