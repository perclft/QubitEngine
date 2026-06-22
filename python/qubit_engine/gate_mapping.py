# Shared gate dispatch mapping to map Python instructions to C++ QuantumRegister methods.
# Format: "GATE_NAME": ("cpp_method_name", num_qubits, num_params)

GATE_MAP = {
    "H": ("applyHadamard", 1, 0),
    "h": ("applyHadamard", 1, 0),
    "X": ("applyX", 1, 0),
    "x": ("applyX", 1, 0),
    "Y": ("applyY", 1, 0),
    "y": ("applyY", 1, 0),
    "Z": ("applyZ", 1, 0),
    "z": ("applyZ", 1, 0),
    "S": ("applyPhaseS", 1, 0),
    "s": ("applyPhaseS", 1, 0),
    "T": ("applyPhaseT", 1, 0),
    "t": ("applyPhaseT", 1, 0),
    "RX": ("applyRotationX", 1, 1),
    "rx": ("applyRotationX", 1, 1),
    "RY": ("applyRotationY", 1, 1),
    "ry": ("applyRotationY", 1, 1),
    "RZ": ("applyRotationZ", 1, 1),
    "rz": ("applyRotationZ", 1, 1),
    "CX": ("applyCNOT", 2, 0),
    "cx": ("applyCNOT", 2, 0),
    "CZ": ("applyCZ", 2, 0),
    "cz": ("applyCZ", 2, 0),
    "SWAP": ("applySWAP", 2, 0),
    "swap": ("applySWAP", 2, 0),
    "CCX": ("applyToffoli", 3, 0),
    "ccx": ("applyToffoli", 3, 0),
}

def dispatch_gate(qreg, name: str, qubits: list, params: list):
    """Applies a gate to the QuantumRegister based on the shared GATE_MAP."""
    if name.upper() == "MEASURE" or name.lower() == "barrier":
        return

    mapping = GATE_MAP.get(name)
    if not mapping:
        raise ValueError(f"Gate '{name}' is not currently supported natively by QubitEngine.")

    method_name, expected_qubits, expected_params = mapping
    method = getattr(qreg, method_name)

    if expected_params > 0 and expected_qubits == 1:
        method(qubits[0], float(params[0]))
    elif expected_qubits == 1:
        method(qubits[0])
    elif expected_qubits == 2:
        method(qubits[0], qubits[1])
    elif expected_qubits == 3:
        method(qubits[0], qubits[1], qubits[2])
