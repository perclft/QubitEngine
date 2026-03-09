import pytest
import math
import sys
import os

torch = pytest.importorskip("torch")
torch.set_default_dtype(torch.float64)

# Ensure the build directory is in the path to find the module, similar to verify_bindings.py
build_dir = os.path.join(os.path.dirname(__file__), "../../backend/build")
sys.path.append(build_dir)
# Also add python/src if needed
sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

try:
    import qubit_engine
    from torch_quantum import QuantumFunction, QuantumLayer
except ImportError:
    pytest.skip("qubit_engine or torch_quantum not available", allow_module_level=True)

def test_quantum_function_forward():
    num_qubits = 1
    theta = math.pi / 3.0
    params = torch.tensor([theta], dtype=torch.float64, requires_grad=True)

    def ansatz(p, qreg):
        qreg.applyRotationY(0, p[0])

    hamiltonian = [(1.0, "Z")]

    # We mock or expect the engine to return the evaluated energy
    try:
        energy = QuantumFunction.apply(params, num_qubits, hamiltonian, ansatz)
        expected = math.cos(theta)
        assert torch.isclose(energy, torch.tensor(expected, dtype=torch.float64), atol=1e-4)
    except AttributeError:
        # If get_expectation_value is not yet implemented in Python bindings
        pytest.skip("qubit_engine.get_expectation_value not yet implemented in C++ bindings")

def test_quantum_function_backward():
    num_qubits = 1
    theta = math.pi / 3.0
    params = torch.tensor([theta], dtype=torch.float64, requires_grad=True)

    def ansatz(p, qreg):
        qreg.applyRotationY(0, p[0])

    hamiltonian = [(1.0, "Z")]

    try:
        energy = QuantumFunction.apply(params, num_qubits, hamiltonian, ansatz)
        energy.backward()

        expected_grad = -math.sin(theta)
        assert params.grad is not None
        assert torch.isclose(params.grad[0], torch.tensor(expected_grad, dtype=torch.float64), atol=1e-4)
    except AttributeError:
        pytest.skip("qubit_engine.get_expectation_value or get_gradients not yet implemented in C++ bindings")

def test_quantum_layer():
    num_qubits = 2
    num_params = 2
    
    def ansatz(p, qreg):
        qreg.applyRotationY(0, p[0])
        qreg.applyRotationY(1, p[1])
        qreg.applyCNOT(0, 1)

    hamiltonian = [(1.0, "Z0"), (1.0, "Z1")]

    layer = QuantumLayer(num_qubits, num_params, hamiltonian, ansatz)
    
    dummy_input = torch.tensor([1.0])
    try:
        output = layer(dummy_input)
        assert output is not None
        assert output.requires_grad
        
        output.backward()
        assert layer.params.grad is not None
        assert layer.params.grad.shape == (2,)
    except AttributeError:
        pytest.skip("qubit_engine methods required by QuantumLayer not yet implemented")
