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
    # Wait, because params is a shared weight in neural network, its gradient shape is (2, 1) in params.grad
    # since we passed params as [[pi/4], [pi/3]] representing batched weights (if used as individual weights).
    # Wait! If params is passed as batched tensor (batch_size, n_params) to QuantumFunction, PyTorch expects
    # its gradient to match that shape (batch_size, n_params).
    # In QuantumFunction.backward, we check if is_batched: grad_params = grad_params.sum(dim=0).
    # Wait, if we sum it, then grad_params will have shape (n_params,) instead of (batch_size, n_params)!
    # Let's think about this:
    # If the user passes a batched parameter tensor `params` to `QuantumFunction.forward`, PyTorch expects the
    # returned gradient to have the EXACT SAME shape as the input tensor `params`!
    # So if `params` has shape `(batch_size, n_params)`, its gradient must have shape `(batch_size, n_params)`.
    # It only needs to be summed over batch if `params` was a 1D tensor (non-batched shared weight) but the inputs
    # were batched!
    # Wait, can `params` be 1D (non-batched) while `inputs` is 2D (batched)?
    # YES! In `QuantumLayer.forward`, `self.params` is a 1D nn.Parameter of shape `(num_params,)`, while `x` is
    # a 2D batch of shape `(batch_size, num_inputs)`.
    # When `QuantumFunction.apply` is called, it receives a 1D `self.params` and a 2D `x`!
    # In this case:
    # `params.ndim` is 1, so `is_batched` is `False`!
    # But wait! If `is_batched` is `False`, then `params_2d = params.unsqueeze(0)` which has shape `(1, num_params)`.
    # But `inputs_2d = inputs` which has shape `(batch_size, num_inputs)`.
    # To pass these to `get_expectation_value_batched` and `get_gradients_batched`, they must have the SAME batch dimension!
    # So if `params` is 1D and `inputs` is 2D, we must tile/expand `params` to `(batch_size, num_params)`!
    # Ah! That is a very important detail!
    # Let's check: if `params_2d` has shape `(1, num_params)` and `inputs_2d` has `(batch_size, num_inputs)`,
    # we should expand `params_2d` to match `batch_size`:
    # `params_2d = params_2d.expand(batch_size, -1)`!
    # Let's modify `torch_quantum.py` to handle this expansion correctly!
    # And then, in `backward`:
    # If `params` was 1D (i.e. `not is_batched` at the input of `forward`), then `grad_params` must be summed over the
    # batch dimension to return a 1D gradient of shape `(num_params,)`!
    # If `params` was 2D, we do NOT sum it, we keep it as `(batch_size, num_params)`.
    # This is extremely logical and correct!
    # Let's check our test again: in `test_quantum_function_batched_backward`, we defined `params` as 2D:
    # `params = torch.tensor([[math.pi / 4.0], [math.pi / 3.0]], ...)`
    # Since it is 2D, PyTorch expects `params.grad` to have shape `(2, 1)`, containing the individual gradients:
    # `[-sin(pi/4 + 0.1)]` and `[-sin(pi/3 + 0.2)]`.
    # Let's write the test assertion to expect this:
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

