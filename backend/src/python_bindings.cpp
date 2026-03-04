#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "AdamOptimizer.hpp"
#include "MolecularHamiltonian.hpp"
#include "QuantumDifferentiator.hpp"
#include "QuantumRegister.hpp"
#include "SPSAOptimizer.hpp"

// Include GPU headers
#include "backends/GPUQuantumRegister.hpp"

// Include Stabilizer Backend
#include "backends/StabilizerBackend.hpp"

namespace py = pybind11;

// Wrapper to adapt Python callables to AnsatzFunction signature
void applyAnsatzWrapper(
    const std::function<void(std::vector<double>, QuantumRegister *)> &py_func,
    const std::vector<double> &params, QuantumRegister &qreg) {
  py_func(params, &qreg);
}

PYBIND11_MODULE(core, m) {
  m.doc() = "QubitEngine Python Bindings";

  // --- StabilizerBackend Binding ---
  py::class_<qubit_engine::StabilizerBackend>(m, "StabilizerBackend")
      .def(py::init<size_t>(), py::arg("num_qubits"))
      .def("applyHadamard", &qubit_engine::StabilizerBackend::applyHadamard,
           py::arg("target"), "Apply Hadamard gate")
      .def("applyX", &qubit_engine::StabilizerBackend::applyX,
           py::arg("target"), "Apply Pauli-X gate")
      .def("applyY", &qubit_engine::StabilizerBackend::applyY,
           py::arg("target"), "Apply Pauli-Y gate")
      .def("applyZ", &qubit_engine::StabilizerBackend::applyZ,
           py::arg("target"), "Apply Pauli-Z gate")
      .def("applyCNOT", &qubit_engine::StabilizerBackend::applyCNOT,
           py::arg("control"), py::arg("target"), "Apply CNOT gate")
      .def("applyPhaseS", &qubit_engine::StabilizerBackend::applyPhaseS,
           py::arg("target"), "Apply Phase S gate")
      .def("applyPhaseT", &qubit_engine::StabilizerBackend::applyPhaseT,
           py::arg("target"), "Apply Phase T gate (Throws Exception)")
      .def("applyCZ", &qubit_engine::StabilizerBackend::applyCZ,
           py::arg("control"), py::arg("target"), "Apply CZ gate")
      .def("applySWAP", &qubit_engine::StabilizerBackend::applySWAP,
           py::arg("qubit1"), py::arg("qubit2"), "Apply SWAP gate")
      .def("measure", &qubit_engine::StabilizerBackend::measure,
           py::arg("target"), "Measure a qubit (collapses state)")
      .def(
          "get_state_vector",
          [](const qubit_engine::StabilizerBackend &s) {
            // Wrap throwing call to ensure error passes cleanly to Python
            return s.getStateVector();
          },
          "Get the full state vector (throws exception due to exponential "
          "scaling)")
      .def("num_qubits", &qubit_engine::StabilizerBackend::getNumQubits,
           "Get number of qubits registered to backend");

  // --- QuantumRegister Binding ---
  py::class_<QuantumRegister>(m, "QuantumRegister")
      .def(py::init<size_t>(), "Initialize a quantum register with N qubits")
      .def("applyHadamard", &QuantumRegister::applyHadamard)
      .def("applyX", &QuantumRegister::applyX)
      .def("applyY", &QuantumRegister::applyY)
      .def("applyZ", &QuantumRegister::applyZ)
      .def("applyCNOT", &QuantumRegister::applyCNOT)
      .def("applyToffoli", &QuantumRegister::applyToffoli)
      .def("applyRotationX", &QuantumRegister::applyRotationX)
      .def("applyRotationY", &QuantumRegister::applyRotationY)
      .def("applyRotationZ", &QuantumRegister::applyRotationZ)
      .def("applyPhaseS", &QuantumRegister::applyPhaseS)
      .def("applyPhaseT", &QuantumRegister::applyPhaseT)
      .def("applySWAP", &QuantumRegister::applySWAP)
      .def("applyCZ", &QuantumRegister::applyCZ)
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

  // --- PyTorch / QML Helpers ---
  m.def(
      "get_expectation_value",
      [](int num_qubits, std::vector<double> params, py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data) {
        // Parse Hamiltonian
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }

        // Apply Ansatz
        // Default constructor uses Distributed State if MPI > 1, or Local if
        // MPI=1. This is correct for a single Forward pass.
        QuantumRegister qreg(num_qubits);

        // Use direct lambda invocation
        ansatz_func(params, &qreg);

        // Calculate Energy
        double energy = 0.0;
        for (const auto &term : hamiltonian) {
          energy += term.coefficient * qreg.expectationValue(term.pauli_string);
        }
        return energy;
      },
      "Calculate expectation value <H> for PyTorch forward pass");

  m.def(
      "get_gradients",
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

        // Note: verify_mpi.py and unit tests show we want the Parallel
        // Gradients logic which is inside
        // QuantumDifferentiator::calculateGradients.
        return QuantumDifferentiator::calculateGradients(
            num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate gradients for PyTorch backward pass");

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
        // Note: GPU adjoint currently delegates to CPU adjoint.
        // The ansatz is executed with QuantumRegister (CPU).
        // Future: Add GPU-accelerated adjoint path.
        QuantumDifferentiator::AnsatzFunc<QuantumRegister> cpp_ansatz =
            [&](const std::vector<double> &p, QuantumRegister &q) {
              ansatz_func(p, &q);
            };
        return QuantumDifferentiator::calculateGradientsAdjoint(
            num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate analytical gradients using Adjoint Method (GPU - currently "
      "delegates to CPU)");

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

  // --- SPSAOptimizer Binding ---
  using qubit_engine::optimizers::SPSAOptimizer;
  py::class_<SPSAOptimizer>(m, "SPSAOptimizer")
      .def(py::init<>())
      .def(
          "minimize",
          [](SPSAOptimizer &optimizer, int num_qubits, py::function ansatz_func,
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
          "Run SPSA Optimizer natively in C++");
}
