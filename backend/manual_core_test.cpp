#include "src/QuantumRegister.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

void assertComplexc(Complex a, Complex b, std::string msg) {
    if (std::abs(a - b) > 1e-5) {
        std::cerr << "FAIL: " << msg << " Expected " << b << " Got " << a << std::endl;
        exit(1);
    }
}

void printState(const std::vector<Complex>& state) {
    for (size_t i = 0; i < state.size(); ++i) {
        if (std::abs(state[i]) > 1e-5) {
            std::cout << "|" << i << ">: " << state[i] << std::endl;
        }
    }
}

int main() {
    std::cout << "Running Core Physics Engine Verification..." << std::endl;

    // Test 1: Hadamard Gate
    {
        std::cout << "[Test 1] Hadamard on Q0" << std::endl;
        QuantumRegister reg(1); // 1 qubit
        reg.applyHadamard(0);
        auto state = reg.getStateVector();
        // Expected: |+> = 1/sqrt(2) (|0> + |1>)
        double v = 1.0 / std::sqrt(2.0);
        assertComplexc(state[0], Complex(v, 0), "H|0> -> |0> coeff");
        assertComplexc(state[1], Complex(v, 0), "H|0> -> |1> coeff");
        std::cout << "PASS" << std::endl;
    }

    // Test 2: CNOT
    {
        std::cout << "[Test 2] Bell State (H + CNOT)" << std::endl;
        QuantumRegister reg(2);
        reg.applyHadamard(0); // |+>|0>
        reg.applyCNOT(0, 1);  // |00> + |11>
        auto state = reg.getStateVector();
        double v = 1.0 / std::sqrt(2.0);
        assertComplexc(state[0], Complex(v, 0), "|00> coeff");
        assertComplexc(state[1], Complex(0, 0), "|01> coeff"); // Should be 0
        assertComplexc(state[2], Complex(0, 0), "|10> coeff"); // Should be 0
        assertComplexc(state[3], Complex(v, 0), "|11> coeff");
        std::cout << "PASS" << std::endl;
    }
    
     // Test 3: Pauli X
    {
        std::cout << "[Test 3] Pauli X" << std::endl;
        QuantumRegister reg(1);
        reg.applyX(0); // |1>
        auto state = reg.getStateVector();
        assertComplexc(state[0], Complex(0, 0), "|0> coeff");
        assertComplexc(state[1], Complex(1, 0), "|1> coeff");
        std::cout << "PASS" << std::endl;
    }

    std::cout << "ALL CORE TESTS PASSED" << std::endl;
    return 0;
}
