#include "ServiceImpl.hpp"
#include "GateDispatch.hpp"
#include "HardwareConfig.hpp"
#include "ConfigManager.hpp"
#include <atomic>
#include "QuantumRegister.hpp"
#include "QuantumJIT.hpp"
#include "Exceptions.hpp"
#include "ipc/SharedMemory.hpp"
#include <cmath>
#include <cstdint>
#include <future>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <memory>
#include <complex>
#include <complex>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h> // For gethostname
#endif
#include <spdlog/spdlog.h>
#include <jwt-cpp/jwt.h>

using grpc::ServerContext;
using grpc::Status;
using qubit_engine::CircuitRequest;
using qubit_engine::GateOperation;
using qubit_engine::QuantumRegister;
using qubit_engine::StateResponse;

namespace qubit_engine {
  using Complex = std::complex<double>;
}

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
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  if (GlobalMemoryStatusEx(&memInfo)) {
    unsigned __int64 available_ram = memInfo.ullAvailPhys;

    // Memory needed = 2^N * sizeof(complex<double>) (16 bytes)
    size_t required_elements = 1ULL << num_qubits;
    size_t required_bytes = required_elements * sizeof(std::complex<double>);
    size_t overhead = required_bytes / 20;

    return available_ram > (required_bytes + overhead);
  }
  return true; // Fallback if API fails
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
      // Generate secure random descriptor instead of simple counter
      static std::random_device rd;
      static std::mt19937 gen(rd());
      static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
      std::string random_id;
      for (int i = 0; i < 16; ++i) {
          random_id += charset[gen() % (sizeof(charset) - 1)];
      }
      
      std::string desc = "qe_shm_" + random_id;
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
        spdlog::warn("SharedMemory creation failed for {}, falling back to gRPC stream: {}", full_desc, e.what());
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
  if (const char* skip = std::getenv("QUBIT_ENGINE_JWT_SECRET")) {
      // Secret is set, verification is mandatory unless SKIP_AUTH is 1
      if (const char* skip_auth = std::getenv("QUBIT_ENGINE_SKIP_AUTH")) {
          if (std::string(skip_auth) == "1") return true;
      }
  } else {
      // SECURITY WARNING: Secret missing
      spdlog::error("CRITICAL: QUBIT_ENGINE_JWT_SECRET not set. Authentication disabled!!!");
      return true; 
  }

  const auto& client_metadata = context->client_metadata();
  std::map<std::string, std::string> metadata;
  for (auto it = client_metadata.begin(); it != client_metadata.end(); ++it) {
      metadata[std::string(it->first.data(), it->first.length())] = 
          std::string(it->second.data(), it->second.length());
  }

  try {
    ValidateAuthInternal(metadata);
    return true;
  } catch (const std::exception& e) {
    spdlog::warn("Authentication failed: {}", e.what());
    // Attach error to context if needed? (ServerContext trailing metadata)
    return false;
  }
}

