#pragma once
#include <complex>
#include <map>
#include <string>
#include <vector>

// Data structure for a Pauli term: coeff * P_0 * P_1 ...
struct PauliTerm {
  double coefficient;
  std::string pauli_string; // e.g., "XZ" means X on qubit 0, Z on qubit 1
};

class MolecularHamiltonian {
public:
  enum MoleculeType { H2 = 0, LiH = 1 };

  static std::vector<PauliTerm> getHamiltonian(MoleculeType type) {
    std::vector<PauliTerm> hamiltonian;

    if (type == H2) {
      // H2 at bond distance 0.7414 Angstroms
      // Values from standard quantum chemistry datasets (e.g.,
      // Qiskit/OpenFermion) 2 Qubits mapping (Parity or Jordan-Wigner) Using a
      // simplified 2-qubit Hamiltonian for H2 (Parity mapping / BK) g0 I I + g1
      // Z I + g2 I Z + g3 Z Z + g4 X X + g5 Y Y

      // Coefficients (Hartrees)
      hamiltonian.push_back({-1.052373245772859, "II"});
      hamiltonian.push_back({0.397937424843187, "IZ"});
      hamiltonian.push_back({-0.397937424843187, "ZI"});
      hamiltonian.push_back({-0.011280104256235, "ZZ"});
      hamiltonian.push_back({0.180931199784231, "XX"});

      // Note: We'll stick to real-valued Hamiltonians for this demo.
    } else if (type == LiH) {
      // LiH STO-3G (Standard 4-qubit mapping tapered/simplified)
      // Ground state energy is approximately -7.86 Ha
      hamiltonian.push_back({-7.75, "IIII"});
      hamiltonian.push_back({0.011, "ZIII"});
      hamiltonian.push_back({0.011, "IZII"});
      hamiltonian.push_back({0.035, "IIZI"});
      hamiltonian.push_back({0.035, "IIIZ"});
      hamiltonian.push_back({0.045, "ZZII"});
      hamiltonian.push_back({0.045, "IIZZ"});
      hamiltonian.push_back({0.012, "XXXX"});
    }

    return hamiltonian;
  }

  // Helper to get Number of Qubits needed
  static int getNumQubits(MoleculeType type) {
    if (type == H2)
      return 2;
    if (type == LiH)
      return 4;
    return 2;
  }
};
