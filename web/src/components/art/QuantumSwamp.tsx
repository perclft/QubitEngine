import React, { useRef, useMemo, useState, useEffect } from 'react';
import { Canvas, useFrame, useThree, type ThreeEvent } from '@react-three/fiber';
import { OrbitControls, Stars, Cloud, Sparkles, Html } from '@react-three/drei';
import { EffectComposer, Bloom } from '@react-three/postprocessing';
import * as THREE from 'three';
import { backend, type QuantumStateData } from '../../services/BackendBridge';
import './MatterSculpt.css'; // Re-using existing styles for now

// --- Quantum Water Shader ---
// Moves based on the 'phase' prop, creating a dark, oily, magical look.
function SwampWater({ phase }: { phase: number }) {
    const meshRef = useRef<THREE.Mesh>(null);
    
    // Optimization: Reduced geometry segments to 32x32 for better performance
    const geometry = useMemo(() => new THREE.PlaneGeometry(30, 30, 32, 32), []);

    useFrame((state) => {
        if (!meshRef.current) return;
        const time = state.clock.getElapsedTime();
        
        const positions = meshRef.current.geometry.attributes.position;
        const count = positions.count;
        
        for (let i = 0; i < count; i++) {
            const x = positions.getX(i);
            const y = positions.getY(i); 
            
            // Smoother wave function
            const z = 
                Math.sin(x * 0.4 + time + phase) * 0.3 + 
                Math.sin(y * 0.2 + time * 0.5) * 0.3 + 
                Math.sin((x + y) * 0.1 + phase * 1.5) * 0.2;
                
            positions.setZ(i, z);
        }
        positions.needsUpdate = true;
    });

    return (
        <mesh ref={meshRef} geometry={geometry} rotation={[-Math.PI / 2, 0, 0]} position={[0, -1, 0]}>
            <meshStandardMaterial 
                color="#051a1a" 
                roughness={0.2} 
                metalness={0.6}
                emissive="#002233"
                emissiveIntensity={0.1}
                transparent
                opacity={0.85}
            />
        </mesh>
    );
}

// --- Superposition Tree ---
// Trees that glitch/jitter if the "observation" (hover) isn't happening
function QuantumTree({ position, entropy }: { position: [number, number, number], entropy: number }) {
    const groupRef = useRef<THREE.Group>(null);
    const [hovered, setHovered] = useState(false);

    useFrame((state) => {
        if (!groupRef.current) return;
        
        // If high entropy and NOT observed (hovered), the tree jitters (uncertainty principle)
        if (!hovered && entropy > 0.3) {
            groupRef.current.rotation.y += (Math.random() - 0.5) * 0.1;
            groupRef.current.scale.y = 1 + (Math.random() - 0.5) * 0.1;
        } else {
            // Collapse to stable state
            groupRef.current.rotation.y = Math.sin(state.clock.getElapsedTime() * 0.5) * 0.05;
            groupRef.current.scale.setScalar(1);
        }
    });

    return (
        <group ref={groupRef} position={position} 
               onPointerOver={() => setHovered(true)} 
               onPointerOut={() => setHovered(false)}>
            {/* Trunk */}
            <mesh position={[0, 1, 0]}>
                <cylinderGeometry args={[0.2, 0.4, 2, 8]} />
                <meshStandardMaterial color="#3d2817" roughness={0.9} />
            </mesh>
            {/* Canopy (Cypress/Willow style) */}
            <mesh position={[0, 2.5, 0]}>
                <coneGeometry args={[1.5, 3, 8]} />
                <meshStandardMaterial 
                    color={hovered ? "#4ecdc4" : "#1a4d1a"} // Glows cyan when observed
                    wireframe={!hovered && entropy > 0.4}   // Wireframe when unstable
                    emissive={hovered ? "#4ecdc4" : "#000000"}
                    emissiveIntensity={0.5}
                />
            </mesh>
        </group>
    );
}

