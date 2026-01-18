#include "QuantumRegister.hpp"
#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace qubit_engine;

PYBIND11_MODULE(qubit_engine, m) {
  m.doc() = "QubitEngine High-Performance Quantum Simulator Backend";

  py::class_<QuantumRegister>(m, "QuantumRegister")
      .def(py::init<size_t>(), py::arg("num_qubits"))
      .def("applyHadamard", &QuantumRegister::applyHadamard, py::arg("target"),
           "Apply Hadamard gate")
      .def("applyX", &QuantumRegister::applyX, py::arg("target"),
           "Apply Pauli-X gate")
      .def("applyY", &QuantumRegister::applyY, py::arg("target"),
           "Apply Pauli-Y gate")
      .def("applyZ", &QuantumRegister::applyZ, py::arg("target"),
           "Apply Pauli-Z gate")
      .def("applyCNOT", &QuantumRegister::applyCNOT, py::arg("control"),
           py::arg("target"), "Apply CNOT gate")
      .def("applyToffoli", &QuantumRegister::applyToffoli, py::arg("control1"),
           py::arg("control2"), py::arg("target"), "Apply Toffoli gate")
      .def("applyPhaseS", &QuantumRegister::applyPhaseS, py::arg("target"),
           "Apply Phase S gate")
      .def("applyPhaseT", &QuantumRegister::applyPhaseT, py::arg("target"),
           "Apply Phase T gate")
      .def("applyRotationY", &QuantumRegister::applyRotationY,
           py::arg("target"), py::arg("angle"), "Apply Rotation-Y gate")
      .def("applyRotationZ", &QuantumRegister::applyRotationZ,
           py::arg("target"), py::arg("angle"), "Apply Rotation-Z gate")
      .def("measure", &QuantumRegister::measure, py::arg("target"),
           "Measure a qubit (collapses state)")
      .def("get_state_vector", &QuantumRegister::getStateVector,
           "Get the full state vector as a list of complex numbers")
      .def("expectation_value", &QuantumRegister::expectationValue,
           py::arg("pauli_string"),
           "Calculate expectation value of a Pauli string")
      .def("num_qubits", [](const QuantumRegister &q) {
        return q.getStateVector().size();
      }); // Approximation, actual num_qubits getter needed in class
}
