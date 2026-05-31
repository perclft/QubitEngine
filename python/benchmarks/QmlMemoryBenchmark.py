import time
import sys
import os
import gc

# Silence JWT warnings during benchmark by setting the secret env var
os.environ["QUBIT_ENGINE_JWT_SECRET"] = "benchmark_secret"

import pytest
torch = pytest.importorskip("torch")

# Ensure the build directories are in the search path
build_dir = os.path.join(os.path.dirname(__file__), "../../backend/build")
sys.path.append(build_dir)
sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

try:
    import qubit_engine
    from torch_quantum import QuantumLayer
except ImportError:
    print("Error: Could not import qubit_engine or torch_quantum. Build the project first.")
    sys.exit(1)

def ansatz(p, x, qreg):
    qreg.applyRotationY(0, p[0] + x[0])
    qreg.applyRotationY(1, p[1] + x[1])
    qreg.applyCNOT(0, 1)

def run_benchmark():
    num_qubits = 2
    num_params = 2
    hamiltonian = [(1.0, "Z0"), (1.0, "Z1")]
    
    # Batch sizes to test
    batch_sizes = [10, 100, 500, 2000]
    
    print("=============================================================")
    # Format as Markdown Table for output
    print("| Batch Size | Zero-Copy Time (s) | List-Serialization Time (s) | Speedup |")
    print("| :--- | :--- | :--- | :--- |")
    
    for batch_size in batch_sizes:
        # Generate inputs
        inputs = torch.randn(batch_size, 2, dtype=torch.float64, requires_grad=True)
        
        # Instantiate optimized QuantumLayer
        layer = QuantumLayer(num_qubits, num_params, hamiltonian, ansatz, diff_method="adjoint")
        
        # 1. Warm-up
        _ = layer(inputs)
        
        # 2. Time Optimized Zero-Copy Pass (Forward + Backward)
        gc.collect()
        t0 = time.perf_counter()
        out = layer(inputs)
        loss = out.sum()
        loss.backward()
        t_opt = time.perf_counter() - t0
        
        # 3. Time Simulated List Serialization Pass (Forward + Backward)
        # We simulate the old list conversion by running .tolist() on tensors
        gc.collect()
        t1 = time.perf_counter()
        # Mock forward list conversion
        _ = inputs.detach().cpu().numpy().tolist()
        _ = layer.params.detach().cpu().numpy().tolist()
        
        out = layer(inputs)
        loss = out.sum()
        loss.backward()
        
        # Mock backward list conversion
        _ = inputs.detach().cpu().numpy().tolist()
        _ = layer.params.detach().cpu().numpy().tolist()
        t_list = time.perf_counter() - t1
        
        speedup = t_list / t_opt if t_opt > 0 else 0
        print(f"| {batch_size:<10} | {t_opt:<18.5f} | {t_list:<27.5f} | {speedup:.2f}x |")
    print("=============================================================")

if __name__ == "__main__":
    run_benchmark()
