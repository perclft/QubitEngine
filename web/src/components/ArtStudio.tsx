import { useState, useRef } from 'react';
import { Canvas } from '@react-three/fiber';
import { OrbitControls } from '@react-three/drei';
import { BlochSphere } from './BlochSphere';
import { GenerativeCanvas } from './GenerativeCanvas';
import { QuantumAudio } from './QuantumAudio';
import { ChaosDice } from './art/ChaosDice';
import { MatterSculpt } from './art/MatterSculpt';
import { SoundWaves } from './art/SoundWaves';
import './ArtStudio.css';

// Art mode definitions
type ArtMode = 'chaos' | 'matter' | 'sound' | 'classic';

interface ModeConfig {
    id: ArtMode;
    name: string;
    icon: string;
    description: string;
}

const MODES: ModeConfig[] = [
    { id: 'chaos', name: 'Chaos Dice', icon: '🎲', description: 'Quantum random dice with visual bursts' },
    { id: 'matter', name: 'Matter Sculpt', icon: '⚛️', description: 'VQE molecular visualization' },
    { id: 'sound', name: 'Sound Waves', icon: '🎵', description: 'Quantum music as flowing ribbons' },
    { id: 'classic', name: 'Classic', icon: '🌀', description: 'Bloch sphere simulation' },
];

interface ArtStudioProps {
    onBack: () => void;
}

