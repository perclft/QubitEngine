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
    
    int n = request->num_qubits();
    // Validate negative input manually (proto int32 -> C++ int conversion safety)
    if (n <= 0 || n > 25) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Qubits must be between 1 and 25");
    }

    try {
        // Initialize Engine
        QuantumRegister qreg(static_cast<size_t>(n));

        // Process Gates
        for (const auto& op : request->operations()) {
            
            // Explicit Validation: CNOT Control vs Target
            if (op.type() == GateOperation::CNOT) {
                if (op.control_qubit() == op.target_qubit()) {
                    return Status(grpc::StatusCode::INVALID_ARGUMENT, "CNOT: Control cannot equal Target");
                }
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

        // Serialize Response
        const auto& state = qreg.getStateVector();
        for (const auto& amp : state) {
            auto* c = response->add_state_vector();
            c->set_real(amp.real());
            c->set_imag(amp.imag());
        }

    } catch (const std::out_of_range& e) {
        // Map C++ bounds check failure to Client Error
        return Status(grpc::StatusCode::INVALID_ARGUMENT, std::string("Index Error: ") + e.what());
    } catch (const std::invalid_argument& e) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, std::string("Logic Error: ") + e.what());
    } catch (const std::exception& e) {
        // Catch-all for unexpected errors (Allocation failures, etc.)
        return Status(grpc::StatusCode::INTERNAL, std::string("Internal Engine Error: ") + e.what());
    }

    return Status::OK;
}
