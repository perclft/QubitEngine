import grpc
import quantum_pb2
import quantum_pb2_grpc
import os

def run():
    addr = '127.0.0.1:50051'
    channel = grpc.insecure_channel(addr)
    # Re-use the interface generated for python
    # We might need to generate them if they don't exist
    print(f"Connecting to {addr}...")
    
    # Just try a simple health check or RunCircuit if RunVqe is too complex to setup here
    # But let's try to mock the VQERequest
    try:
        stub = quantum_pb2_grpc.QuantumComputeStub(channel)
        req = quantum_pb2.VQERequest(
            molecule=quantum_pb2.VQERequest.BEH2,
            max_iterations=10,
            learning_rate=0.1,
            optimizer_type=quantum_pb2.VQERequest.SPSA
        )
        print("Sending VQE Request...")
        responses = stub.RunVqe(req)
        for resp in responses:
            print(f"Iteration {resp.iteration}: Energy {resp.energy}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    run()
