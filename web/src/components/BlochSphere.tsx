"use client";

import React, { useRef, useMemo } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
import { OrbitControls, Text, Sphere, Line } from "@react-three/drei";
import * as THREE from "three";

interface BlochSphereProps {
  theta?: number; // Polar angle [0, PI]
  phi?: number;   // Azimuthal angle [0, 2PI]
  animating?: boolean;
}

const BlochVector = ({ theta = 0, phi = 0, animating = false }: BlochSphereProps) => {
  const arrowRef = useRef<THREE.ArrowHelper>(null);

  const dir = useMemo(() => new THREE.Vector3(), []);
  
  useFrame((state) => {
    if (arrowRef.current) {
      if (animating) {
        // Spin wildly when animating
        const t = state.clock.getElapsedTime() * 5;
        dir.set(
          Math.sin(t) * Math.cos(t * 1.5),
          Math.cos(t),
          Math.sin(t) * Math.sin(t * 1.5)
        ).normalize();
      } else {
        // Point to specific theta/phi
        dir.set(
          Math.sin(theta) * Math.cos(phi),
          Math.cos(theta),
          Math.sin(theta) * Math.sin(phi)
        ).normalize();
      }
      arrowRef.current.setDirection(dir);
    }
  });

  return (
    <primitive
      object={useMemo(() => new THREE.ArrowHelper(new THREE.Vector3(0, 1, 0), new THREE.Vector3(0, 0, 0), 1.2, 0x8b5cf6, 0.2, 0.1), [])}
      ref={arrowRef}
    />
  );
};

export const BlochSphere: React.FC<BlochSphereProps> = ({ theta = 0, phi = 0, animating = false }) => {
  return (
    <div className="w-full h-[300px] bg-slate-900/50 rounded-xl overflow-hidden border border-white/10 relative cursor-move">
      <Canvas camera={{ position: [2, 1.5, 2], fov: 50 }}>
        <ambientLight intensity={0.5} />
        <pointLight position={[5, 5, 5]} intensity={1} />
        <OrbitControls enableZoom={false} autoRotate={!animating} autoRotateSpeed={1} />
        
        {/* Sphere shell */}
        <Sphere args={[1, 32, 32]}>
          <meshBasicMaterial color="#ffffff" transparent opacity={0.05} wireframe />
        </Sphere>
        
        {/* Axes */}
        <Line points={[[-1.2, 0, 0], [1.2, 0, 0]]} color="rgba(255,255,255,0.2)" lineWidth={1} />
        <Line points={[[0, -1.2, 0], [0, 1.2, 0]]} color="rgba(255,255,255,0.2)" lineWidth={1} />
        <Line points={[[0, 0, -1.2], [0, 0, 1.2]]} color="rgba(255,255,255,0.2)" lineWidth={1} />
        
        {/* Axis Labels */}
        <Text position={[1.3, 0, 0]} fontSize={0.15} color="#94a3b8">x</Text>
        <Text position={[0, 1.3, 0]} fontSize={0.15} color="#94a3b8">|0⟩ (z)</Text>
        <Text position={[0, -1.3, 0]} fontSize={0.15} color="#94a3b8">|1⟩ (-z)</Text>
        <Text position={[0, 0, 1.3]} fontSize={0.15} color="#94a3b8">y</Text>
        
        {/* The State Vector Arrow */}
        <BlochVector theta={theta} phi={phi} animating={animating} />
      </Canvas>
      <div className="absolute bottom-2 right-3 text-[10px] text-slate-500 font-mono">
        {animating ? "SUPERPOSITION" : `θ: ${theta.toFixed(2)}, φ: ${phi.toFixed(2)}`}
      </div>
    </div>
  );
};
