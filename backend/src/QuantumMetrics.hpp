#pragma once
#include <iostream>
#include <memory>
#include <string>

// Mock classes to satisfy usage
namespace prometheus {
class Registry {};
} // namespace prometheus

class QuantumMetrics {
public:
  static QuantumMetrics &Instance() {
    static QuantumMetrics instance;
    return instance;
  }

  void Start(const std::string &bind_address = "0.0.0.0:9090") {
    std::cout << "[Metrics] Prometheus (Mock) disabled for build stability."
              << std::endl;
  }

  std::shared_ptr<prometheus::Registry> GetRegistry() { return registry_; }

private:
  QuantumMetrics() { registry_ = std::make_shared<prometheus::Registry>(); }

  std::shared_ptr<prometheus::Registry> registry_;
};
