import { ComplexNumber } from "./types";
import { motion } from "framer-motion";

interface TutorialWavefunctionChartProps {
  stateVector: ComplexNumber[] | undefined;
  numQubits: number;
}

export function TutorialWavefunctionChart({
  stateVector,
  numQubits,
}: TutorialWavefunctionChartProps) {
  if (!stateVector) {
    return (
      <div className="m-auto flex flex-col items-center justify-center gap-2 opacity-30 py-12">
        <div className="w-6 h-6 border-2 border-indigo-400 border-t-transparent rounded-full animate-spin" />
        <p className="text-sm">Awaiting simulation...</p>
      </div>
    );
  }

  return (
    <div className="flex-grow overflow-x-auto custom-scrollbar w-full">
      <div className="flex items-end justify-start min-w-max gap-6 h-36 border-b border-white/5 pb-4 px-4 mx-auto w-fit">
        {stateVector.map((sv, i) => {
          const prob = sv.real * sv.real + sv.imag * sv.imag;
          const height = Math.max(prob * 100, 3);
          const bitstring = i.toString(2).padStart(numQubits, "0");
          return (
            <div key={i} className="flex flex-col items-center gap-1.5 min-w-[48px]">
              <span className="text-[10px] text-indigo-300 font-mono">{(prob * 100).toFixed(0)}%</span>
              <motion.div
                initial={{ height: 0 }}
                animate={{ height: `${height}%` }}
                transition={{ type: "spring", bounce: 0.4 }}
                className="w-8 rounded-t bg-gradient-to-t from-indigo-600/30 to-indigo-400 border border-indigo-400/40 shadow-[0_0_10px_rgba(99,102,241,0.2)]"
              />
              <span className="text-[11px] font-mono text-slate-400">|{bitstring}⟩</span>
            </div>
          );
        })}
      </div>
    </div>
  );
}
