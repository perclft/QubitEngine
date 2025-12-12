import React, { useState } from 'react';
import { GATES } from './GatePalette';

interface CircuitGridProps {
    numQubits: number;
    numSteps: number;
    onCircuitChange: (grid: string[][]) => void;
}

export const CircuitGrid: React.FC<CircuitGridProps> = ({ numQubits, numSteps, onCircuitChange }) => {
    // Grid State: [qubitIndex][stepIndex] = gateType string or null
    const [grid, setGrid] = useState<string[][]>(
        Array(numQubits).fill(null).map(() => Array(numSteps).fill(null))
    );

    const onDragOver = (event: React.DragEvent) => {
        event.preventDefault();
        event.dataTransfer.dropEffect = 'copy';
    };

    const onDrop = (event: React.DragEvent, qubitIdx: number, stepIdx: number) => {
        event.preventDefault();
        const gateType = event.dataTransfer.getData('gateType');

        const newGrid = grid.map(row => [...row]);
        newGrid[qubitIdx][stepIdx] = gateType;

        // Simple CNOT logic: If placing CNOT Target (⊕), we need a Control? 
        // For simplicity in this demo, we just place the icon. 
        // Real validation would go here.

        setGrid(newGrid);
        onCircuitChange(newGrid);
    };

    const clearCell = (qubitIdx: number, stepIdx: number) => {
        const newGrid = grid.map(row => [...row]);
        newGrid[qubitIdx][stepIdx] = null as any; // Cast to satisfy simple string[][] assuming null is mostly empty string in logic
        // Actually let's use check:
        setGrid(newGrid);
        onCircuitChange(newGrid);
    };

    return (
        <div style={styles.container}>
            {grid.map((row, qIdx) => (
                <div key={qIdx} style={styles.wireContainer}>
                    <div style={styles.qubitLabel}>Q{qIdx}</div>
                    <div style={styles.wireLine} />
                    {row.map((cell, sIdx) => {
                        const gateDef = GATES.find(g => g.type === cell);
                        return (
                            <div
                                key={sIdx}
                                style={{
                                    ...styles.slot,
                                    backgroundColor: gateDef ? gateDef.color : 'transparent',
                                    border: gateDef ? 'none' : '1px dashed rgba(255,255,255,0.2)'
                                }}
                                onDragOver={onDragOver}
                                onDrop={(e) => onDrop(e, qIdx, sIdx)}
                                onClick={() => cell && clearCell(qIdx, sIdx)}
                                title={cell ? `Click to remove ${cell}` : 'Drop gate here'}
                            >
                                {gateDef ? gateDef.label : ''}
                            </div>
                        );
                    })}
                </div>
            ))}
        </div>
    );
};

const styles: { [key: string]: React.CSSProperties } = {
    container: {
        flex: 1,
        backgroundColor: 'rgba(15, 23, 42, 0.9)',
        padding: '40px',
        overflowX: 'auto',
        display: 'flex',
        flexDirection: 'column',
        gap: '20px',
    },
    wireContainer: {
        display: 'flex',
        alignItems: 'center',
        position: 'relative',
        height: '60px',
    },
    qubitLabel: {
        color: '#94a3b8',
        fontWeight: 'bold',
        marginRight: '20px',
        width: '30px',
    },
    wireLine: {
        position: 'absolute',
        left: '50px',
        right: 0,
        top: '50%',
        height: '2px',
        backgroundColor: '#475569',
        zIndex: 0,
    },
    slot: {
        width: '50px',
        height: '50px',
        marginLeft: '10px',
        borderRadius: '4px',
        zIndex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: 'white',
        fontWeight: 'bold',
        fontSize: '1.2rem',
        cursor: 'pointer',
        transition: 'all 0.2s',
    },
};
