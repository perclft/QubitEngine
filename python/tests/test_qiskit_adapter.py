import pytest
import numpy as np
import os

try:
    from qiskit import QuantumCircuit
    from qubit_engine.adapters.qiskit_adapter import QubitEngineBackend
    from qubit_engine.core import NoiseModel
    QISKIT_AVAILABLE = True
except ImportError:
    QISKIT_AVAILABLE = False


@pytest.mark.skipif(not QISKIT_AVAILABLE, reason="Qiskit is not installed")
def test_qiskit_adapter_bell_state():
    backend = QubitEngineBackend()
    qc = QuantumCircuit(2)
    qc.h(0)
    qc.cx(0, 1)

    job = backend.run(qc)
    result = job.result()
    st = result.get_statevector(qc)

    assert len(st) == 4
    inv_sqrt2 = 1.0 / np.sqrt(2)
    np.testing.assert_almost_equal(abs(st[0]), inv_sqrt2, decimal=5)
    np.testing.assert_almost_equal(abs(st[3]), inv_sqrt2, decimal=5)
    np.testing.assert_almost_equal(abs(st[1]), 0.0, decimal=5)
    np.testing.assert_almost_equal(abs(st[2]), 0.0, decimal=5)


@pytest.mark.skipif(not QISKIT_AVAILABLE, reason="Qiskit is not installed")
def test_qiskit_adapter_advanced_gates():
    backend = QubitEngineBackend()
    
    # 1. CCX (Toffoli) Gate
    qc = QuantumCircuit(3)
    qc.x(0)
    qc.x(1)
    qc.ccx(0, 1, 2)
    st = backend.run(qc).result().get_statevector(qc)
    assert np.abs(st[7]) > 0.99  # State is |111>

    # 2. CZ Gate
    qc = QuantumCircuit(2)
    qc.h(0)
    qc.x(1)
    qc.cz(0, 1)
    st = backend.run(qc).result().get_statevector(qc)
    # Initial: (|01> + |11>)/sqrt(2) -> (|2> + |3>)/sqrt(2)
    # CZ(0, 1) applies phase -1 to |11> (|3>)
    assert np.abs(st[2] - 1/np.sqrt(2)) < 1e-5
    assert np.abs(st[3] + 1/np.sqrt(2)) < 1e-5

    # 3. SWAP Gate
    qc = QuantumCircuit(2)
    qc.x(0)
    qc.swap(0, 1)
    st = backend.run(qc).result().get_statevector(qc)
    assert np.abs(st[2]) > 0.99  # State is |10> (q1=1, q0=0)


@pytest.mark.skipif(not QISKIT_AVAILABLE, reason="Qiskit is not installed")
def test_qiskit_adapter_noiseless_sampling():
    backend = QubitEngineBackend()
    qc = QuantumCircuit(2, 2)
    qc.h(0)
    qc.cx(0, 1)
    qc.measure(0, 0)
    qc.measure(1, 1)
    
    job = backend.run(qc, shots=500)
    counts = job.result().get_counts(qc)
    
    assert "00" in counts or "11" in counts
    assert sum(counts.values()) == 500


@pytest.mark.skipif(not QISKIT_AVAILABLE, reason="Qiskit is not installed")
def test_qiskit_adapter_noisy_sampling():
    backend = QubitEngineBackend()
    qc = QuantumCircuit(1, 1)
    qc.x(0)
    qc.measure(0, 0)

    # Inject significant depolarizing noise to guarantee some error flips
    noise = NoiseModel.Depolarizing(0.2, 0.2)
    
    job = backend.run(qc, shots=200, noise_model=noise)
    counts = job.result().get_counts(qc)
    
    assert sum(counts.values()) == 200
    # Qiskit get_counts converts keys to binary representation (e.g. '1', '0')
    assert "1" in counts
    assert "0" in counts


@pytest.mark.skipif(not QISKIT_AVAILABLE, reason="Qiskit is not installed")
def test_qiskit_adapter_options_overrides():
    backend = QubitEngineBackend()
    qc = QuantumCircuit(1)
    
    # Run with custom env options
    backend.run(qc, force_local=True, cloud_url="http://dummy-cloud-backend:50051", auth_token="super-secret-token")
    
    # Check that environment variables were restored correctly
    assert os.environ.get("QUBIT_CLOUD_URL") is None
    assert os.environ.get("QUBIT_ENGINE_AUTH_TOKEN") is None
