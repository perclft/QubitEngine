import { useState, useRef, useEffect } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, Text, RoundedBox } from '@react-three/drei';
import * as THREE from 'three';
import './ChaosDice.css';

interface DiceProps {
    value: number;
    rolling: boolean;
    onRollComplete?: () => void;
}

// 3D Dice component
function Dice3D({ value, rolling, onRollComplete }: DiceProps) {
    const meshRef = useRef<THREE.Mesh>(null);
    const [targetRotation, setTargetRotation] = useState({ x: 0, y: 0, z: 0 });

    // Dice face rotations (which rotation shows which number on top)
    const faceRotations: Record<number, { x: number; y: number; z: number }> = {
        1: { x: 0, y: 0, z: 0 },
        2: { x: -Math.PI / 2, y: 0, z: 0 },
        3: { x: 0, y: Math.PI / 2, z: 0 },
        4: { x: 0, y: -Math.PI / 2, z: 0 },
        5: { x: Math.PI / 2, y: 0, z: 0 },
        6: { x: Math.PI, y: 0, z: 0 },
    };

    useEffect(() => {
        if (!rolling && value >= 1 && value <= 6) {
            setTargetRotation(faceRotations[value]);
        }
    }, [value, rolling]);

    useFrame((state, delta) => {
        if (!meshRef.current) return;

        if (rolling) {
            // Chaotic spinning while rolling
            meshRef.current.rotation.x += delta * 8;
            meshRef.current.rotation.y += delta * 10;
            meshRef.current.rotation.z += delta * 6;
        } else {
            // Smooth interpolation to final position
            meshRef.current.rotation.x = THREE.MathUtils.lerp(
                meshRef.current.rotation.x,
                targetRotation.x,
                delta * 5
            );
            meshRef.current.rotation.y = THREE.MathUtils.lerp(
                meshRef.current.rotation.y,
                targetRotation.y,
                delta * 5
            );
            meshRef.current.rotation.z = THREE.MathUtils.lerp(
                meshRef.current.rotation.z,
                targetRotation.z,
                delta * 5
            );
        }
    });

    // Pip positions for each face
    const renderPips = (count: number, faceNormal: [number, number, number]) => {
        const pipPositions: Record<number, [number, number][]> = {
            1: [[0, 0]],
            2: [[-0.25, 0.25], [0.25, -0.25]],
            3: [[-0.25, 0.25], [0, 0], [0.25, -0.25]],
            4: [[-0.25, 0.25], [0.25, 0.25], [-0.25, -0.25], [0.25, -0.25]],
            5: [[-0.25, 0.25], [0.25, 0.25], [0, 0], [-0.25, -0.25], [0.25, -0.25]],
            6: [[-0.25, 0.3], [0.25, 0.3], [-0.25, 0], [0.25, 0], [-0.25, -0.3], [0.25, -0.3]],
        };

        return pipPositions[count]?.map((pos, i) => {
            const [u, v] = pos;
            let position: [number, number, number];

            // Calculate pip position based on face normal
            if (faceNormal[0] !== 0) {
                position = [faceNormal[0] * 0.51, v, u * faceNormal[0]];
            } else if (faceNormal[1] !== 0) {
                position = [u, faceNormal[1] * 0.51, v * faceNormal[1]];
            } else {
                position = [u, v, faceNormal[2] * 0.51];
            }

            return (
                <mesh key={i} position={position}>
                    <sphereGeometry args={[0.08, 16, 16]} />
                    <meshStandardMaterial color="#111" />
                </mesh>
            );
        });
    };

    return (
        <group ref={meshRef}>
            <RoundedBox args={[1, 1, 1]} radius={0.1} smoothness={4}>
                <meshStandardMaterial
                    color={rolling ? "#ff6b6b" : "#4ecdc4"}
                    metalness={0.3}
                    roughness={0.4}
                />
            </RoundedBox>

            {/* Face 1 (front, z+) */}
            {renderPips(1, [0, 0, 1])}
            {/* Face 6 (back, z-) */}
            {renderPips(6, [0, 0, -1])}
            {/* Face 2 (top, y+) */}
            {renderPips(2, [0, 1, 0])}
            {/* Face 5 (bottom, y-) */}
            {renderPips(5, [0, -1, 0])}
            {/* Face 3 (right, x+) */}
            {renderPips(3, [1, 0, 0])}
            {/* Face 4 (left, x-) */}
            {renderPips(4, [-1, 0, 0])}
        </group>
    );
}

