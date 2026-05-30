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
    inputs = torch.tensor([0.0], dtype=torch.float64, requires_grad=True)

    def ansatz(p, x, qreg):
        qreg.applyRotationY(0, p[0] + x[0])

    hamiltonian = [(1.0, "Z")]

    energy = QuantumFunction.apply(params, inputs, num_qubits, hamiltonian, ansatz)
    expected = math.cos(theta)
    assert torch.isclose(energy, torch.tensor(expected, dtype=torch.float64), atol=1e-4)

def test_quantum_function_backward():
    num_qubits = 1
    theta = math.pi / 3.0
    params = torch.tensor([theta], dtype=torch.float64, requires_grad=True)
    inputs = torch.tensor([0.5], dtype=torch.float64, requires_grad=True)

    def ansatz(p, x, qreg):
        qreg.applyRotationY(0, p[0] + x[0])

    hamiltonian = [(1.0, "Z")]

    energy = QuantumFunction.apply(params, inputs, num_qubits, hamiltonian, ansatz)
    energy.backward()

    expected_grad = -math.sin(theta + 0.5)
    assert params.grad is not None
    assert inputs.grad is not None
    assert torch.isclose(params.grad[0], torch.tensor(expected_grad, dtype=torch.float64), atol=1e-4)
    assert torch.isclose(inputs.grad[0], torch.tensor(expected_grad, dtype=torch.float64), atol=1e-4)

def test_quantum_function_batched_forward():
    num_qubits = 1
    # Batch size = 2
    params = torch.tensor([[math.pi / 4.0], [math.pi / 3.0]], dtype=torch.float64, requires_grad=True)
    inputs = torch.tensor([[0.0], [0.2]], dtype=torch.float64, requires_grad=True)

    def ansatz(p, x, qreg):
        qreg.applyRotationY(0, p[0] + x[0])

    hamiltonian = [(1.0, "Z")]

    energy = QuantumFunction.apply(params, inputs, num_qubits, hamiltonian, ansatz)
    assert energy.shape == (2,)
    
    expected_0 = math.cos(math.pi / 4.0)
    expected_1 = math.cos(math.pi / 3.0 + 0.2)
    assert torch.isclose(energy[0], torch.tensor(expected_0, dtype=torch.float64), atol=1e-4)
    assert torch.isclose(energy[1], torch.tensor(expected_1, dtype=torch.float64), atol=1e-4)

def test_quantum_function_batched_backward():
    num_qubits = 1
    # Batch size = 2
    params = torch.tensor([[math.pi / 4.0], [math.pi / 3.0]], dtype=torch.float64, requires_grad=True)
    inputs = torch.tensor([[0.1], [0.2]], dtype=torch.float64, requires_grad=True)

    def ansatz(p, x, qreg):
        qreg.applyRotationY(0, p[0] + x[0])

    hamiltonian = [(1.0, "Z")]

    energy = QuantumFunction.apply(params, inputs, num_qubits, hamiltonian, ansatz)
    # Loss is sum of energies
    loss = energy.sum()
    loss.backward()

    # Shared params gradient is sum over batch
    expected_grad_params_0 = -math.sin(math.pi / 4.0 + 0.1)
    expected_grad_params_1 = -math.sin(math.pi / 3.0 + 0.2)
    expected_grad_params = expected_grad_params_0 + expected_grad_params_1
    
    assert params.grad is not None
    assert inputs.grad is not None
    # When `params` is a 2D tensor (batch_size, n_params), PyTorch expects `params.grad` 
    # to have the exact same shape. If `params` was a 1D tensor representing a shared 
    # network weight, its gradient would be summed over the batch dimension in `backward`.
    # Here, we test the 2D case where each batch item has its own parameters.
    assert torch.isclose(params.grad[0, 0], torch.tensor(expected_grad_params_0, dtype=torch.float64), atol=1e-4)
    assert torch.isclose(params.grad[1, 0], torch.tensor(expected_grad_params_1, dtype=torch.float64), atol=1e-4)
    assert torch.isclose(inputs.grad[0, 0], torch.tensor(expected_grad_params_0, dtype=torch.float64), atol=1e-4)
    assert torch.isclose(inputs.grad[1, 0], torch.tensor(expected_grad_params_1, dtype=torch.float64), atol=1e-4)

def test_quantum_function_adjoint():
    num_qubits = 1
    theta = math.pi / 3.0
    params = torch.tensor([theta], dtype=torch.float64, requires_grad=True)
    inputs = torch.tensor([0.5], dtype=torch.float64, requires_grad=True)

    def ansatz(p, x, qreg):
        qreg.applyRotationY(0, p[0] + x[0])

    hamiltonian = [(1.0, "Z")]

    # Test CPU Adjoint
    energy = QuantumFunction.apply(params, inputs, num_qubits, hamiltonian, ansatz, "adjoint", "cpu")
    energy.backward()

    expected_grad = -math.sin(theta + 0.5)
    assert params.grad is not None
    assert torch.isclose(params.grad[0], torch.tensor(expected_grad, dtype=torch.float64), atol=1e-4)

def test_quantum_layer():
    num_qubits = 2
    num_params = 2
    
    def ansatz(p, x, qreg):
        qreg.applyRotationY(0, p[0] + x[0])
        qreg.applyRotationY(1, p[1] + x[1])
        qreg.applyCNOT(0, 1)

    hamiltonian = [(1.0, "Z0"), (1.0, "Z1")]

    layer = QuantumLayer(num_qubits, num_params, hamiltonian, ansatz, diff_method="adjoint")
    
    # 2D Batched Input features (batch_size = 3, n_inputs = 2)
    inputs = torch.tensor([[0.0, 0.0], [0.1, 0.2], [0.3, 0.4]], dtype=torch.float64, requires_grad=True)
    
    output = layer(inputs)
    assert output is not None
    assert output.shape == (3,)
    assert output.requires_grad
    
    output.sum().backward()
    assert layer.params.grad is not None
    assert layer.params.grad.shape == (2,)
    assert inputs.grad is not None
    assert inputs.grad.shape == (3, 2)

