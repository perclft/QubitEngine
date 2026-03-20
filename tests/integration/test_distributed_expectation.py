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

def test_distributed_expectation():
    num_qubits = 4
    reg = qe.QuantumRegister(num_qubits, False) # MPI enabled
    
    rank = reg.getRank()
    size = reg.getSize()
    
    if rank == 0:
        print(f"Verifying Distributed Expectation Value with {size} ranks...")

    # Create a simple |+000> state
    reg.applyHadamard(3)
    
    # Pauli-Z on qubit 3 should have expectation value 0.0 for |+>
    # Pauli-I on others
    pauli_string = "IIIZ"
    exp_val = reg.expectationValue(pauli_string)
    
    if rank == 0:
        print(f"Expectation of {pauli_string}: {exp_val}")
        if abs(exp_val) < 1e-6:
            print(f"SUCCESS: Expectation of {pauli_string} is 0.0 as expected.")
        else:
            print(f"FAILURE: Expectation of {pauli_string} is {exp_val}, expected 0.0")
            return False

    # Apply X on qubit 3 to get |1000>
    # Wait, apply X on qubit 3 after Hadamard gives |-> if starting from |0>?
    # No, Hadamard |0> -> |+>. 
    # Let's just do a simple Z measurement on |0000>
    reg2 = qe.QuantumRegister(num_qubits, False)
    exp_val_z = reg2.expectationValue("ZIII")
    if rank == 0:
        print(f"Expectation of ZIII on |0000>: {exp_val_z}")
        if abs(exp_val_z - 1.0) < 1e-6:
            print("SUCCESS: Expectation of Z on |0> is 1.0")
        else:
            print(f"FAILURE: Expectation of Z on |0> is {exp_val_z}, expected 1.0")
            return False

    # Apply X on qubit 3 to get |1000>
    reg2.applyX(3)
    exp_val_z_1 = reg2.expectationValue("IIIZ")
    if rank == 0:
        print(f"Expectation of IIIZ on |1000>: {exp_val_z_1}")
        if abs(exp_val_z_1 + 1.0) < 1e-6:
            print("SUCCESS: Expectation of Z on |1> is -1.0")
        else:
            print(f"FAILURE: Expectation of Z on |1> is {exp_val_z_1}, expected -1.0")
            return False

    return True

if __name__ == "__main__":
    try:
        if test_distributed_expectation():
            sys.exit(0)
        else:
            sys.exit(1)
    except Exception as e:
        print(f"Test EXCEPTION: {e}")
        sys.exit(1)
    finally:
        qe.finalize()
