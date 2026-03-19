#include "ServiceImpl.hpp"
#include "GateDispatch.hpp"
#include "HardwareConfig.hpp"
#include "ConfigManager.hpp"
#include <atomic>
#include "QuantumRegister.hpp"
#include "Exceptions.hpp"
#include "ipc/SharedMemory.hpp"
#include <cmath>
#include <cstdint>
#include <future>
#include <iostream>
#include <random>
#include <string>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h> // For gethostname
#endif
#include <spdlog/spdlog.h>

using grpc::ServerContext;
using grpc::Status;
using qubit_engine::CircuitRequest;
using qubit_engine::GateOperation;
using qubit_engine::QuantumRegister;
using qubit_engine::StateResponse;

// HELPER: Check if the server has enough free RAM for the requested qubits
bool hasEnoughMemory(int num_qubits) {
#ifdef __linux__
  struct sysinfo memInfo;
  sysinfo(&memInfo);

  long long available_ram = memInfo.freeram * memInfo.mem_unit;

  // Memory needed = 2^N * sizeof(complex<double>) (16 bytes)
  // 1ULL ensures we do 64-bit arithmetic
  size_t required_elements = 1ULL << num_qubits;
  size_t required_bytes = required_elements * sizeof(std::complex<double>);

  // Add 5% buffer for OS/Process overhead
  size_t overhead = required_bytes / 20;

  return available_ram > (required_bytes + overhead);
#elif defined(_WIN32)
  return true; // Assume enough memory on Windows for now (or use
               // GlobalMemoryStatusEx)
#else
  return true; // Default for Mac/Other
#endif
}

// Helper to map GateOperation to QuantumRegister calls.
// Delegates to the shared dispatchGate() to avoid duplicate switch blocks.
void QubitEngineServiceImpl::applyGate(QuantumRegister &qreg,
                                       const qubit_engine::GateOperation &op,
                                       qubit_engine::StateResponse *response) {
  qubit_engine::dispatchGate(qreg, op, nullptr,
                             response->mutable_classical_results());
}

// Helper to serialize state vector
void QubitEngineServiceImpl::serializeState(
    const QuantumRegister &qreg, qubit_engine::StateResponse *response,
    qubit_engine::CircuitRequest::MeasurementStrategy strategy, bool use_shm) {
  if (strategy == qubit_engine::CircuitRequest::FULL_STATE) {
    bool shm_success = false;
    if (use_shm) {
      // Generate pseudo-random descriptor
      static std::atomic<uint64_t> shm_counter{0};
      std::string desc = "qe_shm_" + std::to_string(shm_counter.fetch_add(1, std::memory_order_relaxed));
#ifdef _WIN32
      std::string full_desc = "Local\\" + desc;
#else
      std::string full_desc = "/" + desc;
#endif
      const auto &state = qreg.getStateVector();
      size_t sizeBytes = state.size() * sizeof(std::complex<double>);

      try {
        void *ptr =
            qubit_engine::ipc::SharedMemory::createSegment(full_desc, sizeBytes);
        std::memcpy(ptr, state.data(), sizeBytes);

        // Schedule RAII / Timeout cleanup to prevent IPC leaks across OS environments
        qubit_engine::ipc::SharedMemory::scheduleCleanup(full_desc, ptr, sizeBytes, 5000);

        // Pass only the descriptor pointer over the gRPC stream
        response->set_shm_descriptor(full_desc);
        shm_success = true;
      } catch (const std::exception& e) {
        spdlog::warn("SharedMemory creation failed, falling back to gRPC stream: {}", e.what());
        shm_success = false;
      }
    }
    
    if (!shm_success) {
      response->clear_state_vector();
      const auto &state = qreg.getStateVector();
      for (const auto &amp : state) {
        auto *c = response->add_state_vector();
        c->set_real(amp.real());
        c->set_imag(amp.imag());
      }
    }
  } else if (strategy == qubit_engine::CircuitRequest::SPARSE_STATE) {
    const auto &state = qreg.getStateVector();
    for (size_t i = 0; i < state.size(); ++i) {
      double prob =
          std::norm(std::complex<double>(state[i].real(), state[i].imag()));
      if (prob > 1e-6) {
        auto *m = response->add_sparse_states();
        m->set_qubit_index(i);
        m->set_probability(prob);
      }
    }
  } else if (strategy == qubit_engine::CircuitRequest::EXPECTATION_VALUES) {
    // Expectation values placeholder
  }

  // Populate Server ID (Pod Hostname)
  char hostname[1024];
  std::string id_str = "unknown-host";
  if (gethostname(hostname, 1024) == 0) {
    id_str = std::string(hostname);
  }

  // Phase 23: Distributed Info
  if (qreg.getSize() > 1) {
    id_str += " (MPI Rank " + std::to_string(qreg.getRank()) + "/" +
              std::to_string(qreg.getSize()) + ")";
  }

  response->set_server_id(id_str);
}

