import uuid
from typing import Optional, Union, List

from qiskit.providers import BackendV2, JobV1
from qiskit.result import Result
from qiskit.result.models import ExperimentResult, ExperimentResultData
from qiskit.circuit import QuantumCircuit

# Import our C++ backend
from qubit_engine.core import QuantumRegister

class QubitEngineJob(JobV1):
    def __init__(self, backend, job_id, result):
        super().__init__(backend, job_id)
        self._result = result

    def submit(self):
        pass

    def result(self):
        return self._result

    def status(self):
        from qiskit.providers import JobStatus
        return JobStatus.DONE


class QubitEngineBackend(BackendV2):
    """
    A Qiskit Backend provider that translates and runs standard Qiskit
    QuantumCircuits on the high-performance QubitEngine C++ Simulator.
    """
    def __init__(self):
        super().__init__(name="qubit_engine_simulator")
        self._target = None # We could populate a Qiskit Target here for transpilation

    @property
    def target(self):
        # By dynamically defining target, Qiskit knows what instructions we support
        if self._target is None:
            from qiskit.transpiler import Target
            from qiskit.circuit.library import HGate, XGate, YGate, ZGate, CXGate, SGate, TGate, RYGate, RZGate, RXGate, Measure
            
            self._target = Target(num_qubits=30)
            self._target.add_instruction(HGate(), name="h")
            self._target.add_instruction(XGate(), name="x")
            self._target.add_instruction(YGate(), name="y")
            self._target.add_instruction(ZGate(), name="z")
            self._target.add_instruction(CXGate(), name="cx")
            self._target.add_instruction(SGate(), name="s")
            self._target.add_instruction(TGate(), name="t")
            self._target.add_instruction(RYGate(), name="ry")
            self._target.add_instruction(RZGate(), name="rz")
            self._target.add_instruction(RXGate(), name="rx") # Will map to Ry/Rz combo if rx not direct
            self._target.add_instruction(Measure(), name="measure")
        return self._target

    @property
    def max_circuits(self):
        return None

    @classmethod
    def _default_options(cls):
        from qiskit.providers import Options
        return Options(shots=1024, memory=False)

    def run(self, run_input: Union[QuantumCircuit, List[QuantumCircuit]], **options):
        """
        Execute circuits on the QubitEngine backend.
        Returns a QubitEngineJob wrapping the result.
        """
        if isinstance(run_input, QuantumCircuit):
            circuits = [run_input]
        else:
            circuits = run_input

        # Resolve options
        shots = options.get("shots", self.options.shots)

        experiments = []
        for circ in circuits:
            # 1. Initialize C++ Engine Register
            n_qubits = circ.num_qubits
            reg = QuantumRegister(n_qubits)

            # 2. Iterate Qiskit AST and map to C++ Backend
            # For this MVP adapter, we handle core gates explicitly
            # A full implementation would use a translation dict
            for instruction in circ.data:
                inst = instruction.operation
                qargs = instruction.qubits
                
                # Qiskit qubit.index gives absolute index
                target = circ.find_bit(qargs[-1]).index
                
                name = inst.name
                
                if name == "h":
                    reg.applyHadamard(target)
                elif name == "x":
                    reg.applyX(target)
                elif name == "y":
                    reg.applyY(target)
                elif name == "z":
                    reg.applyZ(target)
                elif name == "s":
                    reg.applyPhaseS(target)
                elif name == "t":
                    reg.applyPhaseT(target)
                elif name == "cx":
                    control = circ.find_bit(qargs[0]).index
                    reg.applyCNOT(control, target)
                elif name == "ry":
                    # inst.params[0] is the angle
                    reg.applyRotationY(target, float(inst.params[0]))
                elif name == "rz":
                    reg.applyRotationZ(target, float(inst.params[0]))
                elif name == "rx":
                    # If C++ bindings expose applyRotationX, use it. Otherwise compose via H-Rz-H
                    try:
                        getattr(reg, "applyRotationX")(target, float(inst.params[0]))
                    except AttributeError:
                        # Fallback synthesis for Rx(theta) = H Rz(theta) H
                        reg.applyHadamard(target)
                        reg.applyRotationZ(target, float(inst.params[0]))
                        reg.applyHadamard(target)
                elif name == "measure" or name == "barrier":
                    # Simulators handle statevector natively, ignoring barrier
                    pass
                else:
                    raise Exception(f"Gate '{name}' is not currently supported natively by QubitEngineAdapter.")

            # 3. Retrieve final state vector
            state = reg.getStateVector()
            
            # Format generic result
            exp_res = ExperimentResult(
                shots=shots,
                success=True,
                status="DONE",
                data=ExperimentResultData(statevector=state),
                header={"name": circ.name}
            )
            experiments.append(exp_res)

        job_id = str(uuid.uuid4())
        res = Result(
            backend_name=self.name,
            backend_version="0.2.0",
            qobj_id=job_id,
            job_id=job_id,
            success=True,
            results=experiments
        )
        return QubitEngineJob(self, job_id, res)