// Particle burst effect
function ParticleBurst({ active, color }: { active: boolean; color: string }) {
    const particlesRef = useRef<THREE.Points>(null);
    const [particles] = useState(() => {
        const positions = new Float32Array(100 * 3);
        const velocities = new Float32Array(100 * 3);
        for (let i = 0; i < 100; i++) {
            positions[i * 3] = 0;
            positions[i * 3 + 1] = 0;
            positions[i * 3 + 2] = 0;
            velocities[i * 3] = (Math.random() - 0.5) * 2;
            velocities[i * 3 + 1] = Math.random() * 2;
            velocities[i * 3 + 2] = (Math.random() - 0.5) * 2;
        }
        return { positions, velocities };
    });

    useFrame((state, delta) => {
        if (!particlesRef.current || !active) return;

        const positions = particlesRef.current.geometry.attributes.position.array as Float32Array;

        for (let i = 0; i < 100; i++) {
            positions[i * 3] += particles.velocities[i * 3] * delta * 3;
            positions[i * 3 + 1] += particles.velocities[i * 3 + 1] * delta * 3;
            positions[i * 3 + 2] += particles.velocities[i * 3 + 2] * delta * 3;
            particles.velocities[i * 3 + 1] -= delta * 2; // gravity
        }

        particlesRef.current.geometry.attributes.position.needsUpdate = true;
    });

    useEffect(() => {
        if (active && particlesRef.current) {
            // Reset particles
            const positions = particlesRef.current.geometry.attributes.position.array as Float32Array;
            for (let i = 0; i < 100; i++) {
                positions[i * 3] = 0;
                positions[i * 3 + 1] = 0;
                positions[i * 3 + 2] = 0;
                particles.velocities[i * 3] = (Math.random() - 0.5) * 4;
                particles.velocities[i * 3 + 1] = Math.random() * 4 + 2;
                particles.velocities[i * 3 + 2] = (Math.random() - 0.5) * 4;
            }
            particlesRef.current.geometry.attributes.position.needsUpdate = true;
        }
    }, [active]);

    if (!active) return null;

    return (
        <points ref={particlesRef}>
            <bufferGeometry>
                <bufferAttribute
                    attach="attributes-position"
                    count={100}
                    array={particles.positions}
                    itemSize={3}
                />
            </bufferGeometry>
            <pointsMaterial size={0.1} color={color} transparent opacity={0.8} />
        </points>
    );
}

interface ChaosDiceProps {
    onRollResult?: (result: number) => void;
}

