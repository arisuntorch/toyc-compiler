#!/usr/bin/env python3
"""Resilient EduCG OJ submitter.

This wrapper is intentionally small: it reuses educg_oj_loop's page parsing, but
adds retry policy for the unstable parts of the platform, especially GitHub
clone timeouts and transient judge-server failures.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import sys
import time
from urllib.error import HTTPError, URLError

from educg_oj_loop import (
    DEFAULT_ASSIGNMENT_URL,
    DEFAULT_REPO_URL,
    EduCGClient,
    fetch_current_json_url,
    format_summary,
    poll_result,
    should_retry_terminal_error,
    submit_once,
)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "tmp" / "educg_oj_results.jsonl"

TRANSIENT_TEXT = (
    "Git Clone 时发生错误",
    "failed to clone",
    "Failed to connect to github.com",
    "连接超时",
    "评测服务器连接失败",
    "提交项目为空",
    "Empty reply from server",
    "RPC 失败",
    "port 443",
    "正在准备运行",
)

FATAL_TEXT = (
    "鉴权失败",
    "could not read Username",
    "Authentication failed",
    "Repository not found",
    "Cannot find uploadFORM",
)


def load_env(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    out: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip().strip('"').strip("'")
    return out


def current_branch() -> str | None:
    try:
        import subprocess

        proc = subprocess.run(
            ["git", "branch", "--show-current"],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=5,
        )
        branch = proc.stdout.strip()
        return branch or None
    except Exception:
        return None


def message(parsed: dict) -> str:
    return str(parsed.get("message") or "")


def has_score(parsed: dict) -> bool:
    return parsed.get("total_score") is not None


def is_fatal_text(text: str) -> bool:
    return any(x in text for x in FATAL_TEXT)


def is_transient_text(text: str) -> bool:
    return any(x in text for x in TRANSIENT_TEXT)


def should_retry(parsed: dict) -> bool:
    text = message(parsed)
    if is_fatal_text(text):
        return False
    if should_retry_terminal_error(parsed):
        return True
    if not has_score(parsed) and is_transient_text(text):
        return True
    return False


def write_record(path: Path, attempt: int, parsed: dict, *, ok: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    record = {
        "time": dt.datetime.now().isoformat(timespec="seconds"),
        "attempt": attempt,
        "ok": ok,
        "total_score": parsed.get("total_score"),
        "functional_score": parsed.get("functional_score"),
        "performance_score": parsed.get("performance_score"),
        "terminal_error": parsed.get("terminal_error"),
        "message": parsed.get("message"),
        "tests": parsed.get("tests", []),
    }
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")


def print_score_table(parsed: dict) -> None:
    print(format_summary(parsed), flush=True)
    for test in parsed.get("tests", []):
        if test.get("section") != "performance":
            continue
        print(
            f"{test.get('name')}\t{test.get('result')}\t"
            f"{test.get('time')}\t{test.get('performance')}\t{test.get('score')}",
            flush=True,
        )


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Robust EduCG OJ submit/retry wrapper.")
    p.add_argument("--url", default=DEFAULT_ASSIGNMENT_URL)
    p.add_argument("--repo-url", default=None)
    p.add_argument("--branch", default=None)
    p.add_argument("--env-file", default=".env.local")
    p.add_argument("--session", default=None)
    p.add_argument("--lang", default=None)
    p.add_argument("--attempts", type=int, default=10, help="max submit attempts")
    p.add_argument("--delay", type=int, default=120, help="seconds between retry attempts")
    p.add_argument("--poll-interval", type=int, default=15)
    p.add_argument("--max-wait", type=int, default=1500, help="seconds per attempt")
    p.add_argument("--http-timeout", type=int, default=30)
    p.add_argument("--wtime", type=int, default=1)
    p.add_argument("--until-total", type=float, default=None)
    p.add_argument("--until-performance", type=float, default=None)
    p.add_argument("--no-submit", action="store_true", help="only poll current result")
    p.add_argument("--out", default=str(DEFAULT_OUT))
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    env = load_env((ROOT / args.env_file).resolve())
    session = args.session or os.getenv("EDUCG_SESSION") or env.get("EDUCG_SESSION")
    if not session:
        print("missing EDUCG_SESSION; put it in .env.local or pass --session", file=sys.stderr)
        return 2

    repo_url = args.repo_url or os.getenv("EDUCG_REPO_URL") or env.get("EDUCG_REPO_URL") or DEFAULT_REPO_URL
    branch = args.branch or os.getenv("EDUCG_BRANCH") or env.get("EDUCG_BRANCH") or current_branch()
    lang = args.lang or os.getenv("EDUCG_LANG") or env.get("EDUCG_LANG") or "zh-CN"
    out_path = Path(args.out)
    if not out_path.is_absolute():
        out_path = ROOT / out_path

    client = EduCGClient(session, lang, args.http_timeout)
    print(f"[OJ] repo={repo_url} branch={branch or '(default)'} attempts={args.attempts}", flush=True)

    last: dict | None = None
    for attempt in range(1, args.attempts + 1):
        print(f"[attempt {attempt}/{args.attempts}]", flush=True)
        try:
            if args.no_submit:
                json_url, referer = fetch_current_json_url(client, args.url)
            else:
                json_url, referer = submit_once(client, args.url, repo_url, branch, args.wtime)
            parsed = poll_result(
                client,
                json_url,
                referer=referer,
                poll_interval=args.poll_interval,
                max_wait=args.max_wait,
            )
            last = parsed
            write_record(out_path, attempt, parsed, ok=has_score(parsed))
        except (HTTPError, URLError, TimeoutError, RuntimeError, json.JSONDecodeError) as exc:
            text = str(exc)
            if is_fatal_text(text):
                print(f"[fatal] {text}", file=sys.stderr, flush=True)
                return 2
            parsed = {"message": text, "terminal_error": True, "tests": []}
            last = parsed
            write_record(out_path, attempt, parsed, ok=False)
            print(f"[transient] {text}", flush=True)

        if last and has_score(last):
            print_score_table(last)
            total = last.get("total_score")
            perf = last.get("performance_score")
            if args.until_total is None and args.until_performance is None:
                return 0
            total_ok = args.until_total is None or (total is not None and total >= args.until_total)
            perf_ok = args.until_performance is None or (perf is not None and perf >= args.until_performance)
            if total_ok and perf_ok:
                print("[done] target reached", flush=True)
                return 0
            print("[retry] scored result below target", flush=True)
        elif last and not should_retry(last):
            print(f"[fatal] non-retryable result: {message(last)}", file=sys.stderr, flush=True)
            return 2
        else:
            print("[retry] transient platform result", flush=True)

        if attempt < args.attempts:
            print(f"sleep {args.delay}s", flush=True)
            time.sleep(args.delay)

    print("[fail] attempts exhausted", file=sys.stderr, flush=True)
    if last:
        print(message(last), file=sys.stderr, flush=True)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
