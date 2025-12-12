import React, { useMemo } from 'react';
import { Sphere, Line, Text, Trail, Stars } from '@react-three/drei';
import { EffectComposer, Bloom, Noise, Vignette, ChromaticAberration } from '@react-three/postprocessing';
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

    // 2. Calculate Phase-based Color
    // Phase of |1⟩ relative to |0⟩ determines the hue
    const phase = Math.atan2(c1_imag, c1_real); // -PI to PI
    // Map phase to hue (0-360 degrees)
    const hue = ((phase + Math.PI) / (2 * Math.PI)) * 360;

    // Create dynamic color using HSL
    const dynamicColor = useMemo(() => {
        return new THREE.Color().setHSL(hue / 360, 1.0, 0.5);
    }, [hue]);

    const colorHex = `hsl(${hue}, 100%, 50%)`;

    // Debug logging
    console.log(`[BlochSphere] Phase: ${phase.toFixed(3)}, Hue: ${hue.toFixed(1)}, Color: ${colorHex}`);

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
            <Text position={[0, 1.7, 0]} fontSize={0.3} color={dynamicColor} anchorX="center" anchorY="middle">
                {enableEffects ? "VISUAL OVERDRIVE" : "✨ FUNKY MODE ✨"}
            </Text>

            {/* 5. The DYNAMIC COLOR State Vector - Changes with Phase! */}
            <Line points={[[0, 0, 0], [x, y, z]]} color={dynamicColor} lineWidth={5} opacity={0.9} transparent />

            {/* 6. The Psychedelic Trail Tip - Also Dynamic Color */}
            <Trail
                width={3}
                length={25}
                color={dynamicColor}
                attenuation={(t) => t * t}
            >
                <mesh position={[x, y, z]}>
                    <sphereGeometry args={[0.1]} />
                    <meshStandardMaterial
                        color={dynamicColor}
                        emissive={dynamicColor}
                        emissiveIntensity={5}
                        toneMapped={false}
                    />
                </mesh>
            </Trail>

            {/* 7. Inner Glow - Also Dynamic! */}
            <pointLight position={[0, 0, 0]} intensity={3} color={dynamicColor} distance={4} />

            {/* 8. Debug: Show current hue value */}
            <Text position={[0, -1.7, 0]} fontSize={0.12} color="white" anchorX="center" anchorY="middle">
                Phase: {(phase * 180 / Math.PI).toFixed(0)}°
            </Text>
        </group>
    );
};

