import React, { useRef, useState, useEffect } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, Text, Html } from '@react-three/drei';
import { QuantumComputeClient } from '../../generated/quantum.client';
import { VQERequest, VQERequest_Molecule, VQEResponse } from '../../generated/quantum';
import { GrpcWebFetchTransport } from "@protobuf-ts/grpcweb-transport";
import './QuantumChemistry.css';

// 3D Atom Component
const Atom = ({ position, color, size, label }: any) => (
    <group position={position}>
        <mesh>
            <sphereGeometry args={[size, 32, 32]} />
            <meshStandardMaterial color={color} roughness={0.3} metalness={0.8} />
        </mesh>
        <Text position={[0, size + 0.3, 0]} fontSize={0.3} color="white">
            {label}
        </Text>
    </group>
);

// 3D Bond Component
const Bond = ({ start, end }: any) => {
    const mid = [(start[0] + end[0]) / 2, (start[1] + end[1]) / 2, (start[2] + end[2]) / 2];
    // Simple cylinder for now, rotation logic omitted for brevity in demo
    return (
        <mesh position={mid as any} rotation={[0, 0, Math.PI / 2]}>
            <cylinderGeometry args={[0.05, 0.05, 1, 8]} />
            <meshStandardMaterial color="#888" />
        </mesh>
    );
};

// Molecule View
const H2Molecule = () => {
    return (
        <group>
            <Atom position={[-0.37, 0, 0]} color="#00ffff" size={0.3} label="H" />
            <Atom position={[0.37, 0, 0]} color="#00ffff" size={0.3} label="H" />
            <Bond start={[-0.37, 0, 0]} end={[0.37, 0, 0]} />
        </group>
    );
};

export const QuantumChemistry: React.FC = () => {
    const [running, setRunning] = useState(false);
    const [energyData, setEnergyData] = useState<{ iter: number, energy: number }[]>([]);
    const [logs, setLogs] = useState<string[]>([]);
    const abortController = useRef<AbortController | null>(null);

    const runVQE = async () => {
        if (running) return;
        setRunning(true);
        setEnergyData([]);
        setLogs(["Starting Quantum VQE Simulation for Hydrogen (H₂)..."]);

        const transport = new GrpcWebFetchTransport({ baseUrl: "http://localhost:8080" });
        const client = new QuantumComputeClient(transport);

        abortController.current = new AbortController();

        const request = VQERequest.create({
            molecule: VQERequest_Molecule.H2,
            maxIterations: 100,
            learningRate: 0.1
        });

        try {
            const stream = client.runVQE(request, { abort: abortController.current.signal });

            for await (const response of stream.responses) {
                setEnergyData(prev => [...prev, {
                    iter: response.iteration,
                    energy: response.energy
                }]);

                if (response.iteration % 10 === 0 || response.converged) {
                    setLogs(prev => [...prev.slice(-4), `Iter ${response.iteration}: E = ${response.energy.toFixed(5)} Ha`]);
                }

                if (response.converged) {
                    setLogs(prev => [...prev, "✅ CONVERGED to Ground State!"]);
                }
            }
        } catch (err: any) {
            setLogs(prev => [...prev, `Error: ${err.message}`]);
        } finally {
            setRunning(false);
        }
    };

    return (
        <div className="chemistry-container">
            <div className="chem-sidebar">
                <h2>🧪 Quantum VQE</h2>
                <div className="chem-controls">
                    <button onClick={runVQE} disabled={running}>
                        {running ? "Simulating..." : "Start Optimization"}
                    </button>
                    <div className="chem-logs">
                        {logs.map((L, i) => <div key={i}>{L}</div>)}
                    </div>
                    {energyData.length > 0 && (
                        <div className="energy-stat">
                            <h3>Current Energy</h3>
                            <span className="er-val">
                                {energyData[energyData.length - 1].energy.toFixed(5)} Ha
                            </span>
                            <small>Target: -1.137 Ha</small>
                        </div>
                    )}
                </div>
            </div>

            <div className="chem-viz">
                <Canvas camera={{ position: [0, 0, 3] }}>
                    <ambientLight intensity={0.5} />
                    <pointLight position={[10, 10, 10]} />
                    <OrbitControls />
                    <H2Molecule />
                    {/* Orbital Cloud could go here */}
                </Canvas>

                {/* Overlay Graph */}
                <div className="graph-overlay">
                    <svg viewBox="0 0 300 100" preserveAspectRatio="none">
                        {/* Simple Line Graph */}
                        <path
                            d={`M 0 100 ${energyData.map((d, i) => {
                                // Map -1.2 to 0.0 energy range to Y axis
                                const y = 100 - ((d.energy + 1.2) / 1.5) * 100; // rough scaling
                                const x = (i / 100) * 300;
                                return `L ${x} ${y}`;
                            }).join(' ')}`}
                            fill="none"
                            stroke="#00ffff"
                            strokeWidth="2"
                        />
                    </svg>
                </div>
            </div>
        </div>
    );
};
