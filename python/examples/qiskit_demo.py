"""
QubitEngine Qiskit Adapter Demonstration

This script demonstrates how to execute standard Qiskit quantum circuits
using the high-performance QubitEngine C++ Simulator Backend.
"""

from qiskit import QuantumCircuit
from qubit_engine.adapters.qiskit_adapter import QubitEngineBackend
import numpy as np

def main():
    print("Initializing QubitEngine Qiskit Backend...")
    backend = QubitEngineBackend()

    print("\nCreating a 3-qubit GHZ State Circuit... |000> + |111>")
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    
    print("Circuit created.")

    print("\nRunning circuit on QubitEngine Simulator...")
    job = backend.run(qc)
    result = job.result()
    st = result.get_statevector(qc)

    print("\nFinal State Vector Amplitudes:")
    
    # State Vector returns all 2^N amplitudes
    # For |000> + |111>, amplitudes at indices 0 and 7 should be 1/sqrt(2)
    size = len(st)
    for i in range(size):
        amp = st[i]
        prob = abs(amp)**2
        if prob > 0.001:
            binary_state = format(i, f"03b")
            print(f"  |{binary_state}> : {amp:.3f}  (Probability: {prob*100:.1f}%)")
            
    # Verify mathematically
    inv_sqrt2 = 1.0 / np.sqrt(2)
    assert np.isclose(abs(st[0]), inv_sqrt2), "Amplitude for |000> is incorrect!"
    assert np.isclose(abs(st[7]), inv_sqrt2), "Amplitude for |111> is incorrect!"
    
    print("\nVerification successful! QubitEngine executed the Qiskit circuit perfectly.")

if __name__ == "__main__":
    main()
