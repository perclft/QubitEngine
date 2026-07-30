#include "NoiseModel.hpp"
#include <iostream>
#include <stdexcept>

using namespace qubit_engine;

int main() {
    std::cout << "Running NoiseModel Bounds Check Test..." << std::endl;
    
    try {
        auto ch = makeDepolarizingChannel1Q(0.8);
        std::cout << "FAIL: 1Q p=0.8 did not throw!" << std::endl;
        return 1;
    } catch(const std::invalid_argument& e) {
        std::cout << "PASS: 1Q p=0.8 threw exception: " << e.what() << std::endl;
    }
    
    try {
        auto ch = makeDepolarizingChannel2Q(0.95);
        std::cout << "FAIL: 2Q p=0.95 did not throw!" << std::endl;
        return 1;
    } catch(const std::invalid_argument& e) {
        std::cout << "PASS: 2Q p=0.95 threw exception: " << e.what() << std::endl;
    }
    
    return 0;
}
