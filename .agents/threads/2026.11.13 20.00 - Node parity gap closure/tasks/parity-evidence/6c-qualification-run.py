import collections
import datetime
import hashlib
import json
import os
import pathlib
import re
import signal
import subprocess
import time

ROOT = pathlib.Path.cwd()
EVIDENCE = ROOT / ".agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence"
OUT = EVIDENCE / "resumed-final-recheck"
OUT.mkdir(mode=0o700, exist_ok=False)
SCRATCH = ROOT / "build/6c-resumed-recheck-runtime"
SCRATCH.mkdir(mode=0o700, exist_ok=False)
NODE = pathlib.Path("/Users/briandombrowski/.copilot/session-state/1b166e26-5961-4b5d-989b-4468a20c047e/files/node-v24.11.0-darwin-arm64/bin/node")
EXCLUDED = ""
START = time.monotonic()
DEADLINE = min(START + 340, datetime.datetime.fromisoformat("2026-11-13T23:01:40+00:00").timestamp() - time.time() + START)
env = dict(os.environ, BUGSINK_DSN="", TMPDIR=str(SCRATCH), TMP=str(SCRATCH), TEMP=str(SCRATCH),
           ASAN_OPTIONS="abort_on_error=1:halt_on_error=1",
           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
env["PATH"] = str(NODE.parent) + os.pathsep + env.get("PATH", "")


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(name, value):
    path = OUT / name
    path.write_text(json.dumps(value, indent=2) + "\n")
    os.chmod(path, 0o600)


def inputs():
    names = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--",
         "src", "tests", "scripts", "CMakeLists.txt", "justfile"], text=True).split("\0")
    result = {}
    for name in sorted(set(names) - {""}):
        if name == EXCLUDED:
            result[name] = {"status": "policy-excluded-not-read"}
            continue
        path = ROOT / name
        result[name] = {"sha256": sha(path)} if path.is_file() else {"status": "deleted"}
    return result


before = inputs()
write("inputs-start.json", {"recordedAt": now(), "inputs": before,
                          "head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                          "sourceFreezeAuthority": "parent explicit 22:56Z test-only final recheck release",
                          "scope": "qualification only, not release acceptance"})
report = {"startedAt": now(), "classification": "frozen qualification, not release acceptance",
          "gates": [], "sourcePolicy": "Original view test_export.hpp lines1-12 succeeded under explicit parent release; actual file hash included. messages.md remains out of scope."}
write("status.json", report)


def gate(name, command, limit=180, cwd=ROOT):
    remaining = DEADLINE - time.monotonic()
    if remaining < 5:
        result = {"name": name, "status": "not-run-deadline", "command": command}
        report["gates"].append(result)
        write("status.json", report)
        return result
    result = {"name": name, "command": command, "startedAt": now()}
    print("START", name, flush=True)
    start = time.monotonic()
    child = subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, start_new_session=True)
    result["pid"] = child.pid
    report["activeGate"] = result
    write("status.json", report)
    try:
        text, _ = child.communicate(timeout=min(limit, remaining - 2))
        result.update(status="completed", exitCode=child.returncode)
    except subprocess.TimeoutExpired:
        os.killpg(child.pid, signal.SIGTERM)
        try:
            text, _ = child.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            text, _ = child.communicate()
        result.update(status="timeout-stopped", exitCode=child.returncode)
    result.update(completedAt=now(), elapsedSeconds=round(time.monotonic() - start, 3),
                  outputSha256=hashlib.sha256(text.encode()).hexdigest(),
                  outputBytes=len(text.encode()), rawOutputRetained=False)
    summaries = []
    failure_metadata = []
    diagnostics = []
    for line in text.splitlines():
        s = line.strip()
        if s.startswith(("[doctest] test cases:", "[doctest] assertions:", "[doctest] Status:",
                         "100% tests passed", "Total Test time", "tests passed,", "tests failed",
                         "MCP ", "stdio ", "HTTP ")):
            summaries.append(s)
        if "TEST CASE:" in s:
            failure_metadata.append(s.split("TEST CASE:", 1)[1].strip()[:250])
        m = re.match(r"^(.*\.(?:hpp|cpp)):(\d+)(?::(\d+))?:\s*(warning|error|fatal error):", s)
        if m:
            diagnostics.append({"file": m[1].replace(str(ROOT) + "/", ""),
                                "line": int(m[2]), "severity": m[4]})
        if s.startswith("Errors while processing "):
            diagnostics.append({"processingError": True})
    result["summaries"] = summaries
    result["failingCaseMetadata"] = failure_metadata
    result["compilerDiagnosticLocations"] = diagnostics
    report.pop("activeGate", None)
    report["gates"].append(result)
    write(name + ".json", result)
    write("status.json", report)
    print("END", name, result["status"], result["exitCode"], result["elapsedSeconds"],
          json.dumps(summaries), flush=True)
    return result


