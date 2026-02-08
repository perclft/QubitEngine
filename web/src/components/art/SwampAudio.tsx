import React, { useEffect, useRef, useImperativeHandle, forwardRef } from 'react';

export interface SwampAudioRef {
    triggerThunder: () => void;
}

interface SwampAudioProps {
    entropy: number;
    enabled: boolean;
}

// "Good Days" inspired riff (E Major Pentatonic dreaminess)
// Frequencies: E4, F#4, G#4, B4, C#5
const RIFF_NOTES = [
    329.63, // E4
    369.99, // F#4
    415.30, // G#4
    493.88, // B4
    554.37, // C#5
    493.88, // B4
    415.30, // G#4
    369.99, // F#4
];

export const SwampAudio = forwardRef<SwampAudioRef, SwampAudioProps>(({ entropy, enabled }, ref) => {
    const ctxRef = useRef<AudioContext | null>(null);
    const rainGainRef = useRef<GainNode | null>(null);
    const melodyGainRef = useRef<GainNode | null>(null);
    const melodyTimerRef = useRef<number | null>(null);

    // Initialize Audio Context
    useEffect(() => {
        if (!enabled) return;

        const AudioContext = window.AudioContext || (window as any).webkitAudioContext;
        const ctx = new AudioContext();
        ctxRef.current = ctx;

        // --- 1. Rain Synthesis (Pink Noise) ---
        const bufferSize = 2 * ctx.sampleRate;
        const buffer = ctx.createBuffer(1, bufferSize, ctx.sampleRate);
        const output = buffer.getChannelData(0);
        
        let b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (let i = 0; i < bufferSize; i++) {
            const white = Math.random() * 2 - 1;
            b0 = 0.99886 * b0 + white * 0.0555179;
            b1 = 0.99332 * b1 + white * 0.0750759;
            b2 = 0.96900 * b2 + white * 0.1538520;
            b3 = 0.86650 * b3 + white * 0.3104856;
            b4 = 0.55000 * b4 + white * 0.5329522;
            b5 = -0.7616 * b5 - white * 0.0168980;
            output[i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
            output[i] *= 0.11;
            b6 = white * 0.115926;
        }

        const rainNode = ctx.createBufferSource();
        rainNode.buffer = buffer;
        rainNode.loop = true;

        const rainGain = ctx.createGain();
        rainGain.gain.value = 0;

        const rainFilter = ctx.createBiquadFilter();
        rainFilter.type = 'lowpass';
        rainFilter.frequency.value = 800;

        rainNode.connect(rainFilter);
        rainFilter.connect(rainGain);
        rainGain.connect(ctx.destination);
        rainNode.start();
        
        rainGainRef.current = rainGain;

        // --- 2. Melody Engine ("Good Days" loop) ---
        const melodyGain = ctx.createGain();
        melodyGain.gain.value = 0.15; // Gentle background volume
        melodyGain.connect(ctx.destination);
        melodyGainRef.current = melodyGain;

        let noteIndex = 0;

        const playNote = () => {
            // Stop if destroyed
            if (ctx.state === 'closed') return;

            const t = ctx.currentTime;
            const osc = ctx.createOscillator();
            
            // "Guitar-ish" pluck: Triangle waves
            osc.type = 'triangle'; 
            
            // Base frequency
            let freq = RIFF_NOTES[noteIndex % RIFF_NOTES.length];

            // QUANTUM DISTORTION:
            // If entropy is high, notes detune and wobble
            // Entropy > 0.5: Significant detuning
            if (entropy > 0.3) {
                const detuneAmount = (entropy - 0.3) * 20; // Up to 20Hz off
                freq += (Math.random() - 0.5) * detuneAmount;
            }

            osc.frequency.setValueAtTime(freq, t);

            // Envelope: Pluck (Fast attack, long decay)
            const envGain = ctx.createGain();
            envGain.gain.setValueAtTime(0, t);
            envGain.gain.linearRampToValueAtTime(0.2, t + 0.05);
            envGain.gain.exponentialRampToValueAtTime(0.001, t + 2.0); // Ring out
            
            osc.connect(envGain);
            envGain.connect(melodyGain);
            
            osc.start(t);
            osc.stop(t + 2.5);

            // Tempo: 
            // Normal: 300ms (approx 100bpm 8th notes)
            // Chaos: Tempo jitters with entropy
            const baseTempo = 400;
            const jitter = entropy * 200 * (Math.random() - 0.5);
            
            noteIndex++;
            melodyTimerRef.current = window.setTimeout(playNote, baseTempo + jitter);
        };

        playNote();

        return () => {
            if (melodyTimerRef.current) clearTimeout(melodyTimerRef.current);
            ctx.close();
        };
    }, [enabled]); // Re-init if enabled toggles, but entropy handled in effect below

    // Update Rain & Distortion based on Entropy
    useEffect(() => {
        if (rainGainRef.current && ctxRef.current) {
            const ctx = ctxRef.current;
            const targetVol = entropy > 0.5 ? (entropy - 0.5) * 0.8 : 0;
            rainGainRef.current.gain.setTargetAtTime(targetVol, ctx.currentTime, 0.5);
        }
        
        // Melody Volume ducks when storm is loud? 
        // Or gets drowned out. 
        if (melodyGainRef.current && ctxRef.current) {
             // If storm is super loud (>0.8), melody fades slightly
            const melodyVol = entropy > 0.8 ? 0.05 : 0.15;
            melodyGainRef.current.gain.setTargetAtTime(melodyVol, ctxRef.current.currentTime, 1.0);
        }
    }, [entropy]);

    // Thunder Trigger
    useImperativeHandle(ref, () => ({
        triggerThunder: () => {
            if (!ctxRef.current || !enabled) return;
            const ctx = ctxRef.current;
            const t = ctx.currentTime;

            // 1. Deep Rumble
            const osc = ctx.createOscillator();
            osc.type = 'sawtooth';
            osc.frequency.setValueAtTime(50, t);
            osc.frequency.exponentialRampToValueAtTime(10, t + 1.5);

            const osc2 = ctx.createOscillator();
            osc2.type = 'square';
            osc2.frequency.setValueAtTime(40, t);
            osc2.frequency.exponentialRampToValueAtTime(5, t + 1.5);

            const filter = ctx.createBiquadFilter();
            filter.type = 'lowpass';
            filter.frequency.setValueAtTime(300, t);
            filter.frequency.linearRampToValueAtTime(100, t + 1.0);

            const thunderGain = ctx.createGain();
            thunderGain.gain.setValueAtTime(0, t);
            thunderGain.gain.linearRampToValueAtTime(0.8, t + 0.1); 
            thunderGain.gain.exponentialRampToValueAtTime(0.01, t + 2.0); 

            osc.connect(filter);
            osc2.connect(filter);
            filter.connect(thunderGain);
            thunderGain.connect(ctx.destination);

            osc.start(t);
            osc2.start(t);
            osc.stop(t + 2.5);
            osc2.stop(t + 2.5);
        }
    }));

    return null;
});
