import React, { useEffect, useRef } from 'react';

interface QuantumAudioProps {
    amplitude0: { real: number; imag: number };
    amplitude1: { real: number; imag: number };
    isActive: boolean;
}

export const QuantumAudio: React.FC<QuantumAudioProps> = ({ amplitude0, amplitude1, isActive }) => {
    const audioCtxRef = useRef<AudioContext | null>(null);
    const osc0Ref = useRef<OscillatorNode | null>(null);
    const gain0Ref = useRef<GainNode | null>(null);
    const osc1Ref = useRef<OscillatorNode | null>(null);
    const gain1Ref = useRef<GainNode | null>(null);
    const initializedRef = useRef(false);

    // Initialize Audio Context on first interaction/activation
    useEffect(() => {
        if (isActive && !initializedRef.current) {
            const AudioContext = window.AudioContext || (window as any).webkitAudioContext;
            const ctx = new AudioContext();
            audioCtxRef.current = ctx;

            // Oscillator for State |0>
            const osc0 = ctx.createOscillator();
            const gain0 = ctx.createGain();
            osc0.type = 'sine';
            osc0.frequency.setValueAtTime(440, ctx.currentTime); // A4 Base
            osc0.connect(gain0);
            gain0.connect(ctx.destination);
            gain0.gain.setValueAtTime(0, ctx.currentTime);
            osc0.start();

            // Oscillator for State |1>
            const osc1 = ctx.createOscillator();
            const gain1 = ctx.createGain();
            osc1.type = 'triangle'; // Different timbre
            osc1.frequency.setValueAtTime(554.37, ctx.currentTime); // C#5
            osc1.connect(gain1);
            gain1.connect(ctx.destination);
            gain1.gain.setValueAtTime(0, ctx.currentTime);
            osc1.start();

            osc0Ref.current = osc0;
            gain0Ref.current = gain0;
            osc1Ref.current = osc1;
            gain1Ref.current = gain1;
            initializedRef.current = true;
        }

        // Ensure context is running (Autoplay policy)
        if (isActive && audioCtxRef.current && audioCtxRef.current.state === 'suspended') {
            audioCtxRef.current.resume();
        }

        // Cleanup
        return () => {
            // We don't fully close to avoid needing to recreate on every toggle, but for this demo:
            // If we turn it off, we silence it.
            if (!isActive && gain0Ref.current && gain1Ref.current) {
                gain0Ref.current.gain.setTargetAtTime(0, audioCtxRef.current!.currentTime, 0.1);
                gain1Ref.current.gain.setTargetAtTime(0, audioCtxRef.current!.currentTime, 0.1);
            }
        };
    }, [isActive]);

    // Update Params based on Quantum State
    useEffect(() => {
        // Log to debug if this is firing
        // console.log("QuantumAudio Update:", isActive, amplitude0, amplitude1);

        if (!audioCtxRef.current || !gain0Ref.current || !gain1Ref.current || !osc0Ref.current || !osc1Ref.current) {
            // console.log("Audio refs missing, skipping update");
            return;
        }

        const ctx = audioCtxRef.current;
        if (ctx.state === 'suspended') {
            ctx.resume();
        }

        const now = ctx.currentTime;
        const rampTime = 0.1;

        // Calculate Probabilities
        const p0 = amplitude0.real ** 2 + amplitude0.imag ** 2;
        const p1 = amplitude1.real ** 2 + amplitude1.imag ** 2;

        // Console Log values to verify non-zero inputs
        console.log(`[Audio] P0: ${p0.toFixed(3)}, P1: ${p1.toFixed(3)}`);

        // Map Phase
        const phase0 = Math.atan2(amplitude0.imag, amplitude0.real); // -PI to PI
        const phase1 = Math.atan2(amplitude1.imag, amplitude1.real);

        // Robust Update using setTargetAtTime (Exponential approach handles "current value" implicitly)
        // Gain: Scaling factor 0.3 to avoid clipping
        gain0Ref.current.gain.setTargetAtTime(p0 * 0.3, now, rampTime);
        gain1Ref.current.gain.setTargetAtTime(p1 * 0.3, now, rampTime);

        // Frequency
        const baseFreq0 = 440;
        const baseFreq1 = 554.37;

        // Massive pitch shift (200Hz per radian) to make it obvious
        osc0Ref.current.frequency.setTargetAtTime(baseFreq0 + (phase0 * 200), now, rampTime);
        osc1Ref.current.frequency.setTargetAtTime(baseFreq1 + (phase1 * 200), now, rampTime);

    }, [amplitude0, amplitude1, isActive]); // Added isActive to dependency to ensure it runs on enable

    const testAudio = () => {
        if (!audioCtxRef.current) return;
        if (audioCtxRef.current.state === 'suspended') audioCtxRef.current.resume();

        const osc = audioCtxRef.current.createOscillator();
        const gain = audioCtxRef.current.createGain();
        osc.connect(gain);
        gain.connect(audioCtxRef.current.destination);
        osc.frequency.value = 880;
        gain.gain.value = 0.5;
        osc.start();
        osc.stop(audioCtxRef.current.currentTime + 0.2); // Short blip
        console.log("Test Tone Played.");
    };

    if (!isActive) return null;

    return (
        <div style={{ position: 'absolute', bottom: 20, right: 20, textAlign: 'right' }}>
            <div style={{ color: '#00ff00', fontSize: '0.8rem', marginBottom: '5px' }}>
                🔊 Audio Engine Active
            </div>
            <button
                onClick={testAudio}
                style={{
                    background: '#333',
                    color: '#fff',
                    border: '1px solid #00ff00',
                    padding: '5px 10px',
                    cursor: 'pointer',
                    fontSize: '0.7rem'
                }}
            >
                Test Output
            </button>
        </div>
    );
};
