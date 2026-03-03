"""
QubitEngine Python Subsystem
"""

import sys

# Try to import the compiled C++ extension directly
try:
    from .core import (
        QuantumRegister,
        GPUQuantumRegister,
        calculate_gradients,
        get_expectation_value,
        get_gradients,
        calculate_gradients_adjoint,
        calculate_gradients_adjoint_gpu,
        AdamOptimizer,
        SPSAOptimizer
    )
except ImportError as e:
    import platform
    import warnings
    warnings.warn(f"Failed to load QubitEngine C++ extension ('{e}'). Check compilation for {platform.system()}!")
    raise

__all__ = [
    "QuantumRegister",
    "GPUQuantumRegister",
    "calculate_gradients",
    "get_expectation_value",
    "get_gradients",
    "calculate_gradients_adjoint",
    "calculate_gradients_adjoint_gpu",
    "AdamOptimizer",
    "SPSAOptimizer"
]
