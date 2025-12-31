#!/usr/bin/env python3
"""
QubitEngine Performance Benchmark Suite
Tests quantum simulation performance across various qubit counts and gate types.
"""

import sys
import time
import os

# Add the bin directory to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'bin'))

import qubit_engine

def benchmark_gate(name, gate_func, num_qubits, iterations=100):
    """Benchmark a single gate operation."""
    q = qubit_engine.QuantumRegister(num_qubits)
    
    start = time.perf_counter()
    for _ in range(iterations):
        for qubit in range(num_qubits):
            gate_func(q, qubit)
    end = time.perf_counter()
    
    total_gates = iterations * num_qubits
    duration = end - start
    throughput = total_gates / duration if duration > 0 else 0
    
    return {
        'name': name,
        'qubits': num_qubits,
        'total_gates': total_gates,
        'duration_s': duration,
        'gates_per_sec': throughput,
        'us_per_gate': (duration / total_gates) * 1e6 if total_gates > 0 else 0
    }

def benchmark_two_qubit_gate(name, gate_func, num_qubits, iterations=50):
    """Benchmark two-qubit gate operations."""
    if num_qubits < 2:
        return None
    
    q = qubit_engine.QuantumRegister(num_qubits)
    
    start = time.perf_counter()
    for _ in range(iterations):
        for qubit in range(num_qubits - 1):
            gate_func(q, qubit, qubit + 1)
    end = time.perf_counter()
    
    total_gates = iterations * (num_qubits - 1)
    duration = end - start
    throughput = total_gates / duration if duration > 0 else 0
    
    return {
        'name': name,
        'qubits': num_qubits,
        'total_gates': total_gates,
        'duration_s': duration,
        'gates_per_sec': throughput,
        'us_per_gate': (duration / total_gates) * 1e6 if total_gates > 0 else 0
    }

def benchmark_bell_state(num_qubits):
    """Benchmark creating Bell pairs."""
    q = qubit_engine.QuantumRegister(num_qubits)
    
    start = time.perf_counter()
    for i in range(0, num_qubits - 1, 2):
        q.applyHadamard(i)
        q.applyCNOT(i, i + 1)
    end = time.perf_counter()
    
    return end - start

def benchmark_ghz_state(num_qubits):
    """Benchmark creating GHZ state (maximally entangled)."""
    q = qubit_engine.QuantumRegister(num_qubits)
    
    start = time.perf_counter()
    q.applyHadamard(0)
    for i in range(num_qubits - 1):
        q.applyCNOT(i, i + 1)
    end = time.perf_counter()
    
    return end - start

