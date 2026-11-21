#include "MpiCoordinator.hpp"
#include "../QuantumRegister.hpp"
#include "../GateDispatch.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <iostream>

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

namespace qubit_engine {
namespace mpi {

MpiCoordinator& MpiCoordinator::instance() {
  static MpiCoordinator coordinator;
  return coordinator;
}

MpiCoordinator::MpiCoordinator() = default;

MpiCoordinator::~MpiCoordinator() {
  finalize();
}

void MpiCoordinator::init(int *argc, char ***argv) {
#ifdef MPI_ENABLED
  int init_flag = 0;
  MPI_Initialized(&init_flag);
  if (!init_flag) {
    int provided = 0;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size_);
  initialized_ = true;
  if (world_size_ > 1) {
    spdlog::info("MPI SPMD Fabric Initialized: Rank {} of {} ranks in MPI_COMM_WORLD", rank_, world_size_);
  }
#else
  rank_ = 0;
  world_size_ = 1;
  initialized_ = false;
#endif
}

void MpiCoordinator::finalize() {
#ifdef MPI_ENABLED
  int init_flag = 0;
  MPI_Initialized(&init_flag);
  int fin_flag = 0;
  MPI_Finalized(&fin_flag);
  if (init_flag && !fin_flag) {
    MPI_Finalize();
  }
#endif
  initialized_ = false;
}

bool MpiCoordinator::tryAcquireClusterLock(int timeout_seconds) {
  if (world_size_ <= 1) return true;
  return cluster_exec_mutex_.try_lock_for(std::chrono::seconds(timeout_seconds));
}

void MpiCoordinator::releaseClusterLock() {
  if (world_size_ > 1) {
    cluster_exec_mutex_.unlock();
  }
}

void MpiCoordinator::broadcastRunCircuit(const qubit_engine::CircuitRequest &request) {
#ifdef MPI_ENABLED
  if (world_size_ <= 1) return;

  int cmd = CMD_RUN_CIRCUIT;
  MPI_Bcast(&cmd, 1, MPI_INT, 0, MPI_COMM_WORLD);

  std::string serialized;
  request.SerializeToString(&serialized);
  int len = static_cast<int>(serialized.size());
  MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(const_cast<char*>(serialized.data()), len, MPI_BYTE, 0, MPI_COMM_WORLD);
#endif
}

void MpiCoordinator::broadcastShutdown() {
#ifdef MPI_ENABLED
  if (world_size_ <= 1) return;
  int cmd = CMD_SHUTDOWN;
  MPI_Bcast(&cmd, 1, MPI_INT, 0, MPI_COMM_WORLD);
  spdlog::info("MPI Coordinator: Broadcasted CMD_SHUTDOWN to worker ranks.");
#endif
}

int MpiCoordinator::calculateWatchdogTimeoutSec(size_t num_qubits, size_t bytes_to_transfer) const {
  (void)num_qubits;
  double interconnect_mbps = 100.0;
  if (const char* env_bw = std::getenv("QUBIT_MPI_INTERCONNECT_MBPS")) {
    double val = std::atof(env_bw);
    if (val > 0.0) interconnect_mbps = val;
  }

  double mb = static_cast<double>(bytes_to_transfer) / (1024.0 * 1024.0);
  int transfer_sec = static_cast<int>(std::ceil(mb / interconnect_mbps));
  int timeout_sec = 15 + transfer_sec;

  if (const char* env_timeout = std::getenv("QUBIT_MPI_CLUSTER_TIMEOUT_SEC")) {
    int val = std::atoi(env_timeout);
    if (val > 0) timeout_sec = val;
  }
  return timeout_sec;
}

void MpiCoordinator::gatherStateVector(const std::vector<qubit_engine::Complex> &local_state,
                                      std::vector<qubit_engine::Complex> &full_state,
                                      size_t num_qubits) {
#ifdef MPI_ENABLED
  if (world_size_ <= 1) {
    full_state = local_state;
    return;
  }

  size_t total_elements = 1ULL << num_qubits;
  size_t local_elements = total_elements / world_size_;
  if (local_state.size() != local_elements) {
    spdlog::error("gatherStateVector: local_state size {} != expected {}", local_state.size(), local_elements);
  }

  if (rank_ == 0) {
    full_state.resize(total_elements);
  }

  // Deterministic fault-injection hook for rank hang / failure tests
  if (const char* hook = std::getenv("QUBIT_MPI_TEST_HOOK")) {
    if (std::string(hook) == "worker_hang_before_gather" && rank_ > 0) {
      std::cout << "[SYNC:WORKER_ABOUT_TO_HANG]" << std::endl;
      std::fflush(stdout);
      std::this_thread::sleep_for(std::chrono::hours(1));
    }
  }

  size_t bytes_to_transfer = total_elements * sizeof(qubit_engine::Complex);
  int timeout_sec = calculateWatchdogTimeoutSec(num_qubits, bytes_to_transfer);

  std::atomic<bool> collective_done{false};
  std::thread watchdog([timeout_sec, &collective_done]() {
    auto start = std::chrono::steady_clock::now();
    while (!collective_done.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - start).count();
      if (elapsed >= timeout_sec) {
        if (!collective_done.load(std::memory_order_relaxed)) {
          spdlog::critical("MPI Watchdog: Collective operation timed out after {}s. Aborting MPI cluster.", timeout_sec);
          MPI_Abort(MPI_COMM_WORLD, 1);
        }
        break;
      }
    }
  });

