import { useState, useRef, useEffect, useMemo } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, Line } from '@react-three/drei';
import * as THREE from 'three';
import './SoundWaves.css';

// Musical scales
const SCALES = {
    major: [0, 2, 4, 5, 7, 9, 11],
    minor: [0, 2, 3, 5, 7, 8, 10],
    pentatonic: [0, 2, 4, 7, 9],
    blues: [0, 3, 5, 6, 7, 10],
};

const MOODS = [
    { id: 'calm', name: 'Calm', baseFreq: 220, color: '#4ecdc4' },
    { id: 'energetic', name: 'Energetic', baseFreq: 440, color: '#ff6b6b' },
    { id: 'dreamy', name: 'Dreamy', baseFreq: 330, color: '#8b5cf6' },
    { id: 'mysterious', name: 'Mysterious', baseFreq: 294, color: '#ffd93d' },
];

interface WaveRibbonProps {
    frequency: number;
    amplitude: number;
    phase: number;
    color: string;
    position: [number, number, number];
}

// 3D Wave ribbon component
function WaveRibbon({ frequency, amplitude, phase, color, position }: WaveRibbonProps) {
    const points = useMemo(() => {
        const pts: THREE.Vector3[] = [];
        for (let i = 0; i <= 100; i++) {
            const x = (i - 50) * 0.1;
            const y = Math.sin(x * frequency + phase) * amplitude;
            const z = Math.cos(x * frequency * 0.5 + phase) * amplitude * 0.3;
            pts.push(new THREE.Vector3(x, y, z));
        }
        return pts;
    }, [frequency, amplitude, phase]);

    return (
        <group position={position}>
            <Line
                points={points}
                color={color}
                lineWidth={3}
                transparent
                opacity={0.8}
            />
        </group>
    );
}

// Floating note particle
function NoteParticle({ position, color, size }: { position: [number, number, number]; color: string; size: number }) {
    const meshRef = useRef<THREE.Mesh>(null);

    useFrame((state) => {
        if (meshRef.current) {
            meshRef.current.position.y = position[1] + Math.sin(state.clock.elapsedTime * 2 + position[0]) * 0.3;
            meshRef.current.rotation.z = state.clock.elapsedTime;
        }
    });

    return (
        <mesh ref={meshRef} position={position}>
            <octahedronGeometry args={[size, 0]} />
            <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.5} />
        </mesh>
    );
}

// Animated frequency visualizer
function FrequencyBars({ frequencies }: { frequencies: number[] }) {
    return (
        <group position={[0, -2, 0]}>
            {frequencies.map((freq, i) => (
                <mesh key={i} position={[(i - frequencies.length / 2) * 0.4, freq * 2, 0]}>
                    <boxGeometry args={[0.3, freq * 4, 0.3]} />
                    <meshStandardMaterial
                        color={`hsl(${200 + i * 20}, 70%, 50%)`}
                        emissive={`hsl(${200 + i * 20}, 70%, 30%)`}
                        emissiveIntensity={0.3}
                    />
                </mesh>
            ))}
        </group>
    );
}

interface SoundWavesProps {
    onNotePlay?: (note: number) => void;
}

