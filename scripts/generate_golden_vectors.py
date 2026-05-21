import json
import os
import sys

try:
    import qiskit
    from qiskit import QuantumCircuit, transpile
    from qiskit_aer import Aer
    from qiskit.quantum_info import Statevector
except ImportError:
    print("Qiskit not installed. Run 'pip install qiskit qiskit-aer' first.")
    sys.exit(1)

GOLDEN_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "backend", "tests", "validation", "golden")

def save_golden(name, circuit):
    print(f"Generating golden state for {name} ({circuit.num_qubits} qubits)...")
    simulator = Aer.get_backend('statevector_simulator')
    compiled_circuit = transpile(circuit, simulator)
    job = simulator.run(compiled_circuit)
    result = job.result()
    statevector = result.get_statevector(compiled_circuit)
    
    # Format: array of [real, imag]
    state_list = [[v.real, v.imag] for v in statevector.data]
    
    os.makedirs(GOLDEN_DIR, exist_ok=True)
    out_path = os.path.join(GOLDEN_DIR, f"{name}.json")
    
    data = {
        "circuit_name": name,
        "num_qubits": circuit.num_qubits,
        "state_vector": state_list
    }
    
    with open(out_path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Saved to {out_path}")

def generate_all():
    # 1. Bell state (2 qubits)
    qc_bell = QuantumCircuit(2)
    qc_bell.h(0)
    qc_bell.cx(0, 1)
    save_golden("bell", qc_bell)

    # 2. GHZ state (4 qubits)
    qc_ghz = QuantumCircuit(4)
    qc_ghz.h(0)
    qc_ghz.cx(0, 1)
    qc_ghz.cx(1, 2)
    qc_ghz.cx(2, 3)
    save_golden("ghz_4q", qc_ghz)

    # 3. QFT (4 qubits)
    qc_qft = QuantumCircuit(4)
    import numpy as np
    # To compare correctly, remember Qiskit's endianness. Qubit 0 is LSB.
    for j in range(4):
        qc_qft.h(j)
        for k in range(j+1, 4):
            qc_qft.cp(np.pi / (2**(k-j)), k, j)
    # Reverse qubits for standard QFT
    for i in range(4 // 2):
        qc_qft.swap(i, 4 - i - 1)
    save_golden("qft_4q", qc_qft)

    # 4. Random circuit (4 qubits, seed for reproducibility)
    from qiskit.circuit.random import random_circuit
    qc_rand = random_circuit(4, depth=5, seed=42)
    save_golden("random_4q_d5", qc_rand)

if __name__ == "__main__":
    generate_all()
