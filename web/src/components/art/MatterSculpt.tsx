import { useState, useRef, useEffect, useMemo } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, Sphere, Line } from '@react-three/drei';
import * as THREE from 'three';
import './MatterSculpt.css';

// Molecule configurations
const MOLECULES = [
    { id: 'h2', name: 'H₂', atoms: 2, groundEnergy: -1.137, description: 'Hydrogen molecule' },
    { id: 'heh', name: 'HeH⁺', atoms: 2, groundEnergy: -2.862, description: 'Helium hydride ion' },
    { id: 'lih', name: 'LiH', atoms: 2, groundEnergy: -7.882, description: 'Lithium hydride' },
];

// Atom colors
const ATOM_COLORS: Record<string, string> = {
    H: '#ffffff',
    He: '#d9ffff',
    Li: '#cc80ff',
    O: '#ff0d0d',
    N: '#3050f8',
    C: '#909090',
};

interface AtomProps {
    position: [number, number, number];
    element: string;
    scale?: number;
    vibrating?: boolean;
}

// 3D Atom component
function Atom({ position, element, scale = 1, vibrating }: AtomProps) {
    const meshRef = useRef<THREE.Mesh>(null);
    const [hovered, setHovered] = useState(false);

    useFrame((state) => {
        if (!meshRef.current) return;

        if (vibrating) {
            const t = state.clock.elapsedTime;
            meshRef.current.position.x = position[0] + Math.sin(t * 10) * 0.02;
            meshRef.current.position.y = position[1] + Math.cos(t * 12) * 0.02;
            meshRef.current.position.z = position[2] + Math.sin(t * 8) * 0.02;
        }
    });

    return (
        <Sphere
            ref={meshRef}
            position={position}
            args={[0.3 * scale, 32, 32]}
            onPointerOver={() => setHovered(true)}
            onPointerOut={() => setHovered(false)}
        >
            <meshStandardMaterial
                color={ATOM_COLORS[element] || '#888'}
                emissive={hovered ? ATOM_COLORS[element] : '#000'}
                emissiveIntensity={hovered ? 0.3 : 0}
                metalness={0.3}
                roughness={0.4}
            />
        </Sphere>
    );
}

// Bond between atoms
function Bond({ start, end }: { start: [number, number, number]; end: [number, number, number] }) {
    return (
        <Line
            points={[start, end]}
            color="#4ecdc4"
            lineWidth={3}
            opacity={0.8}
        />
    );
}

// Energy surface visualization
function EnergySurface({ energy, iteration }: { energy: number; iteration: number }) {
    const meshRef = useRef<THREE.Mesh>(null);

    const geometry = useMemo(() => {
        const geo = new THREE.PlaneGeometry(4, 4, 32, 32);
        const positions = geo.attributes.position.array as Float32Array;

        for (let i = 0; i < positions.length; i += 3) {
            const x = positions[i];
            const y = positions[i + 1];
            const r = Math.sqrt(x * x + y * y);

            // Create potential energy surface (Morse potential inspired)
            positions[i + 2] = Math.exp(-r * 0.5) * Math.sin(r * 2 + iteration * 0.1) * 0.3;
        }

        geo.computeVertexNormals();
        return geo;
    }, [iteration]);

    useFrame((state) => {
        if (meshRef.current) {
            meshRef.current.rotation.z = state.clock.elapsedTime * 0.1;
        }
    });

    return (
        <mesh ref={meshRef} geometry={geometry} rotation={[-Math.PI / 2, 0, 0]} position={[0, -2, 0]}>
            <meshStandardMaterial
                color="#6366f1"
                wireframe
                transparent
                opacity={0.3}
            />
        </mesh>
    );
}

// Gradient descent path
function OptimizationPath({ points }: { points: [number, number, number][] }) {
    if (points.length < 2) return null;

    return (
        <Line
            points={points}
            color="#ff6b6b"
            lineWidth={2}
            opacity={0.8}
        />
    );
}

