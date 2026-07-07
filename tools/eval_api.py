#!/usr/bin/env python3
"""Small authenticated HTTP API for running the EduCG evaluator.

The API keeps EDUCG_SESSION on the server. Callers only need EVAL_API_TOKEN.
It intentionally runs one OJ job at a time to avoid accidental submission spam.
"""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
JOB_DIR = ROOT / "tmp" / "eval_api_jobs"
DEFAULT_REPO_URL = "https://github.com/arisuntorch/toyc-compiler.git"
DEFAULT_BRANCH = "codex/perf-iteration-90"

JOBS: dict[str, dict] = {}
JOBS_LOCK = threading.Lock()
ACTIVE_LOCK = threading.Lock()
CONFIG: dict[str, str] = {}


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


def json_response(handler: BaseHTTPRequestHandler, status: int, payload: dict) -> None:
    data = json.dumps(payload, ensure_ascii=False, indent=2).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(data)))
    handler.end_headers()
    handler.wfile.write(data)


def read_body(handler: BaseHTTPRequestHandler) -> dict:
    length = int(handler.headers.get("Content-Length") or "0")
    if length <= 0:
        return {}
    raw = handler.rfile.read(length)
    return json.loads(raw.decode("utf-8"))


def check_auth(handler: BaseHTTPRequestHandler) -> bool:
    token = CONFIG.get("EVAL_API_TOKEN") or os.getenv("EVAL_API_TOKEN")
    if not token:
        json_response(handler, 500, {"error": "server missing EVAL_API_TOKEN"})
        return False
    got = handler.headers.get("Authorization", "")
    if got != f"Bearer {token}":
        json_response(handler, 401, {"error": "missing or invalid bearer token"})
        return False
    return True


def active_job_id() -> str | None:
    with JOBS_LOCK:
        for job_id, job in JOBS.items():
            if job["status"] == "running":
                return job_id
    return None


def log_tail(path: Path, limit: int = 20000) -> str:
    if not path.exists():
        return ""
    data = path.read_bytes()
    return data[-limit:].decode("utf-8", errors="replace")


def create_job(payload: dict) -> dict:
    repo_url = str(payload.get("repo_url") or CONFIG.get("EDUCG_REPO_URL") or DEFAULT_REPO_URL)
    branch = str(payload.get("branch") or CONFIG.get("EDUCG_BRANCH") or DEFAULT_BRANCH)
    runs = int(payload.get("runs") or 1)
    until_total = payload.get("until_total")
    local_checks = bool(payload.get("local_checks") or False)

    if runs < 1 or runs > 5:
        raise ValueError("runs must be between 1 and 5")

    job_id = uuid.uuid4().hex[:12]
    JOB_DIR.mkdir(parents=True, exist_ok=True)
    log_path = JOB_DIR / f"{job_id}.log"
    job = {
        "id": job_id,
        "status": "queued",
        "repo_url": repo_url,
        "branch": branch,
        "runs": runs,
        "until_total": until_total,
        "local_checks": local_checks,
        "created_at": time.time(),
        "started_at": None,
        "finished_at": None,
        "returncode": None,
        "log_path": str(log_path.relative_to(ROOT)),
    }
    with JOBS_LOCK:
        JOBS[job_id] = job
    thread = threading.Thread(target=run_job, args=(job_id,), daemon=True)
    thread.start()
    return job


