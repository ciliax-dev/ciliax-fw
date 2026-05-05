#!/usr/bin/env python3
"""PreToolUse hook: block `git commit` when HEAD is on `main`.

Reads Claude Code's hook input JSON from stdin, looks for a `git commit`
invocation in the Bash tool's command, and if the repository at the tool's
cwd is on `main`, exits 2 with a message. Otherwise exits 0 (allow).
"""
import json
import re
import subprocess
import sys


def main() -> int:
    try:
        data = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0

    cmd = data.get("tool_input", {}).get("command", "") or ""
    cwd = data.get("cwd") or "."

    if not re.search(r"(^|[^A-Za-z0-9_])git\s+commit", cmd):
        return 0

    try:
        branch = subprocess.run(
            ["git", "-C", cwd, "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, check=True, timeout=5,
        ).stdout.strip()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError):
        return 0

    if branch == "main":
        print(
            "Blocked: 'git commit' on 'main' is not allowed by project policy "
            "(CLAUDE.md > Workflow conventions). "
            "Switch to a feature/<short-name>, fix/<short-name>, or refactor/<short-name> branch first.",
            file=sys.stderr,
        )
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
