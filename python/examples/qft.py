import sys
import os
import math

# Add build path
sys.path.append(os.path.join(os.getcwd(), 'python/build'))

try:
    import qubit_engine
except ImportError:
    # Try alternate path if running from python dir
    sys.path.append(os.path.join(os.getcwd(), '../build'))
    import qubit_engine

def apply_controlled_phase(q, c, t, angle):
    """
    Applies Controlled-Phase(angle) = diag(1, 1, 1, e^{i*angle})
    Decomposition using CNOT and Rz:
    CP(theta) ~ Rz(theta/2) on t, CNOT(c,t), Rz(-theta/2) on t, CNOT(c,t), Rz(theta/2) on c
    Note: Matches definition up to global phase.
    """
    q.applyRotationZ(t, angle/2.0)
    q.applyCNOT(c, t)
    q.applyRotationZ(t, -angle/2.0)
    q.applyCNOT(c, t)
    q.applyRotationZ(c, angle/2.0)

def apply_qft(q, n):
    """
    Applies QFT to the first n qubits.
    """
    for i in range(n):
        q.applyHadamard(i)
        for j in range(i + 1, n):
            # Controlled Phase R_k where k = j - i + 1
            # Angle = 2*pi / 2^k = pi / 2^(k-1)
            k = j - i + 1
            angle = math.pi / (2**(k-1))
            apply_controlled_phase(q, j, i, angle)
            
    # Swap qubits for correct order
    for i in range(n // 2):
        # Swap i and n - 1 - i
        a, b = i, n - 1 - i
        q.applyCNOT(a, b)
        q.applyCNOT(b, a)
        q.applyCNOT(a, b)

def main():
    print("--- Quantum Fourier Transform (QFT) Demo ---")
    n = 3
    q = qubit_engine.QuantumRegister(n)
    
    # 1. Test on |000>
    # QFT|0> should be Equal Superposition
    print(f"\nRunning QFT on |{'0'*n}>...")
    apply_qft(q, n)
    state = q.get_state_vector()
    
    print("State Vector (First 4 amplitudes):")
    for i in range(min(4, len(state))):
        print(f"|{i:0{n}b}>: {state[i]}")
        
    # Check if amplitudes are uniform (1/sqrt(8) approx 0.3535)
    amp = abs(state[0])
    expected = 1.0 / math.sqrt(2**n)
    if abs(amp - expected) < 1e-5:
        print(f"SUCCESS: Uniform superposition amplitude |{amp:.4f}| matches expected {expected:.4f}")
    else:
        print(f"FAILURE: Amplitude {amp} != expected {expected}")

    # 2. Inverse QFT? 
    # Not implemented, but QFT(QFT(x)) != x. inverse is needed.
    
if __name__ == "__main__":
    main()