// Factory Helper Removed - Access via QuantumRegister directly

// Authentication Helper
bool QubitEngineServiceImpl::ValidateAuth(grpc::ServerContext *context) const {
  // Allow disabling auth for unit tests (direct service calls bypass gRPC channel)
  const char* skip = std::getenv("QUBIT_ENGINE_SKIP_AUTH");
  if (skip && std::string(skip) == "1") {
    return true;
  }

  const auto& client_metadata = context->client_metadata();
  auto iter = client_metadata.find("authorization");
  if (iter == client_metadata.end()) {
    return false;
  }
  std::string token(iter->second.data(), iter->second.length());
  // Basic bearer token extraction
  if (token.rfind("Bearer ", 0) == 0) {
    token = token.substr(7);
  }
  
  const char* env_token = std::getenv("QUBIT_ENGINE_AUTH_TOKEN");
  std::string expected_token = env_token ? env_token : "default-secret-token";
  return token == expected_token;
}

grpc::Status
QubitEngineServiceImpl::RunCircuit(grpc::ServerContext *context,
                                   const qubit_engine::CircuitRequest *request,
                                   qubit_engine::StateResponse *response) {

  spdlog::debug("RunCircuit method invoked!");

  if (!ValidateAuth(context)) {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or missing authorization token");
  }

  int n = request->num_qubits();

  // 1. HARD LIMIT CHECK
  if (n <= 0 || n > 30) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Qubits must be between 1 and 30");
  }

  // 2. DYNAMIC MEMORY CHECK
  if (!hasEnoughMemory(n)) {
    return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED,
                        "Insufficient Server Memory for " + std::to_string(n) +
                            " qubits.");
  }

  // Check Backend Request
  if (request->execution_backend() != qubit_engine::CircuitRequest::SIMULATOR) {
    spdlog::warn(
        "Requested backend {} but currently defaulting to Local CPU Simulator.",
        static_cast<int>(request->execution_backend()));
    // Future: Pass backend type to QuantumRegister constructor
  }

  try {
    // Instantiate Register (Frontend)
    QuantumRegister qreg(n);

    // Phase 4: Thread the JIT compiler out utilizing std::async for CPU-GPU
    // concurrent overlap
    int num_ops = request->operations_size();
    if (num_ops > 0) {
      if (n >= 25) {
        // Phase 5: MPS Backend SVD topological routing
        qreg.enableRecording(true);
        qreg.enableExecution(false);
        for (int i = 0; i < num_ops; ++i) {
          applyGate(qreg, request->operations(i), response);
        }
        qreg.mapTo1DTopology();
        qreg.enableExecution(true);
        qreg.enableRecording(false);

        for (const auto &gate : qreg.getTape()) {
          qreg.applyRegisteredGate(gate);
        }
        qreg.clearTape();
      } else {
        const int CHUNK_SIZE = 1000;
        std::vector<qubit_engine::GateOperation> ops;
        ops.reserve(num_ops);
        for (int i = 0; i < num_ops; ++i) {
          ops.push_back(request->operations(i));
        }

        auto compile_chunk =
            [](const std::vector<qubit_engine::GateOperation> &ch) {
              // Pre-compilation / routing logic goes here (mock delay for
              // structure)
              return ch;
            };

        int first_end = std::min(CHUNK_SIZE, num_ops);
        std::vector<qubit_engine::GateOperation> next_chunk(
            ops.begin(), ops.begin() + first_end);

        auto future = std::async(std::launch::async, compile_chunk, next_chunk);

        for (int i = 0; i < num_ops; i += CHUNK_SIZE) {
          auto current_chunk = future.get(); // Await JIT thread

          // Dispatch next branch of JIT asynchronously
          if (i + CHUNK_SIZE < num_ops) {
            int next_start = i + CHUNK_SIZE;
            int next_end = std::min(next_start + CHUNK_SIZE, num_ops);
            next_chunk = std::vector<qubit_engine::GateOperation>(
                ops.begin() + next_start, ops.begin() + next_end);
            future = std::async(std::launch::async, compile_chunk, next_chunk);
          }

          // Because cudaDeviceSynchronize was stripped from gate_kernels,
          // applying these gates dispatches instantly to the async command
          // queue.
          for (const auto &op : current_chunk) {
            applyGate(qreg, op, response);
          }
        }
      }
    }

    // Serialize Result
    serializeState(qreg, response, request->measurement_strategy(),
                   request->use_shm());
  } catch (const qubit_engine::InvalidArgumentException &e) {
    spdlog::error("Invalid argument during RunCircuit: {}", e.what());
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
  } catch (const qubit_engine::QubitOutOfRangeException &e) {
    spdlog::error("Out of range error during RunCircuit: {}", e.what());
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, e.what());
  } catch (const std::exception &e) {
    spdlog::error("Internal Engine Error during RunCircuit: {}", e.what());
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string("Internal Engine Error: ") + e.what());
  }

  return grpc::Status::OK;
}

