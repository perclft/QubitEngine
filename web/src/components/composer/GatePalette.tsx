import React from 'react';

// Common gate definitions
export const GATES = [
    { type: 'H', label: 'H', description: 'Hadamard Gate', color: '#6366f1' },
    { type: 'X', label: 'X', description: 'Pauli-X (NOT)', color: '#ef4444' },
    { type: 'Z', label: 'Z', description: 'Pauli-Z (Phase Flip)', color: '#10b981' },
    { type: 'CNOT', label: '⊕', description: 'CNOT (Target)', color: '#f59e0b', controls: 1 },
    { type: 'CTRL', label: '●', description: 'Control Node', color: '#000000', isControl: true },
    { type: 'S', label: 'S', description: 'S Gate (Phase)', color: '#8b5cf6' },
    { type: 'T', label: 'T', description: 'T Gate (Phase)', color: '#ec4899' },
    { type: 'Y', label: 'Y', description: 'Pauli-Y', color: '#ef4444' },
    { type: 'M', label: 'M', description: 'Measure', color: '#6b7280' },
];

export const GatePalette = () => {
    const onDragStart = (event: React.DragEvent, gateType: string) => {
        event.dataTransfer.setData('gateType', gateType);
        event.dataTransfer.effectAllowed = 'copy';
    };

    return (
        <div style={styles.container}>
            <h3 style={styles.header}>Gate Palette</h3>
            <div style={styles.grid}>
                {GATES.map((gate) => (
                    <div
                        key={gate.type}
                        draggable
                        onDragStart={(e) => onDragStart(e, gate.type)}
                        style={{ ...styles.gate, backgroundColor: gate.color }}
                        title={gate.description}
                    >
                        {gate.label}
                    </div>
                ))}
            </div>
        </div>
    );
};

const styles: { [key: string]: React.CSSProperties } = {
    container: {
        width: '200px',
        backgroundColor: 'rgba(30, 41, 59, 0.8)',
        backdropFilter: 'blur(10px)',
        borderRight: '1px solid rgba(255, 255, 255, 0.1)',
        padding: '20px',
        display: 'flex',
        flexDirection: 'column',
    },
    header: {
        marginTop: 0,
        marginBottom: '20px',
        color: '#fff',
        fontSize: '1.2rem',
        textAlign: 'center',
    },
    grid: {
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '15px',
    },
    gate: {
        width: '60px',
        height: '60px',
        borderRadius: '8px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        cursor: 'grab',
        fontSize: '1.5rem',
        fontWeight: 'bold',
        color: 'white',
        boxShadow: '0 4px 6px rgba(0, 0, 0, 0.3)',
        transition: 'transform 0.2s, box-shadow 0.2s',
    },
};
