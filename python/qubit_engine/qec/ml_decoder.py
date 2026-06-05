import torch
import numpy as np

# Try to import torch_geometric; if missing, fail gracefully.
try:
    from torch_geometric.data import Data
    HAS_PYG = True
except ImportError:
    HAS_PYG = False

class MLDecoder:
    """
    ML Decoder interface for QubitEngine using PyTorch Geometric.
    
    This converts zero-copy syndrome defect tensors from the C++ core into PyG 
    Data objects that can be natively batched and passed into Graph Neural Networks.
    """
    def __init__(self, code, distance):
        """
        Args:
            code: The QubitEngine SurfaceCode or ColorCode instance.
            distance: The distance of the code.
        """
        self.code = code
        self.distance = distance
        
        if not HAS_PYG:
            raise ImportError("torch_geometric is required for MLDecoder. Please pip install torch_geometric.")

    def extract_syndrome_graph(self, noise_probability: float) -> Data:
        """
        Extracts syndromes from the underlying code and returns a PyG Data object.
        
        Returns:
            torch_geometric.data.Data: A graph where nodes are defects.
                - x: Node features [type, x, y, time]
                - edge_index: Edges connecting nodes (fully connected for attention/transformer models, 
                              or sparsified based on Chebyshev distance).
        """
        # zero-copy NumPy array of shape [N, 5]: [id, type, x, y, time]
        defects_array = self.code.extract_syndromes_tensor(noise_probability)
        
        N = defects_array.shape[0]
        if N == 0:
            # Return empty graph if no defects
            return Data(x=torch.empty((0, 4), dtype=torch.float), 
                        edge_index=torch.empty((2, 0), dtype=torch.long))
        
        # We don't need 'id' as a feature, so slice [:, 1:]
        # Node features: type, x, y, time
        x_tensor = torch.from_numpy(defects_array[:, 1:]).float()
        
        # Build edges. 
        # For a GNN, we can connect defects of the same type that are close to each other.
        # Alternatively, we could connect all nodes and let a Transformer figure it out.
        # Let's create edges between all defects of the SAME type (to emulate MWPM matching constraints).
        
        edge_indices = []
        edge_attr = []
        
        types = defects_array[:, 1]
        for i in range(N):
            for j in range(i + 1, N):
                if types[i] == types[j]:
                    # Edge exists
                    edge_indices.append([i, j])
                    edge_indices.append([j, i])
                    
                    # Compute Chebyshev distance for edge attribute
                    dx = abs(defects_array[i, 2] - defects_array[j, 2])
                    dy = abs(defects_array[i, 3] - defects_array[j, 3])
                    dt = abs(defects_array[i, 4] - defects_array[j, 4])
                    dist = max(dx, dy) + dt
                    
                    edge_attr.append([dist])
                    edge_attr.append([dist])
                    
        if len(edge_indices) > 0:
            edge_index_tensor = torch.tensor(edge_indices, dtype=torch.long).t().contiguous()
            edge_attr_tensor = torch.tensor(edge_attr, dtype=torch.float)
        else:
            edge_index_tensor = torch.empty((2, 0), dtype=torch.long)
            edge_attr_tensor = torch.empty((0, 1), dtype=torch.float)
            
        return Data(x=x_tensor, edge_index=edge_index_tensor, edge_attr=edge_attr_tensor)
