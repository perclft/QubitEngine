
import sys
import os

# Ensure the build directory is in the path to find the module
# Adjust path as necessary
build_dir = os.path.join(os.path.dirname(__file__), "../build")
sys.path.append(build_dir)

try:
    import qubit_engine
    print("Successfully imported qubit_engine module!")
except ImportError as e:
    print(f"Failed to import qubit_engine: {e}")
    sys.exit(1)

def test_basic_circuit():
    print("\n--- Testing Basic Circuit ---")
    # 2 Qubits
    q = qubit_engine.QuantumRegister(2)
    
    # H(0)
    print("Applying Hadamard on 0...")
    q.applyHadamard(0)
    
    # CNOT(0, 1)
    print("Applying CNOT(0, 1)...")
    q.applyCNOT(0, 1)
    
    # Check Probabilities
    # Should be 0.5 for |00> and 0.5 for |11>
    probs = q.getProbabilities() # Note: C++ implementation returned {} in the deep dive, need to check if implemented
    state = q.getStateVector()
    
    print(f"State Vector size: {len(state)}")
    # |00> is index 0, |11> is index 3
    amp00 = state[0]
    amp11 = state[3]
    
    print(f"Amplitude |00>: {amp00}")
    print(f"Amplitude |11>: {amp11}")

    # Check approximate equality using squared magnitude
    p00 = abs(amp00)**2
    p11 = abs(amp11)**2
    
    if abs(p00 - 0.5) < 1e-5 and abs(p11 - 0.5) < 1e-5:
        print("PASS: Bell State Created Successfully")
    else:
        print(f"FAIL: Unexpected probabilities: P(00)={p00}, P(11)={p11}")

def test_gradients():
    print("\n--- Testing Gradients (Parameter Shift) ---")
    import math
    
    num_qubits = 1
    theta = math.pi / 3.0 # 60 deg
    params = [theta]
    
    # Ansatz: Ry(theta) on q0
    def ansatz(p, qreg):
        qreg.applyRotationY(0, p[0])
        
    # Hamiltonian: 1.0 * Z
    hamiltonian = [(1.0, "Z")]
    
    # Calculate Gradient
    grads = qubit_engine.calculate_gradients(num_qubits, params, ansatz, hamiltonian)
    
    expected = -math.sin(theta)
    calculated = grads[0]
    
    print(f"Calculated Gradient: {calculated}")
    print(f"Expected Gradient: {expected}")
    
    if abs(calculated - expected) < 1e-5:
        print("PASS: Gradient calculation correct")
    else:
        print("FAIL: Gradient mismatch")

def test_mps_backend():
    print("\n--- Testing MPS Backend via environment variables ---")
    os.environ["QUBIT_MPS_THRESHOLD"] = "2"
    os.environ["QUBIT_MPS_BOND_DIMENSION"] = "16"
    
    q = qubit_engine.QuantumRegister(3)
    
    print("Applying Hadamard on 0...")
    q.applyHadamard(0)
    print("Applying CNOT(0, 2)...")
    q.applyCNOT(0, 2)
    
    probs = q.getProbabilities()
    state = q.getStateVector()
    
    print(f"MPS Probabilities: {probs}")
    print(f"MPS State Vector size: {len(state)}")
    
    # Assert correct probabilities of non-adjacent CNOT(0, 2)
    p000 = probs[0]
    p101 = probs[5]
    print(f"MPS P(000): {p000}")
    print(f"MPS P(101): {p101}")
    
    if abs(p000 - 0.5) < 1e-5 and abs(p101 - 0.5) < 1e-5:
        print("PASS: MPS Non-Adjacent CNOT Correct")
    else:
        print("FAIL: MPS Non-Adjacent CNOT Incorrect")
        sys.exit(1)

if __name__ == "__main__":
    test_basic_circuit()
    test_gradients()
    test_mps_backend()
