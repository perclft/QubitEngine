#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
#include <iostream>
#include <cmath>
#include <sys/sysinfo.h> // Linux system headers for memory check

using grpc::ServerContext;
using grpc::Status;
using qubit_engine::CircuitRequest;
using qubit_engine::StateResponse;
using qubit_engine::GateOperation;

// HELPER: Check if the server has enough free RAM for the requested qubits
bool hasEnoughMemory(int num_qubits) {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    
    long long available_ram = memInfo.freeram * memInfo.mem_unit;
    
    // Memory needed = 2^N * sizeof(complex<double>) (16 bytes)
    // 1ULL ensures we do 64-bit arithmetic
    size_t required_elements = 1ULL << num_qubits;
    size_t required_bytes = required_elements * sizeof(std::complex<double>);

    // Add 100MB buffer for OS/Process overhead
    size_t overhead = 100 * 1024 * 1024; 

    return available_ram > (required_bytes + overhead);
}

Status QubitEngineServiceImpl::RunCircuit(ServerContext* context, 
                                        const CircuitRequest* request,
                                        StateResponse* response) {
    
    int n = request->num_qubits();

    // 1. HARD LIMIT CHECK
    // 28 qubits is ~4GB of RAM. A safe upper bound for a single node.
    if (n <= 0 || n > 28) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Qubits must be between 1 and 28");
    }

    // 2. DYNAMIC MEMORY CHECK
    // Prevents the OOM Killer from killing the pod
    if (!hasEnoughMemory(n)) {
        std::string err = "Insufficient Server Memory for " + std::to_string(n) + " qubits.";
        std::cerr << "ResourceExhausted: " << err << std::endl;
        return Status(grpc::StatusCode::RESOURCE_EXHAUSTED, err);
    }

    try {
        // Initialize Engine
        QuantumRegister qreg(static_cast<size_t>(n));

        // Process Gates
        for (const auto& op : request->operations()) {
            
            // Logic Validation
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
        return Status(grpc::StatusCode::INVALID_ARGUMENT, std::string("Index Error: ") + e.what());
    } catch (const std::invalid_argument& e) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, std::string("Logic Error: ") + e.what());
    } catch (const std::exception& e) {
        return Status(grpc::StatusCode::INTERNAL, std::string("Internal Engine Error: ") + e.what());
    }

    return Status::OK;
}
