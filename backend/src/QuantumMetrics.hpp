#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "qubit_engine_export.h"

// Guarded by ENABLE_PROMETHEUS if we wanted to be strictly optional,
// but we'll provide a clean interface.
namespace prometheus {
class Registry;
class Counter;
class Histogram;
class Exposer;
}

class QUBIT_ENGINE_EXPORT QuantumMetrics {
public:
  static QuantumMetrics &Instance();

  // Starts the Prometheus HTTP server (Exposer)
  void Start(const std::string &bind_address = "0.0.0.0:9090");

  void IncrementJobCounter(const std::string &status); // e.g., "success", "failed"
  void RecordJobDuration(double seconds);
  void RecordGateApplication();

  std::shared_ptr<prometheus::Registry> GetRegistry() { return registry_; }

private:
  QuantumMetrics();
  ~QuantumMetrics();

  std::shared_ptr<prometheus::Registry> registry_;
  std::unique_ptr<prometheus::Exposer> exposer_; // HTTP server

  // Families / Metrics
  prometheus::Counter *job_counter_ = nullptr;
  prometheus::Histogram *job_duration_ = nullptr;
  prometheus::Counter *gate_counter_ = nullptr;
};
