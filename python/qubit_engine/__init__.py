"""
QubitEngine Python Subsystem
"""

import sys
import os
import platform
import warnings

if platform.system() == "Windows":
    # Add vcpkg bin dir to DLL search path for testing
    vcpkg_bin = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), 
                             "build", "temp.win-amd64-cpython-314", "Release", "qubit_engine.core", "vcpkg_installed", "x64-windows", "bin")
    if os.path.exists(vcpkg_bin):
        os.add_dll_directory(vcpkg_bin)

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

from .circuit import Circuit
from .result import Result
from . import noise
from .vqe import vqe

__all__ = [
    "QuantumRegister",
    "GPUQuantumRegister",
    "calculate_gradients",
    "get_expectation_value",
    "get_gradients",
    "calculate_gradients_adjoint",
    "calculate_gradients_adjoint_gpu",
    "AdamOptimizer",
    "SPSAOptimizer",
    "Circuit",
    "Result",
    "noise",
    "vqe"
]
