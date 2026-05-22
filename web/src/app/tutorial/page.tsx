"use client";

import { useState, useEffect, useCallback, useMemo } from "react";
import { visualizeCustomCircuit } from "../actions";
import { ComplexNumber } from "../../components/types";
import { motion, AnimatePresence } from "framer-motion";
import { 
  BookOpen, 
  HelpCircle, 
  Lightbulb, 
  RotateCcw, 
  ArrowRight, 
  CheckCircle2, 
  Play, 
  Pause, 
  SkipForward, 
  SkipBack, 
  Sliders,
  Sparkles,
  Award,
  ChevronRight,
  BookOpenCheck
} from "lucide-react";
import { BlochSphere } from "../../components/BlochSphere";
import { stateToBloch } from "../../lib/quantum-math";
import { TutorialWavefunctionChart } from "../../components/TutorialWavefunctionChart";

// --- Types ---
interface GateEntry {
  id: string;
  type: string;
  label: string;
  target: number;
  control: number;
  step: number; // 1-indexed time step (e.g. 1 to 8)
  angle: number;
}

interface Lesson {
  id: number;
  title: string;
  difficulty: string;
  qubits: number;
  description: string;
  goalDescription: string;
  instructions: string[];
  hint: string;
  verify: (stateVector: ComplexNumber[], gates: GateEntry[]) => boolean;
}

// --- Lessons Configuration ---
const LESSONS: Lesson[] = [
  {
    id: 1,
    title: "Superposition (H-Gate)",
    difficulty: "Beginner",
    qubits: 1,
    description: "In classical computing, a bit is strictly 0 or 1. Quantum computing introduces superposition, which allows qubits to exist in a linear combination of both states simultaneously. The Hadamard (H) gate is the fundamental operator used to create superposition.",
    goalDescription: "Place a single Hadamard (H) gate on Qubit 0 to put it into a 50/50 superposition state.",
    instructions: [
      "Select the 'H' gate from the palette on the left.",
      "Click on Qubit 0 at Step 1 on the timeline grid to place the gate.",
      "Observe the final probabilities (50% |0⟩ and 50% |1⟩) and watch the Bloch sphere point horizontally."
    ],
    hint: "Click Qubit 0 at Step 1 with the H gate selected in the palette.",
    verify: (stateVector: ComplexNumber[]) => {
      if (!stateVector || stateVector.length < 2) return false;
      const p0 = stateVector[0].real ** 2 + stateVector[0].imag ** 2;
      const p1 = stateVector[1].real ** 2 + stateVector[1].imag ** 2;
      return Math.abs(p0 - 0.5) < 0.05 && Math.abs(p1 - 0.5) < 0.05;
    }
  },
  {
    id: 2,
    title: "Entanglement (CNOT & Bell State)",
    difficulty: "Intermediate",
    qubits: 2,
    description: "Quantum entanglement links qubits such that the state of one instantly dictates the state of another, regardless of distance. We will create the famous Bell state: (|00⟩ + |11⟩) / √2. When one qubit is measured, the other will always yield the same result.",
    goalDescription: "Create a Bell state by placing an H gate on Qubit 0, followed by a CNOT (CX) gate controlled by Qubit 0 targeting Qubit 1.",
    instructions: [
      "Place an 'H' gate on Qubit 0 at Step 1.",
      "Select the 'CX' (CNOT) gate from the palette.",
      "Click on Qubit 1 at Step 2 on the timeline grid to place the CNOT gate.",
      "Ensure the Control is set to Qubit 0 in the controller cards below the grid.",
      "Observe that the state vector now shows 50% probability for |00⟩ and 50% for |11⟩."
    ],
    hint: "Place H on q[0] at Step 1, then CX on q[1] at Step 2 with control q[0].",
    verify: (stateVector: ComplexNumber[]) => {
      if (!stateVector || stateVector.length < 4) return false;
      const p00 = stateVector[0].real ** 2 + stateVector[0].imag ** 2;
      const p01 = stateVector[1].real ** 2 + stateVector[1].imag ** 2;
      const p10 = stateVector[2].real ** 2 + stateVector[2].imag ** 2;
      const p11 = stateVector[3].real ** 2 + stateVector[3].imag ** 2;
      return (
        Math.abs(p00 - 0.5) < 0.05 &&
        Math.abs(p11 - 0.5) < 0.05 &&
        p01 < 0.01 &&
        p10 < 0.01
      );
    }
  },
  {
    id: 3,
    title: "Quantum Teleportation",
    difficulty: "Advanced",
    qubits: 3,
    description: "Quantum teleportation is a fundamental protocol that transfers the exact quantum state of a qubit (Qubit 0) to another qubit (Qubit 2) using shared entanglement and classical communication. We will build the quantum logic gates that facilitate this protocol.",
    goalDescription: "Construct the quantum teleportation circuit structure: 1) Entangle Qubit 1 and 2 (H on 1, CNOT 1->2). 2) Interact the state on Qubit 0 with Qubit 1 (CNOT 0->1, H on 0).",
    instructions: [
      "Place an 'H' gate on Qubit 1 at Step 1.",
      "Place a 'CX' gate on Qubit 2 at Step 2 (set control to Qubit 1).",
      "Place a 'CX' gate on Qubit 1 at Step 3 (set control to Qubit 0).",
      "Place an 'H' gate on Qubit 0 at Step 4.",
      "Observe the uniform distribution: 25% probability for each of |000⟩, |001⟩, |110⟩, and |111⟩."
    ],
    hint: "H on q[1] at Step 1. CX on q[2] (ctrl q[1]) at Step 2. CX on q[1] (ctrl q[0]) at Step 3. H on q[0] at Step 4.",
    verify: (stateVector: ComplexNumber[]) => {
      if (!stateVector || stateVector.length < 8) return false;
      const p0 = stateVector[0].real ** 2 + stateVector[0].imag ** 2; // 000
      const p1 = stateVector[1].real ** 2 + stateVector[1].imag ** 2; // 001
      const p6 = stateVector[6].real ** 2 + stateVector[6].imag ** 2; // 110
      const p7 = stateVector[7].real ** 2 + stateVector[7].imag ** 2; // 111
      return (
        Math.abs(p0 - 0.25) < 0.05 &&
        Math.abs(p1 - 0.25) < 0.05 &&
        Math.abs(p6 - 0.25) < 0.05 &&
        Math.abs(p7 - 0.25) < 0.05
      );
    }
  }
];

