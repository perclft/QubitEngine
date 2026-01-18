import sys
import os
import math
import random

# Add build path
sys.path.append(os.path.join(os.getcwd(), 'python/build'))

try:
    import qubit_engine
except ImportError:
    sys.path.append(os.path.join(os.getcwd(), '../build'))
    import qubit_engine

def main():
    print("--- Variational Quantum Eigensolver (VQE) Demo ---")
    print("Target: H2 Molecule Ground State Energy (~ -1.137 Hartrees)")
    
    # 1. Setup Hamiltonian
    # H2 at 0.74A bond length
    hamiltonian = qubit_engine.MolecularHamiltonian.getHamiltonian(qubit_engine.MoleculeType.H2)
    n_qubits = qubit_engine.MolecularHamiltonian.getNumQubits(qubit_engine.MoleculeType.H2) # 2
    
    # 2. Define Ansatz (Hardware Efficient)
    # 4 Parameters: Ry on each qubit, CNOT, Ry on each qubit
    num_params = 4
    params = [random.uniform(0, 2*math.pi) for _ in range(num_params)]
    
    def ansatz_func(p, q):
        # Layer 1: Ry rotations
        q.applyRotationY(0, p[0])
        q.applyRotationY(1, p[1])
        
        # Entanglement
        q.applyCNOT(0, 1)
        
        # Layer 2: Ry rotations
        q.applyRotationY(0, p[2])
        q.applyRotationY(1, p[3])

    # 3. Optimization Loop (Gradient Descent)
    learning_rate = 0.2
    epochs = 40
    
    print(f"Starting Optimization (LR={learning_rate}, Epochs={epochs})...")
    
    # Helper to calculate energy
    def get_energy(p):
        q = qubit_engine.QuantumRegister(n_qubits)
        ansatz_func(p, q)
        e = 0.0
        for term in hamiltonian:
            val = q.expectation_value(term.pauli_string)
            e += term.coefficient * val
        return e

    for i in range(epochs):
        # Calculate Gradients using C++ Backend (Parallel/Adjoint)
        grads = qubit_engine.QuantumDifferentiator.calculate_gradients(
            n_qubits, params, ansatz_func, hamiltonian
        )
        
        # Update Params
        for j in range(num_params):
            params[j] -= learning_rate * grads[j]
            
        if i % 5 == 0 or i == epochs - 1:
            current_energy = get_energy(params)
            print(f"Epoch {i}: Energy = {current_energy:.6f} Ha")
            
    final_energy = get_energy(params)
    print(f"\nFinal Energy: {final_energy:.6f} Hartrees")
    
    # Check convergence
    exact_energy = -1.137
    error = abs(final_energy - exact_energy)
    if error < 0.05:
        print(f"SUCCESS: Converged to within {error:.4f} of exact energy.")
    else:
        print(f"WARNING: Convergence gap is {error:.4f}. May need more epochs or diff ansatz.")

if __name__ == "__main__":
    main()
