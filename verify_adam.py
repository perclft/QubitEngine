import sys
import os
import math

# Add build directory to path
build_dir = os.path.join(os.getcwd(), 'backend/build')
sys.path.append(build_dir)

try:
    import qubit_engine as qe
    print("Successfully imported qubit_engine from", build_dir)
except ImportError as e:
    print(f"Failed to import qubit_engine: {e}")
    sys.exit(1)

def ansatz(params, qreg):
    # Simple Hardware Efficient Ansatz for H2
    # Ry(theta0) on q0
    # Ry(theta1) on q1
    # CNOT 0->1
    qreg.applyRotationY(0, params[0])
    qreg.applyRotationY(1, params[1])
    qreg.applyCNOT(0, 1)

def test_adam():
    print("\n--- Testing Native Adam Optimizer ---")
    
    # H2 Hamiltonian at 0.74A
    hamiltonian_data = [
        (-1.052373245772859, "II"),
        (0.397937424843187, "IZ"),
        (-0.397937424843187, "ZI"),
        (-0.011280104256235, "ZZ"),
        (0.180931199784231, "XX")
    ]
    
    # Initialize Optimizer
    optimizer = qe.AdamOptimizer()
    
    # Initial parameters (near zero)
    initial_params = [0.1, 0.1]
    
    print(f"Initial Params: {initial_params}")
    
    # Run Optimization
    # Signature: minimize(num_qubits, ansatz_func, hamiltonian, initial_params)
    final_params = optimizer.minimize(2, ansatz, hamiltonian_data, initial_params)
    
    print(f"Final Params: {final_params}")
    
    # Verify Final Energy
    qreg = qe.QuantumRegister(2)
    ansatz(final_params, qreg)
    
    energy = 0.0
    for coeff, p_str in hamiltonian_data:
        # Note: expectationValue takes just the string.
        # But wait, does it handle II correctly?
        # QuantumRegister.cpp code:
        # expects pauli_string len == num_qubits.
        # "II" -> I on q0, I on q1.
        val = qreg.expectationValue(p_str)
        energy += coeff * val
    
    print(f"Final Energy: {energy:.6f} Hartrees")
    
    # H2 Ground State is approx -1.137
    if energy < -1.1:
        print("[PASS] Energy converged to ground state.")
    else:
        print("[WARN] Energy did not fully converge (might need more iter or better ansatz).")

if __name__ == "__main__":
    test_adam()
