#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "AdamOptimizer.hpp"
#include "MolecularHamiltonian.hpp"
#include "NoiseModel.hpp"
#include "QuantumDifferentiator.hpp"
#include "QuantumRegister.hpp"
#include "SPSAOptimizer.hpp"

// Include GPU headers
#include "backends/GPUQuantumRegister.hpp"

// Include Stabilizer Backend
#include "backends/StabilizerBackend.hpp"

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

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
            auto* vec_ptr = new std::vector<std::complex<double>>(std::move(s.getStateVector()));
            auto capsule = py::capsule(vec_ptr, [](void *p) {
                delete reinterpret_cast<std::vector<std::complex<double>>*>(p);
            });
            return py::array_t<std::complex<double>>(
                vec_ptr->size(),
                vec_ptr->data(),
                capsule
            );
          },
          "Get the full state vector (throws exception due to exponential "
          "scaling)")
      .def("num_qubits", &qubit_engine::StabilizerBackend::getNumQubits,
           "Get number of qubits registered to backend");

  // --- QuantumRegister Binding ---
  py::class_<QuantumRegister>(m, "QuantumRegister")
      .def(py::init<size_t, bool>(), py::arg("num_qubits"), py::arg("force_local") = false, "Initialize a quantum register with N qubits")
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
      .def("getRank", &QuantumRegister::getRank)
      .def("getSize", &QuantumRegister::getSize)
      .def("getStateVector", [](QuantumRegister &q) {
          auto* vec_ptr = new std::vector<std::complex<double>>(std::move(q.getStateVector()));
          auto capsule = py::capsule(vec_ptr, [](void *p) {
              delete reinterpret_cast<std::vector<std::complex<double>>*>(p);
          });
          return py::array_t<std::complex<double>>(
              vec_ptr->size(),
              vec_ptr->data(),
              capsule
          );
      })
      .def("__repr__", [](const QuantumRegister &q) {
          return "<qubit_engine.QuantumRegister qubits=" + std::to_string(q.getNumQubits()) + " rank=" + std::to_string(q.getRank()) + ">";
      })
      .def("__str__", [](const QuantumRegister &q) {
          return "QuantumRegister(qubits=" + std::to_string(q.getNumQubits()) + " mpi_rank=" + std::to_string(q.getRank()) + ")";
      })
      .def("setNoiseModel", &QuantumRegister::setNoiseModel,
           py::arg("model"), "Attach a noise model for automatic post-gate noise injection")
      .def("getNoiseModel", &QuantumRegister::getNoiseModel,
           py::return_value_policy::reference, "Get the current noise model (or None)");

  // --- ReadoutError Binding ---
  py::class_<qubit_engine::ReadoutError>(m, "ReadoutError")
      .def(py::init<>())
      .def(py::init([](double p0g1, double p1g0) {
          qubit_engine::ReadoutError e;
          e.p0_given_1 = p0g1;
          e.p1_given_0 = p1g0;
          return e;
      }), py::arg("p0_given_1") = 0.0, py::arg("p1_given_0") = 0.0)
      .def_readwrite("p0_given_1", &qubit_engine::ReadoutError::p0_given_1)
      .def_readwrite("p1_given_0", &qubit_engine::ReadoutError::p1_given_0);

  // --- NoiseModel Binding ---
  py::class_<qubit_engine::NoiseModel>(m, "NoiseModel")
      .def(py::init<>())
      .def_static("Depolarizing", &qubit_engine::NoiseModel::Depolarizing,
           py::arg("p1q"), py::arg("p2q"),
           "Create a noise model with 1Q and 2Q depolarizing channels")
      .def_static("Realistic", &qubit_engine::NoiseModel::Realistic,
           py::arg("p1q"), py::arg("p2q"),
           py::arg("t1_gamma"), py::arg("t2_gamma"),
           py::arg("readout"),
           "Create a realistic noise model with depolarizing, T1, T2, and readout error")
      .def_static("IBMBrisbane", &qubit_engine::NoiseModel::IBMBrisbane,
           "Create a noise model configured with median calibration data for IBM Brisbane (127Q)")
      .def_static("GoogleSycamore", &qubit_engine::NoiseModel::GoogleSycamore,
           "Create a noise model configured with median calibration data for Google Sycamore (53Q)")
      .def("add_single_qubit_noise", static_cast<void (qubit_engine::NoiseModel::*)(qubit_engine::NoiseChannel1Q)>(&qubit_engine::NoiseModel::addSingleQubitNoise),
           py::arg("channel"), "Add a single-qubit noise channel")
      .def("add_two_qubit_noise", static_cast<void (qubit_engine::NoiseModel::*)(qubit_engine::NoiseChannel2Q)>(&qubit_engine::NoiseModel::addTwoQubitNoise),
           py::arg("channel"), "Add a two-qubit noise channel")
      .def("add_single_qubit_noise", static_cast<void (qubit_engine::NoiseModel::*)(size_t, qubit_engine::NoiseChannel1Q)>(&qubit_engine::NoiseModel::addSingleQubitNoise),
           py::arg("qubit"), py::arg("channel"), "Add a per-qubit noise channel")
      .def("add_two_qubit_noise", static_cast<void (qubit_engine::NoiseModel::*)(size_t, size_t, qubit_engine::NoiseChannel2Q)>(&qubit_engine::NoiseModel::addTwoQubitNoise),
           py::arg("q1"), py::arg("q2"), py::arg("channel"), "Add a per-edge two-qubit noise channel")
      .def("set_readout_error", &qubit_engine::NoiseModel::setReadoutError,
           py::arg("qubit"), py::arg("error"), "Set readout error for a specific qubit")
      .def("set_readout_error_all", &qubit_engine::NoiseModel::setReadoutErrorAll,
           py::arg("error"), "Set default readout error for all qubits")
      .def("set_enabled", &qubit_engine::NoiseModel::setEnabled,
           py::arg("enabled"), "Enable or disable the noise model")
      .def("is_enabled", &qubit_engine::NoiseModel::isEnabled)
      .def("set_coherent_error", &qubit_engine::NoiseModel::setCoherentError,
           py::arg("gate_type"), py::arg("epsilon"), "Add a systematic rotation bias")
      .def("get_coherent_error", &qubit_engine::NoiseModel::getCoherentError,
           py::arg("gate_type"), "Get the systematic rotation bias for a gate type");

  // --- NoiseChannel1Q Binding ---
  py::class_<qubit_engine::NoiseChannel1Q>(m, "NoiseChannel1Q")
      .def(py::init<>())
      .def_readwrite("name", &qubit_engine::NoiseChannel1Q::name);

  // --- NoiseChannel2Q Binding ---
  py::class_<qubit_engine::NoiseChannel2Q>(m, "NoiseChannel2Q")
      .def(py::init<>())
      .def_readwrite("name", &qubit_engine::NoiseChannel2Q::name);

  // --- Channel Factory Functions ---
  m.def("make_depolarizing_channel_1q", &qubit_engine::makeDepolarizingChannel1Q,
        py::arg("p"), "Create a single-qubit depolarizing noise channel");
  m.def("make_depolarizing_channel_2q", &qubit_engine::makeDepolarizingChannel2Q,
        py::arg("p"), "Create a two-qubit depolarizing noise channel (full 16-operator)");
  m.def("make_amplitude_damping_channel", &qubit_engine::makeAmplitudeDampingChannel,
        py::arg("gamma"), "Create an amplitude damping (T1) noise channel");
  m.def("make_phase_damping_channel", &qubit_engine::makePhaseDampingChannel,
        py::arg("gamma"), "Create a phase damping (T2) noise channel");
  m.def("make_thermal_relaxation_channel", &qubit_engine::makeThermalRelaxationChannel,
        py::arg("t1"), py::arg("t2"), py::arg("gate_time"),
        "Create a thermal relaxation (T1/T2) noise channel");

  // --- GPUQuantumRegister Binding ---
  py::class_<GPUQuantumRegister>(m, "GPUQuantumRegister")
      .def(py::init<size_t>(),
           "Initialize a GPU quantum register with N qubits")
      .def("applyHadamard", &GPUQuantumRegister::applyHadamard)
      .def("applyX", &GPUQuantumRegister::applyX)
      .def("applyY", &GPUQuantumRegister::applyY)
      .def("applyZ", &GPUQuantumRegister::applyZ)
      .def("applyRotationY", &GPUQuantumRegister::applyRotationY)
      .def("getStateVector", [](const GPUQuantumRegister &q) {
          auto* vec_ptr = new std::vector<std::complex<double>>(std::move(q.getStateVector()));
          auto capsule = py::capsule(vec_ptr, [](void *p) {
              delete reinterpret_cast<std::vector<std::complex<double>>*>(p);
          });
          return py::array_t<std::complex<double>>(
              vec_ptr->size(),
              vec_ptr->data(),
              capsule
          );
      });

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
              py::gil_scoped_acquire acquire;
              ansatz_func(p, &q);
            };

        py::gil_scoped_release release;
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
              py::gil_scoped_acquire acquire;
              ansatz_func(p, &q);
            };

        py::gil_scoped_release release;
        // Note: verify_mpi.py and unit tests show we want the Parallel
        // Gradients logic which is inside
        // QuantumDifferentiator::calculateGradients.
        return QuantumDifferentiator::calculateGradients(
            num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate gradients for PyTorch backward pass");

  m.def(
      "get_expectation_value_batched",
      [](int num_qubits,
         std::vector<std::vector<double>> batch_params,
         std::vector<std::vector<double>> batch_inputs,
         py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data) {
        
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }

        size_t batch_size = batch_params.size();
        std::vector<double> energies(batch_size, 0.0);

        // Release GIL for the outer OpenMP loop
        py::gil_scoped_release release;

        #pragma omp parallel for schedule(dynamic)
        for (int b = 0; b < static_cast<int>(batch_size); ++b) {
          QuantumRegister qreg(num_qubits);
          
          {
            py::gil_scoped_acquire acquire;
            ansatz_func(batch_params[b], batch_inputs[b], &qreg);
          }

          double energy = 0.0;
          for (const auto &term : hamiltonian) {
            energy += term.coefficient * qreg.expectationValue(term.pauli_string);
          }
          energies[b] = energy;
        }

        return energies;
      },
      "Calculate expectation values for a batch of parameters and inputs in parallel");

  m.def(
      "get_gradients_batched",
      [](int num_qubits,
         std::vector<std::vector<double>> batch_params,
         std::vector<std::vector<double>> batch_inputs,
         py::function ansatz_func,
         std::vector<std::pair<double, std::string>> hamiltonian_data,
         std::string diff_method,
         std::string backend) {
        
        std::vector<PauliTerm> hamiltonian;
        for (const auto &item : hamiltonian_data) {
          hamiltonian.push_back({item.first, item.second});
        }

        size_t batch_size = batch_params.size();
        size_t num_params = batch_params.empty() ? 0 : batch_params[0].size();
        size_t num_inputs = batch_inputs.empty() ? 0 : batch_inputs[0].size();
        size_t total_vars = num_params + num_inputs;

        std::vector<std::vector<double>> batch_grads(batch_size, std::vector<double>(total_vars, 0.0));

        // Release GIL for the outer OpenMP loop
        py::gil_scoped_release release;

        #pragma omp parallel for schedule(dynamic)
        for (int b = 0; b < static_cast<int>(batch_size); ++b) {
          // Combined ansatz wrapper
          QuantumDifferentiator::AnsatzFunc<QuantumRegister> cpp_ansatz =
              [&](const std::vector<double> &combined_p, QuantumRegister &q) {
                std::vector<double> p(combined_p.begin(), combined_p.begin() + num_params);
                std::vector<double> x(combined_p.begin() + num_params, combined_p.end());
                py::gil_scoped_acquire acquire;
                ansatz_func(p, x, &q);
              };

          // Combine parameters and inputs
          std::vector<double> combined_vars = batch_params[b];
          combined_vars.insert(combined_vars.end(), batch_inputs[b].begin(), batch_inputs[b].end());

          std::vector<double> grads;
          if (diff_method == "adjoint-gpu" || (diff_method == "adjoint" && backend == "gpu")) {
            grads = QuantumDifferentiator::calculateGradientsAdjointGPU(
                num_qubits, combined_vars, cpp_ansatz, hamiltonian);
          } else if (diff_method == "adjoint") {
            grads = QuantumDifferentiator::calculateGradientsAdjoint<QuantumRegister>(
                num_qubits, combined_vars, cpp_ansatz, hamiltonian);
          } else { // default parameter-shift
            grads = QuantumDifferentiator::calculateGradients(
                num_qubits, combined_vars, cpp_ansatz, hamiltonian);
          }
          batch_grads[b] = grads;
        }

        return batch_grads;
      },
      "Calculate analytical gradients for a batch of parameters and inputs in parallel");

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
              py::gil_scoped_acquire acquire;
              ansatz_func(p, &q);
            };
        py::gil_scoped_release release;
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
        
        QuantumDifferentiator::AnsatzFunc<QuantumRegister> cpp_ansatz =
            [&](const std::vector<double> &p, QuantumRegister &q) {
              py::gil_scoped_acquire acquire;
              ansatz_func(p, &q);
            };
        py::gil_scoped_release release;
        return QuantumDifferentiator::calculateGradientsAdjointGPU(
            num_qubits, params, cpp_ansatz, hamiltonian);
      },
      "Calculate analytical gradients using Adjoint Method on GPU (Fallback to CPU if CUDA is disabled)");

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
                  py::gil_scoped_acquire acquire;
                  ansatz_func(p, &q);
                };

            py::gil_scoped_release release;
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
                  py::gil_scoped_acquire acquire;
                  ansatz_func(p, &q);
                };

            py::gil_scoped_release release;
            return optimizer.minimize(cpp_ansatz, hamiltonian, num_qubits,
                                      initial_params);
          },
          "Run SPSA Optimizer natively in C++");

  py::module_ atexit = py::module_::import("atexit");
  atexit.attr("register")(py::cpp_function([]() {
#ifdef MPI_ENABLED
    int initialized;
    MPI_Initialized(&initialized);
    if (initialized) {
      int finalized;
      MPI_Finalized(&finalized);
      if (!finalized) {
        MPI_Finalize();
      }
    }
#endif
  }));
}
