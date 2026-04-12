"use client";

import { useEffect, useState } from "react";
import { ClusterMetricsData } from "./types";
import { motion } from "framer-motion";
import { Server, Activity, Database, Cpu } from "lucide-react";

export function ClusterMetrics() {
  const [metrics, setMetrics] = useState<ClusterMetricsData | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const eventSource = new EventSource('/api/metrics');

    eventSource.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data) as ClusterMetricsData;
        setMetrics(data);
        setError(null);
      } catch (err) {
        console.error("Failed to parse metrics", err);
      }
    };

    eventSource.onerror = () => {
      setError("Lost connection to metric stream");
      eventSource.close();
    };

    return () => {
      eventSource.close();
    };
  }, []);

  if (error) {
    return (
      <div className="bg-red-500/10 border border-red-500/20 rounded-2xl p-4 text-red-400 text-sm">
        Failed to load metrics: {error}
      </div>
    );
  }

  if (!metrics) {
    return (
      <div className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-5 animate-pulse h-32" />
    );
  }

  return (
    <motion.div
      initial={false}
      animate={{ opacity: 1, scale: 1 }}
      className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6 relative overflow-hidden group"
    >
      <div className="absolute inset-0 bg-gradient-to-tl from-emerald-500/5 to-transparent pointer-events-none" />
      
      <h2 className="text-sm font-medium text-slate-300 mb-4 flex items-center gap-2 relative z-10">
        <Activity className="w-4 h-4 text-emerald-400" /> System Metrics
      </h2>
      <div className="grid grid-cols-2 gap-4 relative z-10">
        <div className="bg-slate-900/50 hover:bg-slate-900/80 transition-colors cursor-default rounded-xl p-4 flex flex-col items-center justify-center">
          <Server className="w-6 h-6 text-indigo-400 mb-2" />
          <span className="text-2xl font-semibold text-white">{metrics.activeWorkers ?? 0}</span>
          <span className="text-xs text-slate-400 font-medium tracking-wide">Workers</span>
        </div>
        
        <div className="bg-slate-900/50 hover:bg-slate-900/80 transition-colors cursor-default rounded-xl p-4 flex flex-col items-center justify-center relative overflow-hidden">
          <Database className="w-6 h-6 text-violet-400 mb-2 relative z-10" />
          <span className="text-2xl font-semibold text-white relative z-10">{metrics.queueDepth ?? 0}</span>
          <span className="text-xs text-slate-400 font-medium tracking-wide relative z-10">Queue</span>
        </div>
        
        <div className="col-span-2 bg-slate-900/50 rounded-xl p-4 space-y-2 mt-2">
          <div className="flex justify-between items-center">
            <span className="text-xs flex items-center gap-1.5 text-slate-300 font-medium">
              <Cpu className="w-3.5 h-3.5" /> Core Status
            </span>
            <span className="text-xs text-slate-400 font-mono tracking-tight">{metrics.memoryUsagePercent?.toFixed(1) ?? 0.0}%</span>
          </div>
          <div className="h-1.5 w-full bg-slate-800 rounded-full overflow-hidden">
            <div 
              className="h-full bg-gradient-to-r from-emerald-400 to-emerald-300 transition-all duration-500 ease-out"
              style={{ width: `${Math.min(100, Math.max(0, metrics.memoryUsagePercent ?? 0))}%` }}
            />
          </div>
        </div>
      </div>
    </motion.div>
  );
}
