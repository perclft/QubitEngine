import React, { useState } from 'react';

const steps = [
    {
        title: "Welcome to Qubit Engine!",
        text: "This dashboard allows you to design and simulate quantum circuits in real-time.",
        position: { top: '10%', left: '50%', transform: 'translate(-50%, 0)' }
    },
    {
        title: "The Bloch Sphere",
        text: "The 3D sphere in the center represents the state of a single qubit. The vector moves as you apply gates.",
        position: { top: '60%', left: '50%', transform: 'translate(-50%, -50%)' }
    },
    {
        title: "Control Panel",
        text: "Use the controls on the left to add noise, toggle audio, or switch Modes (Art, Crypto, Chemistry).",
        position: { top: '20%', left: '50px' }
    },
    {
        title: "Run Simulation",
        text: "Click 'Run 1-Qubit Simulation' to send your circuit to the C++ backend via Envoy Proxy.",
        position: { bottom: '100px', left: '20px' }
    }
];

export const TutorialOverlay: React.FC<{ onClose: () => void }> = ({ onClose }) => {
    const [index, setIndex] = useState(0);

    const handleNext = () => {
        if (index < steps.length - 1) {
            setIndex(index + 1);
        } else {
            onClose();
        }
    };

    const current = steps[index];

    return (
        <div style={{
            position: 'fixed', top: 0, left: 0, width: '100vw', height: '100vh',
            pointerEvents: 'none', zIndex: 4000
        }}>
            {/* Dim Background */}
            <div style={{
                position: 'absolute', top: 0, left: 0, width: '100%', height: '100%',
                background: 'rgba(0,0,0,0.5)', pointerEvents: 'auto'
            }} />

            {/* Tooltip Card */}
            <div style={{
                position: 'absolute',
                ...current.position,
                background: 'white', color: 'black', padding: '20px', borderRadius: '8px',
                maxWidth: '300px', boxShadow: '0 0 20px rgba(0,255,255,0.5)', pointerEvents: 'auto',
                animation: 'fadeIn 0.3s ease'
            }}>
                <h3 style={{ marginTop: 0, color: '#2563eb' }}>{current.title}</h3>
                <p style={{ lineHeight: '1.5' }}>{current.text}</p>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '15px' }}>
                    <button onClick={onClose} style={{ border: 'none', background: 'transparent', color: '#666', cursor: 'pointer' }}>
                        Skip
                    </button>
                    <button onClick={handleNext} style={{
                        padding: '6px 12px', background: '#2563eb', color: 'white',
                        border: 'none', borderRadius: '4px', cursor: 'pointer'
                    }}>
                        {index === steps.length - 1 ? "Finish" : "Next"}
                    </button>
                </div>
                <div style={{ marginTop: '10px', fontSize: '0.8rem', color: '#999', textAlign: 'center' }}>
                    Step {index + 1} of {steps.length}
                </div>
            </div>
        </div>
    );
};
