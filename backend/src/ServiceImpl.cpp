#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
#include <iostream>

using grpc::ServerContext;
using grpc::Status;
using qubit_engine::CircuitRequest;
using qubit_engine::StateResponse;
using qubit_engine::GateOperation;

#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
#include <iostream>
#include <sys/sysinfo.h> // Linux system headers
#include <cmath>

// HELPER: Check if we have enough RAM
bool hasEnoughMemory(int num_qubits) {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    
    long long available_ram = memInfo.freeram * memInfo.mem_unit;
    
    // Size = 2^N * sizeof(complex<double>)
    // complex<double> is usually 16 bytes
    size_t required_elements = 1ULL << num_qubits;
    size_t required_bytes = required_elements * sizeof(std::complex<double>);

    // Add 100MB buffer for overhead
    size_t overhead = 100 * 1024 * 1024; 

    return available_ram > (required_bytes + overhead);
}

// ... inside RunCircuit ...

Status QubitEngineServiceImpl::RunCircuit(ServerContext* context, 
                                        const CircuitRequest* request,
                                        StateResponse* response) {
    int n = request->num_qubits();
    
    // 1. HARD LIMIT CHECK (Validation)
    if (n <= 0 || n > 28) { // 28 qubits ~4GB, strict upper bound
         return Status(grpc::StatusCode::INVALID_ARGUMENT, "Qubits must be between 1 and 28");
    }

    // 2. DYNAMIC MEMORY CHECK (Systems Engineering)
    if (!hasEnoughMemory(n)) {
        std::string err = "Insufficient Server Memory for " + std::to_string(n) + " qubits.";
        return Status(grpc::StatusCode::RESOURCE_EXHAUSTED, err);
    }

    // ... continue with try/catch block ...
}


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
