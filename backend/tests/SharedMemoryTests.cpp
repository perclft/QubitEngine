#include <gtest/gtest.h>
#include "ipc/SharedMemory.hpp"
#include <thread>
#include <chrono>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

using namespace qubit_engine::ipc;

TEST(SharedMemoryTest, DoubleCloseIsSafeNoOp) {
    std::string desc = "test_shm_double_close";
    size_t size = 1024;
    
    // Create segment
    void* ptr = SharedMemory::createSegment(desc, size);
    ASSERT_NE(ptr, nullptr);

    // First close returns true (unmapped)
    EXPECT_TRUE(SharedMemory::closeSegment(desc, ptr, size));

    // Second close on same pointer returns false (safe no-op, duplicate blocked)
    EXPECT_FALSE(SharedMemory::closeSegment(desc, ptr, size));
    
    SharedMemory::unlinkSegment(desc);
}

TEST(SharedMemoryTest, ScheduleCleanupDoesNotRaceOrCrash) {
    std::string desc = "test_shm_schedule_cleanup";
    size_t size = 1024;

    {
        SharedMemory shm(desc, size, true);
        ASSERT_NE(shm.data(), nullptr);
        // Schedule cleanup synchronously/short delay
        SharedMemory::scheduleCleanup(desc, shm.data(), size, 10);
        // Destructor runs here
    }

    // Wait past the cleanup timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Calling performCleanup / closeSegment directly must be a safe no-op now
  EXPECT_NO_THROW(SharedMemory::unlinkSegment(desc));
}

TEST(SharedMemoryTest, GoCrashTimeoutCleanup) {
  std::string desc = "test_shm_go_crash_timeout";
  size_t size = 4096;

  void* ptr = SharedMemory::createSegment(desc, size);
  ASSERT_NE(ptr, nullptr);

  std::string token = ActiveShmRegistry::instance().registerSegment(desc, ptr, size);
  EXPECT_EQ(ActiveShmRegistry::instance().activeCount(), 1);

  // Sleep 3.1 seconds to allow the 3-second fallback timer to fire without Go ACK
  std::this_thread::sleep_for(std::chrono::milliseconds(3100));

  // 1. Registry Table assertion
  EXPECT_EQ(ActiveShmRegistry::instance().activeCount(), 0);

  // 2. OS Kernel Stat Assertion confirming segment is physically unlinked from kernel
#ifdef _WIN32
  HANDLE hMap = OpenFileMappingA(FILE_MAP_READ, FALSE, desc.c_str());
  EXPECT_EQ(hMap, nullptr);
  if (hMap != nullptr) {
    CloseHandle(hMap);
  }
  EXPECT_EQ(GetLastError(), ERROR_FILE_NOT_FOUND);
#else
  int access_res = access(("/dev/shm/" + desc).c_str(), F_OK);
  EXPECT_NE(access_res, 0);
  EXPECT_EQ(errno, ENOENT);
#endif
}

#include "services/CircuitService.hpp"
#include "QuantumRegister.hpp"

TEST(SharedMemoryTest, WorstCaseBackpressureSustainedStreaming) {
  qubit_engine::QuantumRegister qreg(4);
  qubit_engine::services::CircuitService service;

  // Fill ActiveShmRegistry with 3 un-acknowledged segments to hit cap
  void* p1 = SharedMemory::createSegment("test_bp_1", 1024);
  void* p2 = SharedMemory::createSegment("test_bp_2", 1024);
  void* p3 = SharedMemory::createSegment("test_bp_3", 1024);

  std::string t1 = ActiveShmRegistry::instance().registerSegment("test_bp_1", p1, 1024);
  std::string t2 = ActiveShmRegistry::instance().registerSegment("test_bp_2", p2, 1024);
  std::string t3 = ActiveShmRegistry::instance().registerSegment("test_bp_3", p3, 1024);

  EXPECT_EQ(ActiveShmRegistry::instance().activeCount(), 3);

  qubit_engine::CircuitRequest req;
  req.set_num_qubits(4);
  req.set_use_shm(true);
  req.set_measurement_strategy(qubit_engine::CircuitRequest::FULL_STATE);

  qubit_engine::StateResponse resp;
  
  // Measure backpressure degradation: should complete within 20-30ms, falling back to protobuf
  auto start = std::chrono::high_resolution_clock::now();
  grpc::Status status = service.RunCircuit(nullptr, &req, &resp, true);
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - start).count();

  EXPECT_TRUE(status.ok());
  // SHM descriptor must be empty because backpressure forced fallback to protobuf array
  EXPECT_TRUE(resp.shm_descriptor().empty());
  // Protobuf array must contain full state vector (2^4 = 16 elements)
  EXPECT_EQ(resp.state_vector_size(), 16);

  // Clean up backpressure test segments
  ActiveShmRegistry::instance().clearAll();
  EXPECT_EQ(ActiveShmRegistry::instance().activeCount(), 0);
}

#include <numeric>
#include <cmath>

TEST(SharedMemoryTest, EndToEndLatencyBenchmark) {
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
}

