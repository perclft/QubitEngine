export default function Loading() {
  return (
    <div className="flex flex-col items-center justify-center min-h-[70vh] space-y-6">
      <div className="relative">
        {/* Outer rotating ring */}
        <div className="absolute inset-0 border-t-2 border-r-2 border-teal-500 rounded-full animate-[spin_1.5s_linear_infinite] w-16 h-16 opacity-70"></div>
        {/* Inner rotating ring (opposite direction) */}
        <div className="absolute inset-2 border-b-2 border-l-2 border-indigo-500 rounded-full animate-[spin_1s_linear_infinite_reverse] w-12 h-12 opacity-80"></div>
        {/* Core block */}
        <div className="w-16 h-16 flex items-center justify-center bg-slate-900 rounded-full shadow-[0_0_15px_rgba(20,184,166,0.3)]">
          <div className="w-3 h-3 bg-teal-400 rounded-full animate-pulse"></div>
        </div>
      </div>
      <h3 className="text-lg font-medium text-slate-400 animate-pulse tracking-widest uppercase">
        Initializing...
      </h3>
    </div>
  );
}
