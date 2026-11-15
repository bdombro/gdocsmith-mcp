# Conformance action log

- Parent recorded the reproduced malformed-fixture false-green and bounded repair contract.
- 20:04Z worker inspected contract, harness, Node comparison and existing isolated caches. Confirmed silent open/parse skips and aggregate `passed > 0`; no tracked implementation changes existed in assigned files.
- Node always compares reply shapes (missing replies means empty), whereas C++ currently skips that comparison when absent. Scope includes restoring this assertion strength with a focused regression.
- Existing `SwarmEmulator` binaries and dependency headers are available; no installs, canonical builds, Node writes or fixture mutations planned.
- 20:06Z cycle 1: implemented explicit file/stream/JSON/shape failures, per-file checks without silent skips, discovered-versus-checked equality, and focused malformed/missing/unreadable/wrong-outcome regressions. All `.json` directory entries are selected, so dangling fixture symlinks cannot silently disappear.
- Restored Node's empty-default reply-shape assertion in `src/emulator/conformance.hpp`; this is conformance validation only.
- Fresh isolated build: `printf '%s\n' '#define SLIMR_TEST_MAIN' '#include "emulator/test_conformance.hpp"' | /usr/bin/clang++ -x c++ - -std=c++26 -g -O0 -ftrivial-auto-var-init=zero -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST -fsanitize=address,undefined -fno-sanitize-recover=all -I src -I tests -isystem build/deps/arm64-osx/include -o build/cmake/SwarmEmulator/conformance_only`
- Fresh check: `ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1 build/cmake/SwarmEmulator/conformance_only --test-suite='emulator::conformance' --no-skip` → exit 0; 4/4 cases, 174/174 assertions, zero skipped. Includes every original fixture and four wrong expected-outcome subcases. `git diff --check` on the two implementation files passed.
- Next: independent process negative controls against the actual fixture-case runner, using generated disposable fixture copies/symlinks only (original fixture bytes remain untouched).
- 20:09Z added standalone focused control runner `tests/emulator/conformance_control.cpp` (no CMake/test_main changes). First controls: valid exit 0 (1 case/152 assertions); malformed, missing, unreadable each exit 1 with 148/149 accounted and 2 failed assertions; wrong replies exit 1 with 2 failed assertions. Each selector excludes three unrelated regression cases; no fixture is skipped. All fatal sanitizer settings retained.
- Cycle 2 refinement: control output revealed doctest formats `FAIL_CHECK(const char*)` as a pointer; changed it to an owning string for explicit diagnostic text. Hardened unreadable control to use a valid copied fixture with mode `000`, so a privileged readable-file fallback cannot falsely count as the intended unreadable failure. No product behavior changes in this refinement.
- 20:12Z final fresh cycle-2 results: focused suite again exit 0, 4/4 cases, 174/174 assertions, zero skipped; all 149 fixtures accounted. Isolated actual-suite controls: valid exit 0 (152 assertions); malformed/missing/unreadable exit 1 each (150 passed, 2 failed); wrong replies exit 1 (151 passed, 2 failed). All negative failures are intended assertion failures, not sanitizer aborts or setup failures.
- Explicit diagnostics now identify `tests/fixtures/conformance/B1.json`: malformed control says `malformed fixture JSON`; missing and unreadable controls say `cannot open fixture`; all three report checked 148 versus discovered 149. Wrong input says `replies: expected [], got [{}]`; checked accounting stays complete. Each control selects one case and excludes the other three regression cases; this is not a fixture skip.
- Corpus preservation: read-only byte comparison reports C++ 149, Node 149, byte-identical 149. No fixture files changed. Scoped `git diff --check` passes.
- Completed in approximately eight minutes, two repair/check cycles. Ready for independent review, NOT accepted; idle/resumable for own repairs. Canonical full-suite builds remain parent-owned.

## Exact final verification commands

Run from the repository root. The focused build command appears above; it was rerun unchanged after the diagnostic refinement.

```sh
/usr/bin/clang++ tests/emulator/conformance_control.cpp -std=c++26 -g -O0 -ftrivial-auto-var-init=zero -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST -fsanitize=address,undefined -fno-sanitize-recover=all -I src -I tests -isystem build/deps/arm64-osx/include -o build/cmake/SwarmEmulator/conformance_control
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1
build/cmake/SwarmEmulator/conformance_only --test-suite='emulator::conformance' --no-skip
for mode in valid malformed missing unreadable wrong; do
  build/cmake/SwarmEmulator/conformance_control "$mode"
  result=$?
  printf 'CONTROL mode=%s exit=%s\n' "$mode" "$result"
  if [ "$mode" = valid ]; then
    test "$result" -eq 0 || exit 3
  else
    test "$result" -eq 1 || exit 4
  fi
done
/usr/bin/python3 -c 'from pathlib import Path; a=Path("tests/fixtures/conformance"); b=Path("../gdocsmith-node/src/core/emulator/__fixtures__/conformance"); x={p.name:p.read_bytes() for p in a.glob("*.json")}; y={p.name:p.read_bytes() for p in b.glob("*.json")}; print("C++ fixtures:",len(x),"Node fixtures:",len(y),"byte-identical:",sum(x[k]==y.get(k) for k in x)); assert len(x)==149 and x==y'
git --no-pager diff --check -- tests/emulator/test_conformance.hpp tests/emulator/conformance_control.cpp src/emulator/conformance.hpp '.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/1-conformance.state.md' '.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/1-conformance.log.md'
```

The control runner creates and removes its disposable corpus automatically; original fixtures are read-only symlink targets except its selected B1 copy. No full source bodies or raw transcripts are retained in this log.
