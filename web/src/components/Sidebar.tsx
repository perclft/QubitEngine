"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { Zap, LayoutDashboard, Cpu, FlaskConical, ListTodo, Settings, Waves, BookOpen } from "lucide-react";
import { motion } from "framer-motion";

const navItems = [
  { href: "/", label: "Dashboard", icon: LayoutDashboard },
  { href: "/circuit-lab", label: "Circuit Lab", icon: Cpu },
  { href: "/tutorial", label: "Tutorial Mode", icon: BookOpen },
  { href: "/vqe", label: "VQE Explorer", icon: FlaskConical },
  { href: "/visualizer", label: "Visualizer", icon: Waves },
  { href: "/jobs", label: "Job Queue", icon: ListTodo },
  { href: "/settings", label: "Settings", icon: Settings },
];

export default function Sidebar() {
  const pathname = usePathname();

  return (
    <aside className="fixed top-0 left-0 h-screen w-64 bg-slate-950/80 backdrop-blur-2xl border-r border-white/5 flex flex-col z-50">
      {/* Logo */}
      <div className="px-6 py-6 flex items-center gap-3 border-b border-white/5">
        <div className="p-2.5 bg-indigo-500/20 rounded-xl border border-indigo-500/30">
          <Zap className="w-5 h-5 text-indigo-400" />
        </div>
        <div>
          <h1 className="text-lg font-semibold tracking-tight text-white">QubitEngine</h1>
          <p className="text-[11px] text-slate-500 font-medium tracking-wider uppercase">Hub</p>
        </div>
      </div>

      {/* Nav */}
      <nav className="flex-1 px-3 py-4 space-y-1 overflow-y-auto">
        {navItems.map((item) => {
          const isActive = pathname === item.href;
          const Icon = item.icon;
          return (
            <Link key={item.href} href={item.href}>
              <div className={`relative flex items-center gap-3 px-3 py-2.5 rounded-xl text-sm font-medium transition-all duration-200 group
                ${isActive
                  ? "text-white"
                  : "text-slate-400 hover:text-slate-200 hover:bg-white/5"
                }`}
              >
                {isActive && (
                  <motion.div
                    layoutId="sidebar-active"
                    className="absolute inset-0 bg-indigo-500/10 border border-indigo-500/20 rounded-xl"
                    transition={{ type: "spring", bounce: 0.2, duration: 0.5 }}
                  />
                )}
                <Icon className={`w-4.5 h-4.5 relative z-10 ${isActive ? "text-indigo-400" : "text-slate-500 group-hover:text-slate-400"}`} />
                <span className="relative z-10">{item.label}</span>
              </div>
            </Link>
          );
        })}
      </nav>

      {/* Status Footer */}
      <div className="px-4 py-4 border-t border-white/5">
        <div className="flex items-center gap-2 px-3 py-2 bg-emerald-500/5 border border-emerald-500/10 rounded-lg">
          <div className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
          <span className="text-[11px] font-medium text-emerald-300/80">Engine Online</span>
        </div>
      </div>
    </aside>
  );
}
