#!/usr/bin/env python3
"""
check_dead_code.py

Scans for potentially dead or unreferenced functions, classes, and exported symbols
across C++ (backend/src), Go (services/, pkg/), and Rust (cli-rs/src).

Enforces that any unreferenced symbol MUST be cataloged in `scripts/dead_code_allowlist.json`
with an explicit justification (e.g. public API, planned, or dead).
Fails with exit code 1 if any uncataloged dead code is found.
"""

import os
import re
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Set, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent
ALLOWLIST_FILE = REPO_ROOT / "scripts" / "dead_code_allowlist.json"

# Files / directories to exclude from candidate extraction and search
EXCLUDE_DIRS = {
    ".git", "build", "vcpkg_installed", "node_modules", "target",
    "__pycache__", ".vscode", "dist", "bin"
}

# Known common or interface symbols to ignore (standard overrides, entry points, etc.)
IGNORED_SYMBOLS = {
    "main", "init", "run", "setup", "teardown", "Close", "String", "Error",
    "Reset", "Execute", "Apply", "Clone", "Type", "Name", "Get", "Set",
    "Value", "Size", "Empty", "Clear", "Begin", "End", "Data", "Capacity",
    "OnMessage", "Connect", "Disconnect", "Start", "Stop", "Update", "Render",
    "Run", "Serve", "Status", "New", "Test", "Benchmark", "Info", "Warn", "Debug"
}

def is_excluded(path: Path) -> bool:
    for part in path.parts:
        if part in EXCLUDE_DIRS:
            return True
    return False

def get_all_source_files() -> List[Path]:
    sources = []
    for root, dirs, files in os.walk(REPO_ROOT):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        for f in files:
            ext = os.path.splitext(f)[1].lower()
            if ext in {".cpp", ".hpp", ".h", ".c", ".cc", ".go", ".rs", ".py"}:
                sources.append(Path(root) / f)
    return sources

def load_file_cache(files: List[Path]) -> Dict[Path, str]:
    cache = {}
    for f in files:
        try:
            cache[f] = f.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            cache[f] = ""
    return cache

def extract_cpp_symbols(file_path: Path, content: str) -> List[Tuple[str, int]]:
    symbols = []
    lines = content.splitlines()

    func_pattern = re.compile(r'^\s*(?:[\w:]+[\s*&]+)+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;{}]*\)\s*(?:const)?\s*(?:override)?\s*[{;]')
    class_pattern = re.compile(r'^\s*(?:class|struct)\s+(?:QUBIT_ENGINE_EXPORT\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\b')

    for line_idx, line in enumerate(lines, start=1):
        line = line.strip()
        if line.startswith("//") or line.startswith("/*") or line.startswith("*"):
            continue

        m_class = class_pattern.match(line)
        if m_class:
            name = m_class.group(1)
            if len(name) > 3 and name not in IGNORED_SYMBOLS:
                symbols.append((name, line_idx))
            continue

        m_func = func_pattern.match(line)
        if m_func:
            name = m_func.group(1)
            if name in {"if", "for", "while", "switch", "catch", "return", "sizeof", "decltype"} or name.startswith("operator"):
                continue
            if len(name) > 3 and name not in IGNORED_SYMBOLS:
                symbols.append((name, line_idx))

    return symbols

def extract_go_symbols(file_path: Path, content: str) -> List[Tuple[str, int]]:
    symbols = []
    lines = content.splitlines()

    func_pattern = re.compile(r'^func\s+(?:\([^)]+\)\s+)?([A-Z][a-zA-Z0-9_]*)\s*\(')
    type_pattern = re.compile(r'^type\s+([A-Z][a-zA-Z0-9_]*)\s+(?:struct|interface)')

    for line_idx, line in enumerate(lines, start=1):
        line = line.strip()
        if line.startswith("//"):
            continue

        m_func = func_pattern.match(line)
        if m_func:
            name = m_func.group(1)
            # Ignore test functions in test files
            if file_path.name.endswith("_test.go") and (name.startswith("Test") or name.startswith("Benchmark")):
                continue
            if name not in IGNORED_SYMBOLS:
                symbols.append((name, line_idx))
            continue

        m_type = type_pattern.match(line)
        if m_type:
            name = m_type.group(1)
            if name not in IGNORED_SYMBOLS:
                symbols.append((name, line_idx))

    return symbols

