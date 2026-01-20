#include "CircuitOptimizer.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace qubit_engine;

void testIdentityCancellation() {
  std::vector<QuantumRegister::RecordedGate> tape;

  // H - H -> Identity
  tape.push_back({QuantumRegister::RecordedGate::H, {0}, {}});
  tape.push_back({QuantumRegister::RecordedGate::H, {0}, {}});

  CircuitOptimizer::optimize(tape);

  assert(tape.empty());
  std::cout << "testIdentityCancellation Passed" << std::endl;
}

void testMixedCancellation() {
  std::vector<QuantumRegister::RecordedGate> tape;

  // H - X - X - H -> H - I - H -> H - H -> I
  tape.push_back({QuantumRegister::RecordedGate::H, {0}, {}});
  tape.push_back({QuantumRegister::RecordedGate::X, {0}, {}});
  tape.push_back({QuantumRegister::RecordedGate::X, {0}, {}});
  tape.push_back({QuantumRegister::RecordedGate::H, {0}, {}});

  CircuitOptimizer::optimize(tape);

  assert(tape.empty());
  std::cout << "testMixedCancellation Passed" << std::endl;
}

void testNonCancellation() {
  std::vector<QuantumRegister::RecordedGate> tape;

  // H - X (No cancel)
  tape.push_back({QuantumRegister::RecordedGate::H, {0}, {}});
  tape.push_back({QuantumRegister::RecordedGate::X, {0}, {}});

  CircuitOptimizer::optimize(tape);

  assert(tape.size() == 2);
  std::cout << "testNonCancellation Passed" << std::endl;
}

int main() {
  testIdentityCancellation();
  testMixedCancellation();
  testNonCancellation();
  return 0;
}