const GATE_PALETTE = [
  { type: "HADAMARD", label: "H", color: "bg-indigo-500/20 border-indigo-500/30 text-indigo-300 hover:bg-indigo-500/30" },
  { type: "PAULI_X", label: "X", color: "bg-rose-500/20 border-rose-500/30 text-rose-300 hover:bg-rose-500/30" },
  { type: "PAULI_Y", label: "Y", color: "bg-amber-500/20 border-amber-500/30 text-amber-300 hover:bg-amber-500/30" },
  { type: "PAULI_Z", label: "Z", color: "bg-emerald-500/20 border-emerald-500/30 text-emerald-300 hover:bg-emerald-500/30" },
  { type: "CNOT", label: "CX", color: "bg-cyan-500/20 border-cyan-500/30 text-cyan-300 hover:bg-cyan-500/30" },
  { type: "CZ", label: "CZ", color: "bg-teal-500/20 border-teal-500/30 text-teal-300 hover:bg-teal-500/30" },
  { type: "SWAP", label: "SWAP", color: "bg-violet-500/20 border-violet-500/30 text-violet-300 hover:bg-violet-500/30" },
];

const MAX_STEPS = 8;

export default function TutorialPage() {
  const [activeLessonIdx, setActiveLessonIdx] = useState(0);
  const [gates, setGates] = useState<GateEntry[]>([]);
  const [selectedPaletteGate, setSelectedPaletteGate] = useState<string>("HADAMARD");
  
  const [simulationSteps, setSimulationSteps] = useState<{ stateVector: ComplexNumber[]; probabilities: number[]; serverId: string }[] | null>(null);
  const [playbackStep, setPlaybackStep] = useState<number>(0);
  const [isPlaying, setIsPlaying] = useState<boolean>(false);
  const [simError, setSimError] = useState<string | null>(null);
  const [isCompleted, setIsCompleted] = useState<boolean>(false);
  const [selectedQubit, setSelectedQubit] = useState<number>(0);
  const [showHint, setShowHint] = useState<boolean>(false);

  const activeLesson = LESSONS[activeLessonIdx];

  const selectLesson = (idx: number) => {
    setActiveLessonIdx(idx);
    setGates([]);
    setSelectedQubit(0);
    setShowHint(false);
    setIsCompleted(false);
  };

  // Simulate circuit steps on gate modification
  useEffect(() => {
    const runSimulation = async () => {
      const dim = 1 << activeLesson.qubits;
      if (gates.length === 0) {
        // Initial state vector
        const initialSV = Array.from({ length: dim }, (_, i) => ({
          real: i === 0 ? 1 : 0,
          imag: 0,
        }));
        const initialProb = Array.from({ length: dim }, (_, i) => (i === 0 ? 1 : 0));
        setSimulationSteps([{ stateVector: initialSV, probabilities: initialProb, serverId: "local" }]);
        setPlaybackStep(0);
        setIsCompleted(false);
        return;
      }

      setSimError(null);
      try {
        const sortedGates = [...gates].sort((a, b) => a.step - b.step);
        const ops = sortedGates.map(g => ({
          type: g.type,
          targetQubit: g.target,
          controlQubit: g.control,
          angle: g.angle || 0,
        }));

        const res = await visualizeCustomCircuit(activeLesson.qubits, ops);
        if ("error" in res && res.error) {
          setSimError(res.error);
        } else if (!("error" in res)) {
          const initialSV = Array.from({ length: dim }, (_, i) => ({
            real: i === 0 ? 1 : 0,
            imag: 0,
          }));
          const initialProb = Array.from({ length: dim }, (_, i) => (i === 0 ? 1 : 0));
          const allSteps = [
            { stateVector: initialSV, probabilities: initialProb, serverId: "initial" },
            ...res.steps
          ];
          setSimulationSteps(allSteps);
          setPlaybackStep(allSteps.length - 1);

          // Verify if target state vector matches requirements
          const finalSV = allSteps[allSteps.length - 1].stateVector;
          const success = activeLesson.verify(finalSV, gates);
          setIsCompleted(success);
        }
      } catch (err) {
        setSimError(err instanceof Error ? err.message : String(err));
      }
    };

    runSimulation();
  }, [gates, activeLesson]);

  // Autoplay step playback
  useEffect(() => {
    let interval: NodeJS.Timeout | null = null;
    if (isPlaying && simulationSteps) {
      interval = setInterval(() => {
        setPlaybackStep(prev => {
          if (prev >= simulationSteps.length - 1) {
            setIsPlaying(false);
            return prev;
          }
          return prev + 1;
        });
      }, 1500);
    }
    return () => {
      if (interval) clearInterval(interval);
    };
  }, [isPlaying, simulationSteps]);

  // Add or remove gate from grid cell
  const handleGridClick = useCallback((qubit: number, step: number) => {
    // Check if there is an existing gate occupying this column for this qubit
    const existingIdx = gates.findIndex(g => g.step === step && (g.target === qubit || (g.type !== "HADAMARD" && g.type !== "PAULI_X" && g.type !== "PAULI_Y" && g.type !== "PAULI_Z" && g.control === qubit)));

    if (existingIdx !== -1) {
      // Toggle off: remove the gate
      setGates(prev => prev.filter((_, idx) => idx !== existingIdx));
      return;
    }

    // Add new gate from selected palette item
    const paletteGate = GATE_PALETTE.find(g => g.type === selectedPaletteGate);
    if (!paletteGate) return;

    const target = qubit;
    let control = qubit;

    // Set control/target mapping logic
    if (selectedPaletteGate === "CNOT" || selectedPaletteGate === "CZ") {
      control = qubit === 0 ? 1 : qubit - 1;
    } else if (selectedPaletteGate === "SWAP") {
      control = (qubit + 1) % activeLesson.qubits;
    }

    const newGate: GateEntry = {
      id: `${Date.now()}-${Math.random()}`,
      type: selectedPaletteGate,
      label: paletteGate.label,
      target,
      control,
      step,
      angle: 0,
    };

    setGates(prev => [...prev, newGate]);
  }, [gates, selectedPaletteGate, activeLesson.qubits]);

  // Update control qubit for CNOT/CZ/SWAP
  const updateGateControl = (id: string, newControl: number) => {
    setGates(prev => prev.map(g => g.id === id ? { ...g, control: newControl } : g));
  };

  const resetLesson = () => {
    setGates([]);
    setIsCompleted(false);
  };

  const handleNextLesson = () => {
    if (activeLessonIdx < LESSONS.length - 1) {
      selectLesson(activeLessonIdx + 1);
    }
  };

  // Derive Bloch vector for active playback step
  const activeStateVector = useMemo(() => {
    if (!simulationSteps || playbackStep >= simulationSteps.length) return undefined;
    return simulationSteps[playbackStep].stateVector;
  }, [simulationSteps, playbackStep]);

  const { theta, phi } = useMemo(() => {
    if (!activeStateVector) return { theta: 0, phi: 0 };
    return stateToBloch(activeStateVector, selectedQubit, activeLesson.qubits);
  }, [activeStateVector, selectedQubit, activeLesson.qubits]);

  // Active step gate context (helps highlight the executing gate)
  const activeGateAtStep = useMemo(() => {
    const sortedGates = [...gates].sort((a, b) => a.step - b.step);
    if (playbackStep === 0 || playbackStep > sortedGates.length) return null;
    return sortedGates[playbackStep - 1];
  }, [gates, playbackStep]);

  return (
    <div className="p-8 space-y-6 relative max-h-screen overflow-y-auto custom-scrollbar">
      {/* Dynamic Glowing Background Orbs */}
      <div className="fixed top-[-10%] right-[-5%] w-[450px] h-[450px] rounded-full bg-indigo-500/10 blur-[130px] pointer-events-none" />
      <div className="fixed bottom-[-15%] left-[10%] w-[550px] h-[550px] rounded-full bg-violet-600/10 blur-[150px] pointer-events-none" />

      {/* Header */}
      <div className="relative z-10 flex flex-col md:flex-row md:items-center md:justify-between gap-4">
        <div>
          <h1 className="text-2xl font-bold text-white flex items-center gap-2">
            <BookOpenCheck className="w-7 h-7 text-indigo-400" /> Quantum Academy
          </h1>
          <p className="text-sm text-slate-400 mt-1">Master quantum physics concepts through interactive challenges</p>
        </div>

        {/* Lesson Breadcrumb/Selector */}
        <div className="flex items-center gap-2 bg-slate-900/60 border border-white/5 p-1.5 rounded-xl">
          {LESSONS.map((lesson, idx) => (
            <button
              key={lesson.id}
              onClick={() => selectLesson(idx)}
              className={`px-3 py-1.5 rounded-lg text-xs font-semibold transition-all duration-200 ${
                activeLessonIdx === idx 
                  ? "bg-indigo-600 text-white shadow-md shadow-indigo-600/20" 
                  : "text-slate-400 hover:text-slate-200"
              }`}
            >
              Lesson {lesson.id}
            </button>
          ))}
        </div>
      </div>

      <div className="grid grid-cols-1 xl:grid-cols-12 gap-6 relative z-10">
        
        {/* Left sidebar: Lesson details & theory */}
        <div className="xl:col-span-4 space-y-5">
          <div className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6 flex flex-col min-h-[500px]">
            <div className="flex items-center justify-between mb-4">
              <span className={`text-[11px] font-bold uppercase tracking-wider px-2.5 py-0.5 rounded-full border ${
                activeLesson.difficulty === "Beginner" 
                  ? "bg-emerald-500/10 border-emerald-500/20 text-emerald-400" 
                  : activeLesson.difficulty === "Intermediate"
                  ? "bg-cyan-500/10 border-cyan-500/20 text-cyan-400"
                  : "bg-violet-500/10 border-violet-500/20 text-violet-400"
              }`}>
                {activeLesson.difficulty}
              </span>
              <span className="text-xs font-mono text-slate-500">Qubits: {activeLesson.qubits}</span>
            </div>

            <h2 className="text-xl font-bold text-white mb-3">{activeLesson.title}</h2>
            <p className="text-sm text-slate-300 leading-relaxed mb-5">{activeLesson.description}</p>

            <div className="bg-indigo-950/20 border border-indigo-500/15 p-4 rounded-xl mb-5">
              <h3 className="text-xs font-bold text-indigo-300 uppercase tracking-wider mb-1.5 flex items-center gap-1.5">
                <Award className="w-3.5 h-3.5" /> Target Challenge
              </h3>
              <p className="text-sm text-indigo-200">{activeLesson.goalDescription}</p>
            </div>

            <div className="space-y-3 mb-6">
              <h3 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2">Instructions</h3>
              {activeLesson.instructions.map((stepStr, idx) => (
                <div key={idx} className="flex gap-2.5 items-start text-xs text-slate-300">
                  <span className="flex-shrink-0 w-4.5 h-4.5 rounded-full bg-slate-800 text-[10px] flex items-center justify-center font-bold text-slate-400 border border-white/5 mt-0.5">{idx + 1}</span>
                  <p className="leading-normal">{stepStr}</p>
                </div>
              ))}
            </div>

            <div className="mt-auto pt-6 border-t border-white/5 flex flex-col gap-3">
              {/* Hint button */}
              <div className="w-full">
                <button
                  onClick={() => setShowHint(prev => !prev)}
                  className="w-full flex items-center justify-between text-xs text-slate-400 hover:text-slate-200 bg-white/5 border border-white/5 hover:bg-white/10 px-4 py-2.5 rounded-xl transition-all"
                >
                  <span className="flex items-center gap-1.5 font-medium"><Lightbulb className="w-3.5 h-3.5 text-amber-400" /> Need a Hint?</span>
                  <ChevronRight className={`w-3.5 h-3.5 transform transition-transform duration-200 ${showHint ? "rotate-90" : ""}`} />
                </button>
                <AnimatePresence>
                  {showHint && (
                    <motion.div
                      initial={{ height: 0, opacity: 0 }}
                      animate={{ height: "auto", opacity: 1 }}
                      exit={{ height: 0, opacity: 0 }}
                      className="overflow-hidden"
                    >
                      <p className="mt-2 text-xs text-amber-300/80 bg-amber-500/5 border border-amber-500/10 p-3 rounded-lg leading-relaxed font-mono">
                        {activeLesson.hint}
                      </p>
                    </motion.div>
                  )}
                </AnimatePresence>
              </div>

              {/* Reset button */}
              <button
                onClick={resetLesson}
                className="w-full bg-white/5 hover:bg-white/10 text-slate-300 border border-white/5 px-4 py-2.5 rounded-xl text-xs font-semibold flex items-center justify-center gap-1.5 transition-colors"
              >
                <RotateCcw className="w-3.5 h-3.5" /> Reset Circuit
              </button>
            </div>
          </div>
        </div>

        {/* Center/Right workspace */}
        <div className="xl:col-span-8 space-y-6">

          {/* Interactive Circuit Grid Editor */}
          <div className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6">
            <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4 mb-6">
              <div>
                <h3 className="text-sm font-semibold text-white flex items-center gap-2">
                  <Sliders className="w-4 h-4 text-indigo-400" /> Interactive Composer
                </h3>
                <p className="text-xs text-slate-400 mt-0.5">Click cells in the grid to place the active gate</p>
              </div>

              {/* Gate Palette selection */}
              <div className="flex flex-wrap items-center gap-1.5">
                {GATE_PALETTE.map(p => {
                  const isSelected = selectedPaletteGate === p.type;
                  return (
                    <button
                      key={p.type}
                      onClick={() => setSelectedPaletteGate(p.type)}
                      className={`px-3 py-1.5 rounded-lg text-xs font-bold border transition-all duration-200 ${
                        isSelected 
                          ? "bg-indigo-600 text-white border-indigo-400/30 scale-105 shadow-md shadow-indigo-600/20" 
                          : `${p.color} border-transparent`
                      }`}
                    >
                      {p.label}
                    </button>
                  );
                })}
              </div>
            </div>

            {/* SVG/HTML Grid */}
            <div className="overflow-x-auto py-6 bg-slate-950/40 rounded-xl border border-white/5 relative">
              <div className="min-w-[650px] px-8 relative flex flex-col gap-10">
                {Array.from({ length: activeLesson.qubits }).map((_, qIdx) => (
                  <div key={qIdx} className="flex items-center gap-6 h-12 relative z-10">
                    <span className="w-10 text-xs font-mono font-bold text-slate-400 flex-shrink-0">
                      q[{qIdx}]
                    </span>

                    {/* Timeline wire */}
                    <div className="absolute left-16 right-4 top-1/2 h-[2px] bg-slate-800 -translate-y-1/2 -z-10" />

                    <div className="flex-1 flex justify-between items-center gap-2">
                      {Array.from({ length: MAX_STEPS }).map((_, stepIdx) => {
                        const stepNum = stepIdx + 1;
                        
                        // Find if there is a gate target or control at this cell
                        const gate = gates.find(g => g.step === stepNum);
                        const isTarget = gate?.target === qIdx;
                        
                        const isCNOT = gate?.type === "CNOT";
                        const isCZ = gate?.type === "CZ";
                        const isSWAP = gate?.type === "SWAP";
                        
                        const isControl = (isCNOT || isCZ) && gate?.control === qIdx;
                        const isSecondTarget = isSWAP && gate?.control === qIdx;
                        const isCurrentPlaybackStep = playbackStep === stepNum;

                        return (
                          <div 
                            key={stepNum} 
                            className="flex-1 flex justify-center items-center h-12 relative"
                          >
                            {/* Visual connector line between control and target */}
                            {gate && isTarget && (isCNOT || isCZ || isSWAP) && (
                              <div 
                                className="absolute bg-sky-400 -z-20 w-[2px]"
                                style={{
                                  top: gate.target < gate.control ? "50%" : "auto",
                                  bottom: gate.target > gate.control ? "50%" : "auto",
                                  height: `${Math.abs(gate.target - gate.control) * 88}px`,
                                  left: "50%",
                                  transform: "translateX(-50%)"
                                }}
                              />
                            )}

                            {isTarget ? (
                              // Gate block
                              <button
                                onClick={() => handleGridClick(qIdx, stepNum)}
                                className={`w-10 h-10 rounded-xl font-bold text-xs flex items-center justify-center border shadow-lg cursor-pointer transform hover:scale-105 transition-all select-none ${
                                  isCurrentPlaybackStep 
                                    ? "bg-violet-500 border-violet-300 text-white scale-110 shadow-violet-500/20" 
                                    : gate.type === "HADAMARD" 
                                    ? "bg-indigo-600 border-indigo-400/40 text-white" 
                                    : gate.type === "PAULI_X" 
                                    ? "bg-rose-600 border-rose-400/40 text-white" 
                                    : gate.type === "PAULI_Y" 
                                    ? "bg-amber-600 border-amber-400/40 text-white" 
                                    : gate.type === "PAULI_Z" 
                                    ? "bg-emerald-600 border-emerald-400/40 text-white"
                                    : gate.type === "SWAP"
                                    ? "bg-violet-600 border-violet-400/40 text-white"
                                    : "bg-cyan-600 border-cyan-400/40 text-white"
                                }`}
                              >
                                {isCNOT ? "⊕" : isCZ ? "CZ" : isSWAP ? "×" : gate.label}
                              </button>
                            ) : isControl ? (
                              // Control node dot
                              <button
                                onClick={() => handleGridClick(qIdx, stepNum)}
                                className={`w-3.5 h-3.5 rounded-full bg-sky-400 border border-sky-300 cursor-pointer hover:scale-125 transition-transform shadow-md ${
                                  isCurrentPlaybackStep ? "scale-125 bg-violet-400 border-violet-300" : ""
                                }`}
                              />
                            ) : isSecondTarget ? (
                              // Swap second node symbol
                              <button
                                onClick={() => handleGridClick(qIdx, stepNum)}
                                className={`w-10 h-10 rounded-xl border border-dashed border-violet-400/30 text-violet-300 font-bold text-xs flex items-center justify-center cursor-pointer hover:bg-violet-500/10 ${
                                  isCurrentPlaybackStep ? "scale-110 border-violet-300 text-violet-200" : ""
                                }`}
                              >
                                ×
                              </button>
                            ) : (
                              // Empty grid slot
                              <button
                                onClick={() => handleGridClick(qIdx, stepNum)}
                                className="w-10 h-10 rounded-xl border border-dashed border-slate-700/50 flex items-center justify-center text-slate-700 hover:text-indigo-400 hover:border-indigo-500/40 hover:bg-indigo-500/5 transition-all cursor-pointer group"
                              >
                                <span className="text-[10px] font-mono group-hover:scale-125 transition-transform">+</span>
                              </button>
                            )}
                          </div>
                        );
                      })}
                    </div>
                  </div>
                ))}
              </div>
            </div>

            {/* Playback Controls */}
            <div className="mt-5 border-t border-white/5 pt-5 flex flex-col md:flex-row items-center justify-between gap-4">
              <div className="flex items-center gap-2">
                <button
                  onClick={() => setPlaybackStep(0)}
                  disabled={!simulationSteps || playbackStep === 0}
                  className="p-2 rounded-lg bg-white/5 hover:bg-white/10 text-slate-300 disabled:opacity-30 transition-colors"
                  title="Go to Start"
                >
                  <SkipBack className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => setPlaybackStep(prev => Math.max(0, prev - 1))}
                  disabled={!simulationSteps || playbackStep === 0}
                  className="p-2 rounded-lg bg-white/5 hover:bg-white/10 text-slate-300 disabled:opacity-30 transition-colors"
                  title="Previous Step"
                >
                  <span className="text-xs font-bold font-mono">Prev</span>
                </button>

                <button
                  onClick={() => setIsPlaying(prev => !prev)}
                  disabled={!simulationSteps || simulationSteps.length <= 1}
                  className="px-4 py-2 rounded-xl bg-indigo-500/20 hover:bg-indigo-500/30 text-indigo-300 border border-indigo-500/30 flex items-center gap-1.5 text-xs font-semibold transition-colors"
                >
                  {isPlaying ? (
                    <>
                      <Pause className="w-3.5 h-3.5" /> Pause
                    </>
                  ) : (
                    <>
                      <Play className="w-3.5 h-3.5" /> Play Steps
                    </>
                  )}
                </button>

                <button
                  onClick={() => setPlaybackStep(prev => Math.min(simulationSteps ? simulationSteps.length - 1 : 0, prev + 1))}
                  disabled={!simulationSteps || playbackStep === simulationSteps.length - 1}
                  className="p-2 rounded-lg bg-white/5 hover:bg-white/10 text-slate-300 disabled:opacity-30 transition-colors"
                  title="Next Step"
                >
                  <span className="text-xs font-bold font-mono">Next</span>
                </button>
                <button
                  onClick={() => setPlaybackStep(simulationSteps ? simulationSteps.length - 1 : 0)}
                  disabled={!simulationSteps || playbackStep === simulationSteps.length - 1}
                  className="p-2 rounded-lg bg-white/5 hover:bg-white/10 text-slate-300 disabled:opacity-30 transition-colors"
                  title="Go to End"
                >
                  <SkipForward className="w-3.5 h-3.5" />
                </button>
              </div>

              {/* Progress Slider */}
              <div className="flex-1 flex items-center gap-3 w-full md:w-auto md:max-w-md">
                <span className="text-xs font-mono text-slate-400">Step: {playbackStep} / {simulationSteps ? simulationSteps.length - 1 : 0}</span>
                <input
                  type="range"
                  min={0}
                  max={simulationSteps ? simulationSteps.length - 1 : 0}
                  value={playbackStep}
                  onChange={e => {
                    setPlaybackStep(Number(e.target.value));
                    setIsPlaying(false);
                  }}
                  className="flex-1 accent-indigo-500 cursor-pointer h-1 bg-slate-800 rounded-lg appearance-none"
                />
              </div>
            </div>

            {/* Gate Configuration Cards (Control selection for CNOT/CZ/SWAP) */}
            {gates.filter(g => g.type === "CNOT" || g.type === "CZ" || g.type === "SWAP").length > 0 && (
              <div className="mt-5 border-t border-white/5 pt-5">
                <h4 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-3">Gate Connections</h4>
                <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-3">
                  {gates
                    .filter(g => g.type === "CNOT" || g.type === "CZ" || g.type === "SWAP")
                    .sort((a, b) => a.step - b.step)
                    .map(g => (
                      <div key={g.id} className="bg-white/[0.02] border border-white/[0.05] p-3 rounded-xl flex items-center justify-between text-xs">
                        <div>
                          <span className="font-bold text-indigo-400 font-mono">Step {g.step}: </span>
                          <span className="text-slate-200">{g.label} on q[{g.target}]</span>
                        </div>
                        <div className="flex items-center gap-1.5">
                          <span className="text-slate-500 font-mono text-[10px]">Ctrl:</span>
                          <select
                            value={g.control}
                            onChange={e => updateGateControl(g.id, Number(e.target.value))}
                            className="bg-slate-900 border border-white/10 rounded px-1.5 py-0.5 text-white font-mono"
                          >
                            {Array.from({ length: activeLesson.qubits })
                              .map((_, i) => i)
                              .filter(i => i !== g.target)
                              .map(i => (
                                <option key={i} value={i}>q[{i}]</option>
                              ))}
                          </select>
                        </div>
                      </div>
                    ))}
                </div>
              </div>
            )}
          </div>

          {/* Visualizations Panel */}
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            
            {/* Wavefunction amplitudes chart */}
            <div className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6 flex flex-col min-h-[300px]">
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-sm font-semibold text-white flex items-center gap-2">
                  <BookOpen className="w-4 h-4 text-violet-400" /> Wavefunction Amplitudes
                </h3>
                <span className="text-[10px] font-mono text-slate-500">
                  {activeGateAtStep ? `After Gate: ${activeGateAtStep.label} at Step ${activeGateAtStep.step}` : "Initial State"}
                </span>
              </div>
              {simError ? (
                <div className="m-auto text-red-400 text-xs bg-red-500/10 px-4 py-3 rounded-lg border border-red-500/15 max-w-sm text-center">
                  {simError}
                </div>
              ) : (
                <TutorialWavefunctionChart 
                  stateVector={activeStateVector} 
                  numQubits={activeLesson.qubits} 
                />
              )}
            </div>

            {/* Bloch sphere visualization */}
            <div className="bg-white/[0.03] border border-white/[0.06] backdrop-blur-xl rounded-2xl p-6 flex flex-col">
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-sm font-semibold text-white flex items-center gap-2">
                  <HelpCircle className="w-4 h-4 text-emerald-400" /> Bloch Sphere
                </h3>
                
                {/* Qubit Selector */}
                {activeLesson.qubits > 1 && (
                  <select
                    value={selectedQubit}
                    onChange={e => setSelectedQubit(Number(e.target.value))}
                    className="bg-slate-900 border border-white/10 rounded px-2 py-0.5 text-xs text-white focus:outline-none font-mono"
                  >
                    {Array.from({ length: activeLesson.qubits }).map((_, i) => (
                      <option key={i} value={i}>q[{i}]</option>
                    ))}
                  </select>
                )}
              </div>

              <div className="flex-1 flex flex-col justify-center items-center">
                <BlochSphere theta={theta} phi={phi} animating={isPlaying} />
              </div>
            </div>
          </div>

          {/* Success Overlay / Lesson completion celebrate card */}
          <AnimatePresence>
            {isCompleted && (
              <motion.div
                initial={{ opacity: 0, y: 30 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0, y: 30 }}
                className="bg-emerald-500/10 border border-emerald-500/20 backdrop-blur-xl p-6 rounded-2xl relative overflow-hidden"
              >
                <div className="absolute top-[-20%] right-[-10%] w-44 h-44 bg-emerald-500/20 rounded-full blur-3xl pointer-events-none" />
                <div className="flex items-center gap-4 relative z-10">
                  <div className="p-3 bg-emerald-500/20 rounded-2xl border border-emerald-500/30 text-emerald-400">
                    <Sparkles className="w-6 h-6 animate-pulse" />
                  </div>
                  <div className="flex-1">
                    <h4 className="text-lg font-bold text-white flex items-center gap-1.5">
                      Challenge Completed! <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                    </h4>
                    <p className="text-xs text-emerald-200/80 mt-1">
                      Outstanding! You&apos;ve correctly engineered the circuit to achieve the target state vector distribution.
                    </p>
                  </div>
                  <div className="flex-shrink-0">
                    {activeLessonIdx < LESSONS.length - 1 ? (
                      <button
                        onClick={handleNextLesson}
                        className="bg-emerald-600 hover:bg-emerald-500 text-white font-semibold text-xs px-4 py-2.5 rounded-xl flex items-center gap-1.5 transition-colors shadow-lg shadow-emerald-600/20"
                      >
                        Next Lesson <ArrowRight className="w-3.5 h-3.5" />
                      </button>
                    ) : (
                      <div className="bg-emerald-500/20 border border-emerald-500/30 text-emerald-300 font-semibold text-xs px-4 py-2.5 rounded-xl flex items-center gap-1.5">
                        Course Finished <Award className="w-3.5 h-3.5 text-amber-400" />
                      </div>
                    )}
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>
        </div>
      </div>
    </div>
  );
}
