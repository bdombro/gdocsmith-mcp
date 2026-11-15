# Final coupled-fixture recheck — unit qualification passes

Completed **219.022 seconds**, with final identities and cleanup verified
**23:00:29Z**, before the 23:02Z stop. No owned commands remain running.
No source fixes, installs, reconfiguration, live calls or original edits.
Prior RED receipts remain historical and unchanged.

## Fresh execution

- Debug TEST target rebuilt; full fatal ASan/UBSan CTest **passed**:
  **398/398 cases, 6521/6521 assertions, zero failures/skips**.
- Release TEST target rebuilt; full CTest **passed**, with the same actual
  **398 cases / 6521 assertions / zero skips**.
- Test translation-unit clang-tidy **passed** with the existing doctest counter
  flag and no diagnostic locations.
- Actual ten shared vectors plus isolated known-ID variant completed in
  **5.828 seconds**, dedicated 120-second allowance:
  **151/151 original + 172/172 target assertions pass**.
- **70/70 controls detected**: 64 observation-corruption controls plus
  **six reached backend-behavior controls**. Zero launch failures.
- Driver exit **1 is expected for raw differences**, not a timeout or a named
  assertion failure: **250 raw leaves / 206 exact groups**, across eleven
  executions, remain retained and unaccepted.

Successful full-suite counts were extracted only from the aggregate runner's
summary lines; no raw transcript or source bodies were copied into evidence.

## Explicitly retained rather than rebuilt

The just-completed `resumed-final-qualification/` Debug app, Release app and
ParityChecks adapter builds; app/adapter tidy; and isolated MCP stdio/HTTP pass
remain valid. Exact production/configuration/driver inputs, all three binaries,
installed dependencies, CMake caches and compilation databases were rechecked
and match. `retained-gates.json` distinguishes these from the fresh test gates.

Persistent Release binary: `build/bin/gdocsmith-mcp`

SHA-256: `d5380255433a528b15457e4ebe150e1182af42af6b8ad42952e93ba63be4fa0c`

Adapter SHA-256:
`cb10ff614af627265cde78f00dc65370cb23b5b60d76e729c744e86a8ea282fc`

## Identity, policy and residuals

All **311 actual current manifest entries** match start/end/final checks.
The explicitly authorized ORIGINAL view of `tests/engine/test_export.hpp`
lines 1–12 succeeded before hashing; its actual current hash is now included,
not the old unavailable marker:
`c7d04d2718f193f5fc74388604e9fa1c94d5ee634b9ce843842712317bb6e863`.
Historical unavailable markers were not retroactively changed.
`messages.md` was not accessed.

Driver `frozenInputs=true`, `compiledInputsCurrent=true`. Exact original inputs
and installed dependencies remain unchanged, retaining the configured
**846-pass/zero-skip Node baseline** with its original corpus-coverage caveat.

V008 exact grammar, actual native versions, recovery content/revision/input
preservation, known-ID path/warning/edited bytes/fresh original, no-send,
cleanup and provider-ID/replay assertions pass. No version translation,
whitespace trimming, warning suppression or raw-ID normalization was added.
Only the existing OB-006/008 recovery-layout disposition applies; all other
raw file/request/state/location/error/provider distinctions remain unaccepted.

This supports bounded unit/build/tool qualification, **not blanket Node parity
or parent release acceptance**. `final-summary.json` and individual receipts
provide exact commands/counts/hashes; raw synthetic observations remain in the
differential receipts. Scratch removed; zero surviving run directories.
