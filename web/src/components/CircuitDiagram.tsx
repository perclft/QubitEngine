"use client";

import React from 'react';
import { motion } from 'framer-motion';

interface Gate {
  type: string;
  targets: number[];
  controls?: number[];
  params?: number[];
}

interface CircuitDiagramProps {
  numQubits: number;
  gates: Gate[];
  width?: number;
}

const GATE_COLORS: Record<string, string> = {
  'H': 'bg-indigo-500',
  'X': 'bg-rose-500',
  'Y': 'bg-emerald-500',
  'Z': 'bg-amber-500',
  'CNOT': 'bg-sky-500',
  'CZ': 'bg-cyan-500',
  'S': 'bg-violet-500',
  'T': 'bg-purple-500',
  'RX': 'bg-fuchsia-500',
  'RY': 'bg-pink-500',
  'RZ': 'bg-orange-500',
};

export function CircuitDiagram({ numQubits, gates, width = 800 }: CircuitDiagramProps) {
  const qubitSpacing = 40;
  const gateSpacing = 60;
  const padding = 40;
  const height = numQubits * qubitSpacing + padding * 2;
  const totalWidth = Math.max(width, gates.length * gateSpacing + padding * 2);

  return (
    <div className="overflow-x-auto bg-slate-900/40 rounded-2xl border border-white/[0.06] backdrop-blur-xl p-6">
      <h3 className="text-xs font-semibold text-slate-400 uppercase tracking-wider mb-6 flex items-center gap-2">
        <div className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
        Circuit Visualization
      </h3>
      
      <svg width={totalWidth} height={height} className="block mx-auto">
        {/* Qubit Lines */}
        {Array.from({ length: numQubits }).map((_, i) => (
          <React.Fragment key={`qubit-${i}`}>
            <text
              x={padding - 10}
              y={padding + i * qubitSpacing + 4}
              textAnchor="end"
              className="fill-slate-500 text-[10px] font-mono font-bold"
            >
              q[{i}]
            </text>
            <line
              x1={padding}
              y1={padding + i * qubitSpacing}
              x2={totalWidth - padding}
              y2={padding + i * qubitSpacing}
              className="stroke-slate-700/50 stroke-1"
            />
          </React.Fragment>
        ))}

        {/* Gates */}
        {gates.map((gate, idx) => {
          const x = padding + idx * gateSpacing + 30;
          
          return (
            <motion.g
              key={`gate-${idx}`}
              initial={{ opacity: 0, x: x - 20 }}
              animate={{ opacity: 1, x: x }}
              transition={{ delay: idx * 0.05 }}
            >
              {gate.controls?.map(control => (
                <React.Fragment key={`control-${control}`}>
                   {/* Vertical line from control to target */}
                   <line
                    x1={0}
                    y1={padding + control * qubitSpacing}
                    x2={0}
                    y2={padding + gate.targets[0] * qubitSpacing}
                    className="stroke-sky-400 stroke-1"
                  />
                  <circle
                    cx={0}
                    cy={padding + control * qubitSpacing}
                    r={3}
                    className="fill-sky-400"
                  />
                </React.Fragment>
              ))}

              {gate.targets.map(target => {
                const isCNOT = gate.type === 'CNOT';
                const color = GATE_COLORS[gate.type] || 'bg-slate-600';
                
                if (isCNOT) {
                  return (
                    <g key={`target-${target}`}>
                      <circle
                        cx={0}
                        cy={padding + target * qubitSpacing}
                        r={8}
                        className="fill-slate-900 stroke-sky-400 stroke-1"
                      />
                      <line x1={-5} y1={padding + target * qubitSpacing} x2={5} y2={padding + target * qubitSpacing} className="stroke-sky-400 stroke-1" />
                      <line x1={0} y1={padding + target * qubitSpacing - 5} x2={0} y2={padding + target * qubitSpacing + 5} className="stroke-sky-400 stroke-1" />
                    </g>
                  );
                }

                return (
                  <rect
                    key={`target-${target}`}
                    x={-12}
                    y={padding + target * qubitSpacing - 12}
                    width={24}
                    height={24}
                    rx={4}
                    className={`${color} stroke-white/20 stroke-1`}
                  />
                );
              })}

              <text
                x={0}
                y={padding + gate.targets[0] * qubitSpacing + 4}
                textAnchor="middle"
                className="fill-white text-[9px] font-bold pointer-events-none"
              >
                {gate.type === 'CNOT' ? '' : gate.type}
              </text>
            </motion.g>
          );
        })}
      </svg>
    </div>
  );
}
