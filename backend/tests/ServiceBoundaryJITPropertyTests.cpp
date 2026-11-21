// Property-based Tests for Service Boundary -> JIT Compiler
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "../src/services/CircuitService.hpp"
#include "../src/QuantumRegister.hpp"
#include "../src/backends/CpuBackend.hpp"
#include "quantum.grpc.pb.h"
#include <complex>
#include <vector>

using namespace qubit_engine;

struct GeneratedOp {
    GateOperation::GateType type;
    int target;
    int second_target; // control for CNOT/CZ, second target for SWAP
    double angle;
};

struct GeneratedCircuit {
    int num_qubits;
    std::vector<GeneratedOp> ops;
};

namespace rc {
template <>
struct Arbitrary<GeneratedCircuit> {
    static Gen<GeneratedCircuit> arbitrary() {
        return gen::exec([] {
            GeneratedCircuit circ;
            circ.num_qubits = *gen::inRange(2, 6);
            int num_ops = *gen::inRange(5, 30);
            for (int i = 0; i < num_ops; ++i) {
                int gate_kind = *gen::inRange(0, 12);
                int q1 = *gen::inRange(0, circ.num_qubits);
                int q2 = *gen::inRange(0, circ.num_qubits);
                if (q1 == q2) {
                    q2 = (q1 + 1) % circ.num_qubits;
                }
                double angle = *gen::inRange(-314, 314) / 100.0;

                GeneratedOp op;
                op.target = q1;
                op.second_target = q2;
                op.angle = angle;

                switch (gate_kind) {
                    case 0: op.type = GateOperation::HADAMARD; break;
                    case 1: op.type = GateOperation::PAULI_X; break;
                    case 2: op.type = GateOperation::PAULI_Y; break;
                    case 3: op.type = GateOperation::PAULI_Z; break;
                    case 4: op.type = GateOperation::PHASE_S; break;
                    case 5: op.type = GateOperation::PHASE_T; break;
                    case 6: op.type = GateOperation::ROTATION_X; break;
                    case 7: op.type = GateOperation::ROTATION_Y; break;
                    case 8: op.type = GateOperation::ROTATION_Z; break;
                    case 9: op.type = GateOperation::CNOT; break;
                    case 10: op.type = GateOperation::CZ; break;
                    case 11: op.type = GateOperation::SWAP; break;
                    default: op.type = GateOperation::HADAMARD; break;
                }
                circ.ops.push_back(op);
            }
            return circ;
        });
    }
};
} // namespace rc

TEST(ServiceBoundaryJITPropertyTest, RunCircuitMatchesReferenceSimulation) {
    rc::check("CircuitService::RunCircuit produces identical state to direct unoptimized simulation",
              [](GeneratedCircuit circ) {
                  // 1. Build Protobuf CircuitRequest
                  CircuitRequest request;
                  request.set_num_qubits(circ.num_qubits);
                  request.set_measurement_strategy(CircuitRequest::FULL_STATE);
                  request.set_use_shm(false);

                  for (const auto& gen_op : circ.ops) {
                      auto* op = request.add_operations();
                      op->set_type(gen_op.type);
                      op->set_target_qubit(gen_op.target);

                      if (gen_op.type == GateOperation::CNOT || gen_op.type == GateOperation::CZ) {
                          op->set_control_qubit(gen_op.second_target);
                          op->set_target_qubit(gen_op.target);
                      } else if (gen_op.type == GateOperation::SWAP) {
                          op->set_target_qubit(gen_op.target);
                          op->set_second_target_qubit(gen_op.second_target);
                      } else if (gen_op.type == GateOperation::ROTATION_X ||
                                 gen_op.type == GateOperation::ROTATION_Y ||
                                 gen_op.type == GateOperation::ROTATION_Z) {
                          op->set_angle(gen_op.angle);
                      }
                  }

                  // 2. Execute via CircuitService (which invokes QuantumJIT O2 chunk compilation)
                  services::CircuitService service;
                  StateResponse response;
                  grpc::ServerContext context;
                  grpc::Status status = service.RunCircuit(&context, &request, &response, /*authorized=*/true);
                  RC_ASSERT(status.ok());

                  // 3. Execute via Direct Reference Simulation on CpuBackend
                  QuantumRegister ref_reg(static_cast<size_t>(circ.num_qubits),
                                          std::make_unique<CpuBackend>(circ.num_qubits));

                  for (const auto& gen_op : circ.ops) {
                      switch (gen_op.type) {
                          case GateOperation::HADAMARD:
                              ref_reg.applyHadamard(gen_op.target);
                              break;
                          case GateOperation::PAULI_X:
                              ref_reg.applyX(gen_op.target);
                              break;
                          case GateOperation::PAULI_Y:
                              ref_reg.applyY(gen_op.target);
                              break;
                          case GateOperation::PAULI_Z:
                              ref_reg.applyZ(gen_op.target);
                              break;
                          case GateOperation::PHASE_S:
                              ref_reg.applyPhaseS(gen_op.target);
                              break;
                          case GateOperation::PHASE_T:
                              ref_reg.applyPhaseT(gen_op.target);
                              break;
                          case GateOperation::ROTATION_X:
                              ref_reg.applyRotationX(gen_op.target, gen_op.angle);
                              break;
                          case GateOperation::ROTATION_Y:
                              ref_reg.applyRotationY(gen_op.target, gen_op.angle);
                              break;
                          case GateOperation::ROTATION_Z:
                              ref_reg.applyRotationZ(gen_op.target, gen_op.angle);
                              break;
                          case GateOperation::CNOT:
                              ref_reg.applyCNOT(gen_op.second_target, gen_op.target);
                              break;
                          case GateOperation::CZ:
                              ref_reg.applyCZ(gen_op.second_target, gen_op.target);
                              break;
                          case GateOperation::SWAP:
                              ref_reg.applySWAP(gen_op.target, gen_op.second_target);
                              break;
                          default:
                              break;
                      }
                  }

                  auto ref_psi = ref_reg.getStateVector();
                  size_t expected_dim = 1ULL << circ.num_qubits;
                  RC_ASSERT(static_cast<size_t>(response.state_vector_size()) == expected_dim);
                  RC_ASSERT(ref_psi.size() == expected_dim);

                  // 4. Calculate State-Vector Fidelity
                  Complex dot_product = 0.0;
                  for (size_t i = 0; i < expected_dim; ++i) {
                      Complex c_svc(response.state_vector(static_cast<int>(i)).real(),
                                    response.state_vector(static_cast<int>(i)).imag());
                      dot_product += std::conj(ref_psi[i]) * c_svc;
                  }
                  double fidelity = std::norm(dot_product);

                  // State-vector fidelity between JIT service execution and unoptimized reference simulation must be >= 1.0 - 1e-6
                  RC_ASSERT(fidelity >= 1.0 - 1e-6);
              });
}
