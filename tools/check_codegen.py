#!/usr/bin/env python3
"""Check that the active compiler emits runtime RISC-V code."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / "main"
CASES = {
    "hot_loop": (
        "int main() { int i = 0; int s = 0; "
        "while (i < 1000000000) { s = s + i; i = i + 1; } return s; }"
    ),
    "nested_loop": (
        "int main() { int i = 0; int s = 0; while (i < 1000) { "
        "int j = 0; while (j < i) { s = s + j; j = j + 1; } i = i + 1; } "
        "return s; }"
    ),
    "helper_loop": (
        "int twice(int x) { return x + x; } int main() { int i = 0; int s = 0; "
        "while (i < 1000000) { s = s + twice(i); i = i + 1; } return s; }"
    ),
}


def compile_source(source: str) -> str:
    proc = subprocess.run(
        [str(COMPILER), "-opt"],
        input=source,
        text=True,
        capture_output=True,
        cwd=ROOT,
        timeout=10,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr or f"compiler exited with {proc.returncode}")
    return proc.stdout


def main() -> int:
    if not COMPILER.exists():
        print("[FAIL] run `make` first", file=sys.stderr)
        return 1

    for name, source in CASES.items():
        asm = compile_source(source)
        lines = asm.splitlines()
        has_runtime_branch = any(
            opcode in asm for opcode in ("blt ", "bge ", "bnez ", "beqz ")
        )
        if ".globl main" not in asm or "ret" not in asm or len(lines) < 8:
            print(f"[FAIL] {name}: output is not a complete RISC-V program", file=sys.stderr)
            return 1
        if not has_runtime_branch:
            print(f"[FAIL] {name}: expected runtime loop/control-flow code", file=sys.stderr)
            return 1
        print(f"[OK] {name}: {len(lines)} assembly lines")

    print("[DONE] compiler uses static analysis and emits runtime RISC-V code")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
