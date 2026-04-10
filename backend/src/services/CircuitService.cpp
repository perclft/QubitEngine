#include "CircuitService.hpp"
#include "../QuantumRegister.hpp"
#include "../GateDispatch.hpp"
#include "../QuantumJIT.hpp"
#include "../Exceptions.hpp"
#include "../ipc/SharedMemory.hpp"
#include <spdlog/spdlog.h>
#include <future>
#include <cmath>
#include <random>
#include <string>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace qubit_engine {
namespace services {

bool CircuitService::hasEnoughMemory(int num_qubits) {
#ifdef __linux__
  struct sysinfo memInfo;
  sysinfo(&memInfo);
  long long available_ram = memInfo.freeram * memInfo.mem_unit;
  size_t required_elements = 1ULL << num_qubits;
  size_t required_bytes = required_elements * sizeof(std::complex<double>);
  size_t overhead = required_bytes / 20;
  return available_ram > (required_bytes + overhead);
#elif defined(_WIN32)
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  if (GlobalMemoryStatusEx(&memInfo)) {
    unsigned __int64 available_ram = memInfo.ullAvailPhys;
    size_t required_elements = 1ULL << num_qubits;
    size_t required_bytes = required_elements * sizeof(std::complex<double>);
    size_t overhead = required_bytes / 20;
    return available_ram > (required_bytes + overhead);
  }
  return true;
#else
  return true;
#endif
}

void CircuitService::applyGate(qubit_engine::QuantumRegister &qreg,
                               const qubit_engine::GateOperation &op,
                               qubit_engine::StateResponse *response) {
  qubit_engine::dispatchGate(qreg, op, nullptr,
                             response->mutable_classical_results());
}

void CircuitService::serializeState(
    const qubit_engine::QuantumRegister &qreg, qubit_engine::StateResponse *response,
    qubit_engine::CircuitRequest::MeasurementStrategy strategy, bool use_shm) {
  
  char hostname[256];
#ifdef _WIN32
  DWORD host_size = sizeof(hostname);
  if (GetComputerNameA(hostname, &host_size)) {
    response->set_server_id(hostname);
  } else {
    response->set_server_id("QubitEngine-Windows");
  }
#else
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    response->set_server_id(hostname);
  } else {
    response->set_server_id("QubitEngine-Unix");
  }
#endif

  if (strategy == qubit_engine::CircuitRequest::FULL_STATE) {
    bool shm_success = false;
    if (use_shm) {
      static std::random_device rd;
      static std::mt19937 gen(rd());
      static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
      std::string shm_name = "/qe_shm_";
      for (int i = 0; i < 16; ++i) {
        shm_name += charset[gen() % (sizeof(charset) - 1)];
      }

      try {
        auto state_vec = qreg.getStateVector();
        size_t bytes = state_vec.size() * sizeof(qubit_engine::Complex);
        qubit_engine::ipc::SharedMemory shm(shm_name, bytes, true);
        std::memcpy(shm.data(), state_vec.data(), bytes);
        response->set_shm_descriptor(shm_name);
        shm_success = true;
      } catch (const std::exception &e) {
        spdlog::error("Shared Memory Error in serializeState: {}", e.what());
      }
    }

    if (!shm_success) {
      auto state_vec = qreg.getStateVector();
      for (const auto &c : state_vec) {
        auto *pb_c = response->add_state_vector();
        pb_c->set_real(c.real());
        pb_c->set_imag(c.imag());
      }
    }
  } else if (strategy == qubit_engine::CircuitRequest::SPARSE_STATE) {
    auto probs = qreg.getProbabilities();
    for (size_t i = 0; i < probs.size(); ++i) {
      if (probs[i] > 1e-6) {
        auto *sparse = response->add_sparse_states();
        sparse->set_qubit_index(static_cast<uint64_t>(i));
        sparse->set_probability(probs[i]);
      }
    }
  } else if (strategy == qubit_engine::CircuitRequest::EXPECTATION_VALUES) {
    double exp_val = qreg.expectationValue("Z");
    spdlog::info("Expectation Value (Z): {}", exp_val);
  }
}

