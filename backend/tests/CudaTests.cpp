#include "../src/Types.hpp"
#include <atomic>
#include <cmath>
#include <complex>
#include <gtest/gtest.h>
#include <thread>
#include <vector>


// We only compile these tests if ENABLE_CUDA is defined
#ifdef ENABLE_CUDA
#include "../src/backends/CudaBackend.hpp"

using namespace qubit_engine;

// Test Fixture for CudaBackend
class CudaBackendTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize 2-qubit backend
    // We use strict 2 qubits for these basic tests
    backend = new CudaBackend(2);
  }

  void TearDown() override { delete backend; }

  CudaBackend *backend;
};

TEST_F(CudaBackendTest, Initialization) {
  // Should initiate to |00> -> [1, 0, 0, 0]
  auto state = backend->getStateVector();
  EXPECT_EQ(state.size(), 4);
  EXPECT_EQ(state[0], Complex(1.0, 0.0));
  EXPECT_EQ(state[1], Complex(0.0, 0.0));
  EXPECT_EQ(state[2], Complex(0.0, 0.0));
  EXPECT_EQ(state[3], Complex(0.0, 0.0));
}

TEST_F(CudaBackendTest, ApplyHadamard) {
  // Apply H to qubit 0: |00> -> (|00> + |01>) / sqrt(2)
  // Note: Qubit 0 corresponds to the least significant bit (stride 1)

  backend->applyHadamard(0);
  auto state = backend->getStateVector();

  double is2 = 1.0 / std::sqrt(2.0);

  // Indices: 0 (|00>) and 1 (|01>) should be non-zero
  EXPECT_NEAR(state[0].real(), is2, 1e-6);
  EXPECT_NEAR(state[1].real(), is2, 1e-6);
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-6);
  EXPECT_NEAR(std::abs(state[3]), 0.0, 1e-6);
}

TEST_F(CudaBackendTest, ApplyX) {
  // |00> -> X(0) -> |01>
  backend->applyX(0);
  auto state = backend->getStateVector();

  EXPECT_NEAR(std::abs(state[0]), 0.0, 1e-6); // |00>
  EXPECT_NEAR(state[1].real(), 1.0, 1e-6);    // |01>
}

TEST_F(CudaBackendTest, ApplyCNOT) {
  // |00> -> H(0) -> |+0> = (|00> + |01>)/sqrt(2)
  // CNOT(0, 1): Control 0, Target 1.
  // If Q0 is 1, Flip Q1.
  // State |00>: Q0=0 -> No Flip -> |00>
  // State |01>: Q0=1 -> Flip Q1 -> |11> (Index 3)
  // Result: (|00> + |11>)/sqrt(2) -> Bell State

  backend->applyHadamard(0);
  backend->applyCNOT(0, 1);

  auto state = backend->getStateVector();
  double is2 = 1.0 / std::sqrt(2.0);

  EXPECT_NEAR(state[0].real(), is2, 1e-6);    // |00>
  EXPECT_NEAR(state[3].real(), is2, 1e-6);    // |11>
  EXPECT_NEAR(std::abs(state[1]), 0.0, 1e-6); // |01>
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-6); // |10>
}

TEST_F(CudaBackendTest, ExpectationValue) {
  // |00> -> H(0) -> X expectation on qubit 0
  // <+|X|+> = 1.0
  backend->applyHadamard(0);

  // "XI" -> Q0=X, Q1=I
  double val = backend->expectationValue("XI");
  EXPECT_NEAR(val, 1.0, 1e-5);

  // Z expectation on qubit 0 -> <+|Z|+> = 0
  val = backend->expectationValue("ZI");
  EXPECT_NEAR(val, 0.0, 1e-5);
}

TEST_F(CudaBackendTest, GetStateVectorAsyncStreamRaceCheck) {
  // NOTE: compute-sanitizer --tool racecheck detects shared memory races
  // WITHIN a kernel, NOT device global memory ordering violations ACROSS
  // streams.  The cudaEventRecord/cudaStreamWaitEvent fix prevents
  // cross-stream RAW hazards on device_state_.  We detect this race via
  // data-corruption: compare the async readback against the sync readback.
  // Without the fix, the async path can copy a partially-written state,
  // producing a mismatch.

  // 1. Use 20-qubit backend (1M amplitudes = 16 MB state)
  delete backend;
  backend = new CudaBackend(20);
  const size_t dim = 1ULL << 20;

  int mismatch_count = 0;

  // 2. Repeatedly: dispatch a batch of gates, then compare async vs sync
  for (int round = 0; round < 20; ++round) {
    // Dispatch gates on the default compute stream
    for (size_t q = 0; q < 20; ++q) {
      backend->applyHadamard(q);
      if (q + 1 < 20) {
        backend->applyCNOT(q, q + 1);
      }
    }

    // Immediately read back via the async telemetry path
    auto async_state = backend->getStateVectorAsync();
    // Read back via the sync path (guaranteed correct)
    auto sync_state = backend->getStateVector();

    ASSERT_EQ(async_state.size(), dim);
    ASSERT_EQ(sync_state.size(), dim);

    // Compare element-by-element
    for (size_t i = 0; i < dim; ++i) {
      if (std::abs(async_state[i] - sync_state[i]) > 1e-6) {
        ++mismatch_count;
        break; // one mismatch per round is enough
      }
    }
  }

  // With the fix in place, async and sync should always agree
  EXPECT_EQ(mismatch_count, 0)
      << "Async readback disagreed with sync readback in " << mismatch_count
      << "/20 rounds — indicates cross-stream race (missing event sync)";
}

TEST_F(CudaBackendTest, GetStateVectorAsyncConcurrentStress) {
  // Stress variant: a reader thread hammers getStateVectorAsync() while
  // the main thread dispatches gates. We check the NORM INVARIANT: a
  // valid quantum state must satisfy sum(|a_i|^2) == 1.0. A torn read
  // from a cross-stream race would return a partially-written vector
  // whose norm deviates from 1.0.

  delete backend;
  backend = new CudaBackend(18); // 18 qubits = 256K amplitudes
  const size_t dim = 1ULL << 18;

  std::atomic<bool> stop_reading{false};
  std::atomic<int> read_count{0};
  std::atomic<int> norm_violation_count{0};

  std::thread reader([this, &stop_reading, &read_count, &norm_violation_count, dim]() {
    while (!stop_reading.load(std::memory_order_relaxed)) {
      auto state = backend->getStateVectorAsync();
      if (state.size() == dim) {
        double norm_sq = 0.0;
        for (size_t i = 0; i < dim; ++i) {
          norm_sq += std::norm(state[i]); // |a_i|^2
        }
        // A valid quantum state has norm == 1.0; a torn read will deviate
        if (std::abs(norm_sq - 1.0) > 1e-3) {
          norm_violation_count.fetch_add(1, std::memory_order_relaxed);
        }
      }
      read_count.fetch_add(1, std::memory_order_relaxed);
    }
  });

  // Main thread: heavy gate workload
  for (int iter = 0; iter < 10; ++iter) {
    for (size_t q = 0; q < 18; ++q) {
      backend->applyHadamard(q);
      if (q + 1 < 18) {
        backend->applyCNOT(q, q + 1);
      }
    }
  }

  stop_reading.store(true, std::memory_order_relaxed);
  reader.join();

  EXPECT_GT(read_count.load(), 0) << "Reader thread never ran";
  EXPECT_EQ(norm_violation_count.load(), 0)
      << "Async readback returned non-unit-norm state "
      << norm_violation_count.load()
      << " times — indicates torn read from cross-stream race";
}

#endif // ENABLE_CUDA
