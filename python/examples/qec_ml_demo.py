import os
import sys

# Ensure QubitEngine bindings can be imported
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "bin", "Release")))
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

try:
    import qubit_engine.core as qe_core
    from qubit_engine.qec.ml_decoder import MLDecoder
    import torch_geometric
    import torch
    HAS_DEPS = True
except ImportError as e:
    print(f"Error importing dependencies: {e}")
    HAS_DEPS = False

def run_ml_decoder_demo():
    print("==================================================")
    print(" QubitEngine ML Decoder Integration Demo")
    print("==================================================")
    
    if not HAS_DEPS:
        print("Cannot run demo due to missing dependencies.")
        return

    # Initialize a Distance 5 Rotated Surface Code
    distance = 5
    print(f"\n[1] Initializing Distance {distance} Rotated Surface Code...")
    surface_code = qe_core.SurfaceCode(distance)
    
    # Wrap it in our ML Decoder interface
    ml_decoder = MLDecoder(surface_code, distance)
    print("[2] MLDecoder initialized.")
    
    # Extract a syndrome graph with some moderate noise
    noise_prob = 0.05
    print(f"\n[3] Extracting syndromes with depolarizing noise p = {noise_prob}...")
    graph = ml_decoder.extract_syndrome_graph(noise_prob)
    
    print("\n[4] PyTorch Geometric Graph Generated:")
    print(graph)
    
    print(f"  - Number of nodes (defects): {graph.num_nodes}")
    if graph.num_nodes > 0:
        print(f"  - Node feature shape (Type, X, Y, Time): {graph.x.shape}")
        print(f"  - Number of edges: {graph.num_edges}")
        if graph.num_edges > 0:
            print(f"  - Edge feature shape (Chebyshev Distance): {graph.edge_attr.shape}")
        
        # Show a few nodes
        print("\n  Sample Node Features:")
        for i in range(min(5, graph.num_nodes)):
            t, x, y, time = graph.x[i].tolist()
            type_str = "Z-Defect" if t == 1 else "X-Defect"
            print(f"    Node {i}: {type_str} at (X:{x}, Y:{y}, Time:{time})")
    
    print("\n[5] Example GNN Processing:")
    try:
        from torch_geometric.nn import GCNConv
        
        # Simple 2-layer GCN to process the defect graph
        class DummyGNN(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv1 = GCNConv(4, 16)
                self.conv2 = GCNConv(16, 8)
                
            def forward(self, data):
                x, edge_index = data.x, data.edge_index
                x = self.conv1(x, edge_index)
                x = torch.relu(x)
                x = self.conv2(x, edge_index)
                return x
        
        model = DummyGNN()
        if graph.num_nodes > 0 and graph.num_edges > 0:
            out = model(graph)
            print("  Successfully passed syndrome graph through GNN!")
            print(f"  Output feature matrix shape: {out.shape}")
        else:
            print("  Graph is empty (no defects) or has no edges; skipping GNN pass.")
    except Exception as e:
        print(f"  Error running GNN: {e}")
        
    print("\nDemo completed successfully!")

if __name__ == "__main__":
    run_ml_decoder_demo()