// --- Quantum Lotus (Memory Lily) ---
function QuantumLotus({ position, hue, text }: { position: [number, number, number], hue: number, text?: string }) {
    const groupRef = useRef<THREE.Group>(null);
    const [hovered, setHovered] = useState(false);
    
    useFrame((state) => {
        if (!groupRef.current) return;
        const time = state.clock.getElapsedTime();
        // Float on the water waves - matched to water shader frequency roughly
        const y = Math.sin(position[0] * 0.4 + time) * 0.1 + 
                 Math.sin(position[2] * 0.2 + time * 0.5) * 0.1;
        
        groupRef.current.position.y = y - 1.1; // adjust height relative to water level
        groupRef.current.rotation.y = time * 0.1; // Slow rotation

        // Scale up on hover
        const targetScale = hovered ? 1.5 : 1.0;
        groupRef.current.scale.lerp(new THREE.Vector3(targetScale, targetScale, targetScale), 0.1);
    });

    return (
        <group 
            ref={groupRef} 
            position={position}
            onPointerOver={(e) => { e.stopPropagation(); setHovered(true); }}
            onPointerOut={(e) => setHovered(false)}
        >
             {/* Tooltip (Whisper) */}
            {hovered && text && (
                <Html position={[0, 1, 0]} center distanceFactor={10}>
                    <div style={{ 
                        background: 'rgba(0, 20, 20, 0.8)', 
                        padding: '8px 12px', 
                        borderRadius: '8px',
                        border: `1px solid hsl(${hue}, 100%, 50%)`,
                        color: `hsl(${hue}, 100%, 90%)`,
                        fontFamily: 'monospace',
                        pointerEvents: 'none',
                        whiteSpace: 'nowrap',
                        boxShadow: `0 0 10px hsl(${hue}, 100%, 30%)`,
                        backdropFilter: 'blur(4px)'
                    }}>
                        "{text}"
                    </div>
                </Html>
            )}

            {/* Center light */}
            <mesh position={[0, 0.2, 0]}>
                <sphereGeometry args={[0.1, 16, 16]} />
                <meshStandardMaterial 
                    color={`hsl(${hue}, 100%, 80%)`}
                    emissive={`hsl(${hue}, 100%, 50%)`}
                    emissiveIntensity={2}
                    toneMapped={false}
                />
            </mesh>
            {/* Petals */}
            {[0, 60, 120, 180, 240, 300].map((angle, i) => (
                <mesh key={i} rotation={[0, (angle * Math.PI) / 180, 0.3]} position={[0, 0, 0]}>
                    <coneGeometry args={[0.2, 0.8, 4]} />
                    <meshStandardMaterial 
                        color={`hsl(${hue}, 80%, 60%)`}
                        roughness={0.2}
                        emissive={`hsl(${hue}, 100%, 20%)`}
                        emissiveIntensity={0.5}
                    />
                </mesh>
            ))}
        </group>
    );
}

// --- Temporary Ghost Lily for Input ---
function InputLily({ position, hue, onConfirm, onCancel }: { position: [number, number, number], hue: number, onConfirm: (text: string) => void, onCancel: () => void }) {
    const [text, setText] = useState('');
    const inputRef = useRef<HTMLInputElement>(null);

    // Auto-focus logic
    useEffect(() => {
        setTimeout(() => inputRef.current?.focus(), 100);
    }, []);

    return (
        <group position={position}>
            <Html position={[0, 0.5, 0]} center>
                <div style={{ pointerEvents: 'auto' }}>
                    <form onSubmit={(e) => { e.preventDefault(); onConfirm(text); }} style={{ display: 'flex', gap: '5px' }}>
                        <input 
                            ref={inputRef}
                            type="text" 
                            value={text} 
                            onChange={(e) => setText(e.target.value)}
                            placeholder="Whisper a memory..."
                            style={{
                                background: 'rgba(0,0,0,0.8)',
                                border: '1px solid #00ffaa',
                                color: '#00ffaa',
                                padding: '5px',
                                borderRadius: '4px',
                                outline: 'none',
                                width: '200px'
                            }}
                            onKeyDown={(e) => { if(e.key === 'Escape') onCancel(); }}
                        />
                        <button type="submit" style={{ background: '#00ffaa', border: 'none', borderRadius: '4px', cursor: 'pointer' }}>Save</button>
                    </form>
                </div>
            </Html>
            {/* Visual preview */}
            <mesh position={[0, 0.2, 0]}>
                <sphereGeometry args={[0.1, 16, 16]} />
                <meshStandardMaterial color="#ffffff" emissive="#ffffff" emissiveIntensity={1} />
            </mesh>
        </group>
    );
}



