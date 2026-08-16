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
    "runtime_loop_helper": (
        "int helper(int x, int limit) { int s = 0; int j = 0; "
        "while (j < limit) { j = j + 1; if (j % 2 == 0) { continue; } "
        "s = s + x * j; } return s; } int main() { int i = 0; int s = 0; "
        "while (i < 10000) { s = s + helper(i % 23, i % 6 + 1); "
        "i = i + 1; } return s; }"
    ),
    "affine_loop_helper": (
        "int helper(int x, int limit) { int a = x + 1; int s = 0; int j = 0; "
        "while (j < limit) { s = s + a + j; a = a + 1; j = j + 1; } "
        "return s + a; } int main() { int i = 0; int s = 0; "
        "while (i < 10000) { s = s + helper(i % 23, i % 6 + 1); "
        "i = i + 1; } return s; }"
    ),
    "descending_periodic_loop_helper": (
        "int helper(int n, int seed) { int s = seed; int j = n; "
        "while (j > 0) { if (j % 97 == 3) { s = s + j; } "
        "else { s = s + 7; } j = j - 1; } return s; } "
        "int main() { int i = 0; int s = 0; while (i < 10000) { "
        "s = s + helper(i % 194 + 1, i % 2); i = i + 1; } return s; }"
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


def function_assembly(assembly: str, name: str) -> str:
    marker = f"{name}:\n"
    if marker not in assembly:
        raise RuntimeError(f"missing function label: {name}")
    body = assembly.split(marker, 1)[1]
    next_function = body.find("\n.globl ")
    return body if next_function < 0 else body[:next_function]


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

    runtime_helper_main = function_assembly(generated["runtime_loop_helper"], "main")
    if "call helper" in runtime_helper_main or \
            ".Lloop_inline_while_body" not in runtime_helper_main:
        print(
            "[FAIL] runtime_loop_helper: proven local helper was not inlined",
            file=sys.stderr,
        )
        return 1

    for name in ("affine_loop_helper", "descending_periodic_loop_helper"):
        main_assembly = function_assembly(generated[name], "main")
        if "call helper" in main_assembly or ".Lwhile_body" in main_assembly:
            print(
                f"[FAIL] {name}: exact helper loop was not summarized",
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
            "int main(){int i=0;int s=0;while(i<10000){"
            "if(i%4099==0){s=s+i;}else{s=s+1;}i=i+1;}return s;}"
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

    loop_helper_fallbacks = {
        "non_affine_loop_helper": (
            "int helper(int n,int x){int j=0;while(j<n){x=x*x+1;j=j+1;}"
            "return x;}int main(){int i=0;int s=0;while(i<100){"
            "s=s+helper(i%7+1,i+2);i=i+1;}return s;}"
        ),
        "state_parameter_loop_helper": (
            "int helper(int n,int x){int j=0;while(j<n){x=x+j;j=j+1;}"
            "return x;}int main(){int i=0;int s=1;while(i<100){"
            "s=s+helper(s%7+1,i);i=i+1;}return s;}"
        ),
        "negative_phase_loop_helper": (
            "int helper(int n){int j=0;int s=0;while(j<n){s=s+j;j=j+1;}"
            "return s;}int main(){int i=-5;int s=0;while(i<100){"
            "s=s+helper(i%7+8);i=i+1;}return s;}"
        ),
        "oversized_period_loop_helper": (
            "int helper(int n){int j=0;int s=0;while(j<n){s=s+j;j=j+1;}"
            "return s;}int main(){int i=0;int s=0;while(i<10000){"
            "s=s+helper(i%4099+1);i=i+1;}return s;}"
        ),
    }
    for name, source in loop_helper_fallbacks.items():
        main_assembly = function_assembly(compile_source(source), "main")
        if ".Lwhile_body" not in main_assembly:
            print(
                f"[FAIL] {name}: unsafe helper summary did not fall back",
                file=sys.stderr,
            )
            return 1
        print(f"[OK] {name}: retained runtime outer loop")

    global_helper = compile_source(
        "int g=1;int helper(int n){int s=0;while(n>0){s=s+g;n=n-1;}return s;}"
        "int main(){g=2;int i=0;int s=0;while(i<20){"
        "s=s+helper(i);i=i+1;}return s;}"
    )
    if "call helper" not in function_assembly(global_helper, "main"):
        print(
            "[FAIL] global_loop_helper: helper with global access was inlined",
            file=sys.stderr,
        )
        return 1
    print("[OK] global_loop_helper: retained runtime call")

    print("[DONE] compiler uses static analysis and emits runtime RISC-V code")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