export function SoundWaves({ onNotePlay }: SoundWavesProps) {
    const [playing, setPlaying] = useState(false);
    const [selectedScale, setSelectedScale] = useState<keyof typeof SCALES>('pentatonic');
    const [selectedMood, setSelectedMood] = useState(MOODS[0]);
    const [tempo, setTempo] = useState(120);
    const [melody, setMelody] = useState<number[]>([]);
    const [currentNote, setCurrentNote] = useState(0);
    const [frequencies, setFrequencies] = useState<number[]>(Array(8).fill(0.1));
    const [phase, setPhase] = useState(0);
    const [noteParticles, setNoteParticles] = useState<{ position: [number, number, number]; color: string; size: number }[]>([]);

    const audioContextRef = useRef<AudioContext | null>(null);
    const oscillatorRef = useRef<OscillatorNode | null>(null);
    const gainRef = useRef<GainNode | null>(null);

    // Generate quantum-random melody
    const generateMelody = () => {
        const scale = SCALES[selectedScale];
        const newMelody: number[] = [];

        for (let i = 0; i < 16; i++) {
            // Simulate quantum randomness
            const scaleIndex = Math.floor(Math.random() * scale.length);
            const octave = Math.floor(Math.random() * 2);
            const note = scale[scaleIndex] + (octave * 12);
            newMelody.push(note);
        }

        setMelody(newMelody);
        return newMelody;
    };

    // Play a note
    const playNote = (noteIndex: number) => {
        if (!audioContextRef.current) {
            audioContextRef.current = new AudioContext();
        }

        const ctx = audioContextRef.current;
        const note = melody[noteIndex % melody.length];
        const freq = selectedMood.baseFreq * Math.pow(2, note / 12);

        // Create oscillator
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();

        osc.type = 'sine';
        osc.frequency.value = freq;

        gain.gain.setValueAtTime(0, ctx.currentTime);
        gain.gain.linearRampToValueAtTime(0.3, ctx.currentTime + 0.05);
        gain.gain.linearRampToValueAtTime(0, ctx.currentTime + 0.3);

        osc.connect(gain);
        gain.connect(ctx.destination);

        osc.start();
        osc.stop(ctx.currentTime + 0.3);

        // Update visuals
        setPhase(prev => prev + 0.5);

        // Update frequency bars
        setFrequencies(prev => {
            const newFreqs = [...prev];
            newFreqs[noteIndex % 8] = Math.random() * 0.5 + 0.5;
            return newFreqs;
        });

        // Add floating particle
        setNoteParticles(prev => [
            ...prev.slice(-10),
            {
                position: [(Math.random() - 0.5) * 6, Math.random() * 2, (Math.random() - 0.5) * 3] as [number, number, number],
                color: selectedMood.color,
                size: 0.1 + (note / 24) * 0.1,
            }
        ]);

        onNotePlay?.(note);
    };

    // Start/stop playback
    const togglePlay = () => {
        if (playing) {
            setPlaying(false);
            // Close audio context when stopping
            if (audioContextRef.current) {
                audioContextRef.current.close();
                audioContextRef.current = null;
            }
        } else {
            const newMelody = melody.length > 0 ? melody : generateMelody();
            setMelody(newMelody);
            setPlaying(true);
            setCurrentNote(0);
        }
    };

    // Cleanup on unmount
    useEffect(() => {
        return () => {
            if (audioContextRef.current) {
                audioContextRef.current.close();
                audioContextRef.current = null;
            }
        };
    }, []);

    // Playback loop
    useEffect(() => {
        if (!playing || melody.length === 0) return;

        const interval = setInterval(() => {
            playNote(currentNote);
            setCurrentNote(prev => (prev + 1) % melody.length);
        }, (60 / tempo) * 1000);

        return () => clearInterval(interval);
    }, [playing, currentNote, tempo, melody.length]);

    // Decay frequencies
    useEffect(() => {
        const decay = setInterval(() => {
            setFrequencies(prev => prev.map(f => Math.max(0.1, f * 0.95)));
        }, 50);
        return () => clearInterval(decay);
    }, []);

    return (
        <div className="sound-waves-container">
            <div className="waves-canvas">
                <Canvas camera={{ position: [0, 2, 8], fov: 50 }}>
                    <ambientLight intensity={0.3} />
                    <pointLight position={[10, 10, 10]} intensity={1} />
                    <pointLight position={[-10, -10, -10]} intensity={0.3} color={selectedMood.color} />

                    {/* Wave ribbons */}
                    <WaveRibbon frequency={1} amplitude={0.5} phase={phase} color={selectedMood.color} position={[0, 1, 0]} />
                    <WaveRibbon frequency={1.5} amplitude={0.3} phase={phase + 1} color={selectedMood.color} position={[0, 0.5, 1]} />
                    <WaveRibbon frequency={2} amplitude={0.4} phase={phase + 2} color={selectedMood.color} position={[0, 0, 2]} />

                    {/* Frequency visualizer */}
                    <FrequencyBars frequencies={frequencies} />

                    {/* Floating note particles */}
                    {noteParticles.map((particle, i) => (
                        <NoteParticle key={i} {...particle} />
                    ))}

                    <OrbitControls enableZoom={true} enablePan={false} />

                    {/* Floor */}
                    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -3, 0]}>
                        <planeGeometry args={[20, 20]} />
                        <meshStandardMaterial color="#0a0a1a" metalness={0.9} roughness={0.1} />
                    </mesh>
                </Canvas>
            </div>

            <div className="waves-controls">
                {/* Mood Selector */}
                <div className="mood-selector">
                    <div className="selector-title">Mood</div>
                    <div className="mood-options">
                        {MOODS.map((mood) => (
                            <button
                                key={mood.id}
                                className={`mood-btn ${selectedMood.id === mood.id ? 'active' : ''}`}
                                style={{ '--mood-color': mood.color } as React.CSSProperties}
                                onClick={() => setSelectedMood(mood)}
                            >
                                {mood.name}
                            </button>
                        ))}
                    </div>
                </div>

                {/* Scale Selector */}
                <div className="scale-selector">
                    <div className="selector-title">Scale</div>
                    <div className="scale-options">
                        {Object.keys(SCALES).map((scale) => (
                            <button
                                key={scale}
                                className={`scale-btn ${selectedScale === scale ? 'active' : ''}`}
                                onClick={() => setSelectedScale(scale as keyof typeof SCALES)}
                            >
                                {scale.charAt(0).toUpperCase() + scale.slice(1)}
                            </button>
                        ))}
                    </div>
                </div>

                {/* Tempo Slider */}
                <div className="tempo-control">
                    <div className="tempo-label">
                        <span>Tempo</span>
                        <span className="tempo-value">{tempo} BPM</span>
                    </div>
                    <input
                        type="range"
                        className="tempo-slider"
                        min="60"
                        max="180"
                        value={tempo}
                        onChange={(e) => setTempo(parseInt(e.target.value))}
                    />
                </div>

                {/* Play Button */}
                <button
                    className={`play-button ${playing ? 'playing' : ''}`}
                    onClick={togglePlay}
                >
                    {playing ? '⏹️ Stop' : '▶️ Play Quantum Music'}
                </button>

                {/* Generate Button */}
                <button
                    className="generate-button"
                    onClick={generateMelody}
                    disabled={playing}
                >
                    🎲 Generate New Melody
                </button>

                {/* Melody Display */}
                <div className="melody-display">
                    <div className="melody-title">Melody</div>
                    <div className="melody-notes">
                        {melody.map((note, i) => (
                            <div
                                key={i}
                                className={`melody-note ${i === currentNote && playing ? 'active' : ''}`}
                                style={{
                                    height: `${20 + note * 2}px`,
                                    backgroundColor: i === currentNote && playing ? selectedMood.color : 'rgba(255,255,255,0.2)'
                                }}
                            />
                        ))}
                    </div>
                </div>

                {/* Current Note */}
                {playing && (
                    <div className="current-note">
                        <div className="note-label">Now Playing</div>
                        <div className="note-value" style={{ color: selectedMood.color }}>
                            Note {melody[currentNote]}
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
}
