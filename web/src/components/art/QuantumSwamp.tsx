import React, { useRef, useMemo, useState } from 'react';
import { Canvas, useFrame, type ThreeEvent } from '@react-three/fiber';
import { OrbitControls, Stars, Cloud, Sparkles } from '@react-three/drei';
import { EffectComposer, Bloom } from '@react-three/postprocessing';
import * as THREE from 'three';
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
function QuantumLotus({ position, hue }: { position: [number, number, number], hue: number }) {
    const groupRef = useRef<THREE.Group>(null);
    
    useFrame((state) => {
        if (!groupRef.current) return;
        const time = state.clock.getElapsedTime();
        // Float on the water waves - matched to water shader frequency roughly
        const y = Math.sin(position[0] * 0.4 + time) * 0.1 + 
                 Math.sin(position[2] * 0.2 + time * 0.5) * 0.1;
        
        groupRef.current.position.y = y - 1.1; // adjust height relative to water level
        groupRef.current.rotation.y = time * 0.1; // Slow rotation
    });

    return (
        <group ref={groupRef} position={position}>
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

export function QuantumSwamp({ entropy = 0.1 }: { entropy?: number }) {
    // State for user-created Memory Lilies
    const [lilies, setLilies] = useState<{ id: number; position: [number, number, number]; hue: number }[]>([]);

    // Generate random tree positions once
    const trees = useMemo(() => {
        // eslint-disable-next-line
        return Array.from({ length: 20 }).map(() => ({
            x: (Math.random() - 0.5) * 25,
            z: (Math.random() - 0.5) * 25,
            scale: 0.8 + Math.random() * 0.5
        }));
    }, []);

    const handleWaterClick = (e: ThreeEvent<MouseEvent>) => {
        e.stopPropagation(); // prevent OrbitControls from overriding sometimes
        const x = e.point.x;
        const z = e.point.z;
        
        // Color determined by Quantum Phase/Entropy + Random variation
        // High entropy = more chaotic colors (Full spectrum)
        // Low entropy = Calm blues/teals
        const baseHue = 180; // Cyan
        const variance = entropy * 360; 
        const hue = baseHue + (Math.random() - 0.5) * variance;

        setLilies(prev => [...prev, { 
            id: Date.now(), 
            position: [x, 0, z], 
            hue: hue 
        }]);
    };

    return (
        <div className="matter-sculpt-container" style={{ background: 'linear-gradient(to bottom, #020205, #001111)' }}>
            <Canvas camera={{ position: [8, 6, 12], fov: 55 }}>
                <color attach="background" args={['#010101']} />
                
                {/* Lighting: Bio-luminescent vibe */}
                <ambientLight intensity={0.2} />
                <pointLight position={[5, 10, 5]} intensity={1.5} color="#00ffaa" distance={25} />
                <pointLight position={[-8, 2, -5]} intensity={2} color="#4400ff" distance={15} />
                
                {/* Environment */}
                <Stars radius={100} depth={50} count={3000} factor={4} saturation={0} fade speed={1} />
                <Cloud opacity={0.3} speed={0.2} segments={15} position={[0, 8, -15]} color="#0a2a2a" />
                
                <Sparkles 
                    count={200} 
                    scale={15} 
                    size={6} 
                    speed={0.2} 
                    opacity={0.8} 
                    color="#aaffff" 
                />

                {/* Interactive Water Surface */}
                <group onClick={handleWaterClick}>
                    <SwampWater phase={entropy * 5} />
                </group>
                
                {trees.map((tree, i) => (
                    <QuantumTree key={i} position={[tree.x, 0, tree.z]} entropy={entropy} />
                ))}

                {/* Render Created Lilies */}
                {lilies.map((lily) => (
                    <QuantumLotus key={lily.id} position={lily.position} hue={lily.hue} />
                ))}

                <OrbitControls maxPolarAngle={Math.PI / 2 - 0.1} enableZoom={true} />
                
                <fog attach="fog" args={['#020205', 2, 25]} />

                {/* Post Processing for Beauty */}
                <EffectComposer>
                    <Bloom 
                        luminanceThreshold={0.2} 
                        luminanceSmoothing={0.9} 
                        height={300} 
                        intensity={1.5} 
                    />
                </EffectComposer>
            </Canvas>
            
            <div className="matter-controls" style={{ background: 'rgba(0,0,0,0.6)' }}>
                <div className="molecule-selector">
                    <div className="selector-title">Biome: Quantum Swamp</div>
                    <div className="molecule-desc">
                        A superposition of organic matter and memory. 
                        Entropy level controls the stability of the vegetation.
                        <br/>
                        <span style={{ fontSize: '0.8em', color: '#4ecdc4' }}>
                            *Click on the water to plant a Memory Lily*
                        </span>
                    </div>
                </div>
            </div>
        </div>
    );
}
