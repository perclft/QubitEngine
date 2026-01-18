import sys
import os

# Add build directory to python path
sys.path.append(os.path.join(os.getcwd(), 'python/build'))

try:
    import qubit_engine
    print(f"Successfully imported qubit_engine: {qubit_engine}")
    
    # Create Register
    n = 2
    q = qubit_engine.QuantumRegister(n)
    print(f"Created QuantumRegister with {n} qubits")
    
    # Apply Gates
    q.applyHadamard(0)
    q.applyCNOT(0, 1)
    
    # Check State
    state = q.get_state_vector()
    print("State vector after H(0), CNOT(0,1):")
    for i, amp in enumerate(state):
        print(f"|{i:0{n}b}>: {amp}")
        
    print("Python Bindings Verification Passed!")
    
except ImportError as e:
    print(f"Failed to import qubit_engine: {e}")
    sys.exit(1)
except Exception as e:
    print(f"Runtime error: {e}")
    sys.exit(1)