grpc::Status CircuitService::RunCircuit(grpc::ServerContext *context,
                                        const qubit_engine::CircuitRequest *request,
                                        qubit_engine::StateResponse *response) {
  spdlog::debug("RunCircuit domain logic invoked!");

  int n = request->num_qubits();
  if (n <= 0 || n > 30) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Qubits must be between 1 and 30");
  }

  if (!hasEnoughMemory(n)) {
    return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, 
                        "Insufficient Server Memory for " + std::to_string(n) + " qubits.");
  }

  try {
    qubit_engine::QuantumRegister qreg(n);
    int num_ops = request->operations_size();

    if (num_ops > 0) {
      if (n >= 25) {
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
        auto compile_chunk = [&jit_compiler, n](const std::vector<qubit_engine::GateOperation> &ch) {
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
              case qubit_engine::GateOperation::CNOT: name = "CX"; qbs = { static_cast<int>(op.control_qubit()), static_cast<int>(op.target_qubit()) }; break;
              case qubit_engine::GateOperation::CZ: name = "CZ"; qbs = { static_cast<int>(op.control_qubit()), static_cast<int>(op.target_qubit()) }; break;
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
          return jit_compiler.compile(n, str_gates, params);
        };

        int first_end = std::min(CHUNK_SIZE, num_ops);
        std::vector<qubit_engine::GateOperation> next_chunk(ops.begin(), ops.begin() + first_end);
        auto future = std::async(std::launch::async, compile_chunk, next_chunk);

        for (int i = 0; i < num_ops; i += CHUNK_SIZE) {
          auto ir_block = future.get();
          if (i + CHUNK_SIZE < num_ops) {
            int next_start = i + CHUNK_SIZE;
            int next_end = std::min(next_start + CHUNK_SIZE, num_ops);
            next_chunk = std::vector<qubit_engine::GateOperation>(ops.begin() + next_start, ops.begin() + next_end);
            future = std::async(std::launch::async, compile_chunk, next_chunk);
          }

          if (!ir_block.gates.empty()) {
            for (const auto &g : ir_block.gates) {
              std::vector<size_t> targets;
              for (int t : g.target_qubits) targets.push_back((size_t)t);

              // Prefer fused_unitary — fuse_tensor_network stores results
              // there even for blocks it labels SINGLE_QUBIT / TWO_QUBIT.
              if (!g.fused_unitary.empty()) {
                qreg.applyDenseUnitary(targets, g.fused_unitary);
              } else if (g.type == qubit_engine::jit::CompiledGate::SINGLE_QUBIT) {
                std::vector<qubit_engine::Complex> matrix(g.single_matrix.begin(), g.single_matrix.end());
                qreg.applyDenseUnitary(targets, matrix);
              } else if (g.type == qubit_engine::jit::CompiledGate::TWO_QUBIT) {
                std::vector<qubit_engine::Complex> matrix(g.two_matrix.begin(), g.two_matrix.end());
                qreg.applyDenseUnitary(targets, matrix);
              }
            }
          } else {
            int end = std::min(i + CHUNK_SIZE, num_ops);
            for (int j = i; j < end; ++j) {
              applyGate(qreg, ops[j], response);
            }
          }
        }
      }
    }

    serializeState(qreg, response, request->measurement_strategy(), request->use_shm());
  } catch (const qubit_engine::InvalidArgumentException &e) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
  } catch (const qubit_engine::QubitOutOfRangeException &e) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, e.what());
  } catch (const std::exception &e) {
    return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Internal Engine Error: ") + e.what());
  }

  return grpc::Status::OK;
}

grpc::Status CircuitService::StreamGates(
    grpc::ServerContext *context,
    grpc::ServerReaderWriter<qubit_engine::StateResponse,
                             qubit_engine::GateStreamRequest> *stream) {
  qubit_engine::GateStreamRequest first;
  if (!stream->Read(&first)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "StreamGates requires an initial init message");
  }
  if (first.msg_case() != qubit_engine::GateStreamRequest::kInit) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "First StreamGates message must be init");
  }

  const auto &init = first.init();
  const int num_qubits = init.num_qubits();
  const bool use_shm = init.use_shm();

  if (num_qubits <= 0 || num_qubits > 30) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Qubits must be between 1 and 30");
  }

  qubit_engine::QuantumRegister qreg(static_cast<size_t>(num_qubits));

  try {
    qubit_engine::GateStreamRequest req;
    while (stream->Read(&req)) {
      qubit_engine::StateResponse response;
      if (req.msg_case() == qubit_engine::GateStreamRequest::kInit) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "StreamGates init message is only allowed as the first message");
      }
      if (req.msg_case() != qubit_engine::GateStreamRequest::kOp) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "StreamGates received unknown message type");
      }
      applyGate(qreg, req.op(), &response);
      serializeState(qreg, &response, qubit_engine::CircuitRequest::FULL_STATE, use_shm);
      stream->Write(response);
    }
  } catch (const std::exception &e) {
    return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
  }

  return grpc::Status::OK;
}

grpc::Status CircuitService::VisualizeCircuit(
    grpc::ServerContext *context, const qubit_engine::CircuitRequest *request,
    grpc::ServerWriter<qubit_engine::StateResponse> *writer) {
  spdlog::info("[VisualizeCircuit] Received request for {} qubits.", request->num_qubits());
  qubit_engine::QuantumRegister qreg(request->num_qubits());

  for (const auto &op : request->operations()) {
    qubit_engine::StateResponse response;
    applyGate(qreg, op, &response);
    if (request->noise_probability() > 0.0) {
      qreg.applyDepolarizingNoise(request->noise_probability());
    }
    serializeState(qreg, &response, request->measurement_strategy(), false);
    if (!writer->Write(response)) {
      return grpc::Status::CANCELLED;
    }
  }
  return grpc::Status::OK;
}

} // namespace services
} // namespace qubit_engine