  int sendcount = static_cast<int>(local_elements * 2);
  int recvcount = sendcount;
  void* recvbuf = (rank_ == 0) ? full_state.data() : nullptr;

  int ret = MPI_Gather(const_cast<qubit_engine::Complex*>(local_state.data()),
                       sendcount, MPI_DOUBLE,
                       recvbuf, recvcount, MPI_DOUBLE,
                       0, MPI_COMM_WORLD);

  collective_done.store(true, std::memory_order_relaxed);
  if (watchdog.joinable()) {
    watchdog.join();
  }

  if (ret != MPI_SUCCESS) {
    spdlog::error("MPI_Gather failed with error code: {}", ret);
    throw std::runtime_error("MPI_Gather collective operation failed");
  }
#else
  (void)num_qubits;
  full_state = local_state;
#endif
}

void MpiCoordinator::runWorkerLoop() {
#ifdef MPI_ENABLED
  if (world_size_ <= 1) return;

  spdlog::info("[Worker Rank {}] Entering MPI SPMD worker loop...", rank_);
  while (!worker_shutdown_) {
    int cmd = CMD_NONE;
    int ret = MPI_Bcast(&cmd, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (ret != MPI_SUCCESS) {
      spdlog::error("[Worker Rank {}] MPI_Bcast command failed with code {}", rank_, ret);
      break;
    }

    if (cmd == CMD_SHUTDOWN) {
      spdlog::info("[Worker Rank {}] Received CMD_SHUTDOWN. Exiting worker loop.", rank_);
      break;
    } else if (cmd == CMD_RUN_CIRCUIT) {
      int len = 0;
      MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
      if (len <= 0) continue;

      std::vector<char> buf(len);
      MPI_Bcast(buf.data(), len, MPI_BYTE, 0, MPI_COMM_WORLD);

      qubit_engine::CircuitRequest req;
      if (!req.ParseFromArray(buf.data(), len)) {
        spdlog::error("[Worker Rank {}] Failed to parse serialized CircuitRequest", rank_);
        continue;
      }

      executeWorkerCircuit(req);
    }
  }
#endif
}

void MpiCoordinator::executeWorkerCircuit(const qubit_engine::CircuitRequest &request) {
  int n = request.num_qubits();
  qubit_engine::QuantumRegister qreg(static_cast<size_t>(n));

  int num_ops = request.operations_size();
  for (int i = 0; i < num_ops; ++i) {
    qubit_engine::dispatchGate(qreg, request.operations(i), nullptr, nullptr);
  }

  if (request.noise_probability() > 0.0) {
    qreg.applyDepolarizingNoise(request.noise_probability());
  }

  if (request.measurement_strategy() == qubit_engine::CircuitRequest::FULL_STATE) {
    auto local_state = qreg.getStateVector();
    std::vector<qubit_engine::Complex> dummy;
    gatherStateVector(local_state, dummy, static_cast<size_t>(n));
  }
}

} // namespace mpi
} // namespace qubit_engine