def benchmark_random_circuit(num_qubits, depth=20):
    """Benchmark a random-like circuit with mixed gates."""
    q = qubit_engine.QuantumRegister(num_qubits)
    import random
    random.seed(42)  # Reproducible
    
    start = time.perf_counter()
    for layer in range(depth):
        # Single qubit gates
        for qubit in range(num_qubits):
            gate = random.choice(['H', 'X', 'Y', 'Z', 'RY', 'RZ'])
            if gate == 'H':
                q.applyHadamard(qubit)
            elif gate == 'X':
                q.applyX(qubit)
            elif gate == 'Y':
                q.applyY(qubit)
            elif gate == 'Z':
                q.applyZ(qubit)
            elif gate == 'RY':
                q.applyRotationY(qubit, 0.5)
            elif gate == 'RZ':
                q.applyRotationZ(qubit, 0.5)
        
        # CNOT layer (entanglement)
        for i in range(0, num_qubits - 1, 2):
            q.applyCNOT(i, i + 1)
    
    end = time.perf_counter()
    
    total_gates = depth * num_qubits + depth * (num_qubits // 2)
    return end - start, total_gates

def format_gates_per_sec(gps):
    if gps >= 1e6:
        return f"{gps/1e6:.2f}M"
    elif gps >= 1e3:
        return f"{gps/1e3:.2f}K"
    else:
        return f"{gps:.0f}"

def print_separator():
    print("=" * 80)

def main():
    print()
    print_separator()
    print("🔬 QubitEngine Performance Benchmark Suite")
    print(f"   Platform: {sys.platform}")
    print(f"   Python: {sys.version.split()[0]}")
    print_separator()
    print()
    
    # Test scaling with qubit count
    print("📊 SCALING TEST: State Vector Size vs Performance")
    print("-" * 80)
    print(f"{'Qubits':>8} {'State Size':>12} {'Memory (KB)':>12} {'Init Time':>12} {'H Gate Time':>12}")
    print("-" * 80)
    
    for n_qubits in [4, 8, 12, 16, 18, 20, 22]:
        state_size = 2 ** n_qubits
        memory_kb = state_size * 16 / 1024  # complex<double> = 16 bytes
        
        # Init time
        start = time.perf_counter()
        q = qubit_engine.QuantumRegister(n_qubits)
        init_time = time.perf_counter() - start
        
        # H gate time (single gate)
        start = time.perf_counter()
        for _ in range(100):
            q.applyHadamard(0)
        h_time = (time.perf_counter() - start) / 100
        
        print(f"{n_qubits:>8} {state_size:>12,} {memory_kb:>12,.1f} {init_time*1000:>10.3f}ms {h_time*1000:>10.4f}ms")
    
    print()
    print_separator()
    print("⚡ GATE THROUGHPUT TEST (16 qubits, 100 iterations)")
    print("-" * 80)
    print(f"{'Gate':>12} {'Total Gates':>12} {'Duration':>10} {'Gates/sec':>12} {'µs/gate':>10}")
    print("-" * 80)
    
    n_qubits = 16
    iterations = 100
    
    # Single qubit gates
    gates = [
        ("Hadamard", lambda q, i: q.applyHadamard(i)),
        ("Pauli-X", lambda q, i: q.applyX(i)),
        ("Pauli-Y", lambda q, i: q.applyY(i)),
        ("Pauli-Z", lambda q, i: q.applyZ(i)),
        ("Rotation-Y", lambda q, i: q.applyRotationY(i, 0.5)),
        ("Rotation-Z", lambda q, i: q.applyRotationZ(i, 0.5)),
    ]
    
    for name, gate_func in gates:
        result = benchmark_gate(name, gate_func, n_qubits, iterations)
        print(f"{result['name']:>12} {result['total_gates']:>12,} {result['duration_s']:>9.4f}s {format_gates_per_sec(result['gates_per_sec']):>12} {result['us_per_gate']:>10.2f}")
    
    # Two-qubit gates
    two_qubit_gates = [
        ("CNOT", lambda q, c, t: q.applyCNOT(c, t)),
    ]
    
    for name, gate_func in two_qubit_gates:
        result = benchmark_two_qubit_gate(name, gate_func, n_qubits, iterations)
        if result:
            print(f"{result['name']:>12} {result['total_gates']:>12,} {result['duration_s']:>9.4f}s {format_gates_per_sec(result['gates_per_sec']):>12} {result['us_per_gate']:>10.2f}")
    
    print()
    print_separator()
    print("🔗 ENTANGLEMENT BENCHMARKS")
    print("-" * 80)
    print(f"{'Circuit':>20} {'Qubits':>8} {'Time':>12} {'Gates':>10} {'Gates/sec':>12}")
    print("-" * 80)
    
    for n in [8, 12, 16, 20]:
        # Bell pairs
        t = benchmark_bell_state(n)
        gates = n  # H + CNOT for each pair
        print(f"{'Bell Pairs':>20} {n:>8} {t*1000:>10.3f}ms {gates:>10} {format_gates_per_sec(gates/t):>12}")
        
        # GHZ state
        t = benchmark_ghz_state(n)
        gates = n  # 1 H + (n-1) CNOTs
        print(f"{'GHZ State':>20} {n:>8} {t*1000:>10.3f}ms {gates:>10} {format_gates_per_sec(gates/t):>12}")
    
    print()
    print_separator()
    print("🎲 RANDOM CIRCUIT BENCHMARK (circuit depth = 20)")
    print("-" * 80)
    print(f"{'Qubits':>8} {'Total Gates':>12} {'Duration':>12} {'Gates/sec':>12}")
    print("-" * 80)
    
    for n in [8, 12, 16, 18, 20]:
        t, gates = benchmark_random_circuit(n, depth=20)
        print(f"{n:>8} {gates:>12,} {t*1000:>10.2f}ms {format_gates_per_sec(gates/t):>12}")
    
    print()
    print_separator()
    print("🏋️ STRESS TEST: Maximum Qubit Count")
    print("-" * 80)
    
    max_qubits = 20
    while max_qubits <= 26:
        try:
            state_size = 2 ** max_qubits
            memory_gb = state_size * 16 / (1024**3)
            print(f"Testing {max_qubits} qubits ({state_size:,} amplitudes, {memory_gb:.2f} GB)...", end=" ", flush=True)
            
            start = time.perf_counter()
            q = qubit_engine.QuantumRegister(max_qubits)
            init_time = time.perf_counter() - start
            
            # Apply some gates
            start = time.perf_counter()
            q.applyHadamard(0)
            q.applyCNOT(0, max_qubits - 1)
            gate_time = time.perf_counter() - start
            
            print(f"✅ Init: {init_time*1000:.1f}ms, Gate: {gate_time*1000:.2f}ms")
            max_qubits += 2
        except Exception as e:
            print(f"❌ Failed: {e}")
            break
    
    print()
    print_separator()
    print("✅ Benchmark Complete!")
    print_separator()

if __name__ == "__main__":
    main()