export function ArtStudio({ onBack }: ArtStudioProps) {
    const [activeMode, setActiveMode] = useState<ArtMode>('classic');
    const [isRecording, setIsRecording] = useState(false);
    const [audioEnabled, setAudioEnabled] = useState(false);
    const [entropy, setEntropy] = useState(0.1);
    const [effectsEnabled, setEffectsEnabled] = useState(true);

    // Quantum state for visualization
    const [q0State, setQ0State] = useState({ real: 1, imag: 0 });
    const [amplitude1, setAmplitude1] = useState({ real: 0, imag: 0 });
    const [step, setStep] = useState(0);

    // Recording refs
    const canvasRef = useRef<HTMLDivElement>(null);
    const mediaRecorderRef = useRef<MediaRecorder | null>(null);
    const recordedChunksRef = useRef<Blob[]>([]);

    // Demo: Animate quantum state
    const runDemoAnimation = () => {
        let frame = 0;
        const animate = () => {
            frame++;
            const t = frame * 0.05;

            // Simulate quantum state evolution
            const phase = t;
            const amp = Math.cos(t * 0.3);

            setQ0State({
                real: Math.cos(phase) * Math.cos(amp),
                imag: Math.sin(phase) * Math.cos(amp)
            });
            setAmplitude1({
                real: Math.cos(phase + Math.PI / 2) * Math.sin(amp),
                imag: Math.sin(phase + Math.PI / 2) * Math.sin(amp)
            });
            setStep(frame);

            if (frame < 200) {
                requestAnimationFrame(animate);
            }
        };
        animate();
    };

    // Recording functions
    const startRecording = async () => {
        if (!canvasRef.current) return;

        try {
            const canvas = canvasRef.current.querySelector('canvas');
            if (!canvas) return;

            const stream = canvas.captureStream(30);
            const recorder = new MediaRecorder(stream, { mimeType: 'video/webm' });

            recordedChunksRef.current = [];
            recorder.ondataavailable = (e) => {
                if (e.data.size > 0) {
                    recordedChunksRef.current.push(e.data);
                }
            };

            recorder.onstop = () => {
                const blob = new Blob(recordedChunksRef.current, { type: 'video/webm' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `quantum-art-${Date.now()}.webm`;
                a.click();
            };

            mediaRecorderRef.current = recorder;
            recorder.start();
            setIsRecording(true);
        } catch (err) {
            console.error('Recording failed:', err);
        }
    };

    const stopRecording = () => {
        if (mediaRecorderRef.current && isRecording) {
            mediaRecorderRef.current.stop();
            setIsRecording(false);
        }
    };

    const renderModeContent = () => {
        switch (activeMode) {
            case 'chaos':
                return <ChaosDice onRollResult={(val) => console.log('Rolled:', val)} />;
            case 'matter':
                return <MatterSculpt onEnergyUpdate={(e) => console.log('Energy:', e)} />;
            case 'sound':
                return <SoundWaves onNotePlay={(n) => console.log('Note:', n)} />;
            case 'classic':
            default:
                return null; // Bloch sphere shown by default
        }
    };

    return (
        <div className="art-studio">
            {/* Header */}
            <header className="art-studio-header">
                <div style={{ display: 'flex', alignItems: 'center' }}>
                    <h1 className="art-studio-title">QUANTUM ART STUDIO</h1>
                    <span className="art-studio-subtitle">Create • Visualize • Export</span>
                </div>

                {/* Mode Tabs */}
                <div className="mode-tabs">
                    {MODES.map((mode) => (
                        <button
                            key={mode.id}
                            className={`mode-tab ${activeMode === mode.id ? 'active' : ''}`}
                            onClick={() => setActiveMode(mode.id)}
                        >
                            <span className="mode-tab-icon">{mode.icon}</span>
                            {mode.name}
                        </button>
                    ))}
                </div>

                {/* Header Controls */}
                <div className="header-controls">
                    <button
                        className={`control-btn ${audioEnabled ? 'active' : ''}`}
                        onClick={() => setAudioEnabled(!audioEnabled)}
                    >
                        {audioEnabled ? '🔊' : '🔇'} Audio
                    </button>
                    <button
                        className={`control-btn ${isRecording ? 'recording' : ''}`}
                        onClick={isRecording ? stopRecording : startRecording}
                    >
                        {isRecording ? '⏹️ Stop' : '🔴 Record'}
                    </button>
                    <button className="control-btn" onClick={onBack}>
                        ← Dashboard
                    </button>
                </div>
            </header>

            {/* Main Canvas Area */}
            <div className="art-canvas-container" ref={canvasRef}>
                {/* Side Panel */}
                <div className="side-panel">
                    <div className="panel-section">
                        <div className="panel-title">Controls</div>

                        <div className="slider-container">
                            <div className="slider-label">
                                <span>Entropy</span>
                                <span className="slider-value">{entropy.toFixed(2)}</span>
                            </div>
                            <input
                                type="range"
                                className="art-slider"
                                min="0"
                                max="0.5"
                                step="0.01"
                                value={entropy}
                                onChange={(e) => setEntropy(parseFloat(e.target.value))}
                            />
                        </div>

                        <div className="toggle-container">
                            <span className="toggle-label">Visual Effects</span>
                            <div
                                className={`toggle-switch ${effectsEnabled ? 'active' : ''}`}
                                onClick={() => setEffectsEnabled(!effectsEnabled)}
                            />
                        </div>

                        <div className="toggle-container">
                            <span className="toggle-label">Audio Synthesis</span>
                            <div
                                className={`toggle-switch ${audioEnabled ? 'active' : ''}`}
                                onClick={() => setAudioEnabled(!audioEnabled)}
                            />
                        </div>
                    </div>

                    <div className="panel-section">
                        <div className="panel-title">Stats</div>
                        <div className="stats-display">
                            <div className="stat-item">
                                <div className="stat-value">{step}</div>
                                <div className="stat-label">Frame</div>
                            </div>
                            <div className="stat-item">
                                <div className="stat-value">{activeMode.toUpperCase()}</div>
                                <div className="stat-label">Mode</div>
                            </div>
                            <div className="stat-item">
                                <div className="stat-value">{(Math.abs(q0State.real) ** 2 + Math.abs(q0State.imag) ** 2).toFixed(2)}</div>
                                <div className="stat-label">|α|²</div>
                            </div>
                            <div className="stat-item">
                                <div className="stat-value">{(Math.abs(amplitude1.real) ** 2 + Math.abs(amplitude1.imag) ** 2).toFixed(2)}</div>
                                <div className="stat-label">|β|²</div>
                            </div>
                        </div>
                    </div>

                    <div className="panel-section" style={{ flex: 1 }}>
                        <div className="panel-title">Actions</div>
                        <button className="action-btn primary" onClick={runDemoAnimation}>
                            ▶️ Run Animation
                        </button>
                        <button className="action-btn secondary" style={{ marginTop: '10px' }}>
                            📷 Screenshot
                        </button>
                    </div>
                </div>

                {/* 3D Canvas */}
                <Canvas camera={{ position: [3, 2, 5] }}>
                    <ambientLight intensity={0.4} />
                    <pointLight position={[10, 10, 10]} intensity={1} />
                    <pointLight position={[-10, -10, -10]} intensity={0.5} color="#ff00ff" />
                    <BlochSphere
                        amplitude0={q0State}
                        amplitude1={amplitude1}
                        enableEffects={effectsEnabled}
                    />
                    <OrbitControls enableDamping dampingFactor={0.05} />
                    <gridHelper args={[10, 10, '#333', '#222']} />
                </Canvas>

                {/* Mode-specific overlay content */}
                {activeMode !== 'classic' && (
                    <div className="mode-content">
                        {renderModeContent()}
                    </div>
                )}
            </div>

            {/* Generative particle canvas */}
            <GenerativeCanvas amplitude0={q0State} amplitude1={amplitude1} />

            {/* Audio synthesis */}
            <QuantumAudio amplitude0={q0State} amplitude1={amplitude1} isActive={audioEnabled} />
        </div>
    );
}
