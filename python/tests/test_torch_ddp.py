import os
import sys
import pytest
import math

# Ensure we can find torch and distributed extensions
torch = pytest.importorskip("torch")
dist = pytest.importorskip("torch.distributed")
mp = pytest.importorskip("torch.multiprocessing")

# Ensure the build directory is in the path to find the module
build_dir = os.path.join(os.path.dirname(__file__), "../../backend/build")
sys.path.append(build_dir)
sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

from torch_quantum import QuantumLayer
from torch.nn.parallel import DistributedDataParallel as DDP

def setup_ddp(rank, world_size):
    store_file = "C:/Users/percl/projects/QubitEngine/ddp_shared_store"
    if rank == 0:
        if os.path.exists(store_file):
            try:
                os.remove(store_file)
            except OSError:
                pass
    # Wait a tiny moment for rank 0 to finish deletion
    import time
    time.sleep(0.1)
    store = dist.FileStore(store_file, world_size)
    os.environ['GLOO_SOCKET_IFNAME'] = 'Loopback Pseudo-Interface 1'
    # Use gloo backend as it supports CPU and runs reliably across all OS platforms
    dist.init_process_group("gloo", store=store, rank=rank, world_size=world_size)

def cleanup_ddp():
    if dist.is_initialized():
        dist.destroy_process_group()

def run_ddp_worker(rank, world_size, num_qubits, num_params, result_queue):
    try:
        setup_ddp(rank, world_size)
        
        # Simple parameterized ansatz for expectation value
        def ansatz(p, x, qreg):
            qreg.applyRotationY(0, p[0] + x[0])
            qreg.applyRotationY(1, p[1] + x[1])
            qreg.applyCNOT(0, 1)

        hamiltonian = [(1.0, "Z0"), (1.0, "Z1")]

        # Create QuantumLayer
        layer = QuantumLayer(num_qubits, num_params, hamiltonian, ansatz, diff_method="adjoint")
        
        # Make initial weights identical across processes for consistent testing
        torch.manual_seed(42)
        with torch.no_grad():
            layer.params.copy_(torch.tensor([0.5, -0.5], dtype=torch.float64))
            
        # Wrap in PyTorch DDP
        ddp_model = DDP(layer)

        # Feed different inputs to simulate different batch pieces per process
        if rank == 0:
            inputs = torch.tensor([[0.1, 0.2]], dtype=torch.float64)
        else:
            inputs = torch.tensor([[-0.1, -0.2]], dtype=torch.float64)

        # Forward, backward passes
        output = ddp_model(inputs)
        loss = output.sum()
        loss.backward()

        # Capture gradients and parameters to send to parent process
        grad = layer.params.grad.clone().detach()
        param_val = layer.params.clone().detach()

        result_queue.put((rank, grad, param_val))

    except Exception as e:
        import traceback
        result_queue.put((rank, e, traceback.format_exc()))
    finally:
        cleanup_ddp()

def test_pytorch_ddp_integration():
    world_size = 2
    num_qubits = 2
    num_params = 2

    ctx = mp.get_context('spawn')
    result_queue = ctx.Queue()

    processes = []
    for rank in range(world_size):
        p = ctx.Process(
            target=run_ddp_worker,
            args=(rank, world_size, num_qubits, num_params, result_queue)
        )
        p.start()
        processes.append(p)

    for p in processes:
        p.join()

    # Retrieve all process results
    results = {}
    for _ in range(world_size):
        rank, grad, param_val = result_queue.get()
        if isinstance(grad, Exception):
            err_msg = str(grad)
            if "unsupported gloo device" in err_msg or "makeDevice" in err_msg:
                pytest.skip("Gloo distributed process group is unsupported or buggy on this Windows host.")
            # Print traceback from worker
            print(f"Worker {rank} failed:\n{param_val}")
            pytest.fail(f"Worker {rank} failed with exception: {grad}")
        results[rank] = (grad, param_val)

    # Verify identical gradients synchronized via DDP
    grad0, param0 = results[0]
    grad1, param1 = results[1]

    # PyTorch DDP averages gradients across workers, so they must be equal
    assert torch.allclose(grad0, grad1, atol=1e-5), f"DDP gradients did not synchronize: {grad0} vs {grad1}"
    print("DDP gradient verification passed successfully.")
