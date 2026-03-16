"use client";

import { useState } from "react";
import { visualizeCircuit } from "../actions";
import { motion, AnimatePresence } from "framer-motion";
import { Waves, Play } from "lucide-react";

export default function VisualizerPage() {
  const [numQubits, setNumQubits] = useState(3);
  const [isRunning, setIsRunning] = useState(false);
  const [steps, setSteps] = useState<any[]>([]);
  const [currentStep, setCurrentStep] = useState(0);
  const [error, setError] = useState<string | null>(null);

  const handleVisualize = async () => {
    setIsRunning(true);
    setSteps([]);
    setCurrentStep(0);
    setError(null);
    try {
      const res: any = await visualizeCircuit(numQubits);
      if (res.error) {
        setError(res.error);
      } else if (res.steps) {
        setSteps(res.steps);
      }
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setIsRunning(false);
    }
  };

  const step = steps[currentStep];

  return (
    <div className="p-8 space-y-6 relative">
      <div className="fixed bottom-[-10%] right-[-5%] w-[500px] h-[500px] rounded-full bg-violet-600/10 blur-[120px] pointer-events-none" />

      <div className="relative z-10">
        <h1 className="text-2xl font-semibold text-white">Circuit Visualizer</h1>
        <p className="text-sm text-slate-400 mt-1">Watch quantum state evolution gate-by-gate</p>
      </div>

      <div className="grid grid-cols-1 xl:grid-cols-4 gap-6 relative z-10">
        {/* Controls */}
        <div className="space-y-4">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5 space-y-4">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest">Config</h3>
            <div>
              <label className="text-xs text-slate-400 block mb-1">Qubits</label>
              <input type="number" min={1} max={8} value={numQubits}
                onChange={e => setNumQubits(Number(e.target.value))}
                className="w-full bg-white/5 border border-white/10 rounded-lg px-3 py-2 text-sm text-white focus:outline-none"
              />
            </div>
            <button onClick={handleVisualize} disabled={isRunning}
              className="w-full bg-violet-500/20 hover:bg-violet-500/30 border border-violet-500/30 text-violet-300 px-4 py-2.5 rounded-xl text-sm font-semibold flex items-center justify-center gap-2 disabled:opacity-30 transition-colors"
            >
              {isRunning ? <div className="w-4 h-4 border-2 border-violet-400 border-t-transparent rounded-full animate-spin" /> : <Play className="w-4 h-4" />}
              Visualize GHZ
            </button>
          </div>

          {/* Step Controls */}
          {steps.length > 0 && (
            <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-5">
              <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-3">Step Navigator</h3>
              <div className="flex items-center gap-3">
                <button onClick={() => setCurrentStep(Math.max(0, currentStep - 1))}
                  disabled={currentStep === 0}
                  className="bg-white/5 border border-white/10 rounded-lg px-3 py-1.5 text-xs font-medium disabled:opacity-30 hover:bg-white/10 transition-colors"
                >
                  ← Prev
                </button>
                <span className="text-sm text-slate-300 font-mono flex-1 text-center">
                  {currentStep + 1} / {steps.length}
                </span>
                <button onClick={() => setCurrentStep(Math.min(steps.length - 1, currentStep + 1))}
                  disabled={currentStep === steps.length - 1}
                  className="bg-white/5 border border-white/10 rounded-lg px-3 py-1.5 text-xs font-medium disabled:opacity-30 hover:bg-white/10 transition-colors"
                >
                  Next →
                </button>
              </div>
              <input type="range" min={0} max={steps.length - 1} value={currentStep}
                onChange={e => setCurrentStep(Number(e.target.value))}
                className="w-full accent-violet-500 mt-3"
              />
            </div>
          )}
        </div>

        {/* Visualization */}
        <div className="xl:col-span-3">
          <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-6 min-h-[450px] flex flex-col">
            <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest mb-6">
              <Waves className="w-3.5 h-3.5 inline mr-1.5" />
              State After Gate {currentStep + 1}
            </h3>
            {error ? (
              <div className="m-auto text-red-400 text-sm bg-red-500/10 px-4 py-3 rounded-lg border border-red-500/15">{error}</div>
            ) : step ? (
              <div className="flex-1 flex items-end justify-center gap-3 flex-wrap pb-6 border-b border-white/5">
                <AnimatePresence mode="wait">
                  {step.probabilities.map((prob: number, i: number) => {
                    if (prob < 0.001) return null;
                    const height = Math.max(prob * 100, 3);
                    const bits = i.toString(2).padStart(numQubits, "0");
                    return (
                      <motion.div
                        key={`${currentStep}-${i}`}
                        initial={{ height: 0, opacity: 0 }}
                        animate={{ height: "auto", opacity: 1 }}
                        transition={{ type: "spring", bounce: 0.3 }}
                        className="flex flex-col items-center gap-1.5"
                      >
                        <span className="text-[10px] text-violet-300 font-mono">{(prob * 100).toFixed(1)}%</span>
                        <motion.div
                          initial={{ height: 0 }}
                          animate={{ height: `${height * 3}px` }}
                          transition={{ type: "spring", bounce: 0.3 }}
                          className="w-10 rounded-t bg-gradient-to-t from-violet-600/30 to-violet-400 border border-violet-400/30"
                          style={{ minHeight: 4 }}
                        />
                        <span className="text-[9px] font-mono text-slate-500">|{bits}⟩</span>
                      </motion.div>
                    );
                  })}
                </AnimatePresence>
              </div>
            ) : (
              <div className="m-auto opacity-20 text-sm text-center">
                <Waves className="w-10 h-10 mx-auto mb-2" />
                Run a visualization to see step-by-step evolution
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
