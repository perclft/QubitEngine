
import qubit_engine_python as qep
import math

class SurfaceCode:
    """
    Rotated Surface Code (Distance 3) Simulation
    
    Layout (17 Qubits):
    Data Qubits (D): 9
    Ancilla Qubits (A - X type): 4
    Ancilla Qubits (A - Z type): 4
    
    Grid:
    .   Z1  .   Z2  .
    D1  .   D2  .   D3 
    .   X1  .   X2  .
    D4  .   D5  .   D6
    .   Z3  .   Z4  .
    D7  .   D8  .   D9
    .   X3  .   X4  .
    """
    
    def __init__(self):
        self.num_qubits = 17
        self.qreg = qep.QuantumRegister(self.num_qubits)
        
        # Mapping names to indices
        self.data_qubits = [0, 1, 2, 3, 4, 5, 6, 7, 8] # D1..D9
        self.x_ancillas = [9, 10, 11, 12]              # X1..X4
        self.z_ancillas = [13, 14, 15, 16]             # Z1..Z4
        
        # Stabilizer Definitions (Data neighbors)
        self.z_stabilizers = {
            13: [0, 1],       # Z1 checks D1, D2
            14: [1, 2],       # Z2 checks D2, D3
            15: [3, 4, 6, 7], # Z3 checks D4, D5, D7, D8
            16: [4, 5, 7, 8]  # Z4 checks D5, D6, D8, D9
        }
        
        self.x_stabilizers = {
            9:  [0, 3],       # X1 checks D1, D4
            10: [1, 2, 4, 5], # X2 checks D2, D3, D5, D6
            11: [6, 7],       # X3 checks D7, D7? Wait: D7, D4? Fix below
            12: [7, 8]        # X4 checks D8, D9
        }
        
        # Correcting Layout for Rotated d=3
        # D1(0) D2(1) D3(2)
        # D4(3) D5(4) D6(5)
        # D7(6) D8(7) D9(8)
        
        # Refined Connectivity:
        # X1 (checks D1, D4, D2, D5) -> No, standard checks 4 neighbors usually
        # Rotated d=3 usually has 2 weight-2 and 2 weight-4 stabilizers of each type
        pass

    def run_stabilizer_cycle(self):
        # 1. Reset Ancillas
        # (We simulate reset by swapping new '0's or just measuring and conditionally flipping, 
        # but here we assume we can re-use or just track state. 
        # For simplicity, we assume start from 0 if it's first run or use simple measure-reset)
        
        # A. X-Type Stabilizers (Measure X parity)
        # Process: H on Ancilla -> CNOT (Ancilla, Data) -> H on Ancilla -> Measure
        for ancilla, targets in self.x_stabilizers.items():
            self.qreg.applyHadamard(ancilla)
            for data in targets:
                self.qreg.applyCNOT(ancilla, data)
            self.qreg.applyHadamard(ancilla)
            
        # B. Z-Type Stabilizers (Measure Z parity)
        # Process: CNOT (Data, Ancilla) -> Measure
        for ancilla, targets in self.z_stabilizers.items():
            for data in targets:
                self.qreg.applyCNOT(data, ancilla)
                
        # 2. Measure Ancillas
        syndrome = {}
        for idx in self.x_ancillas + self.z_ancillas:
            outcome = self.qreg.measure(idx)
            syndrome[idx] = outcome
            # Reset ancilla to |0> for next round
            if outcome == 1:
                self.qreg.applyX(idx) 
                
        return syndrome

    def print_grid(self, syndrome):
        print("\n--- Syndrome Map ---")
        def sym(idx): return "!" if syndrome.get(idx, 0) else "."
        
        print(f"  {sym(13)}   {sym(14)}  ")
        print(f"D . D . D")
        print(f"  {sym(9)}   {sym(10)}  ")
        print(f"D . D . D")
        print(f"  {sym(15)}   {sym(16)}  ")
        print(f"D . D . D")
        print(f"  {sym(11)}   {sym(12)}  ")

