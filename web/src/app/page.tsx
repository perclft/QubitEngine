"use client";

import { useState } from "react";
import { runDemoCircuit, getTopology } from "./actions";
import { motion } from "framer-motion";
import { Activity, Server, Compass } from "lucide-react";
import { WavefunctionChart } from "../components/WavefunctionChart";
import { TopologyGraph } from "../components/TopologyGraph";
import { ClusterMetrics } from "../components/ClusterMetrics";
import { BlochSphere } from "../components/BlochSphere";
import { ExecutionResult, TopologyData, ComplexNumber } from "../components/types";

// Derive Bloch sphere angles (theta, phi) from a multi-qubit state vector
// by computing the single-qubit reduced density matrix via partial trace.
function stateToBloch(
  stateVector: ComplexNumber[],
  qubit: number,
  numQubits: number
): { theta: number; phi: number } {
  const dim = 1 << numQubits;
  if (!stateVector || stateVector.length !== dim) {
    return { theta: 0, phi: 0 }; // |0⟩
  }

  // Partial trace: ρ_q = Tr_{others}(|ψ⟩⟨ψ|)
  // ρ_q is a 2×2 matrix: [[rho00, rho01], [rho10, rho11]]
  let rho00 = 0, rho01_r = 0, rho01_i = 0, rho11 = 0;
  for (let i = 0; i < dim; i++) {
    const bit_i = (i >> qubit) & 1;
    const ai_r = stateVector[i].real;
    const ai_i = stateVector[i].imag;
    if (bit_i === 0) {
      // Contribution to rho00
      rho00 += ai_r * ai_r + ai_i * ai_i;
      // Find partner index with qubit flipped to 1
      const j = i | (1 << qubit);
      const aj_r = stateVector[j].real;
      const aj_i = stateVector[j].imag;
      // rho01 += conj(a_i) * a_j
      rho01_r += ai_r * aj_r + ai_i * aj_i;
      rho01_i += ai_r * aj_i - ai_i * aj_r;
    } else {
      rho11 += ai_r * ai_r + ai_i * ai_i;
    }
  }

  // Bloch vector: sx = 2*Re(rho01), sy = 2*Im(rho01), sz = rho00 - rho11
  const sx = 2 * rho01_r;
  const sy = 2 * rho01_i;
  const sz = rho00 - rho11;

  // Convert to spherical: theta = arccos(sz), phi = atan2(sy, sx)
  const r = Math.sqrt(sx * sx + sy * sy + sz * sz);
  const theta = r > 1e-9 ? Math.acos(Math.max(-1, Math.min(1, sz / r))) : 0;
  const phi = Math.atan2(sy, sx);

  return { theta, phi };
}

export default function DashboardPage() {
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<ExecutionResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [topology, setTopology] = useState<TopologyData | null>(null);

  // Derive Bloch sphere angles from the actual state vector
  const numQubits = 3; // GHZ demo uses 3 qubits
  const bloch0 = result?.stateVector
    ? stateToBloch(result.stateVector, 0, numQubits)
    : { theta: 0, phi: 0 };
  const bloch1 = result?.stateVector
    ? stateToBloch(result.stateVector, 1, numQubits)
    : { theta: 0, phi: 0 };

  const handleRunCircuit = async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);
    try {
      const res = await runDemoCircuit(3) as ExecutionResult;
      if (res.error) {
        setError(res.error);
      } else {
        setResult(res);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setIsRunning(false);
    }
  };

  const handleFetchTopology = async () => {
    try {
      const top = await getTopology() as TopologyData;
      if (top.error) {
        setError(top.error);
      } else {
        setTopology(top);
      }
    } catch (e) {
      console.error(e);
    }
  };

  return (
    <div className="p-8 space-y-8 relative">
      {/* Background Orbs */}
      <div className="fixed top-[-20%] right-[-10%] w-[500px] h-[500px] rounded-full bg-indigo-600/10 blur-[120px] pointer-events-none" />
      <div className="fixed bottom-[-20%] left-[20%] w-[600px] h-[600px] rounded-full bg-violet-600/10 blur-[150px] pointer-events-none" />

      {/* Header */}
      <div className="relative z-10">
        <h1 className="text-2xl font-semibold text-white">Dashboard</h1>
        <p className="text-sm text-slate-400 mt-1">Quick overview and GHZ state demonstration</p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 relative z-10">
        {/* Controls */}
        <div className="space-y-4">
          <ClusterMetrics />
          
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6"
          >
            <h2 className="text-sm font-medium text-slate-300 mb-4 flex items-center gap-2">
              <Activity className="w-4 h-4 text-indigo-400" /> Quick Actions
            </h2>
            <div className="space-y-3">
              <button
                onClick={handleRunCircuit}
                disabled={isRunning}
                className="w-full relative group overflow-hidden rounded-xl p-[1px] disabled:opacity-50"
              >
                <span className="absolute inset-0 bg-gradient-to-r from-indigo-500 to-violet-500 rounded-xl opacity-60 group-hover:opacity-100 transition-opacity blur-sm" />
                <div className="relative bg-slate-900 px-5 py-3 rounded-xl flex items-center justify-center gap-2">
                  {isRunning ? (
                    <div className="w-4 h-4 border-2 border-indigo-400 border-t-transparent rounded-full animate-spin" />
                  ) : (
                    <PlayIcon className="w-4 h-4 text-indigo-300" />
                  )}
                  <span className="text-sm font-semibold text-white">Execute GHZ State</span>
                </div>
              </button>
              <button
                onClick={handleFetchTopology}
                className="w-full bg-white/[0.03] border border-white/[0.06] hover:bg-white/[0.06] transition-colors rounded-xl px-5 py-3 flex items-center justify-center gap-2 text-slate-300 text-sm"
              >
                <Compass className="w-4 h-4 opacity-60" />
                <span className="font-medium">Fetch Topology</span>
              </button>
            </div>
          </motion.div>

          {result?.serverId && (
            <motion.div
              initial={{ opacity: 0, scale: 0.95 }}
              animate={{ opacity: 1, scale: 1 }}
              className="bg-indigo-950/20 border border-indigo-500/15 backdrop-blur-xl rounded-2xl p-5"
            >
              <div className="flex items-center gap-2 mb-1.5">
                <Server className="w-4 h-4 text-indigo-400" />
                <span className="text-xs font-medium text-indigo-200/70">Execution Node</span>
              </div>
              <p className="text-base text-white font-mono break-all">{result.serverId}</p>
            </motion.div>
          )}
        </div>

        {/* State Distribution */}
        <div className="lg:col-span-2 space-y-6">
          <WavefunctionChart result={result} error={error} />
          
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-6">
              <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-4">
                Qubit 0 State
              </h3>
              <BlochSphere animating={isRunning} theta={bloch0.theta} phi={bloch0.phi} />
            </div>
            <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-6">
              <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-4">
                Qubit 1 State
              </h3>
              <BlochSphere animating={isRunning} theta={bloch1.theta} phi={bloch1.phi} />
            </div>
          </div>
        </div>
      </div>

      {/* Topology */}
      {topology && <TopologyGraph topology={topology} />}
    </div>
  );
}

function PlayIcon(props: React.SVGProps<SVGSVGElement>) {
  return (
    <svg {...props} xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"
      fill="currentColor" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <polygon points="6 3 20 12 6 21 6 3" />
    </svg>
  );
}