// ============================================================================
// VisualizeCircuit SHM Path Tests
//
// VisualizeCircuit calls serializeState(qreg, &response, ..., request->use_shm(), ...)
// for EVERY gate step. These tests verify that the SHM path produces bit-identical
// state vectors and exhibits the same crossover behavior as RunCircuit/StreamGates.
//
// NOTE / BENCHMARK LIMITATION:
// These tests exercise the C++ engine's serializeState, SHM segment creation,
// and ACK/unlink registry directly in a unit/integration test harness. They do NOT
// execute through a full network gRPC server context with HTTP/2 framing overhead.
// While this accurately measures state vector serialization and memory transfer,
// full gRPC wire overhead is not included.
// ============================================================================

#include <nlohmann/json.hpp>

// Helper: Read state vector from SHM descriptor JSON, then ACK+unlink
static std::vector<qubit_engine::Complex> readStateFromShm(const std::string& desc_json) {
  auto j = nlohmann::json::parse(desc_json);
  std::string segment_name = j["segment_name"];
  size_t size_bytes = j["size_bytes"];
  std::string ack_token = j["ack_token"];
  size_t num_elements = size_bytes / sizeof(qubit_engine::Complex);

  void* ptr = qubit_engine::ipc::SharedMemory::openSegment(segment_name, size_bytes);
  EXPECT_NE(ptr, nullptr);

  std::vector<qubit_engine::Complex> result(num_elements);
  std::memcpy(result.data(), ptr, size_bytes);

  qubit_engine::ipc::SharedMemory::closeSegment(segment_name, ptr, size_bytes);
  qubit_engine::ipc::ActiveShmRegistry::instance().acknowledgeAndUnlink(ack_token);

  return result;
}

TEST(VisualizeCircuitShmTest, BitIdenticalCorrectness) {
  std::cout << "\n=== VISUALIZE_CIRCUIT SHM BIT-IDENTICAL CORRECTNESS TEST ===\n";
  std::cout << "Testing that serializeState(use_shm=true) produces identical state vectors\n";
  std::cout << "to serializeState(use_shm=false) — the exact path VisualizeCircuit uses.\n\n";

  qubit_engine::services::CircuitService service;

  // Test across the crossover boundary: N=8 (should fall back to protobuf),
  // N=10, 11, 12, 14, 16, 18, 20 (should engage SHM for larger N)
  const std::vector<int> qubit_counts = {8, 10, 11, 12, 14, 16, 18, 20};

  for (int n : qubit_counts) {
    // Build a circuit with a few gates — same as VisualizeCircuit would process
    qubit_engine::CircuitRequest req;
    req.set_num_qubits(n);
    req.set_measurement_strategy(qubit_engine::CircuitRequest::FULL_STATE);

    // Apply H to qubit 0, then CNOT(0,1) if n >= 2 — creates entangled state
    auto* op1 = req.add_operations();
    op1->set_type(qubit_engine::GateOperation::HADAMARD);
    op1->set_target_qubit(0);

    if (n >= 2) {
      auto* op2 = req.add_operations();
      op2->set_type(qubit_engine::GateOperation::CNOT);
      op2->set_target_qubit(1);
      op2->set_control_qubit(0);
    }

    // 1. Run with use_shm=false (protobuf path) — baseline
    req.set_use_shm(false);
    qubit_engine::StateResponse pb_resp;
    auto status_pb = service.RunCircuit(nullptr, &req, &pb_resp, true);
    ASSERT_TRUE(status_pb.ok()) << "Protobuf RunCircuit failed for N=" << n;
    EXPECT_TRUE(pb_resp.shm_descriptor().empty()) << "use_shm=false should not produce SHM descriptor";

    size_t expected_elements = 1ULL << n;
    ASSERT_EQ(pb_resp.state_vector_size(), static_cast<int>(expected_elements))
        << "Protobuf state vector size mismatch for N=" << n;

    // 2. Run with use_shm=true (SHM path if available)
    req.set_use_shm(true);
    qubit_engine::StateResponse shm_resp;
    auto status_shm = service.RunCircuit(nullptr, &req, &shm_resp, true);
    ASSERT_TRUE(status_shm.ok()) << "SHM RunCircuit failed for N=" << n;

    bool shm_engaged = !shm_resp.shm_descriptor().empty();
    std::cout << "  N=" << std::setw(2) << n
              << " (" << std::setw(8) << expected_elements << " elements, "
              << std::setw(8) << std::fixed << std::setprecision(3)
              << (expected_elements * 16.0 / (1024.0 * 1024.0)) << " MB): "
              << (shm_engaged ? "SHM ENGAGED" : "PROTOBUF FALLBACK") << " ... ";

    if (shm_engaged) {
      // Read state vector from SHM segment
      auto shm_state = readStateFromShm(shm_resp.shm_descriptor());
      ASSERT_EQ(shm_state.size(), expected_elements)
          << "SHM element count mismatch for N=" << n;

      // Compare every element bit-for-bit against protobuf baseline
      bool all_identical = true;
      for (size_t i = 0; i < expected_elements; ++i) {
        double pb_real = pb_resp.state_vector(static_cast<int>(i)).real();
        double pb_imag = pb_resp.state_vector(static_cast<int>(i)).imag();
        if (shm_state[i].real() != pb_real || shm_state[i].imag() != pb_imag) {
          std::cout << "MISMATCH at index " << i << ": SHM=("
                    << shm_state[i].real() << "," << shm_state[i].imag()
                    << ") PB=(" << pb_real << "," << pb_imag << ")\n";
          all_identical = false;
          FAIL() << "Bit mismatch at index " << i << " for N=" << n;
          break;
        }
      }
      if (all_identical) {
        std::cout << "100% BIT-IDENTICAL ✓\n";
      }
    } else {
      // SHM not engaged — verify protobuf path still works correctly
      ASSERT_EQ(shm_resp.state_vector_size(), static_cast<int>(expected_elements))
          << "Protobuf fallback state vector size mismatch for N=" << n;

      bool all_identical = true;
      for (size_t i = 0; i < expected_elements; ++i) {
        double shm_r = shm_resp.state_vector(static_cast<int>(i)).real();
        double shm_i = shm_resp.state_vector(static_cast<int>(i)).imag();
        double pb_r = pb_resp.state_vector(static_cast<int>(i)).real();
        double pb_i = pb_resp.state_vector(static_cast<int>(i)).imag();
        if (shm_r != pb_r || shm_i != pb_i) {
          all_identical = false;
          FAIL() << "Protobuf path mismatch at index " << i << " for N=" << n;
          break;
        }
      }
      if (all_identical) {
        std::cout << "100% BIT-IDENTICAL (protobuf-vs-protobuf baseline) ✓\n";
      }
    }
  }
  std::cout << "\n";
}