export function ChaosDice({ onRollResult }: ChaosDiceProps) {
    const [rolling, setRolling] = useState(false);
    const [diceValue, setDiceValue] = useState(1);
    const [rollHistory, setRollHistory] = useState<number[]>([]);
    const [showBurst, setShowBurst] = useState(false);
    const [burstColor, setBurstColor] = useState('#4ecdc4');

    // Color for each dice value
    const valueColors: Record<number, string> = {
        1: '#ff6b6b',
        2: '#ffd93d',
        3: '#6bcb77',
        4: '#4d96ff',
        5: '#9b59b6',
        6: '#e74c3c',
    };

    const rollDice = () => {
        if (rolling) return;

        setRolling(true);
        setShowBurst(false);

        // Simulate quantum randomness with multiple "measurements"
        let rollCount = 0;
        const maxRolls = 20;

        const rollInterval = setInterval(() => {
            const tempValue = Math.floor(Math.random() * 6) + 1;
            setDiceValue(tempValue);
            rollCount++;

            if (rollCount >= maxRolls) {
                clearInterval(rollInterval);

                // Final "quantum measurement"
                const finalValue = Math.floor(Math.random() * 6) + 1;
                setDiceValue(finalValue);
                setRolling(false);
                setShowBurst(true);
                setBurstColor(valueColors[finalValue]);

                setRollHistory(prev => [finalValue, ...prev.slice(0, 9)]);
                onRollResult?.(finalValue);

                // Hide burst after animation
                setTimeout(() => setShowBurst(false), 1000);
            }
        }, 100);
    };

    // Calculate statistics
    const average = rollHistory.length > 0
        ? (rollHistory.reduce((a, b) => a + b, 0) / rollHistory.length).toFixed(2)
        : '—';

    const distribution = [1, 2, 3, 4, 5, 6].map(n =>
        rollHistory.filter(v => v === n).length
    );

    return (
        <div className="chaos-dice-container">
            <div className="dice-canvas">
                <Canvas camera={{ position: [0, 2, 4], fov: 50 }}>
                    <ambientLight intensity={0.5} />
                    <pointLight position={[10, 10, 10]} intensity={1} />
                    <pointLight position={[-10, -10, -10]} intensity={0.3} color="#ff00ff" />
                    <spotLight
                        position={[0, 10, 0]}
                        angle={0.3}
                        penumbra={1}
                        intensity={rolling ? 2 : 0.5}
                        color={rolling ? "#ff6b6b" : "#ffffff"}
                    />

                    <Dice3D value={diceValue} rolling={rolling} />
                    <ParticleBurst active={showBurst} color={burstColor} />

                    <OrbitControls enableZoom={false} enablePan={false} />

                    {/* Floor with glow */}
                    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -1.5, 0]}>
                        <planeGeometry args={[10, 10]} />
                        <meshStandardMaterial
                            color="#1a1a2e"
                            metalness={0.8}
                            roughness={0.2}
                        />
                    </mesh>
                </Canvas>
            </div>

            <div className="dice-controls">
                <button
                    className={`roll-button ${rolling ? 'rolling' : ''}`}
                    onClick={rollDice}
                    disabled={rolling}
                >
                    {rolling ? '🎲 Rolling...' : '🎲 Roll Quantum Dice'}
                </button>

                <div className="dice-result">
                    <span className="result-label">Result:</span>
                    <span
                        className="result-value"
                        style={{ color: valueColors[diceValue] }}
                    >
                        {diceValue}
                    </span>
                </div>

                <div className="dice-stats">
                    <div className="stat-row">
                        <span>Rolls:</span>
                        <span>{rollHistory.length}</span>
                    </div>
                    <div className="stat-row">
                        <span>Average:</span>
                        <span>{average}</span>
                    </div>
                </div>

                <div className="distribution-chart">
                    <div className="chart-title">Distribution</div>
                    <div className="chart-bars">
                        {distribution.map((count, i) => (
                            <div key={i} className="bar-container">
                                <div
                                    className="bar"
                                    style={{
                                        height: `${(count / Math.max(...distribution, 1)) * 100}%`,
                                        backgroundColor: valueColors[i + 1]
                                    }}
                                />
                                <span className="bar-label">{i + 1}</span>
                            </div>
                        ))}
                    </div>
                </div>

                <div className="roll-history">
                    <div className="history-title">History</div>
                    <div className="history-values">
                        {rollHistory.map((val, i) => (
                            <span
                                key={i}
                                className="history-value"
                                style={{
                                    backgroundColor: valueColors[val],
                                    opacity: 1 - (i * 0.08)
                                }}
                            >
                                {val}
                            </span>
                        ))}
                    </div>
                </div>
            </div>
        </div>
    );
}
