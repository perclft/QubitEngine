import { motion } from "framer-motion";
import { TopologyData } from "./types";

interface Props {
  topology: TopologyData;
}

export function TopologyGraph({ topology }: Props) {
  return (
    <motion.div
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6 relative z-10"
    >
      <h3 className="text-xs font-medium text-slate-500 mb-4 uppercase tracking-widest">Hardware Topology</h3>
      <div className="w-full h-[280px] flex items-center justify-center">
        <svg viewBox="0 0 150 100" className="w-full h-full max-w-xl opacity-80">
          {topology.edges.map((edge, i) => {
            const n1 = topology.nodes.find((n) => n.id === edge.node1);
            const n2 = topology.nodes.find((n) => n.id === edge.node2);
            if (!n1 || !n2) return null;
            return (
              <line key={`e-${i}`} x1={n1.x} y1={n1.y} x2={n2.x} y2={n2.y}
                stroke="rgba(139, 92, 246, 0.35)" strokeWidth="1.5" />
            );
          })}
          {topology.nodes.map((node) => (
            <g key={`n-${node.id}`}>
              <circle cx={node.x} cy={node.y} r="3" fill="#8b5cf6" />
              <text x={node.x} y={node.y - 5} fill="#94a3b8" fontSize="3.5" textAnchor="middle" className="font-mono">
                Q{node.id}
              </text>
            </g>
          ))}
        </svg>
      </div>
    </motion.div>
  );
}