grpc::Status QubitEngineServiceImpl::StreamGates(
    grpc::ServerContext *context,
    grpc::ServerReaderWriter<qubit_engine::StateResponse,
                             qubit_engine::GateStreamRequest> *stream) {

  spdlog::debug("StreamGates method invoked!");

  if (!ValidateAuth(context)) {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or missing authorization token");
  }

  qubit_engine::GateStreamRequest first;
  if (!stream->Read(&first)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "StreamGates requires an initial init message");
  }
  if (first.msg_case() != qubit_engine::GateStreamRequest::kInit) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "First StreamGates message must be init");
  }

  const auto &init = first.init();
  const int num_qubits = init.num_qubits();
  const bool use_shm = init.use_shm();

  if (num_qubits <= 0 || num_qubits > 30) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Qubits must be between 1 and 30");
  }

  QuantumRegister qreg(static_cast<size_t>(num_qubits));

  try {
    qubit_engine::GateStreamRequest req;
    while (stream->Read(&req)) {
      qubit_engine::StateResponse response;

      if (req.msg_case() == qubit_engine::GateStreamRequest::kInit) {
        // Init can only appear once at the beginning; reject mid-stream.
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "StreamGates init message is only allowed as the first message");
      }

      if (req.msg_case() != qubit_engine::GateStreamRequest::kOp) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "StreamGates received unknown message type");
      }

      const auto &op = req.op();
      applyGate(qreg, op, &response);
      serializeState(qreg, &response, qubit_engine::CircuitRequest::FULL_STATE, use_shm);

      stream->Write(response);
    }
  } catch (const qubit_engine::InvalidArgumentException &e) {
    spdlog::error("Invalid argument during StreamGates: {}", e.what());
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
  } catch (const qubit_engine::QubitOutOfRangeException &e) {
    spdlog::error("Out of range error during StreamGates: {}", e.what());
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, e.what());
  } catch (const std::exception &e) {
    spdlog::error("Internal error during StreamGates: {}", e.what());
    return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
  }

  return grpc::Status::OK;
}

// -----------------------------------------------------------------
// Phase 19: VQE Implementation
// -----------------------------------------------------------------
#include "MolecularHamiltonian.hpp"
#include <random>

#include "QuantumDifferentiator.hpp"

