#include "QuantumMetrics.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

#if __has_include(<prometheus/exposer.h>)
#define HAS_PROMETHEUS 1
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#else
#define HAS_PROMETHEUS 0
#endif

QuantumMetrics &QuantumMetrics::Instance() {
  static QuantumMetrics instance;
  return instance;
}

QuantumMetrics::QuantumMetrics() {
#if HAS_PROMETHEUS
  registry_ = std::make_shared<prometheus::Registry>();

  auto &job_family = prometheus::BuildCounter()
                         .Name("qubit_engine_jobs_total")
                         .Help("Total number of quantum jobs processed")
                         .Register(*registry_);
  job_counter_ = &job_family.Add({});

  auto &duration_family = prometheus::BuildHistogram()
                              .Name("qubit_engine_job_duration_seconds")
                              .Help("Histogram of job execution times")
                              .Register(*registry_);
  job_duration_ = &duration_family.Add({}, prometheus::Histogram::BucketBoundaries{0.01, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0});

  auto &gate_family = prometheus::BuildCounter()
                          .Name("qubit_engine_gates_applied_total")
                          .Help("Total number of quantum gates applied")
                          .Register(*registry_);
  gate_counter_ = &gate_family.Add({});
#endif
}

QuantumMetrics::~QuantumMetrics() {}

void QuantumMetrics::Start(const std::string &bind_address) {
#if HAS_PROMETHEUS
  try {
    exposer_ = std::make_unique<prometheus::Exposer>(bind_address);
    exposer_->RegisterCollectable(registry_);
    spdlog::info("Metrics server started at http://{}", bind_address);
  } catch (const std::exception &ex) {
    spdlog::error("Failed to start metrics server at {}: {}", bind_address, ex.what());
  }
#else
  spdlog::warn("Prometheus metrics disabled (library not found at compile time)");
#endif
}

void QuantumMetrics::IncrementJobCounter(const std::string &status) {
#if HAS_PROMETHEUS
  if (job_counter_) job_counter_->Increment();
#endif
}

void QuantumMetrics::RecordJobDuration(double seconds) {
#if HAS_PROMETHEUS
  if (job_duration_) job_duration_->Observe(seconds);
#endif
}

void QuantumMetrics::RecordGateApplication() {
#if HAS_PROMETHEUS
  if (gate_counter_) gate_counter_->Increment();
#endif
}


