#!/usr/bin/env python3
"""Check that the active compiler emits runtime RISC-V code."""

from __future__ import annotations

from pathlib import Path
import re
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
    "polynomial_loop": (
        "int main() { int i = 0; int s = 1; int q = 3; "
        "while (i < 1000000000) { int t = i * i + q * i + 7; "
        "s = s + t; i = i + 1; } return s; }"
    ),
    "offset_condition_loop": (
        "int main(){int i=-7;int s=3;while(1&&(i+5<1000000000)){"
        "s=s+i*3+1;i=i+4;}return s+i;}"
    ),
    "runtime_unroll": (
        "int opaque(int x){int y=x;return y;}int main(){int i=0;int x=1;"
        "int s=0;int m=opaque(100);while(i<100000000){"
        "x=x*1103515245+12345;s=s+x%m;i=i+1;}return s;}"
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

    nested_loop_labels = re.findall(
        r"^\.Lwhile_body[^:]*:", generated["nested_loop"], re.MULTILINE
    )
    if len(nested_loop_labels) != 1:
        print(
            "[FAIL] nested_loop: dynamic triangular inner loop was not lowered",
            file=sys.stderr,
        )
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

    if ".Lwhile_body" in generated["polynomial_loop"]:
        print(
            "[FAIL] polynomial_loop: proven polynomial sum was not lowered",
            file=sys.stderr,
        )
        return 1

    if ".Lwhile_body" in generated["offset_condition_loop"]:
        print(
            "[FAIL] offset_condition_loop: affine condition offset was not lowered",
            file=sys.stderr,
        )
        return 1

    runtime_unroll_main = function_assembly(generated["runtime_unroll"], "main")
    if runtime_unroll_main.count("rem ") != 4 or \
            ".Lwhile_body" not in runtime_unroll_main:
        print(
            "[FAIL] runtime_unroll: proven hot loop was not expanded four ways",
            file=sys.stderr,
        )
        return 1

    pure_call_cse = compile_source(
        "int helper(int x){int j=0;int s=x;while(j<8){"
        "s=s*3+j;j=j+1;}return s;}"
        "int twice(int x){return helper(x)+helper(x);}"
        "int main(){return twice(7);}"
    )
    if function_assembly(pure_call_cse, "twice").count("call helper") != 1:
        print(
            "[FAIL] pure_call_cse: duplicate local-pure call was not reused",
            file=sys.stderr,
        )
        return 1

    effect_call_cse = compile_source(
        "int g=0;int helper(int x){g=g+1;return x+g;}"
        "int twice(int x){return helper(x)+helper(x);}"
        "int main(){return twice(7)+g;}"
    )
    if function_assembly(effect_call_cse, "twice").count("call helper") != 2:
        print(
            "[FAIL] effect_call_cse: observable calls were incorrectly merged",
            file=sys.stderr,
        )
        return 1

    short_circuit_cse = compile_source(
        "int spin(int x){while(x>0){}return 7;}"
        "int guarded(int c){return (c&&spin(1))+(c&&spin(1));}"
        "int main(){return guarded(0);}"
    )
    guarded_assembly = function_assembly(short_circuit_cse, "guarded")
    guarded_loops = re.findall(
        r"^\.Lloop_inline_while_body[^:]*:", guarded_assembly, re.MULTILINE
    )
    if len(guarded_loops) != 2:
        print(
            "[FAIL] short_circuit_cse: guarded pure calls were hoisted",
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

    dynamic_dead_loop = compile_source(
        "int opaque(int x){int y=x;return y;}int main(){int i=opaque(0);"
        "int n=opaque(1000000000);int junk=1;while(i<n){"
        "junk=junk*1103515245+12345;i=i+1;}return 42;}"
    )
    if ".Lwhile_body" in function_assembly(dynamic_dead_loop, "main"):
        print(
            "[FAIL] dynamic_dead_loop: proven unit-step dead loop was not removed",
            file=sys.stderr,
        )
        return 1
    print("[OK] dynamic_dead_loop: removed with runtime endpoints")

    dynamic_dead_stride_loop = compile_source(
        "int opaque(int x){int y=x;return y;}int main(){int n=opaque(9);"
        "int j=0;int junk=2;while(j<=n){junk=junk*junk+1;j=j+2;}"
        "return 42;}"
    )
    if ".Lwhile_body" in function_assembly(dynamic_dead_stride_loop, "main"):
        print(
            "[FAIL] dynamic_dead_stride_loop: monotone dead loop was not removed",
            file=sys.stderr,
        )
        return 1
    print("[OK] dynamic_dead_stride_loop: removed monotone runtime loop")

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

    dynamic_fallbacks = {
        "dynamic_changing_bound": (
            "int opaque(int x){int y=x;return y;}int main(){int n=opaque(-1);"
            "int j=0;int s=0;while(j<n){s=s+j;n=n+1;j=j+1;}return s+j+n;}"
        ),
        "dynamic_non_unit_step": (
            "int opaque(int x){int y=x;return y;}int main(){int n=opaque(20);"
            "int j=0;int s=0;while(j<n){s=s+j;j=j+2;}return s+j;}"
        ),
        "dynamic_nonlinear_state": (
            "int opaque(int x){int y=x;return y;}int main(){int n=opaque(10);"
            "int j=0;int s=2;while(j<n){s=s*s+j;j=j+1;}return s+j;}"
        ),
        "changing_induction_step": (
            "int main(){int i=0;int step=1;int x=0;while(i<1024){"
            "step=step+1;x=x*3+step;i=i+step;}return x+i+step;}"
        ),
    }
    for name, source in dynamic_fallbacks.items():
        if ".Lwhile_body" not in function_assembly(compile_source(source), "main"):
            print(
                f"[FAIL] {name}: unsafe dynamic loop did not fall back",
                file=sys.stderr,
            )
            return 1
        print(f"[OK] {name}: retained runtime loop")

    immutable_global = compile_source(
        "int g=7;void dead(){g=99;}int main(){{int g=1;g=2;}"
        "int i=0;int s=0;while(i<1000000000){s=s+g;i=i+1;}return s;}"
    )
    immutable_main = function_assembly(immutable_global, "main")
    if ".Lglob_g" in immutable_main or ".Lwhile_body" in immutable_main:
        print(
            "[FAIL] immutable_global: unreachable write or local shadow blocked propagation",
            file=sys.stderr,
        )
        return 1
    print("[OK] immutable_global: propagated through reachable call graph")

    assigned_global_constant = compile_source(
        "int g=1;int main(){g=7;int i=0;int s=0;"
        "while(i<100000000){s=s+g*i;i=i+1;}return s+g;}"
    )
    if ".Lwhile_body" in function_assembly(assigned_global_constant, "main"):
        print(
            "[FAIL] assigned_global_constant: forward global value was lost",
            file=sys.stderr,
        )
        return 1

    merged_global_constant = compile_source(
        "int g=0;int h=0;int choose(){h=h+1;return h%2;}"
        "int main(){if(choose()){g=7;}else{g=7;}int i=0;int s=0;"
        "while(i<100000000){s=s+g*i;i=i+1;}return s+g+h;}"
    )
    if ".Lwhile_body" in function_assembly(merged_global_constant, "main"):
        print(
            "[FAIL] merged_global_constant: equal branch values were not merged",
            file=sys.stderr,
        )
        return 1

    divergent_global_value = compile_source(
        "int g=0;int h=0;int choose(){h=h+1;return h%2;}"
        "int main(){if(choose()){g=7;}else{g=8;}int i=0;int s=0;"
        "while(i<100000000){s=s+g*i;i=i+1;}return s+g+h;}"
    )
    if ".Lwhile_body" not in function_assembly(divergent_global_value, "main"):
        print(
            "[FAIL] divergent_global_value: unequal branch values were merged",
            file=sys.stderr,
        )
        return 1

    readonly_global_entry = compile_source(
        "int g=1;int helper(){int i=0;int s=0;while(i<100000000){"
        "s=s+g*i;i=i+1;}return s;}"
        "int main(){g=7;return helper();}"
    )
    readonly_helper = function_assembly(readonly_global_entry, "helper")
    if ".Lwhile_body" in readonly_helper or ".Lglob_g" in readonly_helper:
        print(
            "[FAIL] readonly_global_entry: call-site constant was not imported",
            file=sys.stderr,
        )
        return 1

    divergent_global_entry = compile_source(
        "int g=1;int helper(){int i=0;int s=0;while(i<100000000){"
        "s=s+g*i;i=i+1;}return s;}"
        "int main(){g=7;int a=helper();g=8;int b=helper();return a+b;}"
    )
    if ".Lwhile_body" not in function_assembly(
            divergent_global_entry, "helper"):
        print(
            "[FAIL] divergent_global_entry: incompatible call sites were merged",
            file=sys.stderr,
        )
        return 1

    mutable_global = compile_source(
        "int g=7;void set(){g=99;}int main(){set();int i=0;int s=0;"
        "while(i<1000000000){s=s+g;i=i+1;}return s;}"
    )
    if ".Lglob_g" not in function_assembly(mutable_global, "main"):
        print(
            "[FAIL] mutable_global: reachable write was treated as immutable",
            file=sys.stderr,
        )
        return 1
    print("[OK] mutable_global: retained reachable runtime state")

    dead_global_loop = compile_source(
        "int junk=2;int main(){int i=0;int s=0;while(i<1000000000){"
        "junk=junk*junk+1;s=s+i;i=i+1;}return s;}"
    )
    dead_global_main = function_assembly(dead_global_loop, "main")
    if ".Lwhile_body" in dead_global_main or ".Lglob_junk" in dead_global_main:
        print(
            "[FAIL] dead_global_loop: dead global dependency blocked loop summary",
            file=sys.stderr,
        )
        return 1
    print("[OK] dead_global_loop: removed dead global recurrence")

    live_global_dependency = compile_source(
        "int source=3;int result=0;int main(){int i=0;while(i<20){"
        "source=source*5+1;result=source+i;i=i+1;}return result;}"
    )
    live_dependency_main = function_assembly(live_global_dependency, "main")
    if ".Lglob_source" not in live_dependency_main or \
            ".Lglob_result" not in live_dependency_main:
        print(
            "[FAIL] live_global_dependency: observable dependency was deleted",
            file=sys.stderr,
        )
        return 1
    print("[OK] live_global_dependency: retained transitive global state")

    global_call_store = compile_source(
        "int g=0;int side=0;int effect(){side=side+1;return side;}"
        "int main(){g=effect();return side;}"
    )
    global_call_main = function_assembly(global_call_store, "main")
    if "call effect" not in global_call_main or ".Lglob_g" not in global_call_main:
        print(
            "[FAIL] global_call_store: call-bearing global store was deleted",
            file=sys.stderr,
        )
        return 1
    print("[OK] global_call_store: retained call side effects")

    dead_pure_call_loop = compile_source(
        "int pure(int x){int j=0;int s=x;while(j<5){s=s*3+j;j=j+1;}"
        "return s;}int main(){int i=0;int junk=0;while(i<1000000000){"
        "junk=pure(i);i=i+1;}return 42;}"
    )
    dead_pure_main = function_assembly(dead_pure_call_loop, "main")
    if "call pure" in dead_pure_main or ".Lwhile_body" in dead_pure_main:
        print(
            "[FAIL] dead_pure_call_loop: side-effect-free dead call was retained",
            file=sys.stderr,
        )
        return 1
    print("[OK] dead_pure_call_loop: removed pure calls and enclosing loop")

    live_effect_call_loop = compile_source(
        "int g=0;int effect(int x){g=g+x;return g;}int main(){int i=0;"
        "int junk=0;while(i<20){junk=effect(i);i=i+1;}return g;}"
    )
    live_effect_main = function_assembly(live_effect_call_loop, "main")
    if "call effect" not in live_effect_main or ".Lwhile_body" not in live_effect_main:
        print(
            "[FAIL] live_effect_call_loop: observable helper call was deleted",
            file=sys.stderr,
        )
        return 1
    print("[OK] live_effect_call_loop: retained observable calls")

    dead_unobserved_global_call = compile_source(
        "int trash=0;int write_trash(int x){trash=trash+x;return trash;}"
        "int main(){int i=0;int junk=0;while(i<1000000000){"
        "junk=write_trash(i);i=i+1;}return 42;}"
    )
    dead_unobserved_main = function_assembly(dead_unobserved_global_call, "main")
    if "call write_trash" in dead_unobserved_main or \
            ".Lwhile_body" in dead_unobserved_main:
        print(
            "[FAIL] dead_unobserved_global_call: unused effect chain was retained",
            file=sys.stderr,
        )
        return 1
    print("[OK] dead_unobserved_global_call: removed unused global effect chain")

    observed_helper_return = compile_source(
        "int state=1;int update(int x){state=state*3+x;return state;}"
        "int main(){return update(7);}"
    )
    observed_return_main = function_assembly(observed_helper_return, "main")
    if "call update" not in observed_return_main or \
            ".Lglob_state" not in observed_helper_return:
        print(
            "[FAIL] observed_helper_return: used return dependency was deleted",
            file=sys.stderr,
        )
        return 1
    print("[OK] observed_helper_return: retained used helper return state")

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
    global_helper_main = function_assembly(global_helper, "main")
    if ".Lwhile_body" not in global_helper_main:
        print(
            "[FAIL] global_loop_helper: stateful helper was summarized statically",
            file=sys.stderr,
        )
        return 1
    if "call helper" in global_helper_main or \
            ".Lloop_inline_while_body" not in global_helper_main:
        print(
            "[FAIL] global_loop_helper: stateful helper was not runtime-inlined",
            file=sys.stderr,
        )
        return 1
    print("[OK] global_loop_helper: retained runtime state and removed call")

    global_write_helper = compile_source(
        "int g=1;int helper(int n){int s=0;while(n>0){g=g+n;"
        "if(n==3){break;}if(n%2==0){n=n-1;continue;}"
        "s=s+g;n=n-1;}return s;}int main(){int i=0;int s=0;"
        "while(i<20){s=s+helper(i%7+1);i=i+1;}return s+g;}"
    )
    global_write_main = function_assembly(global_write_helper, "main")
    if ".Lwhile_body" not in global_write_main:
        print(
            "[FAIL] global_write_loop_helper: stateful loop was summarized statically",
            file=sys.stderr,
        )
        return 1
    if "call helper" in global_write_main or \
            ".Lloop_inline_while_body" not in global_write_main or \
            ".Lglob_g" not in global_write_main or \
            "sw a0, 0(t6)" not in global_write_main:
        print(
            "[FAIL] global_write_loop_helper: global write was not runtime-inlined",
            file=sys.stderr,
        )
        return 1
    print("[OK] global_write_loop_helper: emitted inline runtime global writes")

    print("[DONE] compiler uses static analysis and emits runtime RISC-V code")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
