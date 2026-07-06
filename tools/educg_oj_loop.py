#!/usr/bin/env python3
"""Submit and poll the EduCG OJ compiler assignment.

The session cookie is a credential. Keep it in the EDUCG_SESSION environment
variable and never commit it.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import html
from html.parser import HTMLParser
import http.cookiejar
import json
import os
from pathlib import Path
import re
import sys
import time
import urllib.parse
import urllib.request


DEFAULT_ASSIGNMENT_URL = (
    "https://cslabcg.whu.edu.cn/assignment/programOJPList_ce.jsp"
    "?assignID=5535&proNum=1&libCenter=false"
)
DEFAULT_REPO_URL = "https://github.com/arisuntorch/toyc-compiler.git"
BUSY_KEYWORDS = ("正在", "等待", "队列", "排队", "处理中", "编译中", "评测中", "获取待评测")
TERMINAL_ERROR_KEYWORDS = (
    "Git Clone 时发生错误",
    "failed to clone",
    "鉴权失败",
    "could not read Username",
    "编译器异常",
)
RETRYABLE_TERMINAL_KEYWORDS = (
    "Git Clone 时发生错误",
    "Failed to connect to github.com",
    "连接超时",
    "Empty reply from server",
    "RPC 失败",
    "port 443",
)
NON_RETRYABLE_TERMINAL_KEYWORDS = (
    "鉴权失败",
    "could not read Username",
    "Authentication failed",
    "Repository not found",
)


class TextOnlyParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.parts: list[str] = []

    def handle_data(self, data: str) -> None:
        if data.strip():
            self.parts.append(data.strip())

    def text(self) -> str:
        return " ".join(self.parts)


def strip_tags(fragment: str) -> str:
    parser = TextOnlyParser()
    parser.feed(html.unescape(fragment))
    return re.sub(r"\s+", " ", parser.text()).strip()


def get_attr(tag: str, name: str) -> str | None:
    pattern = rf"\b{name}\s*=\s*(['\"])(.*?)\1"
    match = re.search(pattern, tag, flags=re.IGNORECASE | re.DOTALL)
    if not match:
        return None
    return html.unescape(match.group(2))


def find_upload_form(page_html: str) -> tuple[str, dict[str, str], str | None]:
    form_match = re.search(
        r"<form\b(?P<open>[^>]*\bid\s*=\s*['\"]uploadFORM['\"][^>]*)>"
        r"(?P<body>.*?)</form>",
        page_html,
        flags=re.IGNORECASE | re.DOTALL,
    )
    if not form_match:
        raise RuntimeError("Cannot find uploadFORM in assignment page")

    opening = form_match.group("open")
    body = form_match.group("body")
    action = get_attr(opening, "action") or "showOJPProcessMsg.jsp"
    values: dict[str, str] = {}
    for input_match in re.finditer(r"<input\b[^>]*>", body, flags=re.IGNORECASE | re.DOTALL):
        tag = input_match.group(0)
        name = get_attr(tag, "name")
        if name:
            values[name] = get_attr(tag, "value") or ""

    textarea_match = re.search(
        r"<textarea\b[^>]*\bname\s*=\s*['\"]cgsoucecode['\"][^>]*>(.*?)</textarea>",
        body,
        flags=re.IGNORECASE | re.DOTALL,
    )
    current_source = html.unescape(textarea_match.group(1)).strip() if textarea_match else None
    return action, values, current_source


def find_json_url(base_url: str, frame_html: str) -> str | None:
    match = re.search(r"showOJPProcessJSON\.jsp\?[^\"'<>\\]+", frame_html)
    if not match:
        return None
    return urllib.parse.urljoin(base_url, html.unescape(match.group(0)))


def make_repo_submission(repo_url: str, branch: str | None) -> str:
    repo_url = repo_url.strip()
    if branch:
        return f"{repo_url} --branch={branch.strip()}"
    return repo_url


class EduCGClient:
    def __init__(self, session_id: str, lang: str, timeout: int) -> None:
        self.cookie = f"educg_session={session_id}; lang={lang}"
        self.timeout = timeout
        jar = http.cookiejar.CookieJar()
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

    def request(
        self,
        url: str,
        *,
        method: str = "GET",
        data: dict[str, str] | None = None,
        referer: str | None = None,
    ) -> tuple[str, str]:
        headers = {
            "Cookie": self.cookie,
            "User-Agent": "Mozilla/5.0 educg-oj-loop/1.0",
            "Accept": "text/html,application/json;q=0.9,*/*;q=0.8",
            "Accept-Language": "zh-CN,zh;q=0.9",
            "Cache-Control": "no-cache",
            "Pragma": "no-cache",
        }
        if referer:
            headers["Referer"] = referer

        body = None
        if data is not None:
            body = urllib.parse.urlencode(data).encode("utf-8")
            headers["Content-Type"] = "application/x-www-form-urlencoded"

        req = urllib.request.Request(url, data=body, headers=headers, method=method)
        with self.opener.open(req, timeout=self.timeout) as resp:
            raw = resp.read()
            charset = resp.headers.get_content_charset() or "utf-8"
            return raw.decode(charset, errors="replace"), resp.geturl()


def parse_result_json(raw_json: str) -> dict:
    start = raw_json.find("[")
    if start < 0:
        raise RuntimeError("OJ JSON response does not contain a JSON array")
    data = json.loads(raw_json[start:])
    content = ""
    ret = None
    if data:
        ret = data[0].get("ret") if isinstance(data[0], dict) else None
    for item in data:
        if isinstance(item, dict) and "content" in item:
            content = item.get("content") or ""
            break

    scores = re.findall(r'<span\s+class="score"\s*>([^<]+)</span>', content)
    total_score = parse_float(scores[0]) if len(scores) >= 1 else None
    functional_score = parse_float(scores[1]) if len(scores) >= 2 else None
    performance_score = parse_float(scores[2]) if len(scores) >= 3 else None

    tests = []
    for row_match in re.finditer(
        r'<tr\s+class="content-row[^"]*"[^>]*>(.*?)</tr>',
        content,
        flags=re.IGNORECASE | re.DOTALL,
    ):
        row = row_match.group(1)
        cells = [
            strip_tags(cell)
            for cell in re.findall(r"<td[^>]*>(.*?)</td>", row, flags=re.IGNORECASE | re.DOTALL)
        ]
        if not cells or not re.match(r"^[fp]\d{2}_", cells[0]):
            continue
        if cells[0].startswith("f") and len(cells) >= 4:
            tests.append(
                {
                    "section": "functional",
                    "name": cells[0],
                    "result": cells[1],
                    "time": cells[2],
                    "score": cells[3],
                }
            )
        elif cells[0].startswith("p") and len(cells) >= 5:
            tests.append(
                {
                    "section": "performance",
                    "name": cells[0],
                    "result": cells[1],
                    "time": cells[2],
                    "performance": cells[3],
                    "score": cells[4],
                }
            )

    text = strip_tags(content)
    terminal_error = any(keyword in text for keyword in TERMINAL_ERROR_KEYWORDS)
    busy = any(keyword in text for keyword in BUSY_KEYWORDS)
    done = total_score is not None or terminal_error or (bool(tests) and not busy)

    return {
        "ret": ret,
        "done": done,
        "total_score": total_score,
        "functional_score": functional_score,
        "performance_score": performance_score,
        "tests": tests,
        "message": summarize_text(text),
        "terminal_error": terminal_error,
    }


def parse_float(value: str) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def summarize_text(text: str, limit: int = 220) -> str:
    text = re.sub(r"\s+", " ", text).strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def fetch_current_json_url(client: EduCGClient, assignment_url: str) -> tuple[str, str]:
    page_html, final_url = client.request(assignment_url)
    _, form_values, _ = find_upload_form(page_html)
    problem_id = form_values.get("problemID")
    assign_id = form_values.get("assignID")
    if not problem_id or not assign_id:
        raise RuntimeError("Cannot find problemID/assignID in assignment page")
    frame_url = urllib.parse.urljoin(
        final_url, f"showOJPProcessMsg.jsp?problemID={problem_id}&assignID={assign_id}"
    )
    frame_html, frame_final_url = client.request(frame_url, referer=final_url)
    json_url = find_json_url(frame_final_url, frame_html)
    if not json_url:
        raise RuntimeError("Cannot find showOJPProcessJSON.jsp URL in result frame")
    return json_url, final_url


def submit_once(
    client: EduCGClient,
    assignment_url: str,
    repo_url: str,
    branch: str | None,
    wtime: int,
) -> tuple[str, str]:
    page_html, final_url = client.request(assignment_url)
    action, form_values, _ = find_upload_form(page_html)

    payload = dict(form_values)
    payload.update(
        {
            "doSubmit": "true",
            "byCE": "true",
            "wtime": str(max(0, wtime)),
            "javaMainCLass": payload.get("javaMainCLass", "Main") or "Main",
            "progLanguage": "git",
            "cgsoucecode": make_repo_submission(repo_url, branch),
        }
    )
    submit_url = urllib.parse.urljoin(final_url, action)
    frame_html, frame_final_url = client.request(
        submit_url,
        method="POST",
        data=payload,
        referer=final_url,
    )
    json_url = find_json_url(frame_final_url, frame_html)
    if json_url:
        return json_url, final_url
    return fetch_current_json_url(client, assignment_url)


def poll_result(
    client: EduCGClient,
    json_url: str,
    *,
    referer: str,
    poll_interval: int,
    max_wait: int,
) -> dict:
    deadline = time.monotonic() + max_wait
    last_summary = ""
    while True:
        raw, _ = client.request(json_url, referer=referer)
        parsed = parse_result_json(raw)
        summary = format_summary(parsed)
        if summary != last_summary:
            print(summary, flush=True)
            last_summary = summary
        if parsed["done"]:
            return parsed
        if time.monotonic() >= deadline:
            raise TimeoutError(f"OJ did not finish within {max_wait} seconds")
        time.sleep(poll_interval)


def format_summary(parsed: dict) -> str:
    if parsed.get("total_score") is not None:
        return (
            f"score total={parsed['total_score']} "
            f"functional={parsed.get('functional_score')} "
            f"performance={parsed.get('performance_score')}"
        )
    return parsed.get("message") or "OJ is still running"


def should_retry_terminal_error(parsed: dict) -> bool:
    if not parsed.get("terminal_error"):
        return False
    message = parsed.get("message") or ""
    if any(keyword in message for keyword in NON_RETRYABLE_TERMINAL_KEYWORDS):
        return False
    return any(keyword in message for keyword in RETRYABLE_TERMINAL_KEYWORDS)


def write_record(path: Path, run_no: int, parsed: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    record = {
        "time": _dt.datetime.now().isoformat(timespec="seconds"),
        "run": run_no,
        "total_score": parsed.get("total_score"),
        "functional_score": parsed.get("functional_score"),
        "performance_score": parsed.get("performance_score"),
        "terminal_error": parsed.get("terminal_error"),
        "message": parsed.get("message"),
        "tests": parsed.get("tests", []),
    }
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Submit and poll the EduCG OJ assignment.")
    parser.add_argument("--url", default=DEFAULT_ASSIGNMENT_URL, help="assignment page URL")
    parser.add_argument("--repo-url", default=os.getenv("EDUCG_REPO_URL", DEFAULT_REPO_URL))
    parser.add_argument("--branch", default=os.getenv("EDUCG_BRANCH"))
    parser.add_argument("--session", default=os.getenv("EDUCG_SESSION"), help="educg_session cookie")
    parser.add_argument("--lang", default=os.getenv("EDUCG_LANG", "zh-CN"), help="lang cookie")
    parser.add_argument("--runs", type=int, default=1, help="number of submissions")
    parser.add_argument("--interval", type=int, default=180, help="seconds between submissions")
    parser.add_argument("--poll-interval", type=int, default=15, help="seconds between result polls")
    parser.add_argument("--max-wait", type=int, default=1200, help="max seconds to wait for one run")
    parser.add_argument("--http-timeout", type=int, default=30, help="HTTP request timeout")
    parser.add_argument("--wtime", type=int, default=1, help="form wtime value")
    parser.add_argument("--until-total", type=float, default=None, help="stop once total score reaches this")
    parser.add_argument("--no-submit", action="store_true", help="only read the current result")
    parser.add_argument(
        "--clone-retries",
        type=int,
        default=3,
        help="retry count for transient Git clone/network errors",
    )
    parser.add_argument(
        "--retry-delay",
        type=int,
        default=90,
        help="seconds to wait before retrying a transient terminal error",
    )
    parser.add_argument(
        "--out",
        default="tmp/educg_oj_results.jsonl",
        help="JSONL result log path",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    if not args.session:
        print("EDUCG_SESSION is required. Do not put it in git-tracked files.", file=sys.stderr)
        return 2
    if args.runs < 1:
        print("--runs must be >= 1", file=sys.stderr)
        return 2
    if not args.no_submit and args.interval < 60 and args.runs > 1:
        print("--interval must be >= 60 seconds for repeated submissions", file=sys.stderr)
        return 2
    if args.clone_retries < 0:
        print("--clone-retries must be >= 0", file=sys.stderr)
        return 2
    if args.retry_delay < 60 and args.clone_retries > 0:
        print("--retry-delay must be >= 60 seconds when retries are enabled", file=sys.stderr)
        return 2

    client = EduCGClient(args.session, args.lang, args.http_timeout)
    out_path = Path(args.out)

    for run_no in range(1, args.runs + 1):
        attempt = 0
        while True:
            attempt += 1
            if args.no_submit or args.clone_retries == 0:
                print(f"run {run_no}/{args.runs}", flush=True)
            else:
                print(f"run {run_no}/{args.runs}, attempt {attempt}/{args.clone_retries + 1}", flush=True)

            if args.no_submit:
                json_url, referer = fetch_current_json_url(client, args.url)
            else:
                json_url, referer = submit_once(
                    client,
                    args.url,
                    args.repo_url,
                    args.branch,
                    args.wtime,
                )
            parsed = poll_result(
                client,
                json_url,
                referer=referer,
                poll_interval=args.poll_interval,
                max_wait=args.max_wait,
            )
            write_record(out_path, run_no, parsed)
            print(f"saved {out_path}", flush=True)

            if (
                not args.no_submit
                and should_retry_terminal_error(parsed)
                and attempt <= args.clone_retries
            ):
                print(
                    f"retryable terminal error; sleep {args.retry_delay}s before retry",
                    flush=True,
                )
                time.sleep(args.retry_delay)
                continue
            break

        total_score = parsed.get("total_score")
        if args.until_total is not None and total_score is not None and total_score >= args.until_total:
            print(f"stop: total score {total_score} >= {args.until_total}", flush=True)
            break
        if args.no_submit or run_no == args.runs:
            break
        print(f"sleep {args.interval}s before next submission", flush=True)
        time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