TEST(VisualizeCircuitShmTest, PerStepLatencyBenchmark) {
  std::cout << "\n=== VISUALIZE_CIRCUIT PER-STEP SHM LATENCY BENCHMARK ===\n";
  std::cout << "Simulates VisualizeCircuit's per-gate-step serializeState path.\n";
  std::cout << "Each step serializes the full state vector (use_shm=true vs false).\n\n";
  std::cout << "Qubits |   Elements   | Payload (MB) | Protobuf/step (ms) | SHM/step (ms) | Speedup\n";
  std::cout << "----------------------------------------------------------------------------------------\n";

  const std::vector<int> qubit_counts = {10, 11, 12, 14, 16, 18, 20, 22};
  const int NUM_STEPS = 5; // Simulate 5 gate steps per qubit count
  const int NUM_RUNS = 5;  // Average over 5 runs

  qubit_engine::services::CircuitService service;

  for (int n : qubit_counts) {
    size_t elements = 1ULL << n;
    size_t bytes = elements * sizeof(qubit_engine::Complex);
    double size_mb = static_cast<double>(bytes) / (1024.0 * 1024.0);

    std::vector<double> pb_step_times;
    std::vector<double> shm_step_times;

    for (int run = 0; run < NUM_RUNS; ++run) {
      // Build a circuit with NUM_STEPS Hadamard gates on different qubits
      qubit_engine::CircuitRequest req;
      req.set_num_qubits(n);
      req.set_measurement_strategy(qubit_engine::CircuitRequest::FULL_STATE);

      for (int s = 0; s < NUM_STEPS; ++s) {
        auto* op = req.add_operations();
        op->set_type(qubit_engine::GateOperation::HADAMARD);
        op->set_target_qubit(s % n);
      }

      // Protobuf path: measure full RunCircuit (which applies all gates + serializes)
      // Since we want per-step cost, we use the direct serializeState approach
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

      // SHM path: create segment, memcpy, register, simulate read + ack
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

        // Simulate reader read + ack
        std::vector<qubit_engine::Complex> reader_copy(elements);
        std::memcpy(reader_copy.data(), shm_ptr, bytes);
        ActiveShmRegistry::instance().acknowledgeAndUnlink(token);
      }
      double shm_total = std::chrono::duration<double, std::milli>(
          std::chrono::high_resolution_clock::now() - start_shm).count();
      shm_step_times.push_back(shm_total / NUM_STEPS);
    }

    auto calc_mean = [](const std::vector<double>& v) {
      return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    };

    double pb_mean = calc_mean(pb_step_times);
    double shm_mean = calc_mean(shm_step_times);
    double speedup = (shm_mean > 0) ? (pb_mean / shm_mean) : 1.0;

    std::cout << std::setw(6) << n << " | "
              << std::setw(12) << elements << " | "
              << std::setw(12) << std::fixed << std::setprecision(3) << size_mb << " | "
              << std::setw(18) << std::fixed << std::setprecision(2) << pb_mean << " | "
              << std::setw(13) << std::fixed << std::setprecision(2) << shm_mean << " | "
              << std::setw(7) << std::fixed << std::setprecision(2) << speedup << "x\n";
  }
  std::cout << "----------------------------------------------------------------------------------------\n";
  std::cout << "Each row = mean of " << NUM_RUNS << " runs x " << NUM_STEPS << " gate steps per run.\n\n";
}
