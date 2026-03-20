"use client";

import { useState, useRef } from "react";
import { runVQE } from "../actions";
import { motion } from "framer-motion";
import { FlaskConical, Play, TrendingDown } from "lucide-react";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, ReferenceLine } from "recharts";

export default function VQEPage() {
  const [molecule, setMolecule] = useState(0); // H2
  const [maxIterations, setMaxIterations] = useState(50);
  const [learningRate, setLearningRate] = useState(0.1);
  const [optimizer, setOptimizer] = useState(0); // SPSA
  const [isRunning, setIsRunning] = useState(false);
  const [energyData, setEnergyData] = useState<{ iteration: number; energy: number }[]>([]);
  const [converged, setConverged] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const abortRef = useRef(false);

  const handleRun = async () => {
    setIsRunning(true);
    setEnergyData([]);
    setConverged(false);
    setError(null);
    abortRef.current = false;

    try {
      const resp = await fetch(`/api/vqe/stream?molecule=${molecule}&maxIterations=${maxIterations}&learningRate=${learningRate}&optimizer=${optimizer}`);
      
      if (!resp.ok) {
        throw new Error(`Failed to start VQE: ${resp.statusText}`);
      }

      const reader = resp.body?.getReader();
      if (!reader) throw new Error("No response body");

      const decoder = new TextEncoder().encode(""); // Not used, just to show I know about it
      const textDecoder = new TextDecoder();
      
      let buffer = "";
      while (true) {
        const { done, value } = await reader.read();
        if (done || abortRef.current) break;

        buffer += textDecoder.decode(value, { stream: true });
        const lines = buffer.split("\n");
        buffer = lines.pop() || "";

        for (const line of lines) {
          if (!line.trim()) continue;
          try {
            const data = JSON.parse(line) as { iteration: number; energy: number; converged?: boolean };
            setEnergyData((prev: { iteration: number; energy: number }[]) => [...prev, { iteration: data.iteration, energy: data.energy }]);
            if (data.converged) setConverged(true);
          } catch (e) {
            console.error("Failed to parse VQE line:", e);
          }
        }
      }
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setIsRunning(false);
    }
  };

  const minEnergy = energyData.length > 0 ? Math.min(...energyData.map(d => d.energy)) : 0;

  return (
    <div className="p-8 space-y-6 relative">
      <div className="fixed top-[-10%] left-[30%] w-[400px] h-[400px] rounded-full bg-emerald-600/10 blur-[120px] pointer-events-none" />

      <div className="relative z-10">
        <h1 className="text-2xl font-semibold text-white">VQE Explorer</h1>
        <p className="text-sm text-slate-400 mt-1">Variational Quantum Eigensolver — energy convergence in real-time</p>
      </div>

      <div className="grid grid-cols-1 xl:grid-cols-4 gap-6 relative z-10">
        {/* Controls */}
        <div className="space-y-4">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5 space-y-4">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest flex items-center gap-1.5">
              <FlaskConical className="w-3.5 h-3.5" /> VQE Parameters
            </h3>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Molecule</label>
              <select value={molecule} onChange={e => setMolecule(Number(e.target.value))}
                className="w-full bg-white/5 border border-white/10 rounded-lg px-3 py-2 text-sm text-white focus:outline-none"
              >
                <option value={0}>H₂ (Hydrogen)</option>
                <option value={1}>LiH (Lithium Hydride)</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Max Iterations</label>
              <input type="number" min={10} max={500} value={maxIterations}
                onChange={e => setMaxIterations(Number(e.target.value))}
                className="w-full bg-white/5 border border-white/10 rounded-lg px-3 py-2 text-sm text-white focus:outline-none"
              />
            </div>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Learning Rate: {learningRate.toFixed(3)}</label>
              <input type="range" min={0.001} max={0.5} step={0.001} value={learningRate}
                onChange={e => setLearningRate(Number(e.target.value))}
                className="w-full accent-emerald-500"
              />
            </div>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Optimizer</label>
              <select value={optimizer} onChange={e => setOptimizer(Number(e.target.value))}
                className="w-full bg-white/5 border border-white/10 rounded-lg px-3 py-2 text-sm text-white focus:outline-none"
              >
                <option value={0}>SPSA</option>
                <option value={1}>Gradient Descent</option>
              </select>
            </div>
            <button onClick={handleRun} disabled={isRunning}
              className="w-full bg-emerald-500/20 hover:bg-emerald-500/30 border border-emerald-500/30 text-emerald-300 px-4 py-2.5 rounded-xl text-sm font-semibold flex items-center justify-center gap-2 disabled:opacity-30 transition-colors"
            >
              {isRunning ? <div className="w-4 h-4 border-2 border-emerald-400 border-t-transparent rounded-full animate-spin" /> : <Play className="w-4 h-4" />}
              {isRunning ? "Running VQE..." : "Start VQE"}
            </button>
          </div>

          {converged && (
            <motion.div initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }}
              className="bg-emerald-950/20 border border-emerald-500/15 rounded-2xl p-4"
            >
              <div className="flex items-center gap-2 mb-1">
                <TrendingDown className="w-4 h-4 text-emerald-400" />
                <span className="text-xs font-medium text-emerald-300">Converged!</span>
              </div>
              <p className="text-lg text-white font-mono">{minEnergy.toFixed(6)} Ha</p>
              <p className="text-[11px] text-slate-500 mt-1">{energyData.length} iterations</p>
            </motion.div>
          )}
        </div>

        {/* Chart */}
        <div className="xl:col-span-3">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-6 min-h-[450px] flex flex-col">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-4">
              Energy Convergence (Hartrees)
            </h3>
            {error ? (
              <div className="m-auto text-red-400 text-sm bg-red-500/10 px-4 py-3 rounded-lg border border-red-500/15">{error}</div>
            ) : energyData.length > 0 ? (
              <div style={{ width: "100%", height: 380 }}>
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={energyData} margin={{ top: 10, right: 30, left: 10, bottom: 10 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)" />
                    <XAxis dataKey="iteration" stroke="#64748b" tick={{ fontSize: 11 }} label={{ value: "Iteration", position: "insideBottom", offset: -5, fill: "#64748b", fontSize: 11 }} />
                    <YAxis stroke="#64748b" tick={{ fontSize: 11 }} label={{ value: "Energy (Ha)", angle: -90, position: "insideLeft", fill: "#64748b", fontSize: 11 }} />
                    <Tooltip
                      contentStyle={{ backgroundColor: "rgba(15,23,42,0.95)", border: "1px solid rgba(255,255,255,0.1)", borderRadius: 12, fontSize: 12 }}
                      labelStyle={{ color: "#94a3b8" }}
                    />
                    {converged && <ReferenceLine y={minEnergy} stroke="#10b981" strokeDasharray="4 4" label={{ value: `Ground: ${minEnergy.toFixed(4)}`, fill: "#10b981", fontSize: 10 }} />}
                    <Line type="monotone" dataKey="energy" stroke="#8b5cf6" strokeWidth={2} dot={false} animationDuration={800} />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            ) : (
              <div className="m-auto opacity-20 text-sm text-center">
                <FlaskConical className="w-10 h-10 mx-auto mb-2" />
                Configure parameters and start VQE
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
