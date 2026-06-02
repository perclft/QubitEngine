import os
import sys

# Add the binary path to import the engine
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'bin', 'Release')))

import core
import matplotlib.pyplot as plt
import numpy as np

def run_benchmark():
    distances = [3, 5, 7]
    physical_error_rates = np.logspace(-3, -1, 10)  # 0.1% to 10%
    num_shots = 100
    num_rounds = 5

    print(f"Running QEC Threshold Benchmark")
    print(f"Rounds per shot: {num_rounds}, Shots per point: {num_shots}")
    print("-" * 50)

    logical_errors = {d: [] for d in distances}

    for p in physical_error_rates:
        print(f"Physical Error Rate: {p:.4f}")
        for d in distances:
            sc = core.SurfaceCode(d)
            failures = 0
            for _ in range(num_shots):
                # Returns True if logical state was preserved (0 failures)
                if not sc.simulate(num_rounds, p):
                    failures += 1
            
            logical_error_rate = failures / num_shots
            logical_errors[d].append(logical_error_rate)
            print(f"  d={d}: Logical Error Rate = {logical_error_rate:.4f}")

    # Plot results
    plt.figure(figsize=(10, 6))
    for d in distances:
        plt.plot(physical_error_rates, logical_errors[d], marker='o', label=f'Distance {d}')
    
    # Plot pseudo-threshold reference line
    plt.plot(physical_error_rates, physical_error_rates, '--', color='gray', label='p_logical = p_physical')

    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Physical Error Rate ($p$)')
    plt.ylabel('Logical Error Rate ($p_L$)')
    plt.title('Surface Code Threshold Benchmark')
    plt.legend()
    plt.grid(True, which="both", ls="--", alpha=0.5)
    
    out_file = 'qec_threshold.png'
    plt.savefig(out_file)
    print(f"\nPlot saved to {out_file}")

if __name__ == "__main__":
    run_benchmark()
