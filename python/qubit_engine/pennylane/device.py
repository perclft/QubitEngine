import pennylane as qml
from pennylane import Device
import numpy as np

try:
    from qubit_engine import core
    _HAS_CORE = True
except ImportError:
    _HAS_CORE = False

class QubitEngineDevice(Device):
    """PennyLane Device for QubitEngine native simulator."""
    name = "QubitEngine Simulator"
    short_name = "qubit_engine.simulator"
    pennylane_requires = ">=0.35.0"
    version = "0.2.0"
    author = "QubitEngine Core Team"

    operations = {"Hadamard", "PauliX", "PauliY", "PauliZ", "CNOT", "RX", "RY", "RZ", "SWAP", "CZ", "Toffoli", "S", "T"}
    observables = {"PauliX", "PauliY", "PauliZ", "Identity", "Hamiltonian"}

    # Map PennyLane gate names to QuantumRegister method names
    _GATE_MAP = {
        "Hadamard": "applyHadamard",
        "PauliX": "applyX",
        "PauliY": "applyY",
        "PauliZ": "applyZ",
        "CNOT": "applyCNOT",
        "RX": "applyRotationX",
        "RY": "applyRotationY",
        "RZ": "applyRotationZ",
        "SWAP": "applySWAP",
        "CZ": "applyCZ",
        "Toffoli": "applyToffoli",
        "S": "applyPhaseS",
        "T": "applyPhaseT",
    }

    def __init__(self, wires, shots=None, **kwargs):
        super().__init__(wires=wires, shots=shots, **kwargs)
        if not _HAS_CORE:
            raise ImportError(
                "qubit_engine.core C++ extension not found. "
                "Build with: pip install -e .[all] or cmake --build backend/build"
            )
        self._reg = core.QuantumRegister(len(self.wires))

    def apply(self, operations, **kwargs):
        """Map PennyLane gates to C++ Engine."""
        # Reset register state for a fresh execution
        self._reg = core.QuantumRegister(len(self.wires))

        for op in operations:
            method_name = self._GATE_MAP.get(op.name)
            if method_name is None:
                raise qml.DeviceError(f"Unsupported gate: {op.name}")

            method = getattr(self._reg, method_name)
            wires = [self.wires.index(w) for w in op.wires]

            if op.name in ("RX", "RY", "RZ"):
                method(wires[0], float(op.parameters[0]))
            elif op.name in ("CNOT", "SWAP", "CZ"):
                method(wires[0], wires[1])
            elif op.name == "Toffoli":
                method(wires[0], wires[1], wires[2])
            else:
                method(wires[0])

    def expval(self, observable, shot_range=None, bin_size=None):
        """Compute native expectation values utilizing Adjoint differentiation."""
        if isinstance(observable, qml.operation.Tensor):
            # Tensor product of Pauli observables
            pauli_map = {"PauliX": "X", "PauliY": "Y", "PauliZ": "Z", "Identity": "I"}
            term_parts = []
            for obs in observable.obs:
                pauli = pauli_map.get(obs.name, "I")
                wire = self.wires.index(obs.wires[0])
                term_parts.append(f"{pauli}{wire}")
            term_string = " ".join(term_parts)
            return core.get_expectation_value(self._reg, [(1.0, term_string)])

        pauli_map = {"PauliX": "X", "PauliY": "Y", "PauliZ": "Z", "Identity": "I"}
        pauli = pauli_map.get(observable.name, "I")
        if observable.wires:
            wire = self.wires.index(observable.wires[0])
            return core.get_expectation_value(self._reg, [(1.0, f"{pauli}{wire}")])
        return 0.0
