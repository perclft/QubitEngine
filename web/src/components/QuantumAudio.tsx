import React, { useEffect, useRef, useCallback, useState } from 'react';
import './QuantumAudio.css';

// Note frequencies for 8 states (C major scale + rest)
const NOTE_FREQUENCIES: Record<number, number> = {
    0: 261.63, // C4
    1: 293.66, // D4
    2: 329.63, // E4
    3: 349.23, // F4
    4: 392.00, // G4
    5: 440.00, // A4
    6: 493.88, // B4
    7: 0,      // Rest (silence)
};

const NOTE_NAMES = ['C', 'D', 'E', 'F', 'G', 'A', 'B', 'REST'];

interface QuantumAudioProps {
    // Legacy 2-state support
    amplitude0?: { real: number; imag: number };
    amplitude1?: { real: number; imag: number };
    // NEW: Full 8-state vector for Quantum Mozart
    stateVector?: { real: number; imag: number }[];
    isActive: boolean;
    mode?: 'classic' | 'mozart'; // classic = 2 oscillators, mozart = 8 oscillators
}

export const QuantumAudio: React.FC<QuantumAudioProps> = ({
    amplitude0,
    amplitude1,
    stateVector,
    isActive,
    mode = 'classic'
}) => {
    const audioCtxRef = useRef<AudioContext | null>(null);
    const oscillatorsRef = useRef<OscillatorNode[]>([]);
    const gainsRef = useRef<GainNode[]>([]);
    const initializedRef = useRef(false);
    const [activeNote, setActiveNote] = useState<number | null>(null);
    const [probabilities, setProbabilities] = useState<number[]>([]);

    // Initialize 8 oscillators for Quantum Mozart mode
    const initMozartMode = useCallback((ctx: AudioContext) => {
        // Create 8 oscillators, one for each note
        for (let i = 0; i < 8; i++) {
            const osc = ctx.createOscillator();
            const gain = ctx.createGain();

            // Use different wave types for richer sound
            osc.type = i % 2 === 0 ? 'sine' : 'triangle';
            osc.frequency.setValueAtTime(NOTE_FREQUENCIES[i] || 0.001, ctx.currentTime);

            osc.connect(gain);
            gain.connect(ctx.destination);
            gain.gain.setValueAtTime(0, ctx.currentTime);
            osc.start();

            oscillatorsRef.current.push(osc);
            gainsRef.current.push(gain);
        }
    }, []);

    // Initialize classic 2-oscillator mode
    const initClassicMode = useCallback((ctx: AudioContext) => {
        // Oscillator for |0⟩
        const osc0 = ctx.createOscillator();
        const gain0 = ctx.createGain();
        osc0.type = 'sine';
        osc0.frequency.setValueAtTime(440, ctx.currentTime);
        osc0.connect(gain0);
        gain0.connect(ctx.destination);
        gain0.gain.setValueAtTime(0, ctx.currentTime);
        osc0.start();

        // Oscillator for |1⟩
        const osc1 = ctx.createOscillator();
        const gain1 = ctx.createGain();
        osc1.type = 'triangle';
        osc1.frequency.setValueAtTime(554.37, ctx.currentTime);
        osc1.connect(gain1);
        gain1.connect(ctx.destination);
        gain1.gain.setValueAtTime(0, ctx.currentTime);
        osc1.start();

        oscillatorsRef.current = [osc0, osc1];
        gainsRef.current = [gain0, gain1];
    }, []);

    // Initialize Audio Context
    useEffect(() => {
        if (isActive && !initializedRef.current) {
            const AudioContext = window.AudioContext || (window as unknown as { webkitAudioContext: typeof window.AudioContext }).webkitAudioContext;
            const ctx = new AudioContext();
            audioCtxRef.current = ctx;

            if (mode === 'mozart') {
                initMozartMode(ctx);
            } else {
                initClassicMode(ctx);
            }

            initializedRef.current = true;
        }

        if (isActive && audioCtxRef.current?.state === 'suspended') {
            audioCtxRef.current.resume();
        }

        return () => {
            if (!isActive) {
                gainsRef.current.forEach(gain => {
                    if (gain && audioCtxRef.current) {
                        gain.gain.setTargetAtTime(0, audioCtxRef.current.currentTime, 0.1);
                    }
                });
            }
        };
    }, [isActive, mode, initMozartMode, initClassicMode]);

    // Update Mozart mode with state vector
    useEffect(() => {
        if (!audioCtxRef.current || mode !== 'mozart' || !stateVector) return;

        const ctx = audioCtxRef.current;
        if (ctx.state === 'suspended') ctx.resume();

        const now = ctx.currentTime;
        const rampTime = 0.05;

        // Calculate probabilities from state vector
        const probs = stateVector.map(amp => amp.real ** 2 + amp.imag ** 2);
        setProbabilities(probs);

        // Find the most probable note
        let maxProb = 0;
        let maxIndex = 0;
        probs.forEach((p, i) => {
            if (p > maxProb) {
                maxProb = p;
                maxIndex = i;
            }
        });
        setActiveNote(maxIndex);

        // Update each oscillator's gain based on probability
        gainsRef.current.forEach((gain, i) => {
            if (gain && i < probs.length) {
                // Volume = probability, scaled to avoid clipping
                const volume = probs[i] * 0.3;
                gain.gain.setTargetAtTime(volume, now, rampTime);
            }
        });

        // Also update frequency slightly based on phase for richness
        oscillatorsRef.current.forEach((osc, i) => {
            if (osc && i < stateVector.length && NOTE_FREQUENCIES[i] > 0) {
                const phase = Math.atan2(stateVector[i].imag, stateVector[i].real);
                const freqOffset = phase * 10; // ±30Hz variation
                osc.frequency.setTargetAtTime(NOTE_FREQUENCIES[i] + freqOffset, now, rampTime);
            }
        });

    }, [stateVector, mode]);

    // Update Classic mode with 2 amplitudes
    useEffect(() => {
        if (!audioCtxRef.current || mode !== 'classic' || !amplitude0 || !amplitude1) return;

        const ctx = audioCtxRef.current;
        if (ctx.state === 'suspended') ctx.resume();

        const now = ctx.currentTime;
        const rampTime = 0.1;

        const p0 = amplitude0.real ** 2 + amplitude0.imag ** 2;
        const p1 = amplitude1.real ** 2 + amplitude1.imag ** 2;

        gainsRef.current[0]?.gain.setTargetAtTime(p0 * 0.3, now, rampTime);
        gainsRef.current[1]?.gain.setTargetAtTime(p1 * 0.3, now, rampTime);

        const phase0 = Math.atan2(amplitude0.imag, amplitude0.real);
        const phase1 = Math.atan2(amplitude1.imag, amplitude1.real);

        oscillatorsRef.current[0]?.frequency.setTargetAtTime(440 + phase0 * 500, now, rampTime);
        oscillatorsRef.current[1]?.frequency.setTargetAtTime(554.37 + phase1 * 500, now, rampTime);

    }, [amplitude0, amplitude1, mode]);

    // Collapse to a single note (play just that note)
    const collapseToNote = useCallback((noteIndex: number) => {
        if (!audioCtxRef.current) return;

        const ctx = audioCtxRef.current;
        const now = ctx.currentTime;

        gainsRef.current.forEach((gain, i) => {
            if (gain) {
                const targetVol = i === noteIndex ? 0.5 : 0;
                gain.gain.setTargetAtTime(targetVol, now, 0.02);
            }
        });

        setActiveNote(noteIndex);
        console.log(`🎵 Collapsed to note: ${NOTE_NAMES[noteIndex]}`);
    }, []);

    // Test audio
    const testAudio = useCallback(() => {
        if (!audioCtxRef.current) return;
        if (audioCtxRef.current.state === 'suspended') audioCtxRef.current.resume();

        const ctx = audioCtxRef.current;
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.connect(gain);
        gain.connect(ctx.destination);
        osc.frequency.value = 880;
        gain.gain.value = 0.3;
        osc.start();
        osc.stop(ctx.currentTime + 0.15);
    }, []);

    if (!isActive) return null;

    return (
        <div className="quantum-audio-panel">
            <div className="audio-header">
                🔊 {mode === 'mozart' ? 'Quantum Mozart' : 'Audio Engine'} Active
            </div>

            {mode === 'mozart' && probabilities.length > 0 && (
                <div className="note-visualizer">
                    {probabilities.map((prob, i) => (
                        <div
                            key={i}
                            className={`note-bar ${activeNote === i ? 'active' : ''}`}
                            style={{
                                height: `${prob * 100}%`,
                                opacity: prob > 0.01 ? 1 : 0.3
                            }}
                            onClick={() => collapseToNote(i)}
                            title={`${NOTE_NAMES[i]}: ${(prob * 100).toFixed(1)}%`}
                        >
                            <span className="note-label">{NOTE_NAMES[i]}</span>
                        </div>
                    ))}
                </div>
            )}

            <div className="audio-controls">
                <button onClick={testAudio} className="audio-btn">
                    Test
                </button>
                {mode === 'mozart' && (
                    <button
                        onClick={() => collapseToNote(Math.floor(Math.random() * 7))}
                        className="audio-btn collapse-btn"
                    >
                        Collapse!
                    </button>
                )}
            </div>
        </div>
    );
};
