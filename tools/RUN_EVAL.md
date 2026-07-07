# Evaluation Wrapper

Run local checks only:

```bash
python3 tools/run_eval.py
```

Run local checks, then submit to EduCG OJ:

```bash
EDUCG_SESSION=<new_educg_session> python3 tools/run_eval.py --oj --branch main
```

Submit an experiment branch:

```bash
EDUCG_SESSION=<new_educg_session> python3 tools/run_eval.py --oj --branch codex/perf-iteration-90
```

Useful options:

```bash
python3 tools/run_eval.py --help
```

The script never writes the cookie to git-tracked files. OJ results are appended
to `tmp/educg_oj_results.jsonl`.
