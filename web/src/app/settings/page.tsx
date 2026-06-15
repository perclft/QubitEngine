"use client";

import { useState, useEffect } from "react";
import { Save, Server, Key, Globe } from "lucide-react";
import { motion } from "framer-motion";
import { getSettingsAction, saveSettingsAction } from "../actions";

export default function SettingsPage() {
  const [engineAddr, setEngineAddr] = useState("127.0.0.1:50051");
  const [schedulerAddr, setSchedulerAddr] = useState("127.0.0.1:50053");
  const [authToken, setAuthToken] = useState("default-secret-token");
  const [saved, setSaved] = useState(false);

  useEffect(() => {
    getSettingsAction().then((settings) => {
      setEngineAddr(settings.engineAddr);
      setSchedulerAddr(settings.schedulerAddr);
      setAuthToken(settings.authToken);
    });
  }, []);

  const handleSave = async () => {
    await saveSettingsAction(engineAddr, schedulerAddr, authToken);
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  };

  return (
    <div className="p-8 space-y-6 relative">
      <div className="fixed bottom-[-10%] left-[-5%] w-[400px] h-[400px] rounded-full bg-slate-600/10 blur-[120px] pointer-events-none" />

      <div className="relative z-10">
        <h1 className="text-2xl font-semibold text-white">Settings</h1>
        <p className="text-sm text-slate-400 mt-1">Configure endpoints and authentication</p>
      </div>

      <div className="max-w-2xl relative z-10 space-y-4">
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }}
          className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-6 space-y-5"
        >
          <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest flex items-center gap-1.5">
            <Globe className="w-3.5 h-3.5" /> Service Endpoints
          </h3>

          <div>
            <label className="text-xs text-slate-400 flex items-center gap-1.5 mb-1.5">
              <Cpu className="w-3 h-3" /> C++ Engine Address
            </label>
            <input type="text" value={engineAddr} onChange={e => setEngineAddr(e.target.value)}
              className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2.5 text-sm text-white font-mono focus:outline-none focus:border-indigo-500/50"
            />
          </div>

          <div>
            <label className="text-xs text-slate-400 flex items-center gap-1.5 mb-1.5">
              <Server className="w-3 h-3" /> Go Scheduler Address
            </label>
            <input type="text" value={schedulerAddr} onChange={e => setSchedulerAddr(e.target.value)}
              className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2.5 text-sm text-white font-mono focus:outline-none focus:border-indigo-500/50"
            />
          </div>
        </motion.div>

        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }}
          className="bg-white/[0.03] border border-white/[0.06] rounded-2xl p-6 space-y-5"
        >
          <h3 className="text-xs font-medium text-slate-500 uppercase tracking-widest flex items-center gap-1.5">
            <Key className="w-3.5 h-3.5" /> Authentication
          </h3>

          <div>
            <label className="text-xs text-slate-400 mb-1.5 block">Bearer Token</label>
            <input type="password" value={authToken} onChange={e => setAuthToken(e.target.value)}
              className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2.5 text-sm text-white font-mono focus:outline-none focus:border-indigo-500/50"
            />
            <p className="text-[11px] text-slate-600 mt-1.5">Used for gRPC Authorization headers. Set via QUBIT_ENGINE_AUTH_TOKEN env var.</p>
          </div>
        </motion.div>

        <button onClick={handleSave}
          className="bg-indigo-500/20 hover:bg-indigo-500/30 border border-indigo-500/30 text-indigo-300 px-6 py-2.5 rounded-xl text-sm font-semibold flex items-center gap-2 transition-colors"
        >
          <Save className="w-4 h-4" />
          {saved ? "✓ Saved" : "Save Settings"}
        </button>
      </div>
    </div>
  );
}

function Cpu(props: React.SVGProps<SVGSVGElement>) {
  return (
    <svg {...props} xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none"
      stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect width="16" height="16" x="4" y="4" rx="2" />
      <rect width="6" height="6" x="9" y="9" rx="1" />
      <path d="M15 2v2" /><path d="M15 20v2" /><path d="M2 15h2" /><path d="M2 9h2" />
      <path d="M20 15h2" /><path d="M20 9h2" /><path d="M9 2v2" /><path d="M9 20v2" />
    </svg>
  );
}
