import { ComplexNumber } from "@/components/types";

// Derive Bloch sphere angles (theta, phi) from a multi-qubit state vector
// by computing the single-qubit reduced density matrix via partial trace.
export function stateToBloch(
  stateVector: ComplexNumber[],
  qubit: number,
  numQubits: number
): { theta: number; phi: number } {
  const dim = 1 << numQubits;
  if (!stateVector || stateVector.length !== dim) {
    return { theta: 0, phi: 0 }; // |0⟩
  }

  // Partial trace: ρ_q = Tr_{others}(|ψ⟩⟨ψ|)
  // ρ_q is a 2×2 matrix: [[rho00, rho01], [rho10, rho11]]
  let rho00 = 0, rho01_r = 0, rho01_i = 0, rho11 = 0;
  for (let i = 0; i < dim; i++) {
    const bit_i = (i >> qubit) & 1;
    const ai_r = stateVector[i].real;
    const ai_i = stateVector[i].imag;
    if (bit_i === 0) {
      // Contribution to rho00
      rho00 += ai_r * ai_r + ai_i * ai_i;
      // Find partner index with qubit flipped to 1
      const j = i | (1 << qubit);
      const aj_r = stateVector[j].real;
      const aj_i = stateVector[j].imag;
      // rho01 += conj(a_i) * a_j
      rho01_r += ai_r * aj_r + ai_i * aj_i;
      // Note the sign of imaginary part of conj(a_i) * a_j is positive for real*imag and negative for imag*real
      // conj(ai_r - i * ai_i) * (aj_r + i * aj_i) = (ai_r * aj_r + ai_i * aj_i) + i * (ai_r * aj_i - ai_i * aj_r)
      rho01_i += ai_r * aj_i - ai_i * aj_r;
    } else {
      rho11 += ai_r * ai_r + ai_i * ai_i;
    }
  }

  // Bloch vector: sx = 2*Re(rho01), sy = 2*Im(rho01), sz = rho00 - rho11
  const sx = 2 * rho01_r;
  const sy = 2 * rho01_i;
  const sz = rho00 - rho11;

  // Convert to spherical: theta = arccos(sz), phi = atan2(sy, sx)
  const r = Math.sqrt(sx * sx + sy * sy + sz * sz);
  const theta = r > 1e-9 ? Math.acos(Math.max(-1, Math.min(1, sz / r))) : 0;
  const phi = Math.atan2(sy, sx);

  return { theta, phi };
}
