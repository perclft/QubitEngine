import React, { useState } from 'react';

const questions = [
    {
        question: "What does the Hadamard (H) gate do?",
        options: [
            "Flips the qubit from |0> to |1>",
            "Creates a superposition state",
            "Entangles two qubits",
            "Measures the qubit"
        ],
        answer: 1 // Index of correct answer
    },
    {
        question: "What is Quantum Entanglement?",
        options: [
            "A state where qubits are independent",
            "A physical connection with wires",
            "A correlation where the state of one qubit depends on another",
            "Moving faster than light"
        ],
        answer: 2
    },
    {
        question: "Which gate is the quantum equivalent of the NOT gate?",
        options: [
            "Z Gate",
            "Pauli-X Gate",
            "CNOT Gate",
            "T Gate"
        ],
        answer: 1
    }
];

export const QuizMode: React.FC<{ onClose: () => void }> = ({ onClose }) => {
    const [step, setStep] = useState(0);
    const [score, setScore] = useState(0);
    const [showResult, setShowResult] = useState(false);

    const handleAnswer = (index: number) => {
        if (index === questions[step].answer) {
            setScore(score + 1);
        }

        if (step < questions.length - 1) {
            setStep(step + 1);
        } else {
            setShowResult(true);
        }
    };

    return (
        <div className="quiz-overlay" style={{
            position: 'fixed', top: 0, left: 0, width: '100%', height: '100%',
            background: 'rgba(0,0,0,0.8)', zIndex: 3000, display: 'flex',
            justifyContent: 'center', alignItems: 'center', color: 'white'
        }}>
            <div style={{
                background: '#1f2937', padding: '30px', borderRadius: '15px',
                maxWidth: '500px', width: '90%', border: '1px solid #3b82f6',
                textAlign: 'center'
            }}>
                {!showResult ? (
                    <>
                        <h2 style={{ marginBottom: '20px', color: '#60a5fa' }}>🧠 Quantum Quiz</h2>
                        <div style={{ fontSize: '1.2rem', marginBottom: '20px' }}>
                            {questions[step].question}
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                            {questions[step].options.map((opt, i) => (
                                <button key={i} onClick={() => handleAnswer(i)} style={optionStyle}>
                                    {opt}
                                </button>
                            ))}
                        </div>
                        <div style={{ marginTop: '20px', fontSize: '0.9rem', color: '#9ca3af' }}>
                            Question {step + 1} of {questions.length}
                        </div>
                    </>
                ) : (
                    <>
                        <h2 style={{ fontSize: '2rem', marginBottom: '10px' }}>Quiz Complete!</h2>
                        <div style={{ fontSize: '1.5rem', marginBottom: '20px' }}>
                            You scored: <span style={{ color: '#10b981' }}>{score} / {questions.length}</span>
                        </div>
                        <p style={{ marginBottom: '20px' }}>
                            {score === questions.length ? "🌟 Perfect! You're a Quantum Master!" : "Keep learning, you're doing great!"}
                        </p>
                        <button onClick={onClose} style={closeStyle}>
                            Back to Dashboard
                        </button>
                    </>
                )}
            </div>
        </div>
    );
};

const optionStyle = {
    padding: '15px',
    background: '#374151',
    border: '1px solid #4b5563',
    borderRadius: '8px',
    color: 'white',
    cursor: 'pointer',
    fontSize: '1rem',
    transition: 'background 0.2s'
};

const closeStyle = {
    padding: '10px 20px',
    background: '#2563eb',
    color: 'white',
    border: 'none',
    borderRadius: '6px',
    cursor: 'pointer',
    fontSize: '1.1rem'
};
