import React from 'react';
import { CircuitRequest, GateOperation, GateOperation_GateType } from '../../generated/quantum';

interface CircuitLibraryProps {
    onLoadCircuit: (req: CircuitRequest) => void;
    onClose: () => void;
}

export const CircuitLibrary: React.FC<CircuitLibraryProps> = ({ onLoadCircuit, onClose }) => {

    const loadBellState = () => {
        // |00> -> (|00> + |11>) / sqrt(2)
        // 1. H on Q0
        // 2. CNOT Q0 -> Q1
        const ops = [
            GateOperation.create({ type: GateOperation_GateType.HADAMARD, targetQubit: 0 }),
            GateOperation.create({ type: GateOperation_GateType.CNOT, controlQubit: 0, targetQubit: 1 })
        ];
        onLoadCircuit(CircuitRequest.create({ numQubits: 2, operations: ops }));
        onClose();
    };

    const loadGHZState = () => {
        // |000> -> (|000> + |111>) / sqrt(2)
        // 1. H on Q0
        // 2. CNOT Q0 -> Q1
        // 3. CNOT Q1 -> Q2
        const ops = [
            GateOperation.create({ type: GateOperation_GateType.HADAMARD, targetQubit: 0 }),
            GateOperation.create({ type: GateOperation_GateType.CNOT, controlQubit: 0, targetQubit: 1 }),
            GateOperation.create({ type: GateOperation_GateType.CNOT, controlQubit: 1, targetQubit: 2 })
        ];
        onLoadCircuit(CircuitRequest.create({ numQubits: 3, operations: ops }));
        onClose();
    };

    const loadSuperposition = () => {
        // |0> -> (|0> + |1>) / sqrt(2)
        const ops = [
            GateOperation.create({ type: GateOperation_GateType.HADAMARD, targetQubit: 0 })
        ];
        onLoadCircuit(CircuitRequest.create({ numQubits: 1, operations: ops }));
        onClose();
    };

    return (
        <div style={{
            position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)',
            background: 'rgba(20, 20, 40, 0.95)', border: '1px solid #00ffff', padding: '20px',
            borderRadius: '10px', zIndex: 2000, minWidth: '300px', color: 'white'
        }}>
            <h2 style={{ borderBottom: '1px solid #444', paddingBottom: '10px' }}>📚 Circuit Library</h2>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', marginTop: '15px' }}>
                <button onClick={loadSuperposition} style={btnStyle}>Superposition (|0&gt; + |1&gt;)</button>
                <button onClick={loadBellState} style={btnStyle}>Bell State (Entanglement)</button>
                <button onClick={loadGHZState} style={btnStyle}>GHZ State (3-Qubit Entanglement)</button>
            </div>
            <button onClick={onClose} style={{ marginTop: '20px', background: 'transparent', border: '1px solid #555', color: '#aaa', cursor: 'pointer', padding: '5px 10px' }}>
                Close
            </button>
        </div>
    );
};

const btnStyle = {
    padding: '12px',
    background: 'linear-gradient(90deg, #3b82f6, #2563eb)',
    border: 'none',
    borderRadius: '6px',
    color: 'white',
    cursor: 'pointer',
    textAlign: 'left' as const,
    fontSize: '1rem'
};