def run_job(job_id: str) -> None:
    acquired = ACTIVE_LOCK.acquire(blocking=False)
    if not acquired:
        with JOBS_LOCK:
            JOBS[job_id]["status"] = "rejected"
            JOBS[job_id]["finished_at"] = time.time()
            JOBS[job_id]["error"] = "another job is already running"
        return
    try:
        with JOBS_LOCK:
            job = JOBS[job_id]
            job["status"] = "running"
            job["started_at"] = time.time()
            log_path = ROOT / job["log_path"]

        cmd = [
            sys.executable,
            str(ROOT / "tools" / "run_eval.py"),
            "--oj",
            "--repo-url",
            job["repo_url"],
            "--branch",
            job["branch"],
            "--runs",
            str(job["runs"]),
        ]
        if not job["local_checks"]:
            cmd.append("--skip-local")
        if job["until_total"] is not None:
            cmd.extend(["--until-total", str(job["until_total"])])

        env = os.environ.copy()
        env.update({k: v for k, v in CONFIG.items() if k.startswith("EDUCG_")})
        with log_path.open("w", encoding="utf-8") as log:
            log.write("$ " + " ".join(cmd) + "\n")
            log.flush()
            proc = subprocess.Popen(
                cmd,
                cwd=ROOT,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
            assert proc.stdout is not None
            for line in proc.stdout:
                log.write(line)
                log.flush()
            rc = proc.wait()

        with JOBS_LOCK:
            JOBS[job_id]["returncode"] = rc
            JOBS[job_id]["status"] = "succeeded" if rc == 0 else "failed"
            JOBS[job_id]["finished_at"] = time.time()
    except Exception as exc:
        with JOBS_LOCK:
            JOBS[job_id]["status"] = "failed"
            JOBS[job_id]["finished_at"] = time.time()
            JOBS[job_id]["error"] = str(exc)
    finally:
        ACTIVE_LOCK.release()


class EvalAPIHandler(BaseHTTPRequestHandler):
    server_version = "ToyCEvalAPI/1.0"

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"{self.address_string()} - {fmt % args}", flush=True)

    def do_GET(self) -> None:
        if self.path == "/health":
            json_response(self, 200, {"ok": True})
            return
        if not check_auth(self):
            return
        if self.path == "/jobs":
            with JOBS_LOCK:
                jobs = [{k: v for k, v in job.items() if k != "log_path"} for job in JOBS.values()]
            json_response(self, 200, {"jobs": jobs})
            return
        if self.path.startswith("/jobs/"):
            job_id = self.path.split("/", 2)[2]
            with JOBS_LOCK:
                job = JOBS.get(job_id)
                job_copy = dict(job) if job else None
            if not job_copy:
                json_response(self, 404, {"error": "job not found"})
                return
            job_copy["log_tail"] = log_tail(ROOT / job_copy["log_path"])
            json_response(self, 200, {"job": job_copy})
            return
        json_response(self, 404, {"error": "not found"})

    def do_POST(self) -> None:
        if self.path != "/submit":
            json_response(self, 404, {"error": "not found"})
            return
        if not check_auth(self):
            return
        if active_job_id():
            json_response(self, 409, {"error": "another job is already running", "active_job": active_job_id()})
            return
        try:
            payload = read_body(self)
            job = create_job(payload)
        except Exception as exc:
            json_response(self, 400, {"error": str(exc)})
            return
        json_response(self, 202, {"job": job})


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Authenticated HTTP API for EduCG OJ evaluation.")
    parser.add_argument("--host", default=os.getenv("EVAL_API_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("EVAL_API_PORT", "8765")))
    parser.add_argument("--env-file", default=".env.local")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    CONFIG.update(load_env_file(args.env_file))
    CONFIG.update({k: v for k, v in os.environ.items() if k in {"EVAL_API_TOKEN", "EDUCG_SESSION", "EDUCG_BRANCH", "EDUCG_REPO_URL"}})
    if not (CONFIG.get("EVAL_API_TOKEN") or os.getenv("EVAL_API_TOKEN")):
        print("EVAL_API_TOKEN is required", file=sys.stderr)
        return 2
    if not (CONFIG.get("EDUCG_SESSION") or os.getenv("EDUCG_SESSION")):
        print("EDUCG_SESSION is required", file=sys.stderr)
        return 2

    server = ThreadingHTTPServer((args.host, args.port), EvalAPIHandler)
    print(f"eval API listening on http://{args.host}:{args.port}", flush=True)
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
