#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
#include <iostream>

using grpc::ServerContext;
using grpc::Status;
using qubit_engine::CircuitRequest;
using qubit_engine::StateResponse;
using qubit_engine::GateOperation;

Status QubitEngineServiceImpl::RunCircuit(ServerContext* context, 
                                        const CircuitRequest* request,
                                        StateResponse* response) {
    
    // 1. Validate Qubit Count
    int n = request->num_qubits();
    if (n <= 0 || n > 25) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Qubits must be between 1 and 25");
    }

    // 2. Initialize Engine
    QuantumRegister qreg(static_cast<size_t>(n));

    // 3. Process Gates with Error Handling
    try {
        for (const auto& op : request->operations()) {
            // FIX: Validate gate indices against N
            if (op.target_qubit() >= (uint32_t)n) {
                return Status(grpc::StatusCode::INVALID_ARGUMENT, "Target qubit index out of range");
            }
            if (op.type() == GateOperation::CNOT && op.control_qubit() >= (uint32_t)n) {
                return Status(grpc::StatusCode::INVALID_ARGUMENT, "Control qubit index out of range");
            }

            switch (op.type()) {
                case GateOperation::HADAMARD:
                    qreg.applyHadamard(op.target_qubit());
                    break;
                case GateOperation::PAULI_X:
                    qreg.applyX(op.target_qubit());
                    break;
                case GateOperation::CNOT:
                    qreg.applyCNOT(op.control_qubit(), op.target_qubit());
                    break;
                default:
                    return Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown Gate Type");
            }
        }
    } catch (const std::exception& e) {
        return Status(grpc::StatusCode::INTERNAL, e.what());
    }

    // 4. Serialize Response
    const auto& state = qreg.getStateVector();
    for (const auto& amp : state) {
        auto* c = response->add_state_vector();
        c->set_real(amp.real());
        c->set_imag(amp.imag());
    }

    return Status::OK;
}
