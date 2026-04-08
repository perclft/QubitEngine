'use client';

import { useEffect } from 'react';
import { AlertCircle } from 'lucide-react';

export default function Error({
  error,
  reset,
}: {
  error: Error & { digest?: string };
  reset: () => void;
}) {
  useEffect(() => {
    console.error("Application Error Boundry Caught:", error);
  }, [error]);

  return (
    <div className="flex flex-col items-center justify-center min-h-[70vh] px-4 space-y-6">
      <div className="p-4 bg-red-950/30 border border-red-900/50 rounded-xl flex items-center justify-center p-6 shadow-2xl">
        <AlertCircle className="w-12 h-12 text-red-500 mb-4 mx-auto" />
      </div>
      <h2 className="text-2xl font-bold bg-clip-text text-transparent bg-gradient-to-r from-red-400 to-rose-300">
        Engine Disrupted
      </h2>
      <p className="text-slate-400 max-w-md text-center">
        {error.message || "An unexpected disturbance occurred while rendering this module."}
      </p>
      <button
        onClick={() => reset()}
        className="px-6 py-2.5 rounded-lg font-medium tracking-wide bg-gradient-to-r from-teal-500 to-emerald-500 text-white shadow-lg shadow-emerald-500/20 hover:shadow-emerald-500/40 transition-all duration-200 active:scale-95"
      >
        Reinitialize Module
      </button>
    </div>
  );
}
