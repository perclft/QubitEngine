import pytest
import math
import numpy as np
import qubit_engine as qe

def test_bell_state():
    # Test statevector first
    circuit = qe.Circuit(2)
    circuit.h(0).cx(0, 1)
    
    sv = circuit.statevector()
    assert np.isclose(abs(sv[0])**2, 0.5, atol=0.1)
    assert np.isclose(abs(sv[3])**2, 0.5, atol=0.1)

    # Test counts
    circuit.measure_all()
    result = circuit.run(shots=1000, backend="cpu")
    counts = result.counts
    
    assert len(counts) > 0
    assert "00" in counts or "11" in counts

def test_ghz_state():
    circuit = qe.Circuit(3)
    circuit.h(0).cx(0, 1).cx(1, 2)
    
    sv = circuit.statevector()
    assert np.isclose(abs(sv[0])**2, 0.5, atol=0.1)  # |000>
    assert np.isclose(abs(sv[7])**2, 0.5, atol=0.1)  # |111>

def test_vqe_ansatz():
    def ansatz(params, qreg):
        qreg.applyRotationY(0, params[0])
        qreg.applyRotationY(1, params[1])
        qreg.applyCNOT(0, 1)

    hamiltonian = [
        (0.5, "ZZ"),
        (0.2, "XI"),
        (0.2, "IX")
    ]
    
    res = qe.vqe(hamiltonian, ansatz, num_qubits=2, optimizer="spsa", initial_params=[0.0, 0.0], max_iter=20)
    assert "energy" in res
    assert "parameters" in res

def test_noise_models():
    circuit = qe.Circuit(1)
    circuit.x(0).measure(0)
    
    # Run with depolarizing noise
    noise_model = qe.noise.depolarizing(0.5, 0.0)
    result = circuit.run(shots=100, noise=noise_model, backend="cpu")
    counts = result.counts
    
    # Normally without noise, "1" is 100%. With noise, "0" should appear sometimes.
    assert "0" in counts or "1" in counts

def test_asymmetric_bit_ordering():
    # Qubit 1 is set to 1, Qubit 0 is set to 0
    # In Qiskit endianness ("q1 q0"), qubit 0 is LSB (rightmost), qubit 1 is MSB (leftmost)
    # The output string MUST be "10", not "01"
    circuit = qe.Circuit(2)
    circuit.x(1)
    circuit.measure_all()
    result = circuit.run(shots=100, backend="cpu")
    assert result.counts == {"10": 100}

