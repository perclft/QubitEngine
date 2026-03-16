import math
import sys
import os

# Try to find the built core module
try:
    import core as qe
except ImportError:
    # Local dev fallback
    sys.path.append(os.path.join(os.getcwd(), 'bin'))
    import core as qe

def test_mpi_sharding():
    num_qubits = 4
    
    # We use force_local=False to ensure it can use MPI backends
    reg = qe.QuantumRegister(num_qubits, False)
    
    rank = reg.getRank()
    size = reg.getSize()
    
    if size != 2:
        if rank == 0:
            print(f"SKIPPING: Test requires exactly 2 MPI ranks, found {size}")
        return True

    if rank == 0:
        print(f"Starting MPI Sharding Integration Test with {size} ranks...")

    reg.applyHadamard(3)
    
    state = reg.getStateVector()
    inv_sqrt2 = 1.0 / math.sqrt(2.0)
    tol = 1e-5

    # state[0] is complex. abs() in Python returns the magnitude.
    if rank == 0:
        if abs(state[0] - inv_sqrt2) < tol:
            print("[Rank 0] Hadamard state verified.")
        else:
            print(f"FAILED: Rank 0: Expected {inv_sqrt2}, got {state[0]}")
            return False
    else:
        if abs(state[0] - inv_sqrt2) < tol:
            print("[Rank 1] Hadamard state verified.")
        else:
            print(f"FAILED: Rank 1: Expected {inv_sqrt2}, got {state[0]}")
            return False

    reg.applyCNOT(3, 0)
    
    state = reg.getStateVector()
    
    if rank == 0:
        if abs(state[0] - inv_sqrt2) < tol:
            print("[Rank 0] CNOT state verified.")
        else:
            print(f"FAILED: Rank 0: |0000> amplitude mismatch. Got {state[0]}")
            return False
    else:
        if abs(state[0]) < tol and abs(state[1] - inv_sqrt2) < tol:
            print("[Rank 1] CNOT state verified.")
        else:
            print(f"FAILED: Rank 1: CNOT state mismatch. state[0]={state[0]}, state[1]={state[1]}")
            return False

    if rank == 0:
        print("MPI Sharding Integration Test: PASSED")
    
    return True

    if rank == 0:
        print("MPI Sharding Integration Test: PASSED")
    
    return True

if __name__ == "__main__":
    try:
        if test_mpi_sharding():
            sys.exit(0)
        else:
            sys.exit(1)
    except Exception as e:
        print(f"Test EXCEPTION: {e}")
        sys.exit(1)
    finally:
        qe.finalize()
