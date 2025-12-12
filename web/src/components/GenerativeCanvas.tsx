import React, { useEffect, useRef, useState } from 'react';

interface GenerativeCanvasProps {
    amplitude0: { real: number; imag: number };
    amplitude1: { real: number; imag: number };
}

class Particle {
    x: number;
    y: number;
    vx: number;
    vy: number;
    life: number;
    maxLife: number;
    hue: number;
    size: number;

    constructor(w: number, h: number, hue: number, speedMultiplier: number) {
        this.x = w / 2;
        this.y = h / 2;
        const angle = Math.random() * Math.PI * 2;
        // MUCH slower particles so bursts are distinct
        const speed = Math.random() * 0.8 * speedMultiplier;
        this.vx = Math.cos(angle) * speed;
        this.vy = Math.sin(angle) * speed;
        this.life = 0;
        // Longer life so particles stay visible
        this.maxLife = 200 + Math.random() * 100;
        this.hue = hue;
        // Larger particles for visibility
        this.size = Math.random() * 5 + 3;
    }

    update(w: number, h: number, currentHue: number) {
        this.x += this.vx;
        this.y += this.vy;

        // Spiraling "Quantum" force
        const dx = w / 2 - this.x;
        const dy = h / 2 - this.y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        if (dist > 10) {
            this.vx += (dy / dist) * 0.08;
            this.vy -= (dx / dist) * 0.08;
        }

        // DYNAMIC: Particles gradually shift their hue toward the current quantum phase
        const hueDiff = currentHue - this.hue;
        this.hue += hueDiff * 0.1; // Smooth transition

        this.life++;
    }

    draw(ctx: CanvasRenderingContext2D) {
        const opacity = 1 - this.life / this.maxLife;
        ctx.fillStyle = `hsla(${this.hue}, 100%, 60%, ${opacity})`;
        ctx.beginPath();
        ctx.arc(this.x, this.y, this.size, 0, Math.PI * 2);
        ctx.fill();
    }

    isDead() {
        return this.life >= this.maxLife;
    }
}

export const GenerativeCanvas: React.FC<GenerativeCanvasProps> = ({ amplitude0, amplitude1 }) => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const particlesRef = useRef<Particle[]>([]);
    const reqIdRef = useRef<number>(0);
    const prevPhaseRef = useRef<number>(0);
    const [displayHue, setDisplayHue] = useState(0);

    // Store amplitude in refs so animation loop always uses latest values
    const amp0Ref = useRef(amplitude0);
    const amp1Ref = useRef(amplitude1);

    // Update refs when props change
    useEffect(() => {
        amp0Ref.current = amplitude0;
        amp1Ref.current = amplitude1;
    }, [amplitude0, amplitude1]);

    // Single animation loop that runs continuously
    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const animate = () => {
            // Read from refs (always latest values)
            const a0 = amp0Ref.current;
            const a1 = amp1Ref.current;

            // 1. Physics Parameters from Quantum State
            const p0 = a0.real ** 2 + a0.imag ** 2;
            const phase = Math.atan2(a1.imag, a1.real);

            // Map phase (-PI to PI) to Hue (0 to 360)
            const targetHue = ((phase + Math.PI) / (2 * Math.PI)) * 360;

            // Detect phase change for burst effect
            const phaseDelta = Math.abs(phase - prevPhaseRef.current);
            prevPhaseRef.current = phase;

            // Update display (for UI) - throttled to avoid re-renders
            if (Math.abs(targetHue - displayHue) > 5) {
                setDisplayHue(Math.round(targetHue));
            }

            // 2. Emission - Only emit 1 particle normally, 3 on phase change (very sparse)
            const burstMultiplier = phaseDelta > 0.1 ? 3 : 1;
            // Just 1 base particle per frame
            const emissionRate = burstMultiplier;

            for (let i = 0; i < emissionRate; i++) {
                particlesRef.current.push(new Particle(canvas.width, canvas.height, targetHue, 0.5 + p0 * 0.5));
            }

            // 3. Clear with trail effect
            ctx.fillStyle = 'rgba(0, 0, 0, 0.15)';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            ctx.globalCompositeOperation = 'lighter';

            // 4. Update & Draw Particles - Pass current hue for dynamic color shift
            for (let i = particlesRef.current.length - 1; i >= 0; i--) {
                const p = particlesRef.current[i];
                p.update(canvas.width, canvas.height, targetHue);
                p.draw(ctx);
                if (p.isDead()) {
                    particlesRef.current.splice(i, 1);
                }
            }
            ctx.globalCompositeOperation = 'source-over';

            // 5. Draw center glow with current phase color
            const gradient = ctx.createRadialGradient(
                canvas.width / 2, canvas.height / 2, 0,
                canvas.width / 2, canvas.height / 2, 50
            );
            gradient.addColorStop(0, `hsla(${targetHue}, 100%, 70%, 0.8)`);
            gradient.addColorStop(1, 'transparent');
            ctx.fillStyle = gradient;
            ctx.beginPath();
            ctx.arc(canvas.width / 2, canvas.height / 2, 50, 0, Math.PI * 2);
            ctx.fill();

            reqIdRef.current = requestAnimationFrame(animate);
        };

        animate();

        return () => cancelAnimationFrame(reqIdRef.current);
    }, []); // Empty dependency array - runs once and uses refs for data

    return (
        <div style={{ position: 'absolute', top: 20, left: 20, pointerEvents: 'none' }}>
            <h3 style={{ color: `hsl(${displayHue}, 100%, 60%)`, margin: 0, textShadow: `0 0 10px hsl(${displayHue}, 100%, 50%)` }}>
                Quantum Particle Field
            </h3>
            <p style={{ color: '#888', margin: '2px 0', fontSize: '0.7rem' }}>
                Hue: {displayHue}° | Particles: {particlesRef.current.length}
            </p>
            <canvas
                ref={canvasRef}
                width={400}
                height={400}
                style={{
                    border: `1px solid hsla(${displayHue}, 100%, 50%, 0.5)`,
                    background: 'rgba(0,0,0,0.8)',
                    borderRadius: '8px',
                    boxShadow: `0 0 20px hsla(${displayHue}, 100%, 50%, 0.3)`
                }}
            />
        </div>
    );
};