interface MatterSculptProps {
    onEnergyUpdate?: (energy: number) => void;
}

export function MatterSculpt({ onEnergyUpdate }: MatterSculptProps) {
    const [selectedMolecule, setSelectedMolecule] = useState(MOLECULES[0]);
    const [optimizing, setOptimizing] = useState(false);
    const [iteration, setIteration] = useState(0);
    const [energy, setEnergy] = useState(-0.5);
    const [energyHistory, setEnergyHistory] = useState<number[]>([]);
    const [converged, setConverged] = useState(false);
    const [bondLength, setBondLength] = useState(1.4);
    const optimizationPath = useRef<[number, number, number][]>([]);

    // VQE optimization simulation
    const runVQE = () => {
        if (optimizing) return;

        setOptimizing(true);
        setConverged(false);
        setIteration(0);
        setEnergyHistory([]);
        optimizationPath.current = [];

        let iter = 0;
        let currentEnergy = -0.5 + Math.random() * 0.3;
        let currentBond = 1.0 + Math.random() * 0.8;

        const targetEnergy = selectedMolecule.groundEnergy;

        const optimize = () => {
            iter++;

            // Simulate gradient descent with noise
            const gradient = (currentEnergy - targetEnergy) * 0.1;
            const noise = (Math.random() - 0.5) * 0.02;
            currentEnergy -= gradient + noise;

            // Update bond length
            const bondGradient = (currentBond - 0.74) * 0.05; // Optimal H2 bond length ~0.74 Å
            currentBond -= bondGradient + (Math.random() - 0.5) * 0.02;

            setEnergy(currentEnergy);
            setBondLength(currentBond);
            setIteration(iter);
            setEnergyHistory(prev => [...prev, currentEnergy]);

            // Add point to optimization path
            optimizationPath.current.push([
                Math.cos(iter * 0.1) * (1 - iter / 100),
                currentEnergy,
                Math.sin(iter * 0.1) * (1 - iter / 100)
            ]);

            onEnergyUpdate?.(currentEnergy);

            // Check convergence
            if (Math.abs(currentEnergy - targetEnergy) < 0.01 || iter >= 50) {
                setOptimizing(false);
                setConverged(true);
            } else {
                setTimeout(optimize, 100);
            }
        };

        optimize();
    };

    // Get molecule geometry
    const getMoleculeGeometry = () => {
        const halfBond = bondLength / 2;

        switch (selectedMolecule.id) {
            case 'h2':
                return {
                    atoms: [
                        { element: 'H', position: [-halfBond, 0, 0] as [number, number, number] },
                        { element: 'H', position: [halfBond, 0, 0] as [number, number, number] },
                    ],
                    bonds: [[[-halfBond, 0, 0], [halfBond, 0, 0]] as [[number, number, number], [number, number, number]]],
                };
            case 'heh':
                return {
                    atoms: [
                        { element: 'He', position: [-halfBond * 0.8, 0, 0] as [number, number, number] },
                        { element: 'H', position: [halfBond * 1.2, 0, 0] as [number, number, number] },
                    ],
                    bonds: [[[-halfBond * 0.8, 0, 0], [halfBond * 1.2, 0, 0]] as [[number, number, number], [number, number, number]]],
                };
            case 'lih':
                return {
                    atoms: [
                        { element: 'Li', position: [-halfBond, 0, 0] as [number, number, number] },
                        { element: 'H', position: [halfBond * 1.5, 0, 0] as [number, number, number] },
                    ],
                    bonds: [[[-halfBond, 0, 0], [halfBond * 1.5, 0, 0]] as [[number, number, number], [number, number, number]]],
                };
            default:
                return { atoms: [], bonds: [] };
        }
    };

    const geometry = getMoleculeGeometry();

    return (
        <div className="matter-sculpt-container">
            <div className="molecule-canvas">
                <Canvas camera={{ position: [0, 2, 5], fov: 50 }}>
                    <ambientLight intensity={0.4} />
                    <pointLight position={[10, 10, 10]} intensity={1} />
                    <pointLight position={[-10, -10, -10]} intensity={0.3} color="#8b5cf6" />
                    <spotLight
                        position={[0, 5, 0]}
                        angle={0.5}
                        penumbra={1}
                        intensity={optimizing ? 1.5 : 0.5}
                        color={optimizing ? "#ff6b6b" : "#4ecdc4"}
                    />

                    {/* Atoms */}
                    {geometry.atoms.map((atom, i) => (
                        <Atom
                            key={i}
                            position={atom.position}
                            element={atom.element}
                            vibrating={optimizing}
                            scale={atom.element === 'Li' ? 1.3 : atom.element === 'He' ? 1.1 : 1}
                        />
                    ))}

                    {/* Bonds */}
                    {geometry.bonds.map((bond, i) => (
                        <Bond key={i} start={bond[0]} end={bond[1]} />
                    ))}

                    {/* Energy surface */}
                    <EnergySurface energy={energy} iteration={iteration} />

                    {/* Optimization path */}
                    <OptimizationPath points={optimizationPath.current} />

                    <OrbitControls enableZoom={true} enablePan={false} />

                    {/* Floor */}
                    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -3, 0]}>
                        <planeGeometry args={[20, 20]} />
                        <meshStandardMaterial color="#0a0a1a" metalness={0.9} roughness={0.1} />
                    </mesh>
                </Canvas>
            </div>

            <div className="matter-controls">
                {/* Molecule Selector */}
                <div className="molecule-selector">
                    <div className="selector-title">Molecule</div>
                    <div className="molecule-options">
                        {MOLECULES.map((mol) => (
                            <button
                                key={mol.id}
                                className={`molecule-btn ${selectedMolecule.id === mol.id ? 'active' : ''}`}
                                onClick={() => {
                                    setSelectedMolecule(mol);
                                    setConverged(false);
                                    setEnergyHistory([]);
                                }}
                                disabled={optimizing}
                            >
                                {mol.name}
                            </button>
                        ))}
                    </div>
                    <div className="molecule-desc">{selectedMolecule.description}</div>
                </div>

                {/* Run Button */}
                <button
                    className={`vqe-button ${optimizing ? 'running' : converged ? 'converged' : ''}`}
                    onClick={runVQE}
                    disabled={optimizing}
                >
                    {optimizing ? '⚛️ Optimizing...' : converged ? '✓ Converged!' : '⚛️ Run VQE'}
                </button>

                {/* Energy Display */}
                <div className="energy-display">
                    <div className="energy-label">Ground State Energy</div>
                    <div className="energy-value" style={{ color: converged ? '#4ecdc4' : '#ff6b6b' }}>
                        {energy.toFixed(4)} Ha
                    </div>
                    <div className="energy-target">Target: {selectedMolecule.groundEnergy.toFixed(4)} Ha</div>
                </div>

                {/* Stats */}
                <div className="vqe-stats">
                    <div className="stat-row">
                        <span>Iteration:</span>
                        <span>{iteration}</span>
                    </div>
                    <div className="stat-row">
                        <span>Bond Length:</span>
                        <span>{bondLength.toFixed(3)} Å</span>
                    </div>
                    <div className="stat-row">
                        <span>Error:</span>
                        <span>{Math.abs(energy - selectedMolecule.groundEnergy).toFixed(4)} Ha</span>
                    </div>
                </div>

                {/* Energy Chart */}
                <div className="energy-chart">
                    <div className="chart-title">Energy Convergence</div>
                    <div className="chart-area">
                        {energyHistory.map((e, i) => (
                            <div
                                key={i}
                                className="energy-bar"
                                style={{
                                    height: `${Math.max(5, ((-e + 2) / 3) * 100)}%`,
                                    backgroundColor: `hsl(${180 + (e + 2) * 60}, 70%, 50%)`,
                                }}
                            />
                        ))}
                    </div>
                </div>
            </div>
        </div>
    );
}
