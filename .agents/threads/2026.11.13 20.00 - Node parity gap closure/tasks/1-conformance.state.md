# 1 — Fail-closed conformance

Status: ready for independent review; NOT accepted. Worker idle/resumable.

## Execution

- Started 2026-11-13T20:04Z. No map dependency; canonical caches untouched.
- Scope: fail-closed fixture loading/accounting, input regressions and Node-equivalent reply validation. Existing fixture bytes and Node reference remain read-only.
- Verification cache: `build/cmake/SwarmEmulator`; independent review and parent acceptance remain outstanding.
- Completed 2026-11-13T20:12Z, two repair/check cycles; below 15-minute/three-cycle bound.

## Changed files

- `tests/emulator/test_conformance.hpp`: fail-closed open/read/parse/shape validation; every discovered JSON entry checked with per-fixture diagnostics and explicit discovered/checked accounting; malformed, missing/unreadable and wrong-outcome regressions.
- `src/emulator/conformance.hpp`: preserve Node's reply assertion strength when expected replies are absent (empty expected list).
- `tests/emulator/conformance_control.cpp`: standalone actual-suite isolated-corpus controls; ordinary valid, malformed B1, missing selected target, unreadable valid B1, and valid JSON with wrong expected replies.
- This state and `1-conformance.log.md`.

## Fresh results

- Fatal ASan/UBSan focused suite: exit 0; 4 cases, 174 assertions, zero skipped; all 149 fixtures loaded/parsed/checked.
- Actual fixture-case control: valid exit 0, 1 case/152 assertions. Malformed, missing, unreadable each exit 1, 150 passed/2 failed assertions and 148/149 checked. Wrong replies exit 1, 151 passed/2 failed assertions; all 149 checked.
- Each control deliberately selects one case and excludes three regression cases (doctest reports 3 skipped); no fixture skips. Full focused suite has zero skips.
- Original corpus remains 149/149 byte-identical with Node; scoped whitespace checks pass. No sanitizer findings.
- Exact reproducible commands and control diagnostics are in the action log. No installs, canonical builds, fixture/source-reference modifications, services, auth operations or commits.

## Outstanding gates / limitations

- Independent reviewer and parent acceptance required; worker does not claim acceptance.
- Canonical full Release/sanitizer runs remain parent-owned; the supplied 308/5302 baseline was not rerun here.
- No implementation blocker. Unreadable external control requires a non-privileged process; it uses a valid mode-000 disposable copy so a privileged read would fail the expected-control outcome rather than masquerade as an unreadability success.

## Contract

- Objective: preserve the Node conformance obligation that every discovered fixture loads, parses and is checked. Close the reproduced false-green result without changing original fixture data or weakening assertions.
- Own: `tests/emulator/test_conformance.hpp`, narrowly necessary `src/emulator/conformance.hpp` validation only if justified, focused new/existing conformance regressions, this state and `1-conformance.log.md`. No `test_main.cpp`, fixtures, other source, docs, map or inbox writes.
- Reference: Node `src/core/emulator/conformance.test.ts`; target silently continues after failed opens/parses at lines 50/53. All 149 fixture files are byte-identical; isolated malformed `B1.json` still passed one target case/four assertions.
- Evidence: valid fixtures pass; malformed and unreadable/missing selected fixture controls fail for the intended reason; every discovered fixture is accounted for. Preserve per-fixture diagnostics and report exact execution/skips.
- Validation: discover the smallest existing conformance selector using an isolated/reused worker cache, run focused sanitizer checks and a known-wrong fixture control. Do not install dependencies; canonical GapChecks/GapRelease belong to final runtime.
- Gate: worker checks, independent verification, parent acceptance. Passing normal fixtures alone is insufficient.
- Budget: 15 minutes, at most three repair/check cycles, within shared 20:00:36–22:00:36Z / 20 launches / four executing turns.
- Unexpected issues: native coordinator `2bd0d79e-d1ee-4f5f-87c3-90ca1a101052` or parent completion; stop conflicting/restricted actions, never bypass exclusions or relax checks.
- Safety: preserve all dirty work and read-only Node reference; no commits, live services, Google calls, auth changes or installs. Sanitize all artifacts: no credentials, sensitive personal data, confidential source bodies or raw transcripts.
