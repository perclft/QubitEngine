"use client";

import { useState, useEffect, useCallback } from "react";
import { listJobs, getJobStatus } from "../actions";
import { motion, AnimatePresence } from "framer-motion";
import { ListTodo, RefreshCw, Clock, CheckCircle2, XCircle, Loader2, Ban } from "lucide-react";

const STATE_LABELS: Record<number, { label: string; color: string; icon: any }> = {
  0: { label: "Unknown", color: "text-slate-400 bg-slate-500/10", icon: Clock },
  1: { label: "Queued", color: "text-amber-300 bg-amber-500/10", icon: Clock },
  2: { label: "Running", color: "text-blue-300 bg-blue-500/10", icon: Loader2 },
  3: { label: "Completed", color: "text-emerald-300 bg-emerald-500/10", icon: CheckCircle2 },
  4: { label: "Failed", color: "text-red-300 bg-red-500/10", icon: XCircle },
  5: { label: "Cancelled", color: "text-slate-300 bg-slate-500/10", icon: Ban },
};

export default function JobsPage() {
  const [jobs, setJobs] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [autoRefresh, setAutoRefresh] = useState(false);

  const fetchJobs = useCallback(async () => {
    setIsLoading(true);
    try {
      const res: any = await listJobs();
      if (res.error) {
        setError(res.error);
      } else {
        setJobs(res.jobs || []);
        setError(null);
      }
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setIsLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchJobs();
  }, [fetchJobs]);

  useEffect(() => {
    if (!autoRefresh) return;
    const interval = setInterval(fetchJobs, 3000);
    return () => clearInterval(interval);
  }, [autoRefresh, fetchJobs]);

  return (
    <div className="p-8 space-y-6 relative">
      <div className="fixed top-[10%] right-[-5%] w-[400px] h-[400px] rounded-full bg-amber-600/10 blur-[120px] pointer-events-none" />

      <div className="relative z-10 flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold text-white">Job Queue</h1>
          <p className="text-sm text-slate-400 mt-1">Monitor submitted quantum computation jobs</p>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={() => setAutoRefresh(!autoRefresh)}
            className={`px-3 py-1.5 rounded-lg text-xs font-medium border transition-colors ${
              autoRefresh
                ? "bg-emerald-500/20 border-emerald-500/30 text-emerald-300"
                : "bg-white/5 border-white/10 text-slate-400"
            }`}
          >
            {autoRefresh ? "⏸ Pause" : "▶ Auto-Refresh"}
          </button>
          <button onClick={fetchJobs} disabled={isLoading}
            className="bg-white/5 border border-white/10 hover:bg-white/10 px-3 py-1.5 rounded-lg text-xs font-medium text-slate-400 flex items-center gap-1.5 disabled:opacity-30 transition-colors"
          >
            <RefreshCw className={`w-3 h-3 ${isLoading ? "animate-spin" : ""}`} />
            Refresh
          </button>
        </div>
      </div>

      {/* Job Table */}
      <div className="relative z-10">
        <div className="bg-white/[0.03] border border-white/[0.06] rounded-2xl overflow-hidden">
          {/* Header */}
          <div className="grid grid-cols-6 gap-4 px-5 py-3 border-b border-white/[0.04] text-[11px] font-medium text-slate-500 uppercase tracking-widest">
            <div>Job ID</div>
            <div>Status</div>
            <div>Queue</div>
            <div>Progress</div>
            <div>Worker</div>
            <div>Error</div>
          </div>

          {error ? (
            <div className="px-5 py-8 text-center text-sm text-slate-400">
              <p className="text-red-400 mb-1">Unable to fetch jobs</p>
              <p className="text-xs text-slate-500">{error}</p>
              <p className="text-xs text-slate-600 mt-2">Ensure the Go Scheduler is running on port 50053</p>
            </div>
          ) : jobs.length === 0 ? (
            <div className="px-5 py-12 text-center">
              <ListTodo className="w-8 h-8 mx-auto mb-2 opacity-20" />
              <p className="text-sm text-slate-500">No jobs in queue</p>
              <p className="text-xs text-slate-600 mt-1">Submit jobs via the CLI or Circuit Lab</p>
            </div>
          ) : (
            <AnimatePresence>
              {jobs.map((job: any, i: number) => {
                const state = STATE_LABELS[job.state] || STATE_LABELS[0];
                const Icon = state.icon;
                return (
                  <motion.div
                    key={job.jobId || i}
                    initial={{ opacity: 0, y: -5 }}
                    animate={{ opacity: 1, y: 0 }}
                    exit={{ opacity: 0 }}
                    className="grid grid-cols-6 gap-4 px-5 py-3 border-b border-white/[0.02] hover:bg-white/[0.02] transition-colors text-sm"
                  >
                    <div className="font-mono text-xs text-slate-300 truncate">{job.jobId || "—"}</div>
                    <div>
                      <span className={`inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[11px] font-medium ${state.color}`}>
                        <Icon className={`w-3 h-3 ${job.state === 2 ? "animate-spin" : ""}`} />
                        {state.label}
                      </span>
                    </div>
                    <div className="text-xs text-slate-400">{job.positionInQueue || "—"}</div>
                    <div>
                      <div className="w-full bg-white/5 rounded-full h-1.5">
                        <div className="bg-indigo-500 h-1.5 rounded-full transition-all" style={{ width: `${job.progressPercent || 0}%` }} />
                      </div>
                    </div>
                    <div className="text-xs text-slate-400 font-mono truncate">{job.workerId || "—"}</div>
                    <div className="text-xs text-red-400 truncate">{job.errorMessage || "—"}</div>
                  </motion.div>
                );
              })}
            </AnimatePresence>
          )}
        </div>
      </div>
    </div>
  );
}
