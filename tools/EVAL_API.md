# Evaluation API

This wraps `tools/run_eval.py` as a small authenticated HTTP API. The server
keeps `EDUCG_SESSION`; callers only get `EVAL_API_TOKEN`.

Create local secrets on the server:

```bash
cp .env.example .env.local
# edit .env.local:
# EDUCG_SESSION=<new_educg_session>
# EVAL_API_TOKEN=<long_random_token>
```

Start the API:

```bash
python3 tools/eval_api.py --host 127.0.0.1 --port 8765
```

Submit a branch:

```bash
curl -sS -X POST http://127.0.0.1:8765/submit \
  -H "Authorization: Bearer <EVAL_API_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{"branch":"codex/perf-iteration-90","repo_url":"https://github.com/arisuntorch/toyc-compiler.git"}'
```

Read job status:

```bash
curl -sS http://127.0.0.1:8765/jobs/<job_id> \
  -H "Authorization: Bearer <EVAL_API_TOKEN>"
```

List jobs:

```bash
curl -sS http://127.0.0.1:8765/jobs \
  -H "Authorization: Bearer <EVAL_API_TOKEN>"
```

Notes:

- Only one OJ job runs at a time.
- Default submit mode skips local checks and asks OJ to clone the requested
  GitHub branch.
- Keep `.env.local` off GitHub. It is ignored by `.gitignore`.
- If exposing the API outside localhost, put it behind a firewall or reverse
  proxy with HTTPS.
