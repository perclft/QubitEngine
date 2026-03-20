import numpy as np
from qiskit.providers import BackendV2, Options
from qiskit.transpiler import Target

try:
    from qubit_engine import core
    _HAS_CORE = True
except ImportError:
    _HAS_CORE = False


class QubitEngineBackend(BackendV2):
    """Qiskit BackendV2 interface for QubitEngine C++ simulator."""

    # Map Qiskit instruction names to (gate_enum, needs_angle, num_qubits)
    _GATE_MAP = {
        'h':       ('applyHadamard',   False, 1),
        'x':       ('applyX',          False, 1),
        'y':       ('applyY',          False, 1),
        'z':       ('applyZ',          False, 1),
        'cx':      ('applyCNOT',       False, 2),
        'swap':    ('applySWAP',       False, 2),
        'cz':      ('applyCZ',         False, 2),
        's':       ('applyPhaseS',     False, 1),
        't':       ('applyPhaseT',     False, 1),
        'rx':      ('applyRotationX',  True,  1),
        'ry':      ('applyRotationY',  True,  1),
        'rz':      ('applyRotationZ',  True,  1),
        'ccx':     ('applyToffoli',    False, 3),
    }

    def __init__(self, num_qubits=30, **kwargs):
        super().__init__(
            provider=None,
            name="qubit_engine_simulator",
            description="High-performance QubitEngine C++ JIT Simulator",
            backend_version="0.2.0"
        )
        self._num_qubits = num_qubits
        self._target = Target(num_qubits=num_qubits)

    @classmethod
    def _default_options(cls):
        return Options(shots=1024, memory=False)

    @property
    def target(self):
        return self._target

    @property
    def max_circuits(self):
        return None

    def run(self, run_input, **options):
        """Runs the circuit using the native C++ backend."""
        if not _HAS_CORE:
            raise ImportError(
                "qubit_engine.core C++ extension not found. "
                "Build with: pip install -e .[all] or cmake --build backend/build"
            )

        circuits = run_input if isinstance(run_input, list) else [run_input]
        shots = options.get('shots', self.options.shots)
        results = []

        for circ in circuits:
            reg = core.QuantumRegister(circ.num_qubits)
            measurement_qubits = []

            for instruction in circ.data:
                inst_name = instruction.operation.name.lower()

                if inst_name == 'measure':
                    qubits = [q._index for q in instruction.qubits]
                    measurement_qubits.extend(qubits)
                    continue

                if inst_name == 'barrier':
                    continue

                if inst_name not in self._GATE_MAP:
                    raise ValueError(f"Unsupported instruction: {inst_name}")

                method_name, needs_angle, n_qubits = self._GATE_MAP[inst_name]
                method = getattr(reg, method_name)
                qubits = [q._index for q in instruction.qubits]

                if needs_angle:
                    angle = float(instruction.operation.params[0])
                    method(qubits[0], angle)
                elif n_qubits == 1:
                    method(qubits[0])
                elif n_qubits == 2:
                    method(qubits[0], qubits[1])
                elif n_qubits == 3:
                    method(qubits[0], qubits[1], qubits[2])

            # Perform measurements
            counts = {}
            if measurement_qubits:
                for _ in range(shots):
                    result = reg.measure()
                    # Format as bitstring
                    bits = ''.join(str(int(result.get(q, False))) for q in measurement_qubits)
                    counts[bits] = counts.get(bits, 0) + 1

            results.append({
                'counts': counts,
                'num_qubits': circ.num_qubits,
                'shots': shots,
            })

        return results
