import React, { useState } from 'react';
import { CircuitGrid } from './CircuitGrid';
import { GatePalette } from './GatePalette';
import { CircuitRequest, GateOperation, GateOperation_GateType } from '../../generated/quantum';

interface CircuitComposerProps {
    onRun: (req: CircuitRequest) => void;
    onBack: () => void;
}

export const CircuitComposer: React.FC<CircuitComposerProps> = ({ onRun, onBack }) => {
    const [numQubits, setNumQubits] = useState(3);
    const [circuitGrid, setCircuitGrid] = useState<string[][]>([]);

    const handleRun = () => {
        // Convert Grid to GateOperations
        const ops: GateOperation[] = [];

        // Naive conversion: Iterate steps, then qubits (parallelism not fully optimized, serialized for now)
        // Actually, for a simulator, order matters.
        // We scan col by col (step by step)
        if (circuitGrid.length === 0) return;

        const numSteps = circuitGrid[0].length;

        for (let s = 0; s < numSteps; s++) {
            for (let q = 0; q < numQubits; q++) {
                const gateType = circuitGrid[q][s];
                if (!gateType) continue;

                let opType = GateOperation_GateType.HADAMARD; // Default
                switch (gateType) {
                    case 'H': opType = GateOperation_GateType.HADAMARD; break;
                    case 'X': opType = GateOperation_GateType.PAULI_X; break;
                    case 'Z': opType = GateOperation_GateType.ROTATION_Z; break; // Wait, Z is Phase Flip (Z gate), usually PauliZ
                    // My proto has ROTATION_Z and PHASE_S/T. Standard Pauli Z is usually RotZ(pi).
                    // Let's use Pauli Z if available, or Rz(pi).
                    // Checking proto... wait, Pauli Z is missing in my enum in proto? 
                    // Let me check proto content again.
                    // AH, I missed Pauli Z in my memory or proto.
                    // Let's assume ROTATION_Z for now with pi angle if Z is missing, OR check if I added it.
                    // Actually, PHASE_S is Z90, PHASE_T is Z45.
                    // Pauli Z is RotZ(180).

                    case 'S': opType = GateOperation_GateType.PHASE_S; break;
                    case 'T': opType = GateOperation_GateType.PHASE_T; break;
                    case 'CNOT': opType = GateOperation_GateType.CNOT; break;
                    case 'M': opType = GateOperation_GateType.MEASURE; break;
                    default: continue;
                }

                // Handling CNOT: We need a control.
                // In this simple UI, CNOT is just a target block '⊕'.
                // Where is the control '●'?
                // Logic: If we see a CNOT Target at (q, s), search for a Control at (other_q, s).
                if (gateType === 'CNOT') {
                    // Find Control in same column
                    let controlQubit = -1;
                    for (let cq = 0; cq < numQubits; cq++) {
                        if (circuitGrid[cq][s] === 'CTRL') {
                            controlQubit = cq;
                            break;
                        }
                    }

                    if (controlQubit !== -1) {
                        ops.push(GateOperation.create({
                            type: opType,
                            targetQubit: q,
                            controlQubit: controlQubit
                        }));
                    } else {
                        console.warn("CNOT placed without Control qubit in same step!");
                    }
                } else if (gateType === 'Z') {
                    // Z Gate = Rz(pi)
                    ops.push(GateOperation.create({
                        type: GateOperation_GateType.ROTATION_Z, // Ensure we use Rz
                        targetQubit: q,
                        angle: Math.PI
                    }));
                } else {
                    ops.push(GateOperation.create({
                        type: opType,
                        targetQubit: q
                    }));
                }
            }
        }

        const req = CircuitRequest.create({
            numQubits: numQubits,
            operations: ops,
            noiseProbability: 0.0 // Default clear for now
        });

        onRun(req);
    };

    return (
        <div style={styles.layout}>
            <div style={styles.topBar}>
                <button onClick={onBack} style={styles.btnSecondary}>← Back</button>
                <h2 style={{ color: 'white', margin: 0 }}>Circuit Composer</h2>
                <div style={styles.controls}>
                    <label style={{ color: '#aaa', marginRight: '10px' }}>Qubits: {numQubits}</label>
                    <button onClick={() => setNumQubits(Math.max(1, numQubits - 1))} style={styles.btnSmall}>-</button>
                    <button onClick={() => setNumQubits(Math.min(5, numQubits + 1))} style={styles.btnSmall}>+</button>
                    <div style={{ width: '20px' }} />
                    <button onClick={handleRun} style={styles.btnPrimary}>▶ Run Circuit</button>
                </div>
            </div>

            <div style={styles.workspace}>
                <GatePalette />
                <CircuitGrid
                    numQubits={numQubits}
                    numSteps={12}
                    onCircuitChange={setCircuitGrid}
                />
            </div>
        </div>
    );
};

const styles: { [key: string]: React.CSSProperties } = {
    layout: {
        display: 'flex',
        flexDirection: 'column',
        height: '100vh',
        width: '100vw',
        position: 'fixed',
        top: 0,
        left: 0,
        backgroundColor: '#0f172a',
        zIndex: 2000,
    },
    topBar: {
        height: '60px',
        backgroundColor: '#1e293b',
        borderBottom: '1px solid #334155',
        display: 'flex',
        alignItems: 'center',
        padding: '0 20px',
        justifyContent: 'space-between',
    },
    workspace: {
        flex: 1,
        display: 'flex',
        overflow: 'hidden',
    },
    controls: {
        display: 'flex',
        alignItems: 'center',
    },
    btnPrimary: {
        backgroundColor: '#10b981',
        color: 'white',
        border: 'none',
        padding: '8px 20px',
        borderRadius: '6px',
        cursor: 'pointer',
        fontWeight: 'bold',
        fontSize: '1rem',
    },
    btnSecondary: {
        backgroundColor: '#334155',
        color: '#e2e8f0',
        border: 'none',
        padding: '8px 16px',
        borderRadius: '6px',
        cursor: 'pointer',
    },
    btnSmall: {
        backgroundColor: '#475569',
        color: 'white',
        border: 'none',
        width: '30px',
        height: '30px',
        borderRadius: '4px',
        cursor: 'pointer',
        margin: '0 2px',
    },
};
