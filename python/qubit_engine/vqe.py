from typing import Callable, List, Tuple, Union
from .circuit import Circuit
from .core import AdamOptimizer, SPSAOptimizer

def vqe(hamiltonian: List[Tuple[float, str]], 
        ansatz_func: Callable[[List[float], 'QuantumRegister'], None], 
        num_qubits: int,
        optimizer: str = "adam", 
        initial_params: List[float] = None,
        max_iter: int = 100) -> dict:
    """
    Run VQE (Variational Quantum Eigensolver).
    
    Args:
        hamiltonian: List of (coefficient, pauli_string) pairs.
        ansatz_func: A function taking (params, qreg) that applies the parameterized circuit.
        num_qubits: Number of qubits.
        optimizer: "adam" or "spsa".
        initial_params: Initial parameter values.
        
    Returns:
        dict with minimum energy and optimal parameters.
    """
    if initial_params is None:
        import numpy as np
        # Simple heuristic to guess parameter count: we might not know it easily, so default 0 unless provided
        raise ValueError("initial_params must be provided")

    if optimizer.lower() == "adam":
        opt = AdamOptimizer()
    elif optimizer.lower() == "spsa":
        opt = SPSAOptimizer()
    else:
        raise ValueError(f"Unknown optimizer: {optimizer}")
        
    # The C++ optimizers take num_qubits, ansatz, hamiltonian, initial_params
    result = opt.minimize(num_qubits, ansatz_func, hamiltonian, initial_params)
    
    return {
        "energy": result.energy if hasattr(result, 'energy') else getattr(result, 'minimum_value', 0),
        "parameters": result.parameters if hasattr(result, 'parameters') else getattr(result, 'optimal_parameters', []),
        "history": result.history if hasattr(result, 'history') else []
    }
