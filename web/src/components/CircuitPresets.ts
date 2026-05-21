export interface GateEntry {
  id: string;
  type: string;
  label: string;
  target: number;
  control: number;
  angle: number;
}

const generateId = () => `${Date.now()}-${Math.random()}`;

export const getBellState = (): GateEntry[] => [
  { id: generateId(), type: "HADAMARD", label: "H", target: 0, control: 0, angle: 0 },
  { id: generateId(), type: "CNOT", label: "CX", target: 1, control: 0, angle: 0 },
];

export const getGHZState = (numQubits: number): GateEntry[] => {
  const gates: GateEntry[] = [
    { id: generateId(), type: "HADAMARD", label: "H", target: 0, control: 0, angle: 0 }
  ];
  for (let i = 0; i < numQubits - 1; i++) {
    gates.push({
      id: generateId(), type: "CNOT", label: "CX", target: i + 1, control: i, angle: 0
    });
  }
  return gates;
};

export const getQFT3 = (): GateEntry[] => [
  { id: generateId(), type: "HADAMARD", label: "H", target: 0, control: 0, angle: 0 },
  { id: generateId(), type: "PHASE_S", label: "S", target: 0, control: 1, angle: 0 }, // C-S (approx)
  { id: generateId(), type: "PHASE_T", label: "T", target: 0, control: 2, angle: 0 }, // C-T (approx)
  { id: generateId(), type: "HADAMARD", label: "H", target: 1, control: 0, angle: 0 },
  { id: generateId(), type: "PHASE_S", label: "S", target: 1, control: 2, angle: 0 }, // C-S (approx)
  { id: generateId(), type: "HADAMARD", label: "H", target: 2, control: 0, angle: 0 },
  { id: generateId(), type: "SWAP", label: "SW", target: 2, control: 0, angle: 0 },
];

export const getTeleportation = (): GateEntry[] => [
  // Entangle Bob and Alice (Q1 and Q2)
  { id: generateId(), type: "HADAMARD", label: "H", target: 1, control: 0, angle: 0 },
  { id: generateId(), type: "CNOT", label: "CX", target: 2, control: 1, angle: 0 },
  // Prepare Alice's state to teleport (Q0)
  { id: generateId(), type: "ROTATION_X", label: "Rx", target: 0, control: 0, angle: Math.PI / 3 },
  // Alice performs Bell measurement
  { id: generateId(), type: "CNOT", label: "CX", target: 1, control: 0, angle: 0 },
  { id: generateId(), type: "HADAMARD", label: "H", target: 0, control: 0, angle: 0 },
  { id: generateId(), type: "MEASURE", label: "M", target: 0, control: 0, angle: 0 },
  { id: generateId(), type: "MEASURE", label: "M", target: 1, control: 0, angle: 0 },
  // Bob's correction (using classical control - represented as CNOT/CZ here for simplicity)
  { id: generateId(), type: "CNOT", label: "CX", target: 2, control: 1, angle: 0 },
  { id: generateId(), type: "CZ", label: "CZ", target: 2, control: 0, angle: 0 },
];

export const PRESETS = [
  { name: "Bell State", getGates: () => getBellState(), qubits: 2 },
  { name: "GHZ State (3Q)", getGates: () => getGHZState(3), qubits: 3 },
  { name: "QFT (3Q)", getGates: () => getQFT3(), qubits: 3 },
  { name: "Quantum Teleportation", getGates: () => getTeleportation(), qubits: 3 },
];