void QubitEngineServiceImpl::ValidateAuthInternal(const std::map<std::string, std::string>& metadata) const {
  auto iter = metadata.find("authorization");
  if (iter == metadata.end()) {
    throw std::runtime_error("Missing authorization token");
  }
  
  std::string token = iter->second;
  if (token.rfind("Bearer ", 0) == 0) {
    token = token.substr(7);
  }
  
  auto decoded = jwt::decode(token);
  const char* env_secret = std::getenv("QUBIT_ENGINE_JWT_SECRET");
  if (!env_secret) throw std::runtime_error("Internal server error: JWT secret misconfigured");
  
  std::string secret = env_secret;
  auto verifier = jwt::verify()
      .allow_algorithm(jwt::algorithm::hs256{secret})
      .with_issuer("qubit-engine");
  
  verifier.verify(decoded);
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

        qubit_engine::jit::QuantumJIT jit_compiler(qubit_engine::jit::QuantumJIT::O2);

        auto compile_chunk =
            [&jit_compiler, n](const std::vector<qubit_engine::GateOperation> &ch) {
              std::vector<std::pair<std::string, std::vector<int>>> str_gates;
              std::vector<double> params;
              for (const auto& op : ch) {
                  std::string name;
                  std::vector<int> qbs = { static_cast<int>(op.target_qubit()) };
                  
                  switch(op.type()) {
                      case qubit_engine::GateOperation::HADAMARD: name = "H"; break;
                      case qubit_engine::GateOperation::PAULI_X: name = "X"; break;
                      case qubit_engine::GateOperation::PAULI_Y: name = "Y"; break;
                      case qubit_engine::GateOperation::PAULI_Z: name = "Z"; break;
                      case qubit_engine::GateOperation::CNOT: name = "CX"; qbs.push_back(op.control_qubit()); break;
                      case qubit_engine::GateOperation::CZ: name = "CZ"; qbs.push_back(op.control_qubit()); break;
                      case qubit_engine::GateOperation::SWAP: name = "SWAP"; qbs.push_back(op.second_target_qubit()); break;
                      case qubit_engine::GateOperation::ROTATION_X: name = "RX"; break;
                      case qubit_engine::GateOperation::ROTATION_Y: name = "RY"; break;
                      case qubit_engine::GateOperation::ROTATION_Z: name = "RZ"; break;
                      case qubit_engine::GateOperation::PHASE_S: name = "S"; break;
                      case qubit_engine::GateOperation::PHASE_T: name = "T"; break;
                      default: name = "U"; break;
                  }
                  
                  str_gates.push_back({name, qbs});
                  params.push_back(op.angle());
              }
              
              // Perform JIT compilation pass
              return jit_compiler.compile(n, str_gates, params);
            };

        int first_end = std::min(CHUNK_SIZE, num_ops);
        std::vector<qubit_engine::GateOperation> next_chunk(
            ops.begin(), ops.begin() + first_end);

        auto future = std::async(std::launch::async, compile_chunk, next_chunk);

        for (int i = 0; i < num_ops; i += CHUNK_SIZE) {
          auto ir_block = future.get(); // Await JIT thread

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
          if (!ir_block.gates.empty()) {
            for (const auto &g : ir_block.gates) {
              if (g.type == qubit_engine::jit::CompiledGate::FUSED_BLOCK) {
                std::vector<size_t> targets;
                for (int t : g.target_qubits) targets.push_back((size_t)t);
                qreg.applyDenseUnitary(targets, g.fused_unitary);
              } else if (g.type == qubit_engine::jit::CompiledGate::SINGLE_QUBIT) {
                // For single qubit gates, JIT provides a 2x2 matrix
                std::vector<size_t> targets = {(size_t)g.target_qubits[0]};
                std::vector<qubit_engine::Complex> matrix(g.single_matrix.begin(), g.single_matrix.end());
                qreg.applyDenseUnitary(targets, matrix);
              } else if (g.type == qubit_engine::jit::CompiledGate::TWO_QUBIT) {
                // For two qubit gates, JIT provides a 4x4 matrix
                std::vector<size_t> targets;
                for (int t : g.target_qubits) targets.push_back((size_t)t);
                std::vector<qubit_engine::Complex> matrix(g.two_matrix.begin(), g.two_matrix.end());
                qreg.applyDenseUnitary(targets, matrix);
              }
            }
          } else {
            // Fallback: apply original operations if JIT output is empty or unrecognized
            int end = std::min(i + CHUNK_SIZE, num_ops);
            for (int j = i; j < end; ++j) {
              applyGate(qreg, ops[j], response);
            }
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
    int n = qreg.getNumQubits();
    // Layer 1: Parametric Rotations
    for (int i = 0; i < n; ++i) {
        if (i < (int)p.size()) qreg.applyRotationY(i, p[i]);
    }

    // Entanglement (Linear chain)
    for (int i = 0; i < n - 1; ++i) {
        qreg.applyCNOT(i, i + 1);
    }

    // Layer 2: Additional rotations for depth
    for (int i = 0; i < n; ++i) {
        if (i + n < (int)p.size()) qreg.applyRotationY(i, p[i + n]);
    }
  };

  std::vector<double> params(num_qubits * 2, 0.0); 

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

      double target_energy = (num_qubits == 4) ? -7.86 : -1.13;
      if (current_energy < target_energy + 0.001) {
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
