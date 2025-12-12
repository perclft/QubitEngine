import React, { useEffect, useRef } from 'react';

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
        const speed = Math.random() * 2 * speedMultiplier;
        this.vx = Math.cos(angle) * speed;
        this.vy = Math.sin(angle) * speed;
        this.life = 0;
        this.maxLife = 100 + Math.random() * 60;
        this.hue = hue;
        this.size = Math.random() * 3 + 1;
    }

    update(w: number, h: number) {
        this.x += this.vx;
        this.y += this.vy;

        // Spiraling "Quantum" force
        // Calculate vector to center
        const dx = w / 2 - this.x;
        const dy = h / 2 - this.y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        // Orthogonal force (spiral)
        if (dist > 10) {
            this.vx += (dy / dist) * 0.05;
            this.vy -= (dx / dist) * 0.05;
        }

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

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const animate = () => {
            // 1. Physics Parameters from Quantum State
            const p0 = amplitude0.real ** 2 + amplitude0.imag ** 2; // Probability of |0>
            // Phase relative to |0>
            const phase = Math.atan2(amplitude1.imag, amplitude1.real);
            // Map phase (-PI to PI) to Hue (0 to 360)
            let targetHue = (phase * 180 / Math.PI);
            if (targetHue < 0) targetHue += 360;

            // 2. Emission
            // More probability = more particles. Base rate + boost.
            // p0 is 0-1.
            const emissionRate = Math.floor(2 + p0 * 5);
            for (let i = 0; i < emissionRate; i++) {
                particlesRef.current.push(new Particle(canvas.width, canvas.height, targetHue, 1 + p0));
            }

            // 3. Clear / Trail Effect
            ctx.fillStyle = 'rgba(0, 0, 0, 0.1)'; // Fade out previous frames for trails
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            ctx.globalCompositeOperation = 'lighter'; // Additive blending

            // 4. Update & Draw Particles
            for (let i = particlesRef.current.length - 1; i >= 0; i--) {
                const p = particlesRef.current[i];
                p.update(canvas.width, canvas.height);
                p.draw(ctx);
                if (p.isDead()) {
                    particlesRef.current.splice(i, 1);
                }
            }
            ctx.globalCompositeOperation = 'source-over';

            reqIdRef.current = requestAnimationFrame(animate);
        };

        // Start animation loop
        cancelAnimationFrame(reqIdRef.current);
        animate();

        return () => cancelAnimationFrame(reqIdRef.current);
    }, [amplitude0, amplitude1]);

    return (
        <div style={{ position: 'absolute', top: 20, left: 20, pointerEvents: 'none' }}>
            <h3 style={{ color: '#ff00ff', margin: 0, textShadow: '0 0 10px #ff00ff' }}>Quantum Particle Field</h3>
            <canvas
                ref={canvasRef}
                width={400}
                height={400}
                style={{
                    border: '1px solid rgba(0, 255, 255, 0.3)',
                    background: 'rgba(0,0,0,0.6)',
                    borderRadius: '8px',
                    boxShadow: '0 0 20px rgba(0, 255, 255, 0.2)'
                }}
            />
        </div>
    );
};
