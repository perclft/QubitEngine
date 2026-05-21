"use client";

import { useState, useCallback } from "react";
import { runCustomCircuit } from "../actions";
import { ExecutionResult } from "../../components/types";
import { motion, AnimatePresence } from "framer-motion";
import { Cpu, Plus, Trash2, Play, Sliders, FolderOpen, MousePointerClick } from "lucide-react";
import { PRESETS } from "../../components/CircuitPresets";
import { BlochSphere } from "../../components/BlochSphere";
import { CircuitDiagram } from "../../components/CircuitDiagram";

const GATE_PALETTE = [
  { type: "HADAMARD", label: "H", color: "bg-indigo-500/20 border-indigo-500/30 text-indigo-300" },
  { type: "PAULI_X", label: "X", color: "bg-rose-500/20 border-rose-500/30 text-rose-300" },
  { type: "PAULI_Y", label: "Y", color: "bg-amber-500/20 border-amber-500/30 text-amber-300" },
  { type: "PAULI_Z", label: "Z", color: "bg-emerald-500/20 border-emerald-500/30 text-emerald-300" },
  { type: "CNOT", label: "CX", color: "bg-cyan-500/20 border-cyan-500/30 text-cyan-300" },
  { type: "SWAP", label: "SW", color: "bg-violet-500/20 border-violet-500/30 text-violet-300" },
  { type: "PHASE_S", label: "S", color: "bg-pink-500/20 border-pink-500/30 text-pink-300" },
  { type: "PHASE_T", label: "T", color: "bg-teal-500/20 border-teal-500/30 text-teal-300" },
  { type: "ROTATION_X", label: "Rx", color: "bg-orange-500/20 border-orange-500/30 text-orange-300" },
  { type: "ROTATION_Y", label: "Ry", color: "bg-lime-500/20 border-lime-500/30 text-lime-300" },
  { type: "ROTATION_Z", label: "Rz", color: "bg-sky-500/20 border-sky-500/30 text-sky-300" },
  { type: "TOFFOLI", label: "CCX", color: "bg-fuchsia-500/20 border-fuchsia-500/30 text-fuchsia-300" },
  { type: "MEASURE", label: "M", color: "bg-slate-500/20 border-slate-500/30 text-slate-300" },
];

interface GateEntry {
  id: string;
  type: string;
  label: string;
  target: number;
  control: number;
  angle: number;
}

