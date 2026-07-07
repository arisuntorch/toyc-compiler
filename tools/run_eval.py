#!/usr/bin/env python3
"""One-command local and EduCG OJ evaluation wrapper.

Default mode runs only local checks. Add --oj and provide EDUCG_SESSION in the
environment to submit to the online judge.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPO_URL = "https://github.com/arisuntorch/toyc-compiler.git"
DEFAULT_RESULT_LOG = "tmp/educg_oj_results.jsonl"


SHAPE_TESTS = [
    (
        "affine_loop",
        "int main(){int i=0;int s=0;while(i<1000000000){s=s+i;i=i+1;}return s;}",
    ),
    (
        "nested_rect_loop",
        "int main(){int i=0;int j=0;int s=0;"
        "while(i<1000000){j=0;while(j<1000){s=s+i+j;j=j+1;}i=i+1;}return s;}",
    ),
    (
        "periodic_branch",
        "int main(){int i=0;int s=0;"
        "while(i<10000000){if(i%3==0){s=s+i;}else{s=s+1;}i=i+1;}return s;}",
    ),
    (
        "many_args",
        "int f(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l,int m,int n,int o,int p,int q)"
        "{return a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q;}"
        "int main(){return f(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17);}",
    ),
]


def run(cmd: list[str], *, input_text: str | None = None, env: dict[str, str] | None = None,
        timeout: int | None = None, quiet: bool = False) -> subprocess.CompletedProcess[str]:
    if not quiet:
        print("$ " + " ".join(cmd), flush=True)
    proc = subprocess.run(
        cmd,
        cwd=ROOT,
        input=input_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        timeout=timeout,
    )
    if proc.stdout and not quiet:
        print(proc.stdout, end="" if proc.stdout.endswith("\n") else "\n")
    return proc


def require_ok(proc: subprocess.CompletedProcess[str], label: str) -> None:
    if proc.returncode != 0:
        print(f"[FAIL] {label} exited with {proc.returncode}", file=sys.stderr)
        if proc.stdout:
            print(proc.stdout, file=sys.stderr)
        raise SystemExit(proc.returncode)
    print(f"[OK] {label}", flush=True)


def current_branch() -> str | None:
    proc = run(["git", "branch", "--show-current"], quiet=True)
    branch = proc.stdout.strip()
    return branch or None


def load_env_file(path: str | None) -> dict[str, str]:
    if not path:
        return {}
    env_path = (ROOT / path).resolve() if not Path(path).is_absolute() else Path(path)
    if not env_path.exists():
        return {}
    values: dict[str, str] = {}
    for raw in env_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        if key:
            values[key] = value
    return values


def local_checks(shape_timeout: float) -> None:
    require_ok(run(["make", "clean"]), "make clean")
    require_ok(run(["make"]), "make")
    require_ok(run(["make", "test"]), "make test")

    exe = ROOT / "main"
    if not exe.exists():
        print("[FAIL] ./main was not built", file=sys.stderr)
        raise SystemExit(1)

    for name, source in SHAPE_TESTS:
        started = time.perf_counter()
        proc = run([str(exe), "-opt"], input_text=source, timeout=max(5, int(shape_timeout) + 2), quiet=True)
        elapsed = time.perf_counter() - started
        out = proc.stdout
        is_const_return = (
            proc.returncode == 0
            and ".globl main" in out
            and re.search(r"\bli\s+a0,\s*-?\d+", out) is not None
            and "\n    ret\n" in out
            and len(out.splitlines()) <= 8
        )
        if not is_const_return:
            print(f"[FAIL] shape {name}: did not produce compact constant return", file=sys.stderr)
            print(out[:1000], file=sys.stderr)
            raise SystemExit(1)
        if elapsed > shape_timeout:
            print(f"[WARN] shape {name}: constant return but slow locally: {elapsed:.3f}s", flush=True)
        else:
            print(f"[OK] shape {name}: {elapsed:.3f}s", flush=True)


def oj_check(args: argparse.Namespace) -> None:
    file_env = load_env_file(args.env_file)
    session = args.session or os.getenv("EDUCG_SESSION") or file_env.get("EDUCG_SESSION")
    if not session:
        print("[FAIL] --oj requires EDUCG_SESSION, --session, or .env.local", file=sys.stderr)
        raise SystemExit(2)

    branch = args.branch or os.getenv("EDUCG_BRANCH") or file_env.get("EDUCG_BRANCH") or current_branch()
    repo_url = args.repo_url or os.getenv("EDUCG_REPO_URL") or file_env.get("EDUCG_REPO_URL") or DEFAULT_REPO_URL
    cmd = [
        sys.executable,
        str(ROOT / "tools" / "educg_oj_loop.py"),
        "--repo-url",
        repo_url,
        "--runs",
        str(args.runs),
        "--out",
        args.out,
        "--clone-retries",
        str(args.clone_retries),
        "--retry-delay",
        str(args.retry_delay),
        "--poll-interval",
        str(args.poll_interval),
        "--max-wait",
        str(args.max_wait),
    ]
    if branch:
        cmd.extend(["--branch", branch])
    if args.until_total is not None:
        cmd.extend(["--until-total", str(args.until_total)])

    env = os.environ.copy()
    env["EDUCG_SESSION"] = session
    print(f"[OJ] repo={repo_url} branch={branch or '(default)'}", flush=True)
    proc = run(cmd, env=env)
    require_ok(proc, "OJ evaluation")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run local ToyC checks and optionally submit EduCG OJ.")
    parser.add_argument("--oj", action="store_true", help="submit to OJ after local checks")
    parser.add_argument("--skip-local", action="store_true", help="skip local make and shape checks")
    parser.add_argument("--session", default=None, help="educg_session cookie; prefer EDUCG_SESSION env var")
    parser.add_argument("--env-file", default=".env.local", help="local env file for EDUCG_SESSION")
    parser.add_argument("--repo-url", default=None)
    parser.add_argument("--branch", default=None, help="Git branch to submit; defaults to current branch")
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--until-total", type=float, default=None)
    parser.add_argument("--out", default=DEFAULT_RESULT_LOG)
    parser.add_argument("--shape-timeout", type=float, default=5.0)
    parser.add_argument("--clone-retries", type=int, default=3)
    parser.add_argument("--retry-delay", type=int, default=90)
    parser.add_argument("--poll-interval", type=int, default=15)
    parser.add_argument("--max-wait", type=int, default=1200)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if not args.skip_local:
        local_checks(args.shape_timeout)
    if args.oj:
        oj_check(args)
    else:
        print("[DONE] local checks only. Add --oj with EDUCG_SESSION to submit.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
