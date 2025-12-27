
import sys
import os

# Add build/ to path to find the module
sys.path.append(os.path.abspath("backend/build"))

try:
    import qubit_engine
    print("Successfully imported qubit_engine")
except ImportError as e:
    print(f"Failed to import qubit_engine: {e}")
    sys.exit(1)

def test_gpu_instantiation():
    print("Attempting to instantiate GPUQuantumRegister...")
    try:
        qreg = qubit_engine.GPUQuantumRegister(2)
        print("Success! (Unexpected if no GPU)")
    except RuntimeError as e:
        print(f"Caught expected RuntimeError: {e}")
        if "GPU not enabled" in str(e) or "No GPU devices" in str(e):
            print("Verified: GPU backend correctly reports unavailability.")
        else:
            print("Warning: unexpected error message.")
    except Exception as e:
        print(f"Caught unexpected Exception: {type(e).__name__}: {e}")

if __name__ == "__main__":
    test_gpu_instantiation()
