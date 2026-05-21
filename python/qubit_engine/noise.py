from typing import Optional
from .core import NoiseModel, ReadoutError

def depolarizing(p1q: float, p2q: float) -> NoiseModel:
    """Create a depolarizing noise model."""
    return NoiseModel.Depolarizing(p1q, p2q)

def realistic(p1q: float, p2q: float, t1_gamma: float, t2_gamma: float, readout_p0g1: float = 0.0, readout_p1g0: float = 0.0) -> NoiseModel:
    """Create a realistic noise model with depolarizing, T1, T2, and readout errors."""
    r_err = ReadoutError(readout_p0g1, readout_p1g0)
    return NoiseModel.Realistic(p1q, p2q, t1_gamma, t2_gamma, r_err)

def ibm_brisbane() -> NoiseModel:
    """Create a noise model configured with median calibration data for IBM Brisbane (127Q)."""
    return NoiseModel.IBMBrisbane()

def google_sycamore() -> NoiseModel:
    """Create a noise model configured with median calibration data for Google Sycamore (53Q)."""
    return NoiseModel.GoogleSycamore()

# We could add from_json here if we bind HardwareConfig to Python, 
# but for now we rely on the C++ preset factories.
