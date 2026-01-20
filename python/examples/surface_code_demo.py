
import sys
import os

# Ensure we can import local modules
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../qubit_engine/qec')))

try:
    from qubit_engine.qec.surface_code import SurfaceCode
except ImportError:
    # Fallback if installed
    try:
        from qubit_engine.qec.surface_code import SurfaceCode
    except:
        print("Error: Could not import SurfaceCode. Ensure python bindings are installed.")
        sys.exit(1)

def main():
    print("Initializing Surface Code (d=3)...")
    sc = SurfaceCode()
    
    print("Running initial cycle (Expect No Errors)...")
    syn = sc.run_stabilizer_cycle()
    sc.print_grid(syn)
    
    print("\n[Injecting Error] Applying X on Data Qubit 5 (Center)")
    # D5 is index 4 in our mapping
    sc.qreg.applyX(4) 
    
    print("Running stabilizer cycle...")
    syn = sc.run_stabilizer_cycle()
    sc.print_grid(syn)
    print("\nAnalysis: Z-checks bordering D5 should light up!")

if __name__ == "__main__":
    main()
