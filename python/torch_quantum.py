import torch
import torch.nn as nn
from typing import List, Callable, Optional, Tuple
import qubit_engine  # The C++ Extension Module

class QuantumFunction(torch.autograd.Function):
    """
    Custom Autograd Function connecting PyTorch to QubitEngine.
    Forward: Calculates Expectation Value <H> for a batch of parameters and inputs in parallel.
    Backward: Calculates Gradients via parameter-shift or adjoint methods in parallel.
    """

    @staticmethod
    def forward(
        ctx,
        params: torch.Tensor,
        inputs: torch.Tensor,
        num_qubits: int,
        hamiltonian: List[Tuple[float, str]],
        ansatz_func: Callable,
        diff_method: str = "parameter-shift",
        backend: str = "cpu"
    ) -> torch.Tensor:
        """
        params: Tensor of shape (n_params,) or (batch_size, n_params)
        inputs: Tensor of shape (n_inputs,) or (batch_size, n_inputs)
        num_qubits: int
        hamiltonian: list of tuples, e.g. [(1.0, "Z0"), (0.5, "X1")]
        ansatz_func: Python function(params, inputs, register) -> void
        diff_method: 'parameter-shift', 'adjoint', or 'adjoint-gpu'
        backend: 'cpu' or 'gpu'
        """
        # Save context attributes
        params_is_1d = (params.ndim == 1)
        inputs_is_1d = (inputs.ndim == 1)
        ctx.params_is_1d = params_is_1d
        ctx.inputs_is_1d = inputs_is_1d
        ctx.num_qubits = num_qubits
        ctx.hamiltonian = hamiltonian
        ctx.ansatz_func = ansatz_func
        ctx.diff_method = diff_method
        ctx.backend = backend
        
        # Promote to 2D
        params_2d = params if params.ndim == 2 else params.unsqueeze(0)
        inputs_2d = inputs if inputs.ndim == 2 else inputs.unsqueeze(0)
        
        batch_size = max(params_2d.shape[0], inputs_2d.shape[0])
        if params_2d.shape[0] < batch_size:
            params_2d = params_2d.expand(batch_size, -1)
        if inputs_2d.shape[0] < batch_size:
            inputs_2d = inputs_2d.expand(batch_size, -1)
            
        ctx.num_params = params_2d.shape[1]
        ctx.num_inputs = inputs_2d.shape[1]
        ctx.save_for_backward(params, inputs)

        # Convert tensors to numpy arrays (zero-copy if on CPU)
        params_np = params_2d.detach().cpu().numpy()
        inputs_np = inputs_2d.detach().cpu().numpy()

        # Call the parallel batched expectation value helper
        energies = qubit_engine.get_expectation_value_batched(
            num_qubits, params_np, inputs_np, ansatz_func, hamiltonian
        )

        out_tensor = torch.from_numpy(energies).to(dtype=params.dtype, device=params.device)
        return out_tensor if (not params_is_1d or not inputs_is_1d) else out_tensor[0]

    @staticmethod
    def backward(ctx, grad_output: torch.Tensor) -> tuple:
        params, inputs = ctx.saved_tensors
        params_is_1d = ctx.params_is_1d
        inputs_is_1d = ctx.inputs_is_1d

        # Promote to 2D and expand
        params_2d = params if params.ndim == 2 else params.unsqueeze(0)
        inputs_2d = inputs if inputs.ndim == 2 else inputs.unsqueeze(0)
        
        batch_size = max(params_2d.shape[0], inputs_2d.shape[0])
        if params_2d.shape[0] < batch_size:
            params_2d = params_2d.expand(batch_size, -1)
        if inputs_2d.shape[0] < batch_size:
            inputs_2d = inputs_2d.expand(batch_size, -1)

        params_np = params_2d.detach().cpu().numpy()
        inputs_np = inputs_2d.detach().cpu().numpy()

        # Call C++ parallel batched gradient solver
        batch_grads = qubit_engine.get_gradients_batched(
            ctx.num_qubits,
            params_np,
            inputs_np,
            ctx.ansatz_func,
            ctx.hamiltonian,
            ctx.diff_method,
            ctx.backend
        )

        grad_tensor = torch.from_numpy(batch_grads).to(dtype=params.dtype, device=params.device)

        # Split gradients
        grad_params_all = grad_tensor[:, :ctx.num_params]
        grad_inputs_all = grad_tensor[:, ctx.num_params:]

        # Apply chain rule
        # grad_output has shape (batch_size,) if batched, else scalar ()
        if grad_output.ndim > 0:
            grad_output_unsqueezed = grad_output.unsqueeze(1)
            grad_params_all = grad_params_all * grad_output_unsqueezed
            grad_inputs_all = grad_inputs_all * grad_output_unsqueezed
        else:
            grad_params_all = grad_params_all * grad_output
            grad_inputs_all = grad_inputs_all * grad_output

        # Reduce back to match original input tensor dimensions
        if params_is_1d:
            grad_params = grad_params_all.sum(dim=0)
        elif grad_params_all.shape != params.shape:
            grad_params = grad_params_all.sum(dim=0, keepdim=True)
        else:
            grad_params = grad_params_all

        if inputs_is_1d:
            grad_inputs = grad_inputs_all.sum(dim=0)
        elif grad_inputs_all.shape != inputs.shape:
            grad_inputs = grad_inputs_all.sum(dim=0, keepdim=True)
        else:
            grad_inputs = grad_inputs_all

        return grad_params, grad_inputs, None, None, None, None, None

class QuantumLayer(nn.Module):
    """
    Variational Quantum Layer (VQC) integrating classical input features with trainable weights.
    """
    def __init__(
        self,
        num_qubits: int,
        num_params: int,
        hamiltonian: List[Tuple[float, str]],
        ansatz_func: Callable,
        diff_method: str = "parameter-shift",
        backend: str = "cpu"
    ):
        super().__init__()
        self.num_qubits = num_qubits
        self.num_params = num_params
        self.hamiltonian = hamiltonian
        self.ansatz = ansatz_func
        self.diff_method = diff_method
        self.backend = backend

        # Trainable weights initialized randomly
        self.params = nn.Parameter(torch.rand(num_params) * 2.0 * 3.141592653589793)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        x: Input feature tensor of shape (n_inputs,) or (batch_size, n_inputs)
        """
        return QuantumFunction.apply(
            self.params,
            x,
            self.num_qubits,
            self.hamiltonian,
            self.ansatz,
            self.diff_method,
            self.backend
        )


