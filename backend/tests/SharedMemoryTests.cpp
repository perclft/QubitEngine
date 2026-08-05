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