export default function CircuitLabPage() {
  const [numQubits, setNumQubits] = useState(3);
  const [gates, setGates] = useState<GateEntry[]>([]);
  const [noiseProbability, setNoiseProbability] = useState(0);
  const [backend, setBackend] = useState(0);
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<ExecutionResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [selectedQubit, setSelectedQubit] = useState<number>(0);

  const addGate = useCallback((type: string, label: string) => {
    setGates(prev => [...prev, {
      id: `${Date.now()}-${Math.random()}`,
      type, label,
      target: 0,
      control: type === "CNOT" || type === "TOFFOLI" || type === "CZ" || type === "SWAP" ? 1 : 0,
      angle: 0,
    }]);
  }, []);

  const removeGate = (id: string) => {
    setGates(prev => prev.filter(g => g.id !== id));
  };

  const updateGate = (id: string, field: string, value: number) => {
    setGates(prev => prev.map(g => g.id === id ? { ...g, [field]: value } : g));
  };

  const loadPreset = (index: number) => {
    if (index < 0) return;
    const preset = PRESETS[index];
    setNumQubits(preset.qubits);
    setGates(preset.getGates());
    setResult(null);
  };

  const handleRun = async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);
    try {
      const ops = gates.map(g => ({
        type: g.type,
        targetQubit: g.target,
        controlQubit: g.control,
        angle: g.angle,
      }));
      const res = await runCustomCircuit(numQubits, ops, noiseProbability, backend);
      if ("error" in res && res.error) {
        setError(res.error);
      } else if (!("error" in res)) {
        setResult(res as ExecutionResult);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setIsRunning(false);
    }
  };

  const needsAngle = (type: string) => ["ROTATION_X", "ROTATION_Y", "ROTATION_Z"].includes(type);
  const needsControl = (type: string) => ["CNOT", "TOFFOLI", "CZ", "SWAP"].includes(type);

  // Map to Diagram Gates
  const diagramGates = gates.map(g => ({
    type: g.label, // use label for brevity
    targets: [g.target],
    controls: needsControl(g.type) ? [g.control] : undefined,
    params: needsAngle(g.type) ? [g.angle] : undefined
  }));

  // Calculate Bloch sphere angles from state vector (simplified extraction)
  const getBlochAngles = (q: number) => {
    if (!result?.stateVector) return { theta: 0, phi: 0 };
    // This is a rough estimation of the reduced state for visual purposes
    // A proper reduced density matrix calculation is needed for exact representation of entangled states
    // In this UI demo we just look at the marginal probability of |1>
    let p1 = 0;
    for (let i = 0; i < result.stateVector.length; i++) {
        if ((i & (1 << q)) !== 0) {
            p1 += result.stateVector[i].real ** 2 + result.stateVector[i].imag ** 2;
        }
    }
    const theta = 2 * Math.asin(Math.sqrt(Math.min(1, Math.max(0, p1))));
    return { theta, phi: 0 }; // phi requires proper phase tracking
  };

  const { theta, phi } = getBlochAngles(selectedQubit);

  return (
    <div className="p-8 space-y-6 relative max-h-screen overflow-y-auto">
      <div className="fixed top-[-15%] right-[-5%] w-[400px] h-[400px] rounded-full bg-cyan-600/10 blur-[120px] pointer-events-none" />

      <div className="relative z-10 flex justify-between items-end">
        <div>
          <h1 className="text-2xl font-semibold text-white">Circuit Lab</h1>
          <p className="text-sm text-slate-400 mt-1">Build and execute custom quantum circuits</p>
        </div>
        
        <div className="flex items-center gap-3">
          <FolderOpen className="w-4 h-4 text-slate-400" />
          <select 
            onChange={(e) => loadPreset(Number(e.target.value))}
            className="bg-white/5 border border-white/10 rounded-lg px-3 py-1.5 text-sm text-white focus:outline-none"
            defaultValue="-1"
          >
            <option value="-1" disabled>Load Preset...</option>
            {PRESETS.map((p, i) => (
              <option key={i} value={i}>{p.name}</option>
            ))}
          </select>
        </div>
      </div>

      {gates.length > 0 && (
        <div className="relative z-10">
           <CircuitDiagram numQubits={numQubits} gates={diagramGates} />
        </div>
      )}

      <div className="grid grid-cols-1 xl:grid-cols-12 gap-6 relative z-10">
        {/* Gate Palette & Config */}
        <div className="xl:col-span-3 space-y-4">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-4 flex items-center gap-1.5">
              <MousePointerClick className="w-3.5 h-3.5" /> Gate Palette
            </h3>
            <div className="grid grid-cols-3 gap-2">
              {GATE_PALETTE.map(gate => (
                <button
                  key={gate.type}
                  onClick={() => addGate(gate.type, gate.label)}
                  className={`${gate.color} border rounded-lg px-2 py-2.5 text-center text-xs font-bold hover:scale-105 transition-transform active:scale-95`}
                >
                  {gate.label}
                </button>
              ))}
            </div>
          </div>

          {/* Config */}
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5 space-y-4">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest flex items-center gap-1.5">
              <Sliders className="w-3.5 h-3.5" /> Configuration
            </h3>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Qubits</label>
              <input type="number" min={1} max={10} value={numQubits}
                onChange={e => {
                  const val = Number(e.target.value);
                  setNumQubits(val);
                  if (selectedQubit >= val) setSelectedQubit(Math.max(0, val - 1));
                }}
                className="w-full bg-white/5 border border-white/10 rounded-lg px-3 py-2 text-sm text-white focus:outline-none focus:border-indigo-500/50"
              />
            </div>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Noise Probability: {noiseProbability.toFixed(3)}</label>
              <input type="range" min={0} max={0.5} step={0.001} value={noiseProbability}
                onChange={e => setNoiseProbability(Number(e.target.value))}
                className="w-full accent-indigo-500"
              />
            </div>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Backend</label>
              <select value={backend} onChange={e => setBackend(Number(e.target.value))}
                className="w-full bg-white/5 border border-white/10 rounded-lg px-3 py-2 text-sm text-white focus:outline-none"
              >
                <option value={0}>Simulator</option>
                <option value={1}>Mock Hardware</option>
              </select>
            </div>
          </div>
        </div>

        {/* Circuit Composer */}
        <div className="xl:col-span-5">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5 min-h-[400px]">
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest">Instructions ({gates.length})</h3>
              <button onClick={handleRun} disabled={isRunning || gates.length === 0}
                className="bg-indigo-500/20 hover:bg-indigo-500/30 border border-indigo-500/30 text-indigo-300 px-4 py-1.5 rounded-lg text-xs font-semibold flex items-center gap-1.5 disabled:opacity-30 transition-colors shadow-[0_0_15px_rgba(99,102,241,0.2)] hover:shadow-[0_0_25px_rgba(99,102,241,0.4)]"
              >
                {isRunning ? <div className="w-3 h-3 border-2 border-indigo-400 border-t-transparent rounded-full animate-spin" /> : <Play className="w-3 h-3" />}
                Run Circuit
              </button>
            </div>

            {gates.length === 0 ? (
              <div className="flex flex-col items-center justify-center h-64 opacity-30">
                <Plus className="w-8 h-8 mb-2" />
                <p className="text-sm">Click gates from the palette to add them</p>
              </div>
            ) : (
              <div className="space-y-2 max-h-[500px] overflow-y-auto pr-2 custom-scrollbar">
                <AnimatePresence>
                  {gates.map((gate, idx) => {
                    const palette = GATE_PALETTE.find(g => g.type === gate.type);
                    return (
                      <motion.div
                        key={gate.id}
                        initial={{ opacity: 0, x: -20 }}
                        animate={{ opacity: 1, x: 0 }}
                        exit={{ opacity: 0, x: 20 }}
                        className="flex items-center gap-3 bg-white/[0.02] hover:bg-white/[0.04] border border-white/[0.04] rounded-xl px-4 py-2.5 transition-colors"
                      >
                        <span className="text-xs text-slate-500 w-5 font-mono">{idx + 1}</span>
                        <span className={`${palette?.color} border rounded-md px-2 py-1 text-xs font-bold min-w-[36px] text-center`}>{gate.label}</span>
                        <div className="flex-1 flex items-center gap-3 text-xs">
                          <label className="text-slate-500 flex items-center gap-1">
                            T: <input type="number" min={0} max={numQubits - 1} value={gate.target}
                              onChange={e => updateGate(gate.id, "target", Number(e.target.value))}
                              className="w-10 bg-black/20 border border-white/10 rounded px-1.5 py-0.5 text-white"
                            />
                          </label>
                          {needsControl(gate.type) && (
                            <label className="text-slate-500 flex items-center gap-1">
                              C: <input type="number" min={0} max={numQubits - 1} value={gate.control}
                                onChange={e => updateGate(gate.id, "control", Number(e.target.value))}
                                className="w-10 bg-black/20 border border-white/10 rounded px-1.5 py-0.5 text-white"
                              />
                            </label>
                          )}
                          {needsAngle(gate.type) && (
                            <label className="text-slate-500 flex items-center gap-1">
                              θ: <input type="number" step={0.1} value={gate.angle}
                                onChange={e => updateGate(gate.id, "angle", Number(e.target.value))}
                                className="w-16 bg-black/20 border border-white/10 rounded px-1.5 py-0.5 text-white"
                              />
                            </label>
                          )}
                        </div>
                        <button onClick={() => removeGate(gate.id)} className="text-slate-600 hover:text-red-400 transition-colors p-1">
                          <Trash2 className="w-3.5 h-3.5" />
                        </button>
                      </motion.div>
                    );
                  })}
                </AnimatePresence>
              </div>
            )}
          </div>
        </div>

        {/* Results */}
        <div className="xl:col-span-4 space-y-4">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5 min-h-[250px] flex flex-col">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-4">
              <Cpu className="w-3.5 h-3.5 inline mr-1.5" />State Vector Amplitudes
            </h3>
            {error ? (
              <div className="m-auto text-red-400 text-sm bg-red-500/10 px-4 py-3 rounded-lg border border-red-500/15">{error}</div>
            ) : result ? (
              <div className="flex-1 flex flex-col">
                {result.serverId && (
                  <div className="text-[11px] text-slate-500 mb-4 font-mono">Node: {result.serverId}</div>
                )}
                <div className="flex-1 flex items-end justify-center gap-3 flex-wrap pb-4">
                  {result.stateVector?.slice(0, 32).map((sv, i) => {
                    const prob = sv.real * sv.real + sv.imag * sv.imag;
                    if (prob < 0.001) return null;
                    const height = Math.max(prob * 100, 3);
                    const bits = i.toString(2).padStart(numQubits, "0");
                    return (
                      <div key={i} className="flex flex-col items-center gap-1">
                        <span className="text-[10px] text-indigo-300 font-mono">{(prob * 100).toFixed(1)}%</span>
                        <motion.div
                          initial={{ height: 0 }}
                          animate={{ height: `${height}%` }}
                          transition={{ type: "spring", bounce: 0.3 }}
                          className="w-8 rounded-t bg-gradient-to-t from-indigo-600/30 to-indigo-400 border border-indigo-400/30 shadow-[0_0_10px_rgba(99,102,241,0.2)]"
                          style={{ minHeight: 4 }}
                        />
                        <span className="text-[9px] font-mono text-slate-500">|{bits}⟩</span>
                      </div>
                    );
                  })}
                </div>
              </div>
            ) : (
              <div className="m-auto opacity-20 text-sm text-center">
                <Cpu className="w-8 h-8 mx-auto mb-2" />
                Build a circuit and hit Run
              </div>
            )}
          </div>
          
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5 flex flex-col">
            <div className="flex justify-between items-center mb-4">
                <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest">
                    Bloch Sphere
                </h3>
                <select 
                    value={selectedQubit} 
                    onChange={e => setSelectedQubit(Number(e.target.value))}
                    className="bg-white/5 border border-white/10 rounded px-2 py-1 text-xs text-white focus:outline-none"
                >
                    {Array.from({length: numQubits}).map((_, i) => (
                        <option key={i} value={i}>Qubit {i}</option>
                    ))}
                </select>
            </div>
            <BlochSphere theta={theta} phi={phi} animating={isRunning} />
          </div>
        </div>
      </div>
    </div>
  );
}
