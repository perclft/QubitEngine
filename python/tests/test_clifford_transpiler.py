import pytest
import math
import sys
import os

# Set paths
sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

try:
    import qubit_engine
    from qubit_engine.core import QuantumRegister, NonCliffordGateException
except ImportError:
    pytest.skip("qubit_engine C++ core bindings not available", allow_module_level=True)

def test_transpile_strict_validation():
    # Strict validation should throw NonCliffordGateException for continuous rotations
    q = QuantumRegister(2)
    q.enableRecording(True)
    q.applyHadamard(0)
    q.applyRotationX(0, 0.4)
    
    with pytest.raises(NonCliffordGateException):
        q.transpileToClifford(approximate=False)

def test_transpile_clifford_only_no_throw():
    # If the tape only has Clifford gates, it should not throw
    q = QuantumRegister(2)
    q.enableRecording(True)
    q.applyHadamard(0)
    q.applyCNOT(0, 1)
    q.applyPhaseS(1)
    
    q.transpileToClifford(approximate=False)

def test_transpile_approximation_snapping():
    # Snaps continuous rotations to nearest multiple of pi/2
    q = QuantumRegister(2)
    q.enableRecording(True)
    
    q.applyRotationZ(0, 0.1) # snaps to 0 (deleted)
    q.applyRotationZ(0, 1.4) # snaps to pi/2 (S gate)
    q.applyRotationZ(0, 3.0) # snaps to pi (Z gate)
    
    q.transpileToClifford(approximate=True, use_stochastic=False)

def test_toffoli_decomposition_and_approximation():
    q = QuantumRegister(3)
    q.enableRecording(True)
    q.applyToffoli(0, 1, 2)
    
    # This should decompose Toffoli and snap T/T† gates to S/I
    q.transpileToClifford(approximate=True, use_stochastic=False)

def test_stochastic_snapping():
    # T gate (pi/4) is exactly halfway.
    # Over many runs in stochastic mode, it should snap differently.
    # We can run 50 times and check it runs without errors.
    for _ in range(50):
        q = QuantumRegister(1)
        q.enableRecording(True)
        q.applyRotationZ(0, math.pi / 4.0)
        q.transpileToClifford(approximate=True, use_stochastic=True)

def test_apply_dense_unitary():
    q = QuantumRegister(2)
    # Apply a 4x4 Identity unitary
    matrix = [
        1+0j, 0j, 0j, 0j,
        0j, 1+0j, 0j, 0j,
        0j, 0j, 1+0j, 0j,
        0j, 0j, 0j, 1+0j
    ]
    q.applyDenseUnitary([0, 1], matrix)
    
    # State should remain |00> (amplitude at index 0 is 1.0)
    sv = q.getStateVector()
    assert abs(sv[0] - 1.0) < 1e-9