import { SwampAudio, type SwampAudioRef } from './SwampAudio';

// --- Decoherence Storm (Rain & Lightning) ---
function DecoherenceStorm({ entropy, onLightning }: { entropy: number, onLightning?: () => void }) {
    const { scene } = useThree();
    const rainRef = useRef<THREE.InstancedMesh>(null);
    const lightRef = useRef<THREE.PointLight>(null);
    const flashIntensity = useRef(0);
    
    // ... (Rain setup same as before) ...
    // 1000 raindrops
    const count = 1000;
    const dummy = useMemo(() => new THREE.Object3D(), []);
    const particles = useMemo(() => {
        return new Array(count).fill(0).map(() => ({
            x: (Math.random() - 0.5) * 50,
            y: Math.random() * 20,
            z: (Math.random() - 0.5) * 50,
            speed: 0.5 + Math.random() * 0.5
        }));
    }, []);

    useFrame((state) => {
        // --- 1. Rain Animation ---
        if (entropy > 0.5 && rainRef.current) {
            particles.forEach((p, i) => {
                p.y -= p.speed;
                if (p.y < 0) p.y = 20; 
                dummy.position.set(p.x, p.y, p.z);
                dummy.rotation.z = 0.1;
                dummy.scale.set(0.05, 1, 0.05); 
                dummy.updateMatrix();
                rainRef.current!.setMatrixAt(i, dummy.matrix);
            });
            rainRef.current.instanceMatrix.needsUpdate = true;
            const rainIntensity = Math.min(1, (entropy - 0.5) * 3);
            if (rainRef.current.material) {
                (rainRef.current.material as THREE.MeshBasicMaterial).opacity = rainIntensity * 0.6;
            }
        } else if (rainRef.current && rainRef.current.material) {
             (rainRef.current.material as THREE.MeshBasicMaterial).opacity = 0; 
        }

        // --- 2. Lightning Animation ---
        if (entropy > 0.6) {
             const chance = (entropy - 0.6) * 0.05; 
             if (Math.random() < chance) {
                 flashIntensity.current = 1.0; 
                 // Trigger Audio
                 onLightning?.();

                 if (lightRef.current) {
                    lightRef.current.position.set(
                        (Math.random() - 0.5) * 40,
                        10 + Math.random() * 10,
                        (Math.random() - 0.5) * 40
                    );
                 }
             }
        }

        // Decay Flash
        flashIntensity.current *= 0.85;

        // Apply Flash
        if (lightRef.current) {
            lightRef.current.intensity = flashIntensity.current * 500; 
        }

        if (scene.fog instanceof THREE.Fog) {
            if (flashIntensity.current > 0.01) {
                const flashColor = new THREE.Color('#8899ff').multiplyScalar(flashIntensity.current * 0.5);
                const baseColor = new THREE.Color('#020205');
                scene.fog.color.copy(baseColor).add(flashColor);
            } else {
                scene.fog.color.set('#020205');
            }
        }
    });

    return (
        <>
            <instancedMesh ref={rainRef} args={[undefined, undefined, count]}>
                <cylinderGeometry args={[0.05, 0.05, 1]} />
                <meshBasicMaterial color="#aaddff" transparent opacity={0} blending={THREE.AdditiveBlending} />
            </instancedMesh>
            <pointLight ref={lightRef} distance={200} color="#ccddee" intensity={0} decay={1} />
        </>
    );
}

