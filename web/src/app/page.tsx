"use client";

import { useState } from "react";
import { runDemoCircuit, getTopology } from "./actions";
import { motion } from "framer-motion";
import { Activity, Server, Compass } from "lucide-react";
import { WavefunctionChart } from "../components/WavefunctionChart";
import { TopologyGraph } from "../components/TopologyGraph";
import { ExecutionResult, TopologyData } from "../components/types";

export default function DashboardPage() {
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<ExecutionResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [topology, setTopology] = useState<TopologyData | null>(null);

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
    } catch (e: any) {
      setError(e.toString());
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
    } catch (e: any) {
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
        <div className="lg:col-span-2">
          <WavefunctionChart result={result} error={error} />
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
