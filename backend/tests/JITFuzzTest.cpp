#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include "../src/QuantumJIT.hpp"

using namespace qubit_engine::jit;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (Size < 4) return 0;

    int num_qubits = (Data[0] % 10) + 2; // 2 to 11 qubits
    QuantumJIT::OptimizationLevel opt_level = static_cast<QuantumJIT::OptimizationLevel>(Data[1] % 5);
    QuantumJIT jit(opt_level);

    std::vector<std::pair<std::string, std::vector<int>>> gates;
    std::vector<double> params;

    size_t offset = 2;
    while (offset + 2 < Size) {
        uint8_t gate_type = Data[offset++];
        int q1 = Data[offset++] % num_qubits;
        
        switch (gate_type % 5) {
            case 0:
                gates.push_back({"H", {q1}});
                break;
            case 1:
                gates.push_back({"X", {q1}});
                break;
            case 2:
                if (offset < Size) {
                    int q2 = Data[offset++] % num_qubits;
                    if (q1 != q2) gates.push_back({"CNOT", {q1, q2}});
                }
                break;
            case 3:
                gates.push_back({"RZ", {q1}});
                params.push_back(3.14159);
                break;
            case 4:
                if (offset < Size) {
                    int q2 = Data[offset++] % num_qubits;
                    if (q1 != q2) gates.push_back({"CZ", {q1, q2}});
                }
                break;
        }
    }

    // Attempt to compile
    try {
        jit.compile(num_qubits, gates, params);
    } catch (...) {
        // Fuzzer ignores standard exceptions, only cares about crashes (segfaults/aborts)
    }

    return 0;
}
