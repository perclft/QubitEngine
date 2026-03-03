import pytest
import numpy as np

try:
    from qiskit import QuantumCircuit
    from qubit_engine.adapters.qiskit_adapter import QubitEngineBackend
    QISKIT_AVAILABLE = True
except ImportError:
    QISKIT_AVAILABLE = False


@pytest.mark.skipif(not QISKIT_AVAILABLE, reason="Qiskit is not installed")
def test_qiskit_adapter_bell_state():
    # 1. Provide the Backend
    backend = QubitEngineBackend()

    # 2. Build standard Qiskit Circuit
    qc = QuantumCircuit(2)
    qc.h(0)
    qc.cx(0, 1)

    # 3. Execute
    job = backend.run(qc)
    result = job.result()
    st = result.get_statevector(qc)

    # 4. Verify properties of Bell State
    assert len(st) == 4
    
    # State should be (|00> + |11>) / sqrt(2)
    inv_sqrt2 = 1.0 / np.sqrt(2)
    
    # Check amplitude of |00> (index 0)
    np.testing.assert_almost_equal(abs(st[0]), inv_sqrt2, decimal=5)
    
    # Check amplitude of |11> (index 3)
    np.testing.assert_almost_equal(abs(st[3]), inv_sqrt2, decimal=5)
    
    # Check others are 0
    np.testing.assert_almost_equal(abs(st[1]), 0.0, decimal=5)
    np.testing.assert_almost_equal(abs(st[2]), 0.0, decimal=5)
