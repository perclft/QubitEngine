#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "AdamOptimizer.hpp"
#include "MolecularHamiltonian.hpp"
#include "QuantumDifferentiator.hpp"
#include "QuantumRegister.hpp"

// Include GPU headers
#include "backends/GPUQuantumRegister.hpp"

namespace py = pybind11;

// Wrapper to adapt Python callables to AnsatzFunction signature
void applyAnsatzWrapper(
    const std::function<void(std::vector<double>, QuantumRegister *)> &py_func,
    const std::vector<double> &params, QuantumRegister &qreg) {
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

  // --- GPUQuantumRegister Binding ---
  py::class_<GPUQuantumRegister>(m, "GPUQuantumRegister")
      .def(py::init<size_t>(),
           "Initialize a GPU quantum register with N qubits")
      .def("applyHadamard", &GPUQuantumRegister::applyHadamard)
      .def("applyX", &GPUQuantumRegister::applyX)
      .def("applyY", &GPUQuantumRegister::applyY)
      .def("applyZ", &GPUQuantumRegister::applyZ)
      .def("applyRotationY", &GPUQuantumRegister::applyRotationY)
      .def("getStateVector", &GPUQuantumRegister::getStateVector);

  // --- QuantumDifferentiator Binding ---
  m.def(
      "calculate_gradients",
      [](int num_qubits, std::vector<double> params, py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data) {
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }

        QuantumDifferentiator::AnsatzFunc<QuantumRegister> cpp_ansatz =
            [&](const std::vector<double> &p, QuantumRegister &q) {
              ansatz_func(p, &q);
            };

        return QuantumDifferentiator::calculateGradients(
            num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate analytical gradients using Parameter Shift Rule");

  m.def(
      "calculate_gradients_adjoint",
      [](int num_qubits, std::vector<double> params, py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data) {
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }
        QuantumDifferentiator::AnsatzFunc<QuantumRegister> cpp_ansatz =
            [&](const std::vector<double> &p, QuantumRegister &q) {
              ansatz_func(p, &q);
            };
        return QuantumDifferentiator::calculateGradientsAdjoint<
            QuantumRegister>(num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate analytical gradients using Adjoint Method (CPU)");

  m.def(
      "calculate_gradients_adjoint_gpu",
      [](int num_qubits, std::vector<double> params, py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data) {
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }
        QuantumDifferentiator::AnsatzFunc<GPUQuantumRegister> cpp_ansatz =
            [&](const std::vector<double> &p, GPUQuantumRegister &q) {
              ansatz_func(p, &q);
            };
        return QuantumDifferentiator::calculateGradientsAdjoint<
            GPUQuantumRegister>(num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate analytical gradients using Adjoint Method (GPU)");

  // --- AdamOptimizer Binding ---
  using qubit_engine::optimizers::AdamOptimizer;
  py::class_<AdamOptimizer>(m, "AdamOptimizer")
      .def(py::init<>()) // Default config
      .def(
          "minimize",
          [](AdamOptimizer &optimizer, int num_qubits, py::function ansatz_func,
             std::vector<std::pair<double, std::string>> hamiltonian_data,
             std::vector<double> initial_params) {
            std::vector<PauliTerm> hamiltonian;
            for (const auto &item : hamiltonian_data) {
              hamiltonian.push_back({item.first, item.second});
            }
            QuantumDifferentiator::AnsatzFunc<QuantumRegister> cpp_ansatz =
                [&](const std::vector<double> &p, QuantumRegister &q) {
                  ansatz_func(p, &q);
                };

            return optimizer.minimize(cpp_ansatz, hamiltonian, num_qubits,
                                      initial_params);
          },
          "Run Adam Optimizer natively in C++");
}
