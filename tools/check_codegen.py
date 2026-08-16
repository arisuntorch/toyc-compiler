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
    "rectangular_loop": (
        "int main() { int i = 0; int j = 0; int s = 0; "
        "while (i < 1000000) { j = 0; while (j < 1000) { "
        "s = s + i + j; j = j + 1; } i = i + 1; } return s; }"
    ),
    "periodic_loop": (
        "int main() { int i = 0; int s = 0; while (i < 10000000) { "
        "if ((i % 3 == 0) || (i % 5 == 1)) { s = s + i; } "
        "else { s = s + 1; } i = i + 1; } return s; }"
    ),
    "helper_loop": (
        "int twice(int x) { return x + x; } int main() { int i = 0; int s = 0; "
        "while (i < 1000000) { s = s + twice(i); i = i + 1; } return s; }"
    ),
    "periodic_branch_helper": (
        "int choose(int x, int y) { if (x == 0) { return y + 3; } "
        "if (x == 1) { return y * 2; } return 7; } "
        "int main() { int i = 0; int s = 0; while (i < 10000000) { "
        "s = s + choose(i % 3, i); i = i + 1; } return s; }"
    ),
    "direct_periodic_branch_helper": (
        "int choose(int x, int y) { if (x % 3 == 0) { return y + x; } "
        "if (x % 5 == 1) { return y * 2; } return y + 7; } "
        "int main() { int i = 0; int s = 1; while (i < 1000000) { "
        "s = choose(i, s); i = i + 1; } return s; }"
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

    generated: dict[str, str] = {}
    for name, source in CASES.items():
        asm = compile_source(source)
        generated[name] = asm
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

    if ".Lwhile_body" in generated["hot_loop"]:
        print("[FAIL] hot_loop: affine recurrence was not lowered", file=sys.stderr)
        return 1

    if ".Lwhile_body" in generated["rectangular_loop"]:
        print(
            "[FAIL] rectangular_loop: nested affine recurrences were not lowered",
            file=sys.stderr,
        )
        return 1

    if ".Lwhile_body" in generated["periodic_loop"]:
        print(
            "[FAIL] periodic_loop: proven residue phases were not lowered",
            file=sys.stderr,
        )
        return 1

    if ".Lwhile_body" in generated["periodic_branch_helper"]:
        print(
            "[FAIL] periodic_branch_helper: proven pure helper was not lowered",
            file=sys.stderr,
        )
        return 1

    if ".Lwhile_body" in generated["direct_periodic_branch_helper"]:
        print(
            "[FAIL] direct_periodic_branch_helper: helper residue proof was not lowered",
            file=sys.stderr,
        )
        return 1

    dead_loop = compile_source(
        "int main(){int i=0;int junk=1;while(i<1000000000){"
        "if(i%2==0){junk=junk+i;}else{junk=junk*3;}i=i+1;}return 42;}"
    )
    if ".Lwhile_body" in dead_loop:
        print("[FAIL] dead_loop: finite unobservable loop was not removed", file=sys.stderr)
        return 1

    fallback_cases = {
        "non_affine": (
            "int main(){int i=0;int x=2;while(i<100){x=x*x+1;i=i+1;}return x;}"
        ),
        "changing_bound": (
            "int main(){int i=0;int n=100;while(i<n){n=n+1;i=i+1;}return i;}"
        ),
        "wrapping_induction": (
            "int main(){int i=2147483640;while(i<2147483647){i=i+100;}return i;}"
        ),
        "nonperiodic_branch": (
            "int main(){int i=0;int s=0;while(i<100){"
            "if(i<50){s=s+i;}else{s=s+1;}i=i+1;}return s;}"
        ),
        "changing_periodic_state": (
            "int main(){int i=0;int s=1;while(i<100){"
            "if(s%3==0){s=s+i;}else{s=s+1;}i=i+1;}return s;}"
        ),
        "oversized_period": (
            "int main(){int i=0;int s=0;while(i<2000){"
            "if(i%257==0){s=s+i;}else{s=s+1;}i=i+1;}return s;}"
        ),
        "helper_state_condition": (
            "int choose(int x,int y){if(x%3==0){return y+1;}return y*2;}"
            "int main(){int i=0;int s=1;while(i<100){"
            "s=s+choose(s,i);i=i+1;}return s;}"
        ),
        "helper_nonperiodic_condition": (
            "int choose(int x){if(x<50){return x+1;}return x*2;}"
            "int main(){int i=0;int s=0;while(i<100){"
            "s=s+choose(i);i=i+1;}return s;}"
        ),
        "helper_global_write": (
            "int g=0;int choose(int x){if(x%3==0){g=g+1;return x;}return 1;}"
            "int main(){int i=0;int s=0;while(i<100){"
            "s=s+choose(i);i=i+1;}return s+g;}"
        ),
        "helper_negative_phase": (
            "int choose(int x){if(x%3==0){return x+1;}return x*2;}"
            "int main(){int i=-5;int s=0;while(i<100){"
            "s=s+choose(i);i=i+1;}return s;}"
        ),
    }
    for name, source in fallback_cases.items():
        if ".Lwhile_body" not in compile_source(source):
            print(f"[FAIL] {name}: unsafe loop optimization did not fall back", file=sys.stderr)
            return 1
        print(f"[OK] {name}: retained runtime loop")

    print("[DONE] compiler uses static analysis and emits runtime RISC-V code")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
