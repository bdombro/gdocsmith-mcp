# Resumed stable-input qualification — RED, completed

Executed **22:48:14–22:52:43Z (268.49 seconds)**. Final identities and cleanup
verified **22:52:59Z**, before the 22:56Z stop. No owned commands remain running.
No source/test/adapter edits, installs, reconfiguration, live calls or services.
Earlier receipts retain their original scope; none were relabeled.

## Results

| Gate | Actual outcome |
| --- | --- |
| Fresh ParityChecks adapter build, performed first | exit 0 |
| Actual ten vectors + known-ID variant, dedicated 120-second allowance | completed **6.624 seconds**, exit 1 for raw differences; **151/151 original + 172/172 target assertions pass** |
| All controls | **70/70 detected**: 64 observation-corruption controls, six reached backend behavioral controls |
| Fresh Debug app/tests build | exit 0 |
| Full fatal ASan/UBSan Debug CTest | exit 8: **398 cases, 397 pass, one fail, zero skips; 6520 assertions, 6519 pass, one fail** |
| Fresh Release app/tests build | exit 0 |
| Full Release CTest | exit 8: same actual 398/397/1/0 cases and 6520/6519/1 assertions |
| App/test/adapter clang-tidy | all three exit 0, no diagnostic locations |
| Isolated MCP stdio + ephemeral HTTP, cached Node24.11.0 | exit 0; MCP 2026-07-28 conformance, five tools |

The sole full-suite failure in **both** configurations is:

`export - pure file projection exposes pending review context without mutating review payloads`

This is not the earlier rich-export failure case. Only sanitized runner failure
metadata was retained; restricted export source was not read, hashed or
reconstructed. No diagnosis or source fix is inferred. Parent decides any repair.

## Shared behavior and remaining differences

Zero shared named assertion failures or launch failures. V008's exact original
suggestion grammar, including `exportLine=13`, now passes. Actual raw versions,
authoritative recovery content/revision, input preservation, exact known-ID
path/warning/edited bytes/fresh original, no-send, exact cleanup, cell-only
write and provider-ID/replay checks all pass their current named assertions.

The raw comparator still correctly exits 1: **250 leaves / 206 exact groups**
across eleven executions, all retained. No version translation, trimming,
warning suppression or created-ID replacement was applied. Only the preexisting
three OB-006/008 recovery-path leaves have their narrow parent disposition.
Whole-file envelope bytes, ordered calls, font/default/run representation,
review location/additional style writes, error phase/location/remediation and
provider-substitute identities/fidelity remain unaccepted distinctions.
All initial native version values are exactly `3.0.20`.

This is completed bounded application evidence, not universal Node parity or
release acceptance. Historical 48 controls meant 42 observation controls plus
six backend faults; fresh counts are explicitly 64 plus six.

## Identity and persistent binaries

All **311 permitted source/config/test/fixture manifest entries** match across
start/end/final verification, with explicit policy-unavailable markers.
All fingerprinted installed C++ and original dependencies remain unchanged.
Driver `frozenInputs=true`, `compiledInputsCurrent=true`.
Original configured baseline remains valid: **846 passing, zero skips**, with
its exact original inputs unchanged. Corpus normal-return coverage caveat remains.
Thread `messages.md` was not accessed.

Persistent **qualification-only** Release binary: `build/bin/gdocsmith-mcp`

SHA-256: `d5380255433a528b15457e4ebe150e1182af42af6b8ad42952e93ba63be4fa0c`

Fresh adapter SHA-256:
`cb10ff614af627265cde78f00dc65370cb23b5b60d76e729c744e86a8ea282fc`

`final-summary.json` records actual gate counts/timing/binary hashes and raw
classification totals. Individual receipts retain exact commands, output
digests and sanitized failure metadata. `differential-results.json` and its
raw-diffs sibling retain synthetic observations; source/dependency/build
manifests bind their inputs. Owned run directories are empty and scratch removed.
