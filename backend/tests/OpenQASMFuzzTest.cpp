#include "parser/QASMParser.hpp"
#include <cstdint>
#include <string>
#include <memory>

// Fuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::string input(reinterpret_cast<const char *>(data), size);

  try {
    qubit_engine::parser::QASMParser parser(input);
    std::shared_ptr<qubit_engine::parser::ASTNode> ast = parser.parse();
  } catch (...) {
  }

  return 0;
}
