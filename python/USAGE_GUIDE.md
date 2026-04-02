# QubitEngine Python Usage Guide

This guide provides examples and instructions for using the QubitEngine core bindings and PyTorch integration.

## Installation

Ensure you have the backend binary and compiled Python module (`core.so` or `core.pyd`) in your `PYTHONPATH`.

```bash
export PYTHONPATH=$PYTHONPATH:/path/to/bin
```

## Core Library Usage

The `qubit_engine.core` module provides direct access to the simulation engine.

```python
import qubit_engine.core as qe

# Create a register with 2 qubits
reg = qe.QuantumRegister(2)

# Apply a Hadamard gate to Qubit 0
reg.apply_hadamard(0)

# Apply a CNOT gate (Control=0, Target=1)
reg.apply_cnot(0, 1)

# Get the full state vector
state = reg.get_state_vector()
print(f"Final State: {state}")
```

## PyTorch Integration (VQE / Machine Learning)

The QubitEngine integrates with PyTorch for differentiable quantum simulations. This is useful for VQE and Quantum Neural Networks (QNNs).

### Example: Custom Ansatz

```python
import torch
from qubit_engine.torch_quantum import QuantumModule

# Define a custom parameterized circuit
class SimpleAnsatz(QuantumModule):
    def __init__(self, num_qubits=2):
        super().__init__()
        self.theta = torch.nn.Parameter(torch.tensor([0.1, 0.2]))

    def forward(self, x):
        # Apply parameterized rotations
        self.ry(0, self.theta[0])
        self.ry(1, self.theta[1])
        self.cnot(0, 1)
        return self.expectation_z(0)

# Run an optimization loop
ansatz = SimpleAnsatz()
optimizer = torch.optim.Adam(ansatz.parameters(), lr=0.01)

for i in range(100):
    optimizer.zero_grad()
    loss = ansatz(None)  # Minimize expectation value
    loss.backward()
    optimizer.step()
    if i % 10 == 0:
        print(f"Iter {i}: Energy = {loss.item():.6f}")
```

## Troubleshooting

- **Symbol Not Found**: Ensure you have linked the `libspdlog`, `libgrpc`, and other dependencies correctly when building the module.
- **CUDA Errors**: If using the GPU backend, ensure `LD_LIBRARY_PATH` contains the CUDA toolkit libraries.
- **Segmentation Faults**: Verify that the qubit indices are within the range specified during `QuantumRegister` initialization.