try:
    gate("ten-vectors-known-id-controls", [
        str(NODE), "tests/parity/run-engine-differential.mjs",
        "--original-root", "../gdocsmith-node",
        "--target-adapter", "build/cmake/ParityChecks/gdocsmith_parity_adapter",
        "--vectors", str(EVIDENCE.relative_to(ROOT) / "proposed-vectors.json"),
        "--case-ids", ",".join(["V001-noop", "V002-body-and-title", "V003-stale",
                                "V004-landed-lost-response-replay", "V005-font-default",
                                "V006-font-nondefault", "V007-cell-style-edit",
                                "V008-reject-hidden-insertion", "V009-image-upload-lifecycle",
                                "V010-cross-doc-image-copy"]),
        "--owned-run-root", str(OUT.relative_to(ROOT) / "runs"),
        "--results", str(OUT.relative_to(ROOT) / "differential-results.json"),
    ], 120)
    gate("debug-build", ["cmake", "--build", "build/cmake/GapChecks", "--target",
                         "gdocsmith_mcp_test", "--parallel", "2"], 180)
    gate("debug-full-tests", ["ctest", "--test-dir", "build/cmake/GapChecks",
                              "--output-on-failure", "--no-tests=error"], 135)
    gate("release-build", ["cmake", "--build", "build/cmake/GapRelease", "--target",
                           "gdocsmith_mcp_test", "--parallel", "2"], 180)
    gate("release-full-tests", ["ctest", "--test-dir", "build/cmake/GapRelease",
                                "--output-on-failure", "--no-tests=error"], 135)
    for name, file, cache in (
        ("tidy-tests", "tests/test_main.cpp", "GapChecks"),
    ):
        gate(name, ["/opt/homebrew/opt/llvm/bin/clang-tidy", "-p", "build/cmake/" + cache,
                    file, "--extra-arg=-DDOCTEST_COUNTER=__COUNTER__"], 90)
finally:
    after = inputs()
    changes = [name for name in sorted(set(before) | set(after)) if before.get(name) != after.get(name)]
    report.update(completedAt=now(), elapsedSeconds=round(time.monotonic() - START, 3),
                  sourceInputsUnchanged=not changes, changedInputs=changes)
    write("inputs-end.json", {"recordedAt": now(), "inputs": after})
    binaries = {}
    for file in ("build/bin/gdocsmith-mcp", "build/cmake/GapChecks/gdocsmith_mcp_test",
                 "build/cmake/GapRelease/gdocsmith_mcp_test",
                 "build/cmake/ParityChecks/gdocsmith_parity_adapter"):
        path = ROOT / file
        if path.is_file():
            binaries[file] = {"sha256": sha(path), "bytes": path.stat().st_size}
    report["binaries"] = binaries
    original = ROOT.parent / "gdocsmith-node"
    baseline = json.loads((EVIDENCE / "6c-preflight/original-baseline-final.json").read_text())
    report["originalBaselineStillCurrent"] = all(
        (original / name).is_file() and sha(original / name) == digest
        for name, digest in baseline["identityAfter"].items())
    report["runningOwnedCommands"] = []
    write("status.json", report)
    print("QUALIFICATION COMPLETE", json.dumps({
        "unchanged": not changes, "changed": changes, "elapsed": report["elapsedSeconds"],
        "gates": [(g["name"], g.get("exitCode"), g["status"]) for g in report["gates"]],
        "originalBaselineStillCurrent": report["originalBaselineStillCurrent"]}), flush=True)
