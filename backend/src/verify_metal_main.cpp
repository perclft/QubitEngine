#include "backends/MetalBackend.hpp"
#include <complex>
#include <iostream>
#include <vector>

int main() {
  std::cout << "Verifying Metal Backend..." << std::endl;
  try {
    size_t n = 2;
    qubit_engine::MetalBackend mb(n);
    std::cout << "MetalBackend initialized successfully." << std::endl;

    // Simple Operation (Hadamard gate)
    mb.applyHadamard(0);
    std::cout << "Applied Hadamard on qubit 0." << std::endl;

    // Note: Without default.metallib compiled, run-time behavior might fail
    // if it relies on kernels, but at least we verify initialization logic
    // finishes.

    auto state = mb.getStateVector();
    std::cout << "State vector retrieval successful. Size: " << state.size()
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Initialization failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
