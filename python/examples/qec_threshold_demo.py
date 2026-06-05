import os
import sys
import numpy as np

# Ensure QubitEngine bindings can be imported
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "bin", "Release")))
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

try:
    import qubit_engine.core as qe_core
    HAS_DEPS = True
except ImportError as e:
    print(f"Error importing dependencies: {e}")
    HAS_DEPS = False

def run_threshold_demo():
    print("==================================================")
    print(" QubitEngine QEC Threshold Sweeping Demo")
    print("==================================================")
    
    if not HAS_DEPS:
        return
        
    distances = [3, 5, 7]
    noise_probs = [0.01, 0.05, 0.1, 0.15, 0.2]
    num_shots = 50
    rounds = 5
    
    print(f"Running threshold sweep for distances {distances} across {num_shots} shots per point...")
    print(f"Number of stabilizer measurement rounds: {rounds}\n")
    
    results = {d: [] for d in distances}
    
    for d in distances:
        code = qe_core.SurfaceCode(d)
        print(f"--- Distance {d} Surface Code ---")
        for p in noise_probs:
            success_count = 0
            for _ in range(num_shots):
                # The C++ simulate() method applies noise, extracts syndromes,
                # decodes using MWPM, applies corrections, and measures logical Z.
                # It returns True if the logical Z measurement was 0 (no logical error).
                if code.simulate(rounds, p):
                    success_count += 1
                    
            success_rate = success_count / num_shots
            logical_error_rate = 1.0 - success_rate
            results[d].append(logical_error_rate)
            print(f"  Physical Noise p={p:.3f} -> Logical Error Rate: {logical_error_rate:.3f}")
            
    print("\n[Sweep Complete!]")
    print("At very low physical noise, higher distances should have LOWER logical error rates.")
    print("Above the threshold, higher distances will perform WORSE than lower distances.")

if __name__ == "__main__":
    run_threshold_demo()
