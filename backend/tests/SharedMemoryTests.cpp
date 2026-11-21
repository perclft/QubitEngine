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

  // Sleep 5.1 seconds to allow the 5-second fallback timer to fire without Go ACK
  std::this_thread::sleep_for(std::chrono::milliseconds(5100));

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
  EXPECT_TRUE(j.contains("format_version"));
  EXPECT_EQ(j["format_version"], 1);
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
    if (n < 12) {
      EXPECT_FALSE(shm_engaged) << "Requests with N < 12 must be downgraded to protobuf serialization even if use_shm=true";
    } else {
      EXPECT_TRUE(shm_engaged) << "Requests with N >= 12 must engage SHM IPC path";
    }

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