def extract_rust_symbols(file_path: Path, content: str) -> List[Tuple[str, int]]:
    symbols = []
    lines = content.splitlines()

    fn_pattern = re.compile(r'^\s*pub\s+(?:async\s+)?fn\s+([a-zA-Z_][a-zA-Z0-9_]*)\b')
    struct_pattern = re.compile(r'^\s*pub\s+(?:struct|enum)\s+([a-zA-Z_][a-zA-Z0-9_]*)\b')

    for line_idx, line in enumerate(lines, start=1):
        line = line.strip()
        if line.startswith("//"):
            continue

        m_fn = fn_pattern.match(line)
        if m_fn:
            name = m_fn.group(1)
            if name not in IGNORED_SYMBOLS:
                symbols.append((name, line_idx))
            continue

        m_struct = struct_pattern.match(line)
        if m_struct:
            name = m_struct.group(1)
            if name not in IGNORED_SYMBOLS:
                symbols.append((name, line_idx))

    return symbols

def main():
    parser = argparse.ArgumentParser(description="Scan repository for unreferenced/dead code symbols.")
    parser.add_argument("--update-allowlist", action="store_true", help="Update the allowlist file with current findings")
    args = parser.parse_args()

    print("Scanning repository for unreferenced / dead symbols across C++, Go, and Rust...")
    all_files = get_all_source_files()
    file_cache = load_file_cache(all_files)

    # Load allowlist if present
    allowlist = {}
    if ALLOWLIST_FILE.exists():
        try:
            with open(ALLOWLIST_FILE, "r", encoding="utf-8") as f:
                allowlist = json.load(f)
        except Exception as e:
            print(f"[WARN] Failed to read {ALLOWLIST_FILE}: {e}")

    candidates = []
    for f in all_files:
        rel = f.relative_to(REPO_ROOT)
        rel_str = str(rel).replace("\\", "/")

        if rel_str.startswith("backend/src"):
            syms = extract_cpp_symbols(f, file_cache[f])
            for s, line in syms:
                candidates.append((f, s, line, "cpp"))
        elif rel_str.startswith("services/") or rel_str.startswith("pkg/"):
            syms = extract_go_symbols(f, file_cache[f])
            for s, line in syms:
                candidates.append((f, s, line, "go"))
        elif rel_str.startswith("cli-rs/src"):
            syms = extract_rust_symbols(f, file_cache[f])
            for s, line in syms:
                candidates.append((f, s, line, "rust"))

    unreferenced = []
    unallowed = []

    for f, sym, line, lang in candidates:
        rel_path = str(f.relative_to(REPO_ROOT)).replace("\\", "/")
        pattern = re.compile(r'\b' + re.escape(sym) + r'\b')

        usages = 0
        for other_file, content in file_cache.items():
            if other_file == f:
                matches = pattern.findall(content)
                if len(matches) > 1:
                    usages += (len(matches) - 1)
            else:
                if pattern.search(content):
                    usages += 1

        if usages == 0:
            unreferenced.append((rel_path, sym, line))
            # Check against allowlist
            allow_key_specific = f"{rel_path}:{line}:{sym}"
            allow_key_loose = f"{rel_path}:{sym}"
            if allow_key_specific not in allowlist and allow_key_loose not in allowlist:
                unallowed.append((rel_path, sym, line))

    print(f"\n--- Scan Complete: {len(unreferenced)} unreferenced symbols found ---")
    if unallowed:
        print(f"\n[FAIL] {len(unallowed)} uncataloged dead code symbols detected (not in dead_code_allowlist.json):")
        for p, s, l in unallowed:
            print(f"  [{p}:{l}] {s} (external usages: 0)")
        sys.exit(1)
    else:
        print(f"[PASS] All {len(unreferenced)} unreferenced symbols are documented with justifications in dead_code_allowlist.json.")
        sys.exit(0)

if __name__ == "__main__":
    main()
