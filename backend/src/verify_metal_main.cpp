#include "backends/MetalBackend.hpp"
#include <complex>
#include <spdlog/spdlog.h>
#include <vector>

int main() {
  spdlog::info("Verifying Metal Backend...");
  try {
    size_t n = 2;
    qubit_engine::MetalBackend mb(n);
    spdlog::info("MetalBackend initialized successfully.");

    // Simple Operation (Hadamard gate)
    mb.applyHadamard(0);
    spdlog::info("Applied Hadamard on qubit 0.");

    // Note: Without default.metallib compiled, run-time behavior might fail
    // if it relies on kernels, but at least we verify initialization logic
    // finishes.

    auto state = mb.getStateVector();
    spdlog::info("State vector retrieval successful. Size: {}", state.size());

  } catch (const std::exception &e) {
    spdlog::error("Initialization failed: {}", e.what());
    return 1;
  }
  return 0;
}
