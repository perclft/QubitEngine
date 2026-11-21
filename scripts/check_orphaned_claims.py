#!/usr/bin/env python3
"""
check_orphaned_claims.py

Scans repository documentation (README.md, docs/*.md, etc.) for:
1. Relative markdown links: [text](path)
2. Inline code referencing files or scripts: `path/to/file.ext`
3. Code symbols attributed to files: `path/to/file.ext` (`SymbolName`) or `path/to/file.ext (SymbolName)`

Verifies that every referenced file exists on disk and every attributed symbol exists in the target file.
Exits with code 1 if any broken reference or orphaned claim is detected.
"""

import os
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

DOC_FILES = [
    REPO_ROOT / "README.md",
    REPO_ROOT / "docs" / "architecture.md",
    REPO_ROOT / "docs" / "api_reference.md",
    REPO_ROOT / "docs" / "python_api.md",
    REPO_ROOT / "benchmarks" / "benchmark_results.md",
    REPO_ROOT / "python" / "USAGE_GUIDE.md",
]

# Regex patterns
# 1. Standard markdown links: [text](path)
MD_LINK_RE = re.compile(r'\[([^\]]+)\]\(([^)]+)\)')

# 2. Inline code referencing files or scripts: `path/to/file.ext`
INLINE_PATH_RE = re.compile(r'`([a-zA-Z0-9_\-./\\]+\.[a-zA-Z0-9_\-]+)`')

# 3. File + Symbol attribution pattern: `path/to/file.ext` (`Symbol`) or `path/to/file.ext (Symbol)` or path/to/file.ext (`Symbol`)
FILE_AND_SYMBOL_RE = re.compile(r'(?:`?)([a-zA-Z0-9_\-./\\]+\.[a-zA-Z0-9_\-]+)(?:`?)\s*\((?:`?)([a-zA-Z0-9_]+)(?:`?)\)')

CHECK_EXTENSIONS = {
    ".cpp", ".hpp", ".h", ".c", ".cc", ".go", ".rs", ".py", ".sh", ".ps1",
    ".json", ".yaml", ".yml", ".proto", ".md", ".txt", ".cmake"
}

CHECK_PREFIXES = [
    "backend/", "services/", "pkg/", "api/", "cmd/", "cli-rs/",
    "benchmarks/", "scripts/", "deploy/", "circuits/", "docs/", "python/"
]

def is_local_target(target: str) -> bool:
    target = target.strip()
    if not target:
        return False
    if target.startswith("http://") or target.startswith("https://") or target.startswith("mailto:"):
        return False
    if target.startswith("#"):
        return False
    return True

def clean_target(target: str) -> str:
    target = target.split("#")[0]
    target = target.split("?")[0]
    target = target.strip()
    if target.startswith("file:///"):
        target = target[len("file:///"):]
    return target

def check_file_references(doc_path: Path):
    if not doc_path.exists():
        return [f"Missing doc file: {doc_path}"]

    content = doc_path.read_text(encoding="utf-8")
    errors = []

    # 1. Check markdown links
    for match in MD_LINK_RE.finditer(content):
        link_text, raw_target = match.groups()
        if not is_local_target(raw_target):
            continue
        cleaned = clean_target(raw_target)
        if not cleaned:
            continue

        resolved_rel = (doc_path.parent / cleaned).resolve()
        resolved_root = (REPO_ROOT / cleaned).resolve()

        if not (resolved_rel.exists() or resolved_root.exists()):
            errors.append(
                f"{doc_path.relative_to(REPO_ROOT)}: Broken link [{link_text}]({raw_target}) -> target not found"
            )

    # 2. Check inline code paths
    for match in INLINE_PATH_RE.finditer(content):
        cand = match.group(1).replace("\\", "/")
        ext = os.path.splitext(cand)[1].lower()
        if ext in CHECK_EXTENSIONS:
            if any(cand.startswith(prefix) for prefix in CHECK_PREFIXES) or cand.endswith(".sh") or cand.endswith(".py"):
                if any(x in cand for x in ["path/to", "your/", "<", ">", "..."]):
                    continue
                resolved_rel = (doc_path.parent / cand).resolve()
                resolved_root = (REPO_ROOT / cand).resolve()
                if not (resolved_rel.exists() or resolved_root.exists()):
                    errors.append(
                        f"{doc_path.relative_to(REPO_ROOT)}: Referenced file `{cand}` does not exist on disk"
                    )

    # 3. Check File + Symbol claims (e.g. `benchmarks/MemoryWallBenchmark.cpp (BM_ApplyHadamard)`)
    for match in FILE_AND_SYMBOL_RE.finditer(content):
        cand_path, cand_symbol = match.groups()
        cand_path = cand_path.replace("\\", "/")
        ext = os.path.splitext(cand_path)[1].lower()
        if ext in CHECK_EXTENSIONS and any(cand_path.startswith(prefix) for prefix in CHECK_PREFIXES):
            resolved_rel = (doc_path.parent / cand_path).resolve()
            resolved_root = (REPO_ROOT / cand_path).resolve()
            target_file = resolved_root if resolved_root.exists() else resolved_rel if resolved_rel.exists() else None
            if not target_file:
                errors.append(
                    f"{doc_path.relative_to(REPO_ROOT)}: Attributed file `{cand_path}` does not exist on disk"
                )
            else:
                try:
                    file_text = target_file.read_text(encoding="utf-8", errors="ignore")
                    if cand_symbol not in file_text:
                        errors.append(
                            f"{doc_path.relative_to(REPO_ROOT)}: Symbol `{cand_symbol}` not found in `{cand_path}`"
                        )
                except Exception as e:
                    errors.append(f"Failed to read `{cand_path}`: {e}")

    return errors

def main():
    print(f"Checking documentation for broken links, orphaned files, and symbol claims across {len(DOC_FILES)} doc files...")
    all_errors = []
    for doc in DOC_FILES:
        errs = check_file_references(doc)
        all_errors.extend(errs)

    if all_errors:
        print(f"\n[FAIL] Found {len(all_errors)} broken references or orphaned claims:")
        for e in all_errors:
            print(f"  - {e}")
        sys.exit(1)
    else:
        print("[PASS] All documentation links, referenced files, and attributed symbols exist.")
        sys.exit(0)

if __name__ == "__main__":
    main()
