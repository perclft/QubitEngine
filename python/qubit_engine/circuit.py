import math
import numpy as np
from typing import List, Tuple, Any, Optional, Dict
from .core import QuantumRegister, StabilizerBackend, GPUQuantumRegister, NoiseModel
from .result import Result
import time

class Circuit:
    """A quantum circuit builder that supports fluent chaining."""
    
    def __init__(self, num_qubits: int):
        self.num_qubits = num_qubits
        self.instructions: List[Tuple[str, List[int], List[float]]] = []
        
    def h(self, target: int) -> 'Circuit':
        self.instructions.append(("H", [target], []))
        return self
        
    def x(self, target: int) -> 'Circuit':
        self.instructions.append(("X", [target], []))
        return self
        
    def y(self, target: int) -> 'Circuit':
        self.instructions.append(("Y", [target], []))
        return self
        
    def z(self, target: int) -> 'Circuit':
        self.instructions.append(("Z", [target], []))
        return self
        
    def s(self, target: int) -> 'Circuit':
        self.instructions.append(("S", [target], []))
        return self
        
    def t(self, target: int) -> 'Circuit':
        self.instructions.append(("T", [target], []))
        return self
        
    def rx(self, target: int, angle: float) -> 'Circuit':
        self.instructions.append(("RX", [target], [angle]))
        return self
        
    def ry(self, target: int, angle: float) -> 'Circuit':
        self.instructions.append(("RY", [target], [angle]))
        return self
        
    def rz(self, target: int, angle: float) -> 'Circuit':
        self.instructions.append(("RZ", [target], [angle]))
        return self
        
    def cx(self, control: int, target: int) -> 'Circuit':
        self.instructions.append(("CX", [control, target], []))
        return self
        
    def cz(self, control: int, target: int) -> 'Circuit':
        self.instructions.append(("CZ", [control, target], []))
        return self
        
    def swap(self, qubit1: int, qubit2: int) -> 'Circuit':
        self.instructions.append(("SWAP", [qubit1, qubit2], []))
        return self
        
    def ccx(self, control1: int, control2: int, target: int) -> 'Circuit':
        self.instructions.append(("CCX", [control1, control2, target], []))
        return self
        
    def measure(self, target: int) -> 'Circuit':
        self.instructions.append(("MEASURE", [target], []))
        return self
        
    def measure_all(self) -> 'Circuit':
        for i in range(self.num_qubits):
            self.measure(i)
        return self
        
    def _apply_to_register(self, qreg) -> None:
        """Replay instructions onto a QuantumRegister backend."""
        for inst, qubits, params in self.instructions:
            if inst == "H": qreg.applyHadamard(qubits[0])
            elif inst == "X": qreg.applyX(qubits[0])
            elif inst == "Y": qreg.applyY(qubits[0])
            elif inst == "Z": qreg.applyZ(qubits[0])
            elif inst == "S": qreg.applyPhaseS(qubits[0])
            elif inst == "T": qreg.applyPhaseT(qubits[0])
            elif inst == "RX": qreg.applyRotationX(qubits[0], params[0])
            elif inst == "RY": qreg.applyRotationY(qubits[0], params[0])
            elif inst == "RZ": qreg.applyRotationZ(qubits[0], params[0])
            elif inst == "CX": qreg.applyCNOT(qubits[0], qubits[1])
            elif inst == "CZ": qreg.applyCZ(qubits[0], qubits[1])
            elif inst == "SWAP": qreg.applySWAP(qubits[0], qubits[1])
            elif inst == "CCX": qreg.applyToffoli(qubits[0], qubits[1], qubits[2])
            elif inst == "MEASURE": qreg.measure(qubits[0])
            
    def run(self, shots: int = 1024, noise: Optional[NoiseModel] = None, backend: str = "auto") -> Result:
        """Execute the circuit."""
        start_time = time.time()
        
        # Decide backend
        if backend == "auto":
            backend = "cpu" if self.num_qubits <= 20 else "mps"
            
        metadata = {"backend": backend, "shots": shots, "noise": noise is not None}
        
        # Check if we have measurements
        has_measurements = any(inst == "MEASURE" for inst, _, _ in self.instructions)
        
        counts = {}
        statevector = np.array([], dtype=np.complex128)
        
        if shots > 0 and has_measurements:
            # We must run multiple shots (naive loop for now, a C++ batch measurement would be faster)
            for _ in range(shots):
                if backend == "stabilizer":
                    qreg = StabilizerBackend(self.num_qubits)
                elif backend == "gpu":
                    qreg = GPUQuantumRegister(self.num_qubits)
                else:
                    qreg = QuantumRegister(self.num_qubits)
                    
                if hasattr(qreg, "setNoiseModel") and noise is not None:
                    qreg.setNoiseModel(noise)
                    
                # Replay
                outcome = 0
                for inst, qubits, params in self.instructions:
                    if inst == "H": qreg.applyHadamard(qubits[0])
                    elif inst == "X": qreg.applyX(qubits[0])
                    elif inst == "Y": qreg.applyY(qubits[0])
                    elif inst == "Z": qreg.applyZ(qubits[0])
                    elif inst == "S": qreg.applyPhaseS(qubits[0])
                    elif inst == "T": qreg.applyPhaseT(qubits[0])
                    elif inst == "RX": qreg.applyRotationX(qubits[0], params[0])
                    elif inst == "RY": qreg.applyRotationY(qubits[0], params[0])
                    elif inst == "RZ": qreg.applyRotationZ(qubits[0], params[0])
                    elif inst == "CX": qreg.applyCNOT(qubits[0], qubits[1])
                    elif inst == "CZ": qreg.applyCZ(qubits[0], qubits[1])
                    elif inst == "SWAP": qreg.applySWAP(qubits[0], qubits[1])
                    elif inst == "CCX": qreg.applyToffoli(qubits[0], qubits[1], qubits[2])
                    elif inst == "MEASURE": 
                        res = qreg.measure(qubits[0])
                        if res: outcome |= (1 << qubits[0])
                
                bits = format(outcome, f'0{self.num_qubits}b')[::-1] # Qiskit-style endianness
                counts[bits] = counts.get(bits, 0) + 1
        else:
            # Single shot for statevector
            if backend == "stabilizer":
                qreg = StabilizerBackend(self.num_qubits)
            elif backend == "gpu":
                qreg = GPUQuantumRegister(self.num_qubits)
            else:
                qreg = QuantumRegister(self.num_qubits)

            if hasattr(qreg, "setNoiseModel") and noise is not None:
                qreg.setNoiseModel(noise)
            self._apply_to_register(qreg)
            
            if hasattr(qreg, "getStateVector"):
                statevector = qreg.getStateVector()
            elif hasattr(qreg, "get_state_vector"):
                statevector = qreg.get_state_vector()
            
        metadata["execution_time_s"] = time.time() - start_time
        return Result(statevector=statevector, counts=counts, metadata=metadata)
        
    def statevector(self, noise: Optional[NoiseModel] = None) -> np.ndarray:
        """Convenience method to get the final statevector without measurements."""
        res = self.run(shots=0, noise=noise, backend="cpu")
        return res.statevector
