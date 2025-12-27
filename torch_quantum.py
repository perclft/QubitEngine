import torch
import torch.nn as nn
from typing import List, Callable, Optional
import qubit_engine  # The C++ Extension Module

class QuantumFunction(torch.autograd.Function):
    """
    Custom Autograd Function connecting PyTorch to QubitEngine.
    Forward: Calculates Expectation Value <H>.
    Backward: Calculates Gradients via Parameter Shift Rule (executed in C++).
    """

    @staticmethod
    def forward(ctx, params, num_qubits, hamiltonian_str, ansatz_func):
        """
        params: Tensor of shape (batch_size, n_params) or (n_params)
        num_qubits: int
        hamiltonian_str: list of "coeff string" e.g. ["1.0 Z0", "0.5 X1"]
        ansatz_func: Python function(params, register) -> void
        """
        # Save context for backward
        ctx.num_qubits = num_qubits
        ctx.hamiltonian_str = hamiltonian_str
        ctx.ansatz_func = ansatz_func
        
        # Detach params to convert to standard list for C++
        params_list = params.detach().cpu().numpy().tolist()
        ctx.save_for_backward(params)

        # Call C++ Engine
        # Expectation value <H>
        # Note: We need a C++ binding that takes (params, ansatz, hamiltonian) and returns energy.
        # Currently we have `QuantumDifferentiator::calculateGradients`.
        # We likely need `QuantumDifferentiator::evaluateEnergy` exposed or similar.
        # Or we manually instantiate Register, apply ansatz, measure.
        
        # Let's assume for MVP we just use the register directly in python?
        # No, for performance (and MPI), we want the C++ engine to handle it.
        
        # NOTE: The current python_bindings.cpp might need updates to expose a helper
        # that does 'RunCircuitAndMeasure'. 
        
        # Placeholder: using single-shot binding if available, else we loop.
        # Using a hypothetical `qubit_engine.run_vqe_step(n, params, ansatz, hamiltonian)`
        # If not present, we will implement it in python_bindings.cpp next.
        
        energy = qubit_engine.get_expectation_value(num_qubits, params_list, ansatz_func, hamiltonian_str)
        
        return torch.tensor(energy, dtype=params.dtype, device=params.device)

    @staticmethod
    def backward(ctx, grad_output):
        params, = ctx.saved_tensors
        params_list = params.detach().cpu().numpy().tolist()
        
        # Call C++ Engine for Gradients (MPI/GPU Accelerated)
        # Returns list [dE/dp0, dE/dp1, ...]
        grads_list = qubit_engine.get_gradients(
            ctx.num_qubits, 
            params_list, 
            ctx.ansatz_func, 
            ctx.hamiltonian_str
        )
        
        grad_input = torch.tensor(grads_list, dtype=params.dtype, device=params.device)
        
        # Chain Rule: dL/dParam = (dL/dEnergy) * (dEnergy/dParam)
        return grad_input * grad_output, None, None, None

class QuantumLayer(nn.Module):
    def __init__(self, num_qubits, num_params, hamiltonian, ansatz_func):
        super().__init__()
        self.num_qubits = num_qubits
        self.num_params = num_params
        self.hamiltonian = hamiltonian
        self.ansatz = ansatz_func
        
        # Learnable Parameters initialized randomly
        self.params = nn.Parameter(torch.rand(num_params) * 2 * 3.14159)

    def forward(self, x):
        # x is input features. 
        # Typically in QML: features encoded into circuit, OR params are the weights.
        # This layer acts as a "Variational Quantum Circuit" with fixed input-independent weights?
        # OR x modifies the weights?
        
        # MVP: Fixed Variational Layer (VQE-style) returning scalar energy.
        # Usually we want: Input -> Encoding(x) -> Variational(theta) -> Measure.
        
        # Let's assume 'x' acts as a scaling factor or additional rotation for now,
        # or we just ignore x if this is a raw VQE optimization layer.
        
        return QuantumFunction.apply(self.params, self.num_qubits, self.hamiltonian, self.ansatz)