grpc::Status QubitEngineServiceImpl::RunVQE(
    grpc::ServerContext *context, const qubit_engine::VQERequest *request,
    grpc::ServerWriter<qubit_engine::VQEResponse> *writer) {

  spdlog::info("Starting VQE Optimization...");

  if (!ValidateAuth(context)) {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or missing authorization token");
  }

  // 1. Setup
  int num_qubits = 0;
  std::vector<PauliTerm> hamiltonian;

  if (request->observables_size() > 0) {
    // Arbitrary Observables
    num_qubits = request->observables(0).pauli_string().length();
    for (const auto &obs : request->observables()) {
      hamiltonian.push_back({obs.coefficient(), obs.pauli_string()});
    }
  } else {
    // Fallback to deprecated Molecule Enum
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    auto molType = (request->molecule() == qubit_engine::VQERequest::LiH)
                       ? MolecularHamiltonian::LiH
                       : MolecularHamiltonian::H2;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
    num_qubits = MolecularHamiltonian::getNumQubits(molType);
    hamiltonian = MolecularHamiltonian::getHamiltonian(molType);
  }

  // Ansatz Definition (Hardware Efficient)
  AnsatzFunction applyAnsatz = [](const std::vector<double> &p,
                                  QuantumRegister &qreg) {
    // Layer 1: Ry rotations
    qreg.applyRotationY(0, p[0]);
    qreg.applyRotationY(1, p[1]);

    // Entanglement
    qreg.applyCNOT(0, 1);

    // Layer 2: Ry rotations
    qreg.applyRotationY(0, p[2]);
    qreg.applyRotationY(1, p[3]);
  };

  std::vector<double> params(4, 0.0); // Initialize with 0

  // Hyperparameters
  double learning_rate =
      request->learning_rate() > 0 ? request->learning_rate() : 0.1;
  int max_iters = request->max_iterations();

  bool use_gradient_descent =
      (request->optimizer_type() == qubit_engine::VQERequest::GRADIENT_DESCENT);

  if (use_gradient_descent) {
    spdlog::info("Using Gradient Descent (Parameter Shift Rule)");
  } else {
    spdlog::info("Using SPSA Optimizer");
  }

  // SPSA Constants
  double alpha = 0.602;
  double gamma = 0.101;
  double A = max_iters * 0.1;
  double a = learning_rate; // Use slider!
  double c = 0.1;           // Increased for better sensitivity

  for (int k = 0; k < max_iters; k++) {
    double current_energy = 0.0;

    auto evalEnergy = [&](const std::vector<double> &p) -> double {
      QuantumRegister qreg(num_qubits);
      applyAnsatz(p, qreg);
      double energy = 0.0;
      for (const auto &term : hamiltonian) {
        energy += term.coefficient * qreg.expectationValue(term.pauli_string);
      }
      return energy;
    };

    if (use_gradient_descent) {
      // --- Gradient Descent Logic ---
      auto grads = QuantumDifferentiator::calculateGradients(
          num_qubits, params, applyAnsatz, hamiltonian);

      for (size_t i = 0; i < params.size(); ++i) {
        params[i] -= learning_rate * grads[i];
      }
      current_energy = evalEnergy(params);

    } else {
      // --- SPSA Logic ---
      double ak = a / std::pow(k + 1 + A, alpha);
      double ck = c / std::pow(k + 1, gamma);

      std::vector<double> delta(params.size());
      thread_local std::mt19937 gen(std::random_device{}());
      std::bernoulli_distribution dist(0.5);
      for (size_t i = 0; i < params.size(); ++i)
        delta[i] = dist(gen) ? 1.0 : -1.0;

      std::vector<double> p_plus = params;
      std::vector<double> p_minus = params;
      for (size_t i = 0; i < params.size(); ++i) {
        p_plus[i] += ck * delta[i];
        p_minus[i] -= ck * delta[i];
      }

      double E_plus = evalEnergy(p_plus);
      double E_minus = evalEnergy(p_minus);
      double g_est = (E_plus - E_minus) / (2.0 * ck);

      for (size_t i = 0; i < params.size(); ++i) {
        params[i] -= ak * g_est * delta[i];
      }

      // Report central energy, not perturbed average, for smoother chart
      current_energy = evalEnergy(params);
    }

    // Stream Progress every iteration for smooth visual
    if (k % 1 == 0 || k == max_iters - 1) {
      qubit_engine::VQEResponse resp;
      resp.set_iteration(k);
      resp.set_energy(current_energy);
      for (double p : params)
        resp.add_parameters(p);
      resp.set_converged(false);

      if (current_energy < -1.13) {
        resp.set_converged(true);
        writer->Write(resp);
        break;
      }

      writer->Write(resp);
    }
  }

  return grpc::Status::OK;
}

// -----------------------------------------------------------------
// Phase 2: Dynamic Hardware Topology
// -----------------------------------------------------------------

grpc::Status QubitEngineServiceImpl::GetHardwareTopology(
    grpc::ServerContext *context,
    const qubit_engine::HardwareTopologyRequest *request,
    qubit_engine::HardwareTopologyResponse *response) {

  spdlog::info("Serving GetHardwareTopology request...");

  if (!ValidateAuth(context)) {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or missing authorization token");
  }

  qubit_engine::HardwareConfig config;
  auto topoPath = qubit_engine::ConfigManager::Instance().getTopologyPath();
  
  bool loaded = false;
  if (topoPath.has_value()) {
      loaded = config.loadFromFile(topoPath.value());
  } else {
      loaded = config.loadFromFile("topology.json");
  }

  if (!loaded) {
      config.loadDefaultHeavyHex();
  }

  for (const auto &n : config.getNodes()) {
    auto *node = response->add_nodes();
    node->set_id(n.id);
    node->set_x(n.x);
    node->set_y(n.y);
  }

  for (const auto &e : config.getEdges()) {
    auto *edge = response->add_edges();
    edge->set_node1(e.node1);
    edge->set_node2(e.node2);
  }

  return grpc::Status::OK;
}
