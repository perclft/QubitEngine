import { motion } from "framer-motion";
import { Activity, Cpu } from "lucide-react";
import { ExecutionResult } from "./types";

interface Props {
  result: ExecutionResult | null;
  error: string | null;
}

export function WavefunctionChart({ result, error }: Props) {
  return (
    <motion.div
      initial={{ opacity: 0, x: 20 }}
      animate={{ opacity: 1, x: 0 }}
      className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6 min-h-[280px] flex flex-col"
    >
      <h2 className="text-sm font-medium text-slate-300 mb-6 flex items-center gap-2">
        <Cpu className="w-4 h-4 text-violet-400" /> Quantum State Distribution
      </h2>

      {error ? (
        <div className="m-auto text-red-400 text-sm bg-red-500/10 px-4 py-3 rounded-lg border border-red-500/15 max-w-md text-center">
          {error}
        </div>
      ) : result ? (
        <div className="flex-1 flex flex-col justify-end">
          <div className="flex items-end justify-center gap-10 h-44 border-b border-white/5 pb-4">
            {result.results.map((r, i) => {
              const height = Math.max(r.probability * 100, 3);
              const bitstring = r.index.toString(2).padStart(3, "0");
              return (
                <div key={i} className="flex flex-col items-center gap-2">
                  <span className="text-xs text-indigo-300 font-mono">{(r.probability * 100).toFixed(1)}%</span>
                  <motion.div
                    initial={{ height: 0 }}
                    animate={{ height: `${height}%` }}
                    transition={{ type: "spring", bounce: 0.4 }}
                    className="w-14 rounded-t-lg bg-gradient-to-t from-indigo-600/30 to-indigo-400 border border-indigo-400/40"
                  />
                  <span className="text-xs font-mono text-slate-400">|{bitstring}⟩</span>
                </div>
              );
            })}
          </div>
        </div>
      ) : (
        <div className="m-auto flex flex-col items-center gap-2 opacity-30">
          <Activity className="w-10 h-10" />
          <p className="text-sm">Awaiting execution...</p>
        </div>
      )}
    </motion.div>
  );
}
