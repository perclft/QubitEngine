"use client";

import { useState } from "react";
import { runDemoCircuit, getTopology } from "./actions";
import { motion } from "framer-motion";
import { Activity, Server, Zap, Compass, Cpu } from "lucide-react";

export default function Home() {
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);
  const [topology, setTopology] = useState<any>(null);

  const handleRunCircuit = async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);
    try {
      // 3 Qubits GHZ State
      const res: any = await runDemoCircuit(3);
      if (res.error) {
        setError(`Failed to connect to Scheduler: ${res.error}. Did you start the Go Scheduler and C++ Engine?`);
      } else {
        setResult(res);
      }
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setIsRunning(false);
    }
  };

  const handleFetchTopology = async () => {
    try {
      const top: any = await getTopology();
      if (top.error) {
        setError(`Failed to fetch topology: ${top.error}. Make sure the Go Scheduler is listening on port 50053.`);
      } else {
        setTopology(top);
      }
    } catch (e: any) {
      console.error(e);
    }
  };

  return (
    <main className="min-h-screen bg-slate-950 text-slate-200 overflow-hidden font-sans relative">
      {/* Background Orbs */}
      <div className="absolute top-[-20%] left-[-10%] w-[500px] h-[500px] rounded-full bg-indigo-600/20 blur-[120px] pointer-events-none" />
      <div className="absolute bottom-[-20%] right-[-10%] w-[600px] h-[600px] rounded-full bg-violet-600/20 blur-[150px] pointer-events-none" />

      <div className="relative z-10 max-w-6xl mx-auto px-6 py-12 flex flex-col gap-12">
        
        {/* Header */}
        <header className="flex items-center justify-between border-b border-white/10 pb-6">
          <div className="flex items-center gap-3">
            <div className="p-3 bg-indigo-500/20 rounded-xl border border-indigo-500/30">
              <Zap className="w-6 h-6 text-indigo-400" />
            </div>
            <div>
              <h1 className="text-2xl font-semibold tracking-tight text-white">QubitEngine Hub</h1>
              <p className="text-sm text-slate-400">High-Performance Quantum Simulation</p>
            </div>
          </div>
          <div className="flex items-center gap-2 px-4 py-2 bg-emerald-500/10 border border-emerald-500/20 rounded-full">
            <div className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse" />
            <span className="text-xs font-medium text-emerald-300">System Online</span>
          </div>
        </header>

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
          
          {/* Controls Panel */}
          <div className="col-span-1 flex flex-col gap-6">
            <motion.div 
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              className="bg-white/5 border border-white/10 backdrop-blur-xl rounded-2xl p-6 shadow-2xl"
            >
              <h2 className="text-lg font-medium text-white mb-6 flex items-center gap-2">
                <Activity className="w-5 h-5 text-indigo-400" /> Control Panel
              </h2>

              <div className="space-y-4">
                <button
                  onClick={handleRunCircuit}
                  disabled={isRunning}
                  className="w-full relative group overflow-hidden rounded-xl p-[1px]"
                >
                  <span className="absolute inset-0 bg-gradient-to-r from-indigo-500 to-violet-500 rounded-xl opacity-70 group-hover:opacity-100 transition-opacity duration-300 blur-sm" />
                  <div className="relative bg-slate-900 border border-white/10 px-6 py-4 rounded-xl flex items-center justify-center gap-2 hover:bg-slate-800 transition-colors">
                    {isRunning ? (
                      <div className="w-5 h-5 border-2 border-indigo-400 border-t-transparent rounded-full animate-spin" />
                    ) : (
                      <PlayIcon className="w-5 h-5 text-indigo-300 group-hover:text-indigo-200 transition-colors" />
                    )}
                    <span className="font-semibold text-white">Execute GHZ State</span>
                  </div>
                </button>

                <button
                  onClick={handleFetchTopology}
                  className="w-full bg-white/5 border border-white/10 hover:bg-white/10 transition-colors rounded-xl px-6 py-4 flex items-center justify-center gap-2 text-slate-300"
                >
                  <Compass className="w-5 h-5 opacity-70" />
                  <span className="font-medium">Fetch Hardware Topology</span>
                </button>
              </div>
            </motion.div>

            {/* Server Status */}
            {result?.serverId && (
              <motion.div 
                initial={{ opacity: 0, scale: 0.95 }}
                animate={{ opacity: 1, scale: 1 }}
                className="bg-indigo-950/30 border border-indigo-500/20 backdrop-blur-xl rounded-2xl p-6"
              >
                <div className="flex items-center gap-3 mb-2">
                  <Server className="w-5 h-5 text-indigo-400" />
                  <h3 className="text-sm font-medium text-indigo-200">Execution Node</h3>
                </div>
                <p className="text-lg text-white font-mono break-all">{result.serverId}</p>
              </motion.div>
            )}
          </div>

          {/* Visualization Area */}
          <div className="col-span-1 lg:col-span-2 flex flex-col gap-6">
            
            {/* Results Chart */}
            <motion.div 
              initial={{ opacity: 0, x: 20 }}
              animate={{ opacity: 1, x: 0 }}
              className="bg-white/5 border border-white/10 backdrop-blur-xl rounded-2xl p-6 min-h-[300px] flex flex-col"
            >
              <h2 className="text-lg font-medium text-white mb-6 flex items-center gap-2">
                <Cpu className="w-5 h-5 text-violet-400" /> Quantum State Distribution
              </h2>

              {error ? (
                <div className="m-auto text-red-400 bg-red-500/10 px-4 py-3 rounded-lg border border-red-500/20">
                  {error}
                </div>
              ) : result ? (
                <div className="flex-1 flex flex-col justify-end gap-6">
                  <div className="flex items-end justify-center gap-8 h-48 border-b border-white/10 pb-4">
                    {result.results.map((r: any, i: number) => {
                      const height = Math.max(r.probability * 100, 2);
                      const bitstring = r.index.toString(2).padStart(3, '0');
                      
                      return (
                        <div key={i} className="flex flex-col items-center gap-3">
                          <span className="text-xs text-indigo-300 font-mono">{(r.probability * 100).toFixed(1)}%</span>
                          <motion.div 
                            initial={{ height: 0 }}
                            animate={{ height: `${height}%` }}
                            transition={{ type: "spring", bounce: 0.4 }}
                            className="w-16 rounded-t-lg bg-gradient-to-t from-indigo-600/40 to-indigo-400 border border-indigo-400/50"
                          />
                          <span className="text-sm font-mono text-slate-300">|{bitstring}⟩</span>
                        </div>
                      );
                    })}
                  </div>
                </div>
              ) : (
                <div className="m-auto flex flex-col items-center gap-3 opacity-40">
                  <Activity className="w-12 h-12" />
                  <p>Awaiting quantum execution...</p>
                </div>
              )}
            </motion.div>

            {/* Topology Map */}
            {topology && (
              <motion.div 
                 initial={{ opacity: 0, y: 20 }}
                 animate={{ opacity: 1, y: 0 }}
                 className="bg-white/5 border border-white/10 backdrop-blur-xl rounded-2xl p-6 overflow-hidden relative"
              >
                <h3 className="text-sm font-medium text-slate-400 mb-6 uppercase tracking-wider">Heavy-Hex Topology Active</h3>
                <div className="relative w-full h-[300px] flex items-center justify-center">
                   {/* Map topology coordinates to viewbox */}
                   <svg viewBox="0 0 150 100" className="w-full h-full opacity-80">
                     {topology.edges.map((edge: any, i: number) => {
                       const n1 = topology.nodes.find((n:any) => n.id === edge.node1);
                       const n2 = topology.nodes.find((n:any) => n.id === edge.node2);
                       if (!n1 || !n2) return null;
                       return (
                         <line 
                           key={`edge-${i}`} 
                           x1={n1.x} y1={n1.y} 
                           x2={n2.x} y2={n2.y} 
                           stroke="rgba(139, 92, 246, 0.4)" 
                           strokeWidth="1.5"
                           className="animate-pulse"
                         />
                       );
                     })}
                     {topology.nodes.map((node: any) => (
                       <g key={`node-${node.id}`}>
                         <circle 
                           cx={node.x} cy={node.y} r="3" 
                           fill="#8b5cf6" 
                           className="shadow-glow"
                         />
                         <text 
                           x={node.x} y={node.y - 5} 
                           fill="#cbd5e1" 
                           fontSize="4" 
                           textAnchor="middle" 
                           className="font-mono"
                         >
                           Q{node.id}
                         </text>
                       </g>
                     ))}
                   </svg>
                </div>
              </motion.div>
            )}

          </div>
        </div>
      </div>
    </main>
  );
}

function PlayIcon(props: React.SVGProps<SVGSVGElement>) {
  return (
    <svg
      {...props}
      xmlns="http://www.w3.org/2000/svg"
      width="24"
      height="24"
      viewBox="0 0 24 24"
      fill="currentColor"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <polygon points="6 3 20 12 6 21 6 3" />
    </svg>
  );
}
