#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include "../Types.hpp"
#include "quantum.pb.h"
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace mpi {

enum MpiCommand : int {
  CMD_NONE = 0,
  CMD_RUN_CIRCUIT = 1,
  CMD_SHUTDOWN = 2
};

class QUBIT_ENGINE_EXPORT MpiCoordinator {
public:
  static MpiCoordinator& instance();

  // Initialization & lifecycle
  void init(int *argc, char ***argv);
  void finalize();
  bool isInitialized() const { return initialized_; }
  bool isMpiActive() const { return world_size_ > 1; }
  int getRank() const { return rank_; }
  int getWorldSize() const { return world_size_; }

  // Rank 0 cluster lock & broadcast methods
  bool tryAcquireClusterLock(int timeout_seconds = 30);
  void releaseClusterLock();

  void broadcastRunCircuit(const qubit_engine::CircuitRequest &request);
  void broadcastShutdown();

  // State gathering: gathers local state from all ranks to rank 0.
  // On rank 0, full_state receives all 2^N elements.
  // On worker ranks, local_state is sent and full_state is unused.
  void gatherStateVector(const std::vector<qubit_engine::Complex> &local_state,
                         std::vector<qubit_engine::Complex> &full_state,
                         size_t num_qubits);

  // Worker loop (executed on ranks > 0)
  void runWorkerLoop();

  // Helper to calculate dynamic watchdog timeout based on payload
  int calculateWatchdogTimeoutSec(size_t num_qubits, size_t bytes_to_transfer) const;

private:
  MpiCoordinator();
  ~MpiCoordinator();

  // Executes a single circuit on worker ranks in lockstep with rank 0
  void executeWorkerCircuit(const qubit_engine::CircuitRequest &request);

  int rank_{0};
  int world_size_{1};
  bool initialized_{false};
  std::timed_mutex cluster_exec_mutex_;
  std::atomic<bool> worker_shutdown_{false};
};

} // namespace mpi
} // namespace qubit_engine
