from dataclasses import dataclass, field
import numpy as np

@dataclass
class Result:
    """Result of a quantum circuit execution."""
    statevector: np.ndarray = field(default_factory=lambda: np.array([], dtype=np.complex128))
    probabilities: dict[str, float] = field(default_factory=dict)
    counts: dict[str, int] = field(default_factory=dict)
    metadata: dict = field(default_factory=dict)

    def get_counts(self) -> dict[str, int]:
        return self.counts

    def get_statevector(self) -> np.ndarray:
        return self.statevector

    def __repr__(self) -> str:
        s = "Result:\n"
        if self.counts:
            s += f"  Counts: {self.counts}\n"
        if len(self.statevector) > 0:
            s += f"  Statevector: {self.statevector[:4]}... (size {len(self.statevector)})\n"
        if self.metadata:
            s += f"  Metadata: {self.metadata}\n"
        return s
