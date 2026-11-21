#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstring>
#include "QuantumRegister.hpp"
#include "ipc/SharedMemory.hpp"
#include "services/CircuitService.hpp"
#include "quantum.pb.h"

using namespace qubit_engine;
using namespace qubit_engine::ipc;

int main(int argc, char** argv) {
  std::cout << "\n=== END-TO-END LATENCY STATISTICAL BENCHMARK (10 Runs per Qubit Count) ===\n";
  std::cout << "Qubits |   Elements   | Payload (MB) | Protobuf Mean +- Std (ms) | SHM+Ack Mean +- Std (ms) | Speedup\n";
  std::cout << "--------------------------------------------------------------------------------------------------\n";

  const std::vector<int> qubit_counts = {10, 11, 12, 14, 16, 18, 20, 22, 24, 25};
  const int NUM_RUNS = 10;

  for (int n : qubit_counts) {
    size_t elements = 1ULL << n;
    size_t bytes = elements * sizeof(qubit_engine::Complex);
    double size_mb = static_cast<double>(bytes) / (1024.0 * 1024.0);

    std::vector<double> pb_times;
    std::vector<double> shm_times;
    pb_times.reserve(NUM_RUNS);
    shm_times.reserve(NUM_RUNS);

    for (int run = 0; run < NUM_RUNS; ++run) {
      qubit_engine::QuantumRegister qreg(n);

      // 1. Protobuf path latency
      auto start_pb = std::chrono::high_resolution_clock::now();
      qubit_engine::StateResponse pb_resp;
      auto state_vec = qreg.getStateVector();
      for (const auto &c : state_vec) {
        auto *pb_c = pb_resp.add_state_vector();
        pb_c->set_real(c.real());
        pb_c->set_imag(c.imag());
      }
      auto dur_pb = std::chrono::duration<double, std::milli>(
          std::chrono::high_resolution_clock::now() - start_pb).count();
      pb_times.push_back(dur_pb);

      // 2. SHM Zero-Copy path latency (including ACK round-trip)
      auto start_shm = std::chrono::high_resolution_clock::now();
      std::string shm_name = "Local\\qe_shm_bench_" + std::to_string(n) + "_" + std::to_string(run);
#ifndef _WIN32
      shm_name = "/qe_shm_bench_" + std::to_string(n) + "_" + std::to_string(run);
#endif
      void* shm_ptr = SharedMemory::createSegment(shm_name, bytes);
      std::memcpy(shm_ptr, state_vec.data(), bytes);
      std::string token = ActiveShmRegistry::instance().registerSegment(shm_name, shm_ptr, bytes);
      
      // Simulate reader mmap copy + Ack RPC round trip
      std::vector<qubit_engine::Complex> reader_copy(elements);
      std::memcpy(reader_copy.data(), shm_ptr, bytes);
      ActiveShmRegistry::instance().acknowledgeAndUnlink(token);

      auto dur_shm = std::chrono::duration<double, std::milli>(
          std::chrono::high_resolution_clock::now() - start_shm).count();
      shm_times.push_back(dur_shm);
    }

    auto calc_stats = [](const std::vector<double>& v) {
      double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
      double sq_sum = 0.0;
      for (double x : v) sq_sum += (x - mean) * (x - mean);
      double stddev = std::sqrt(sq_sum / v.size());
      return std::make_pair(mean, stddev);
    };

    auto [pb_mean, pb_std] = calc_stats(pb_times);
    auto [shm_mean, shm_std] = calc_stats(shm_times);
    double speedup = (shm_mean > 0) ? (pb_mean / shm_mean) : 1.0;

    std::stringstream ss_pb, ss_shm;
    ss_pb << std::fixed << std::setprecision(2) << pb_mean << " +- " << std::setprecision(2) << pb_std;
    ss_shm << std::fixed << std::setprecision(2) << shm_mean << " +- " << std::setprecision(2) << shm_std;

    std::cout << std::setw(6) << n << " | "
              << std::setw(12) << elements << " | "
              << std::setw(12) << std::fixed << std::setprecision(3) << size_mb << " | "
              << std::setw(25) << ss_pb.str() << " | "
              << std::setw(24) << ss_shm.str() << " | "
              << std::setw(7) << std::fixed << std::setprecision(2) << speedup << "x\n";
  }
  std::cout << "--------------------------------------------------------------------------------------------------\n\n";
  
  // === PART 2: VISUALIZE_CIRCUIT PER-STEP SHM LATENCY BENCHMARK ===
  std::cout << "\n=== VISUALIZE_CIRCUIT PER-STEP SHM LATENCY BENCHMARK ===\n";
  std::cout << "Simulates VisualizeCircuit's per-gate-step serializeState path.\n";
  std::cout << "Each step serializes the full state vector (use_shm=true vs false).\n\n";
  std::cout << "Qubits |   Elements   | Payload (MB) | Protobuf/step (ms) | SHM/step (ms) | Speedup\n";
  std::cout << "----------------------------------------------------------------------------------------\n";

  const std::vector<int> viz_qubit_counts = {10, 11, 12, 14, 16, 18, 20, 22};
  const int NUM_STEPS = 5;
  const int NUM_RUNS_VIZ = 5;

  for (int n : viz_qubit_counts) {
    size_t elements = 1ULL << n;
    size_t bytes = elements * sizeof(qubit_engine::Complex);
    double size_mb = static_cast<double>(bytes) / (1024.0 * 1024.0);

    std::vector<double> pb_step_times;
    std::vector<double> shm_step_times;

    for (int run = 0; run < NUM_RUNS_VIZ; ++run) {
      qubit_engine::QuantumRegister qreg_pb(n);
      auto start_pb = std::chrono::high_resolution_clock::now();
      for (int s = 0; s < NUM_STEPS; ++s) {
        qubit_engine::StateResponse resp;
        auto state_vec = qreg_pb.getStateVector();
        for (const auto& c : state_vec) {
          auto* pb_c = resp.add_state_vector();
          pb_c->set_real(c.real());
          pb_c->set_imag(c.imag());
        }
      }
      double pb_total = std::chrono::duration<double, std::milli>(
          std::chrono::high_resolution_clock::now() - start_pb).count();
      pb_step_times.push_back(pb_total / NUM_STEPS);

      qubit_engine::QuantumRegister qreg_shm(n);
      auto start_shm = std::chrono::high_resolution_clock::now();
      for (int s = 0; s < NUM_STEPS; ++s) {
        std::string shm_name = "Local\\qe_viz_bench_" + std::to_string(n) + "_" + std::to_string(run) + "_" + std::to_string(s);
#ifndef _WIN32
        shm_name = "/qe_viz_bench_" + std::to_string(n) + "_" + std::to_string(run) + "_" + std::to_string(s);
#endif
        auto state_vec = qreg_shm.getStateVector();
        void* shm_ptr = SharedMemory::createSegment(shm_name, bytes);
        std::memcpy(shm_ptr, state_vec.data(), bytes);
        std::string token = ActiveShmRegistry::instance().registerSegment(shm_name, shm_ptr, bytes);

        std::vector<qubit_engine::Complex> reader_copy(elements);
        std::memcpy(reader_copy.data(), shm_ptr, bytes);
        ActiveShmRegistry::instance().acknowledgeAndUnlink(token);
      }
      double shm_total = std::chrono::duration<double, std::milli>(
          std::chrono::high_resolution_clock::now() - start_shm).count();
      shm_step_times.push_back(shm_total / NUM_STEPS);
    }

    double pb_avg = std::accumulate(pb_step_times.begin(), pb_step_times.end(), 0.0) / NUM_RUNS_VIZ;
    double shm_avg = std::accumulate(shm_step_times.begin(), shm_step_times.end(), 0.0) / NUM_RUNS_VIZ;
    double speedup = pb_avg / shm_avg;

    std::cout << std::setw(6) << n << " | "
              << std::setw(12) << elements << " | "
              << std::setw(12) << std::fixed << std::setprecision(3) << size_mb << " | "
              << std::setw(18) << std::fixed << std::setprecision(2) << pb_avg << " | "
              << std::setw(13) << std::fixed << std::setprecision(2) << shm_avg << " | "
              << std::setw(9) << std::fixed << std::setprecision(2) << speedup << "x\n";
  }
  std::cout << "----------------------------------------------------------------------------------------\n";

  return 0;
}
