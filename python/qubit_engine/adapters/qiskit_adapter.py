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
            from qiskit.circuit.library import (
                HGate, XGate, YGate, ZGate, CXGate, SGate, TGate, 
                RYGate, RZGate, RXGate, CCXGate, SwapGate, CZGate, Measure
            )
            
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
            self._target.add_instruction(RXGate(), name="rx")
            self._target.add_instruction(CCXGate(), name="ccx")
            self._target.add_instruction(SwapGate(), name="swap")
            self._target.add_instruction(CZGate(), name="cz")
            self._target.add_instruction(Measure(), name="measure")
        return self._target

    @property
    def max_circuits(self):
        return None

    @classmethod
    def _default_options(cls):
        from qiskit.providers import Options
        return Options(
            shots=1024,
            memory=False,
            noise_model=None,
            force_local=False,
            cloud_url=None,
            auth_token=None
        )

    def run(self, run_input: Union[QuantumCircuit, List[QuantumCircuit]], **options):
        """
        Execute circuits on the QubitEngine backend.
        Returns a QubitEngineJob wrapping the result.
        """
        import os
        import numpy as np

        if isinstance(run_input, QuantumCircuit):
            circuits = [run_input]
        else:
            circuits = run_input

        # Resolve options
        shots = options.get("shots", self.options.shots)
        noise_model = options.get("noise_model", self.options.noise_model)
        force_local = options.get("force_local", self.options.force_local)
        cloud_url = options.get("cloud_url", self.options.cloud_url)
        auth_token = options.get("auth_token", self.options.auth_token)

        # Environment variable override injection context
        old_env = {}
        target_env = {
            "QUBIT_FORCE_LOCAL": "1" if force_local else "0"
        }
        if cloud_url is not None:
            target_env["QUBIT_CLOUD_URL"] = str(cloud_url)
        if auth_token is not None:
            target_env["QUBIT_ENGINE_AUTH_TOKEN"] = str(auth_token)

        for k, v in target_env.items():
            old_env[k] = os.environ.get(k)
            os.environ[k] = v

        try:
            experiments = []
            for circ in circuits:
                n_qubits = circ.num_qubits

                # Check if there is any noise model
                is_noisy = (noise_model is not None)

                # Collect measurements to see if we have them
                measurements = []
                for instruction in circ.data:
                    inst = instruction.operation
                    name = inst.name
                    qargs = instruction.qubits
                    cargs = instruction.clbits
                    if name == "measure":
                        q_idx = circ.find_bit(qargs[0]).index
                        c_idx = circ.find_bit(cargs[0]).index
                        measurements.append((q_idx, c_idx))

                # Helper to apply gates on a register
                def apply_gates(r):
                    for instruction in circ.data:
                        inst = instruction.operation
                        qargs = instruction.qubits
                        name = inst.name
                        
                        if name == "h":
                            target = circ.find_bit(qargs[0]).index
                            r.applyHadamard(target)
                        elif name == "x":
                            target = circ.find_bit(qargs[0]).index
                            r.applyX(target)
                        elif name == "y":
                            target = circ.find_bit(qargs[0]).index
                            r.applyY(target)
                        elif name == "z":
                            target = circ.find_bit(qargs[0]).index
                            r.applyZ(target)
                        elif name == "s":
                            target = circ.find_bit(qargs[0]).index
                            r.applyPhaseS(target)
                        elif name == "t":
                            target = circ.find_bit(qargs[0]).index
                            r.applyPhaseT(target)
                        elif name == "cx":
                            control = circ.find_bit(qargs[0]).index
                            target = circ.find_bit(qargs[1]).index
                            r.applyCNOT(control, target)
                        elif name == "ccx":
                            c1 = circ.find_bit(qargs[0]).index
                            c2 = circ.find_bit(qargs[1]).index
                            target = circ.find_bit(qargs[2]).index
                            r.applyToffoli(c1, c2, target)
                        elif name == "swap":
                            q1 = circ.find_bit(qargs[0]).index
                            q2 = circ.find_bit(qargs[1]).index
                            r.applySWAP(q1, q2)
                        elif name == "cz":
                            control = circ.find_bit(qargs[0]).index
                            target = circ.find_bit(qargs[1]).index
                            r.applyCZ(control, target)
                        elif name == "ry":
                            target = circ.find_bit(qargs[0]).index
                            r.applyRotationY(target, float(inst.params[0]))
                        elif name == "rz":
                            target = circ.find_bit(qargs[0]).index
                            r.applyRotationZ(target, float(inst.params[0]))
                        elif name == "rx":
                            target = circ.find_bit(qargs[0]).index
                            r.applyRotationX(target, float(inst.params[0]))
                        elif name == "measure" or name == "barrier":
                            pass
                        else:
                            raise Exception(f"Gate '{name}' is not currently supported natively by QubitEngineAdapter.")

                counts = {}
                state = None

                if is_noisy:
                    # Run shot-by-shot
                    for shot in range(shots):
                        reg = QuantumRegister(n_qubits, force_local)
                        reg.setNoiseModel(noise_model)
                        
                        # Apply gates up to measurements, measuring on the fly
                        cl_val = 0
                        for instruction in circ.data:
                            inst = instruction.operation
                            qargs = instruction.qubits
                            cargs = instruction.clbits
                            name = inst.name

                            if name == "measure":
                                q_idx = circ.find_bit(qargs[0]).index
                                c_idx = circ.find_bit(cargs[0]).index
                                measured_val = reg.measure(q_idx)
                                cl_val |= (measured_val << c_idx)
                            elif name == "barrier":
                                pass
                            else:
                                if name == "h":
                                    reg.applyHadamard(circ.find_bit(qargs[0]).index)
                                elif name == "x":
                                    reg.applyX(circ.find_bit(qargs[0]).index)
                                elif name == "y":
                                    reg.applyY(circ.find_bit(qargs[0]).index)
                                elif name == "z":
                                    reg.applyZ(circ.find_bit(qargs[0]).index)
                                elif name == "s":
                                    reg.applyPhaseS(circ.find_bit(qargs[0]).index)
                                elif name == "t":
                                    reg.applyPhaseT(circ.find_bit(qargs[0]).index)
                                elif name == "cx":
                                    reg.applyCNOT(circ.find_bit(qargs[0]).index, circ.find_bit(qargs[1]).index)
                                elif name == "ccx":
                                    reg.applyToffoli(circ.find_bit(qargs[0]).index, circ.find_bit(qargs[1]).index, circ.find_bit(qargs[2]).index)
                                elif name == "swap":
                                    reg.applySWAP(circ.find_bit(qargs[0]).index, circ.find_bit(qargs[1]).index)
                                elif name == "cz":
                                    reg.applyCZ(circ.find_bit(qargs[0]).index, circ.find_bit(qargs[1]).index)
                                elif name == "ry":
                                    reg.applyRotationY(circ.find_bit(qargs[0]).index, float(inst.params[0]))
                                elif name == "rz":
                                    reg.applyRotationZ(circ.find_bit(qargs[0]).index, float(inst.params[0]))
                                elif name == "rx":
                                    reg.applyRotationX(circ.find_bit(qargs[0]).index, float(inst.params[0]))
                                else:
                                    raise Exception(f"Gate '{name}' is not currently supported natively by QubitEngineAdapter.")

                        if circ.num_clbits > 0:
                            hex_key = hex(cl_val)
                            counts[hex_key] = counts.get(hex_key, 0) + 1
                    
                    state = reg.getStateVector()
                else:
                    # Noiseless run (Simulate once)
                    reg = QuantumRegister(n_qubits, force_local)
                    apply_gates(reg)
                    state = reg.getStateVector()

                    if circ.num_clbits > 0 and len(measurements) > 0:
                        probs = np.array(reg.getProbabilities())
                        probs = probs / np.sum(probs)
                        samples = np.random.choice(len(probs), size=shots, p=probs)
                        for sample in samples:
                            cl_val = 0
                            for q_idx, c_idx in measurements:
                                bit_val = (int(sample) >> q_idx) & 1
                                cl_val |= (bit_val << c_idx)
                            hex_key = hex(cl_val)
                            counts[hex_key] = counts.get(hex_key, 0) + 1

                # Format generic result
                exp_res = ExperimentResult(
                    shots=shots,
                    success=True,
                    status="DONE",
                    data=ExperimentResultData(statevector=state, counts=counts),
                    header={"name": circ.name}
                )
                experiments.append(exp_res)

        finally:
            # Restore environment variables
            for k, v in old_env.items():
                if v is None:
                    if k in os.environ:
                        del os.environ[k]
                else:
                    os.environ[k] = v

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