export function QuantumSwamp({ entropy: userEntropy = 0.1 }: { entropy?: number }) {
    // ... (State same as before)
    const [liveData, setLiveData] = useState<QuantumStateData>({ entropy: userEntropy, phase: 0, amplitudes: [] });
    const [lilies, setLilies] = useState<{ id: number; position: [number, number, number]; hue: number; text: string }[]>([]);
    const [pendingLily, setPendingLily] = useState<{ position: [number, number, number]; hue: number } | null>(null);
    const audioRef = useRef<SwampAudioRef>(null);

    // Subscribe to Backend Stream
    useEffect(() => {
        const dataStream = backend.streamQuantumState();
        let isActive = true;
        const consumeStream = async () => {
             for await (const data of dataStream) {
                 if (!isActive) break;
                 setLiveData(data); 
             }
        };
        consumeStream();
        return () => { isActive = false; };
    }, []);

    const effectiveEntropy = (userEntropy * 0.5) + (liveData.entropy * 0.5);

    // Trees Logic
    const trees = useMemo(() => {
        return Array.from({ length: 20 }).map(() => ({
            x: (Math.random() - 0.5) * 25,
            z: (Math.random() - 0.5) * 25,
            scale: 0.8 + Math.random() * 0.5
        }));
    }, []);

    const handleWaterClick = (e: ThreeEvent<MouseEvent>) => {
        e.stopPropagation();
        if (pendingLily) return; 
        const x = e.point.x;
        const z = e.point.z;
        const baseHue = 180; 
        const variance = effectiveEntropy * 360; 
        const hue = baseHue + (Math.random() - 0.5) * variance;
        setPendingLily({ position: [x, 0, z], hue });
    };

    const confirmLily = (text: string) => {
        if (pendingLily) {
            setLilies(prev => [...prev, { 
                id: Date.now(), 
                position: pendingLily.position, 
                hue: pendingLily.hue,
                text: text
            }]);
            setPendingLily(null);
        }
    };

    return (
        <div className="matter-sculpt-container" style={{ background: 'linear-gradient(to bottom, #020205, #001111)' }}>
            {/* Audio Engine (Hidden) */}
            <SwampAudio 
                ref={audioRef}
                entropy={effectiveEntropy} 
                enabled={true} 
            />

            <Canvas camera={{ position: [8, 6, 12], fov: 55 }}>
                <color attach="background" args={['#010101']} />
                
                <ambientLight intensity={0.2} />
                <pointLight position={[5, 10, 5]} intensity={1.5} color="#00ffaa" distance={25} />
                <pointLight position={[-8, 2, -5]} intensity={2} color="#4400ff" distance={15} />
                
                <Stars radius={100} depth={50} count={3000} factor={4} saturation={0} fade speed={1} />
                <Cloud opacity={0.3} speed={0.2} segments={15} position={[0, 8, -15]} color="#0a2a2a" />
                
                <Sparkles count={200} scale={15} size={6} speed={0.2} opacity={0.8} color="#aaffff" />

                <group onClick={handleWaterClick}>
                    <SwampWater phase={liveData.phase} />
                </group>

                {/* Weather System with Audio Trigger */}
                <DecoherenceStorm 
                    entropy={effectiveEntropy} 
                    onLightning={() => audioRef.current?.triggerThunder()} 
                />
                
                {trees.map((tree, i) => (
                    <QuantumTree key={i} position={[tree.x, 0, tree.z]} entropy={effectiveEntropy} />
                ))}

                {lilies.map((lily) => (
                    <QuantumLotus key={lily.id} position={lily.position} hue={lily.hue} text={lily.text} />
                ))}

                {pendingLily && (
                    <InputLily 
                        position={pendingLily.position} 
                        hue={pendingLily.hue} 
                        onConfirm={confirmLily} 
                        onCancel={() => setPendingLily(null)} 
                    />
                )}

                <OrbitControls maxPolarAngle={Math.PI / 2 - 0.1} enableZoom={true} />
                <fog attach="fog" args={['#020205', 2, 25]} />

                <EffectComposer>
                    <Bloom luminanceThreshold={0.2} luminanceSmoothing={0.9} height={300} intensity={1.5} />
                </EffectComposer>
            </Canvas>
            
            <div className="matter-controls" style={{ background: 'rgba(0,0,0,0.6)' }}>
                <div className="molecule-selector">
                    <div className="selector-title">Biome: Quantum Swamp {liveData.amplitudes.length > 0 ? '(Live State)' : '(Simulation)'}</div>
                    <div className="molecule-desc">
                        System Entropy: {effectiveEntropy.toFixed(3)} | Phase: {liveData.phase.toFixed(2)}rad
                        <br/>
                        <span style={{ fontSize: '0.8em', color: '#4ecdc4' }}>
                            *Now Playing: Neural Dream (Good Days)*
                        </span>
                    </div>
                </div>
            </div>
        </div>
    );
}
