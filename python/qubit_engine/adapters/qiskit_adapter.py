import uuid
from typing import Optional, Union, List

from qiskit.providers import BackendV2, JobV1
from qiskit.result import Result
from qiskit.result.models import ExperimentResult, ExperimentResultData
from qiskit.circuit import QuantumCircuit

# Import our C++ backend
from qubit_engine.core import QuantumRegister
from qubit_engine.gate_mapping import dispatch_gate

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
                        name = inst.name
                        if name == "measure" or name == "barrier":
                            continue
                        qubits_indices = [circ.find_bit(q).index for q in instruction.qubits]
                        dispatch_gate(r, name, qubits_indices, inst.params)

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
                                qubits_indices = [circ.find_bit(q).index for q in qargs]
                                dispatch_gate(reg, name, qubits_indices, inst.params)

                        if circ.num_clbits > 0:
                            bin_key = format(cl_val, f'0{circ.num_clbits}b')
                            counts[bin_key] = counts.get(bin_key, 0) + 1
                    
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
                            bin_key = format(cl_val, f'0{circ.num_clbits}b')
                            counts[bin_key] = counts.get(bin_key, 0) + 1

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
