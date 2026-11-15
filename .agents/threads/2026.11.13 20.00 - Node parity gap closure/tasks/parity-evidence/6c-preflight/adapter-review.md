# 6c independent preflight — changes needed before parity acceptance

Scope: static examination of the three actual `tests/parity` implementations,
referenced original assertions and real original emulator/import dependencies;
configured original unit baseline executed. No target build, target test run,
adapter execution, final differential, network or live suite was performed.
This is not source-freeze or parity acceptance.

## Fresh original baseline

`original-baseline-final.json`: supplied Node **v24.11.0**, original HEAD
`d00069dd188ca995bfda9828cc9eee5016603b63`, exact quoted arguments
`node --test "src/**/*.test.ts" "scripts/**/*.test.ts"` from the original root:
**846 tests, 117 suites, 846 pass, 0 fail, 0 skipped**, exit 0.
Started 21:16:18Z, completed 21:16:26Z. Original source/scripts/config identities
were unchanged across execution. No original files were edited.

Two preliminary executions failed because this host has no ambient `node` and
the initial owned scratch path contains spaces: the original release dry-run
subprocess needs Node on PATH, and its image fixture constructs an unencoded
file URI. The final execution used the SAME supplied runtime bin on the child
process's PATH and project-local `build/6c-preflight-runtime`, with TAP reporting.
Neither global PATH nor source/config/dependencies changed. Both failed receipts
are retained; raw output was not retained. All exact newly owned scratch roots
were removed.

Zero reported skips is not proof of corpus participation: original
`src/core/model/corpus.test.ts` and `src/core/emulator/tape.test.ts` return normally
when the local corpus is unavailable/empty. No private corpus contents or row
counts were collected. The distinct recorded-conformance suite enumerates JSON
fixtures and compares them; its mechanism is not the early-return corpus test.

## Adapter and assertion examination

- Both sides call their actual public file-import engines with real parse,
  reconcile, transaction, artifact and emulator dependencies. C++ export setup
  uses real model/export functions, not a separately implemented expected model.
  Original setup invokes its actual file-export path. Existing-document only;
  document creation, comment mutation and unexpected image operations fail closed.
- The source pointers in `run-engine-differential.mjs` were checked directly:
  original `import.test.ts` 115–149, 330–350, 410–429, 451–463, 512–533,
  569–583, 1323–1351, 1811–1844 and 1917–1947. These ground font omission,
  hidden insertion rejection, unknown landed writes/replay refusal, no-op,
  rename ordering, stale preservation, cell styling and image lifecycle.
- Readback font omission mirrors the original test's provider seam. Revision
  races mutate the actual fake revision before its revision-lock check;
  acknowledgement-without-mutation bypasses actual backend application;
  landed-response loss throws only after actual application. These are reachable
  behavioral faults, not fabricated result objects.
- Upload leak controls reach the actual delete callback and omit backend deletion.
  Original FakeGoogle's public getDocument does not expose deletion absence;
  actual delete-method completion is the original-side limit. C++ additionally
  checks its real fake's deleted flag. Neither is live Google deletion proof.
- The synthetic PNG headers, local/file bytes, inert HTTP response, returned
  upload URI mapping and typed known-ID upload failure are grounded in the
  original `imageInfo`, `imageUploadsCreate` and FakeGoogle contracts. C++'s upload
  substitute mirrors the original fake upload document/URI receipt, not the
  production conversion service. Conversion/downscaling/provider validation,
  unknown-ID failure and cleanup failure are not exercised.
- Historical 20:55 evidence has 141 original + 162 target assertions and 48
  controls: **42 observation-corruption controls, six backend behavioral controls**
  (ack/no mutation, pre-send race and upload leak, on each side). A corrupted
  observation tests assertion sensitivity, not production behavior. This preflight
  did not rerun these controls and does not renew their freshness.

## Consequential acceptance gaps

1. **Known-upload-error variant is underasserted.** Its early return checks error,
   failed/unknown/inconclusive, no content batch and exact cleanup, but NOT original
   same-revision export-path role, kept-local-edits warning, edited-byte
   preservation or fresh recovery content. The reported narrow source repair
   therefore cannot be accepted from a green named-assertion total. Parent must
   examine fresh raw observations and independently verify these branches;
   OB-006/008 layout permission does not apply to this variant.
2. **Recovery waiver predicates are incomplete as named assertions.** V003/V004
   check distinct/contained output, current revision and input preservation,
   but do not assert authoritative fresh output body. V004 independently checks
   landed remote body, not the recovery file body. A same-revision but incorrect
   recovery body must not receive the layout waiver.
3. **The reported raw differential is not literally unnormalized.**
   `versionTranslated` replaces initial version and the native gdocsmith line
   before comparison. Root replacement is approved; this version translation is
   not an additional approval. Complete original observations are retained, and
   strict actual Node parsing rejects incompatible raw exports, but final
   acceptance must separately compare the unmodified version values/lines.
   Created-ID bijections are recorded, not applied; stable IDs and revision
   spellings remain unnormalized.
4. V007 asserts the resulting cell is green, not that only the intended cell
   style was written. V008's main checks test suggestion/comment marker presence
   and parser validity, not complete comment/reply payload equivalence or all
   source suggestion-block grammar. The separate historical strict-review proof
   must not silently substitute for a fresh current-source check.
5. The driver is correctly fail-closed on any raw differing case, launch failure,
   changed inputs, stale compiled inputs, named failure or surviving control.
   Its historical exit 1 is NOT equivalent to named-assertion failure and must
   not be relabeled green. The 224 historical leaves remain unaccepted.

## Raw differences that require classification, not suppression

Only the separate recovery-root layout of V003/V004 (OB-006/008) has narrow
parent permission, with all preservation/current-content predicates retained.
All other classes remain consequential until source-bound evidence and parent
disposition: whole-file YAML/declaration/whitespace bytes; ordered reads/writes;
explicit default font fields and run segmentation; mismatch diagnostic detail;
review commentsLine/phase/line and extra style writes; duplicate alias upload
payloads/URI linkage; known-upload-error warning/path; image/foreign-document
error metadata and remediation. A warning or request change is not an opaque
provider-ID spelling difference. No new normalization or retirement is granted.

## Identity, freshness and next handoff

`input-identities.json` contains explicit permitted relative paths, hashes,
deleted markers, all installed files for the four original runtime dependencies,
639 C++ prefix dependency files and six existing cache/compilation database
identities. The excluded engine test is marked unavailable and was NOT read,
hashed or reconstructed. This creates a source-evidence limitation, not permission
to bypass it.

Compared with the 20:55 receipt, 13 target inputs changed, including the driver,
API, emulator, engine and lens. Source is still changing; no canonical target
output was built or executed. Installed original versions: argsbarg 8.3.0,
yaml 2.9.1, marked 18.0.14, zod 4.6.5. Runtime SHA-256:
`8d66cad090d087ed8fac66d8f7248c8a9a55454680232a6d109f609aa2decf89`.

Return idle/resumable. Canonical GapChecks/GapRelease/bin, tidy, isolated MCP
and rebuilt ParityChecks/ten vectors/known-ID variant/48 controls remain blocked
until explicit parent source-freeze plus independent-review release. Final
manifest must be captured again and checked at completion; current preflight
identities do not freeze concurrent owners.
