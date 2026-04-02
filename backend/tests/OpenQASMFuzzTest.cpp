#include "OpenQASM3Parser.hpp"
#include <cstdint>
#include <string>

// Fuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Convert input data to string
  std::string input(reinterpret_cast<const char *>(data), size);

  try {
    qubit_engine::qasm3::OpenQASM3Parser parser(input);
    parser.parse();
  } catch (...) {
    // Ignore exceptions, we only care about crashes (e.g., memory corruption, segfaults)
  }

  return 0; // Return 0 to indicate successful execution of the fuzzer iteration
}
