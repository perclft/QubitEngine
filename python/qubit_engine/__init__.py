"""
QubitEngine Python Subsystem
"""

import sys
import os
import platform
import warnings

if platform.system() == "Windows":
    # Add vcpkg bin dir to DLL search path for testing
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
    vcpkg_bin = os.path.join(project_root, "build", "temp.win-amd64-cpython-314", "Release", "qubit_engine.core", "vcpkg_installed", "x64-windows", "bin")
    if os.path.exists(vcpkg_bin):
        os.add_dll_directory(vcpkg_bin)
    
    # Also add project's bin dirs where built DLLs and executables live
    for cfg in ["Debug", "Release"]:
        bin_dir = os.path.join(project_root, "bin", cfg)
        if os.path.exists(bin_dir):
            os.add_dll_directory(os.path.abspath(bin_dir))


# Try to import the compiled C++ extension directly
try:
    from .core import (
        QuantumRegister,
        GPUQuantumRegister,
        calculate_gradients,
        get_expectation_value,
        get_gradients,
        get_expectation_value_batched,
        get_gradients_batched,
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
    "get_expectation_value_batched",
    "get_gradients_batched",
    "calculate_gradients_adjoint",
    "calculate_gradients_adjoint_gpu",
    "AdamOptimizer",
    "SPSAOptimizer",
    "Circuit",
    "Result",
    "noise",
    "vqe"
]
