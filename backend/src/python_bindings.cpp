#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "MolecularHamiltonian.hpp"
#include "QuantumDifferentiator.hpp"
#include "QuantumRegister.hpp"

namespace py = pybind11;

// Wrapper to adapt Python callables to AnsatzFunction signature
void applyAnsatzWrapper(
    const std::function<void(std::vector<double>, QuantumRegister *)> &py_func,
    const std::vector<double> &params, QuantumRegister &qreg) {
  // Note: We pass &qreg to Python because pybind11 handles pointers/refs better
  // for mutation
  py_func(params, &qreg);
}

PYBIND11_MODULE(qubit_engine, m) {
  m.doc() = "QubitEngine Python Bindings";

  // --- QuantumRegister Binding ---
  py::class_<QuantumRegister>(m, "QuantumRegister")
      .def(py::init<size_t>(), "Initialize a quantum register with N qubits")
      .def("applyHadamard", &QuantumRegister::applyHadamard)
      .def("applyX", &QuantumRegister::applyX)
      .def("applyY", &QuantumRegister::applyY)
      .def("applyZ", &QuantumRegister::applyZ)
      .def("applyCNOT", &QuantumRegister::applyCNOT)
      .def("applyToffoli", &QuantumRegister::applyToffoli)
      .def("applyRotationY", &QuantumRegister::applyRotationY)
      .def("applyRotationZ", &QuantumRegister::applyRotationZ)
      .def("measure", &QuantumRegister::measure)
      .def("getProbabilities", &QuantumRegister::getProbabilities)
      .def("expectationValue", &QuantumRegister::expectationValue)
      .def("getStateVector", &QuantumRegister::getStateVector);

  // --- QuantumDifferentiator Binding ---
  // We bind a helper function that takes a Python callable for the ansatz
  m.def(
      "calculate_gradients",
      [](int num_qubits, std::vector<double> params, py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data) {
        // Convert simplified Python Hamiltonian data to C++ PauliTerm
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }

        // Adapter for the C++ expected AnsatzFunction signature
        AnsatzFunction cpp_ansatz = [&](const std::vector<double> &p,
                                        QuantumRegister &q) {
          // Call the python function.
          // Warning: releasing GIL might be needed for threading, but we keep
          // it simple for now.
          ansatz_func(p, &q);
        };

        return QuantumDifferentiator::calculateGradients(
            num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate analytical gradients using Parameter Shift Rule");
}
