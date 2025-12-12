import React, { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import { Sphere, Line, Text, Trail, Stars } from '@react-three/drei';
import { EffectComposer, Bloom, Noise, Vignette, ChromaticAberration, Glitch } from '@react-three/postprocessing';
import { BlendFunction } from 'postprocessing';
import { LayerMaterial, Depth, Noise as LayerNoise } from 'lamina';
import * as THREE from 'three';

interface BlochSphereProps {
    amplitude0: { real: number; imag: number };
    amplitude1: { real: number; imag: number };
    enableEffects: boolean;
}

export const BlochSphere: React.FC<BlochSphereProps> = ({ amplitude0, amplitude1, enableEffects }) => {
    // 1. Calculate Bloch Vector (Z, X, Y)
    const c0_real = amplitude0.real, c0_imag = amplitude0.imag;
    const c1_real = amplitude1.real, c1_imag = amplitude1.imag;

    const p0 = c0_real * c0_real + c0_imag * c0_imag;
    const p1 = c1_real * c1_real + c1_imag * c1_imag;
    const z = p0 - p1;

    const ac_bd = c0_real * c1_real + c0_imag * c1_imag;
    const bc_ad = c0_imag * c1_real - c0_real * c1_imag;
    const x = 2 * ac_bd;
    const y = 2 * bc_ad;

    return (
        <group>
            {enableEffects && (
                <>
                    {/* 0. Volumetric Background (Quantum Foam) */}
                    <mesh scale={100}>
                        <sphereGeometry args={[1, 64, 64]} />
                        <LayerMaterial side={THREE.BackSide}>
                            <Depth colorA="#10002b" colorB="#000000" alpha={1} mode="normal" near={0} far={300} origin={[100, 100, 100]} />
                            <LayerNoise mapping="local" type="cell" scale={0.5} mode="softlight" colorA="#240046" colorB="#000000" />
                        </LayerMaterial>
                    </mesh>
                    <Stars radius={50} depth={50} count={5000} factor={4} saturation={0} fade speed={1} />

                    {/* 1. Post-Processing Pipeline */}
                    <EffectComposer>
                        <Bloom luminanceThreshold={0.2} mipmapBlur intensity={1.5} radius={0.8} />
                        <Noise opacity={0.1} blendFunction={BlendFunction.OVERLAY} />
                        <ChromaticAberration offset={new THREE.Vector2(0.002, 0.002)} />
                        <Vignette eskil={false} offset={0.1} darkness={0.7} />
                    </EffectComposer>
                </>
            )}

            {/* 2. Neon Wireframe Sphere */}
            <Sphere args={[1, 64, 64]}>
                <meshStandardMaterial
                    color="#00ffff"
                    emissive="#0044aa"
                    emissiveIntensity={0.5}
                    wireframe
                    transparent
                    opacity={0.15}
                />
            </Sphere>

            {/* 3. Glowing Axes */}
            <Line points={[[0, -1.2, 0], [0, 1.2, 0]]} color="#444444" transparent opacity={0.3} />
            <Line points={[[-1.2, 0, 0], [1.2, 0, 0]]} color="#444444" transparent opacity={0.3} />
            <Line points={[[0, 0, -1.2], [0, 0, 1.2]]} color="#444444" transparent opacity={0.3} />

            {/* 4. Labels */}
            <Text position={[0, 1.3, 0]} fontSize={0.15} color="#00ffff" anchorX="center" anchorY="middle">|0⟩</Text>
            <Text position={[0, -1.3, 0]} fontSize={0.15} color="#00ffff" anchorX="center" anchorY="middle">|1⟩</Text>
            <Text position={[0, 1.7, 0]} fontSize={0.3} color="#ff00ff" font="/fonts/Inter-Bold.woff" anchorX="center" anchorY="middle">
                {enableEffects ? "VISUAL OVERDRIVE" : "✨ FUNKY MODE ✨"}
            </Text>

            {/* 5. The Funky State Vector */}
            <Line points={[[0, 0, 0], [x, y, z]]} color="#ff00ff" lineWidth={3} opacity={0.8} transparent />

            {/* 6. The Psychedelic Trail Tip */}
            <Trail
                width={2}
                length={20}
                color={new THREE.Color("#ff00ff")}
                attenuation={(t) => t * t}
            >
                <mesh position={[x, y, z]}>
                    <sphereGeometry args={[0.08]} />
                    <meshStandardMaterial
                        color="#ffffff"
                        emissive="#ff00ff"
                        emissiveIntensity={4}
                        toneMapped={false}
                    />
                </mesh>
            </Trail>

            {/* 7. Inner Glow */}
            <pointLight position={[0, 0, 0]} intensity={2} color="#00ffff" distance={3} />
        </group>
    );
};
