# Frozen qualification — RED, not release accepted

Parent source-freeze/qualification authority: 21:32Z. All ten independent gates
completed, 21:33:16–21:37:46Z. One bounded targeted confirmation completed
21:38:43Z. Final source/dependency verification and scratch cleanup: 21:39:20Z.
No owned commands remain running. No production/test/adapter source was edited.

## Executed gates

Exact argument arrays, timestamps, exit status, output digests and sanitized
metadata are in the individual gate JSON receipts and `status.json`.

| Gate | Actual result |
| --- | --- |
| Cached GapChecks Debug test build | exit 0 |
| Full fatal ASan/UBSan CTest | exit 8: 395 cases, 391 pass, 4 fail, **0 skipped**; 6440 assertions, 6401 pass, 39 fail |
| Cached GapRelease app + test build | exit 0; final app binary retained |
| Full Release CTest | exit 8: same 395/391/4/0 and 6440/6401/39 |
| Cached isolated ParityChecks adapter build | exit 0 |
| Existing app/test/adapter clang-tidy | all three exit 0, no diagnostic locations |
| Actual isolated MCP stdio + ephemeral HTTP | exit 0; MCP 2026-07-28 conformance, five discovered tools |
| Actual ten vectors + isolated known-ID variant + controls | exit 1, detailed below |

Builds used existing installed prefix/caches, no installation or dependency
configuration changes. Compilation databases bind C++26, zero auto-init,
libc++ hardening, and Debug/adapter fatal ASan/UBSan; Release retains configured
IPO. Tidy used `--extra-arg=-DDOCTEST_COUNTER=__COUNTER__`.

The original configured Node24.11.0 baseline remains **846 passed, 0 failed,
0 skipped**: its complete original input identity still matches the preflight
execution, as do the installed dependencies. It was not unnecessarily rerun.
Normal-return corpus tests remain a coverage caveat, not counted corpus proof.

## Four full-suite failures — not waived

Both full suites reproduce these test-case metadata names:

1. `public import defaults to injected Docs and Drive through revision-locked transaction`
2. `export - rich synthetic content does not claim editable segments are read-only`
3. `transaction Markdown uses typed putback and rejects atom changes atomically`
4. `review export keeps table cell and nonbody context and round-trips immutable snippets`

The second is the known policy-excluded export structural failure. Its source
was **not read, hashed or reconstructed**, including through runner/compiler
diagnostics. Only aggregate failure metadata is reported. It remains unwaived.

One bounded Debug selector rerun confirmed the other three failures: four
selected cases (one additional matching case passed), 276 assertions, 238 pass,
38 fail. Its 391 unselected cases are explicitly selector-skipped, NOT a new
zero-skip full-suite result. Receipt: `targeted-rerun.json`.

Allowed-source failure locations: API test lines 475–501 and 534–535,
transaction test lines 162–171, suggestion-block test case line 92. These cover
public API result/write/recovery checks, atom-mutation refusal/atomicity
expectations and immutable review roundtrip. No source fix or test-expectation
retirement is authorized or inferred. The public-API output's exact underlying
error was not retained; failure location metadata must not become a speculative
root-cause claim. No sanitizer fatal report was observed; ordinary failed
assertions still make both gates RED.

## Fresh differential and sensitivity results

- Ten main vectors plus one known-upload-ID failure variant: **151 original
  assertions passed; 171/172 target assertions passed**. No launch failures.
- The sole named target failure is V008
  `review:full-source-suggestion-grammar`: actual target suggestion header omits
  source `tab=Overview exportLine=13`, and its parsed suggestion text includes
  an additional trailing newline. Full actual original-parser comment/reply/
  quote/orphaned/location assertions passed; that does not waive grammar.
- **69/70 controls detected**: 63/64 observation-corruption controls plus
  **6/6 reached backend-behavior controls**. The C++ corrupt-suggestion-grammar
  control is not credited because its baseline assertion already fails.
  Historical 48/48 meant 42 observation controls plus six backend faults;
  those historical counts were not relabeled as fresh 70/70.
- V003/V004 authoritative fresh recovery-body checks pass on both engines,
  alongside distinct contained paths, current revision and input preservation.
  Only their three raw recovery-path leaves receive the preexisting narrow
  OB-006/008 layout disposition.
- Known-ID variant now passes same canonical existing path, exact kept-local-
  edits warning, edited-byte hashes/preservation and authoritative
  `export.orig.md` body/revision checks on both actual engines. No new layout/
  warning waiver was applied.
- V007's only-intended-green-cell-write assertion passes on both engines.
- Actual positive upload now has **two payloads on each side** for the three
  image occurrences; exact cleanup, no-send and provider-ID/replay checks pass.
  The earlier target three-payload duplication is not reproduced.
- Raw initial native version is exactly `3.0.20` for both sides in all eleven
  executions. The comparator no longer calls `versionTranslated`; version
  values and lines remain in actual observations and raw comparison.

`differential-results.json` and `differential-results-raw-diffs.json` retain all
synthetic raw observations and differences. **250 leaves / 206 exact groups**
remain across all eleven executions. This expanded observation schema includes
fresh original-file captures; counts are not directly comparable to historical
224 leaves and do not alone measure improvement/regression.

## Remaining raw differences — classified, not normalized away

The driver's exact-group classification counts are recorded in
`qualification-summary.json`: 101 ordered external-call/payload groups,
27 native-file-format groups, 35 other observable-contract groups, three
narrowly approved recovery paths, nine write-request groups, 26 provider-state
groups and five unaccepted wording groups.

Consequential examples:

- Native complete-file YAML/declaration/optional metadata and review grammar
  differ. No whitespace, paragraph, warning or version suppression is applied.
- V005/V006 explicit default font/family writes, emulator run segmentation and
  nondefault-font mismatch diagnostic detail remain different.
- V008 comment location values, snippet error line/phase, extra style requests
  and default paragraph fields differ in addition to the named grammar failure.
- V010 foreign-image errors still omit source tab/line, add planning phase and
  shorten remediation/context. These remain unwaived.
- Invalid local-image read diagnostic/phase differs.
- Fake-created upload document IDs/URIs and original-versus-target deletion
  evidence differ. Recorded bijections are **not applied to comparison**.
  These provider-substitute identity/fidelity distinctions are not additional
  production normalization authority.
- Ordered read schedules and call payloads remain visible. Successful named
  behavior does not approve their raw differences.

## Frozen identity and retained binary

Start/end/final-after-targeted manifests agree on all **311 permitted input
entries**, including explicit deleted/policy-unavailable markers. Actual import
and test-import hashes match the parent frozen receipt:

- `src/engine/import.hpp`: `13b9c65659979a373e2e78654eed11eb28445b13b70cb5a949130d1f6e891b22`
- `tests/engine/test_import.hpp`: `25d97e7660fc06741da157b7febf070b1b57f5aca28bd002f893b08e27039b8e`

All fingerprinted installed original/C++ dependencies remain unchanged.
`frozenInputs=true`, `compiledInputsCurrent=true` in the differential receipt.
The parent's separate five-case/66-assertion creation-boundary approval is
acknowledged only for that scoped regression, not used as overall engine proof.

Persistent **qualification-only** Release binary:
`build/bin/gdocsmith-mcp`

SHA-256: `4cb9df48a10fd4a0b627185b1a410268ed0797a60c3bea7a4b61fcae7ea7f04c`

Fresh adapter SHA-256:
`505fe77e0abbaccdf0148735cad8622bbeb01937660beb5f1a4572bdb63241d1`

No source/adapter repair, release acceptance, Google/live/account/auth/network
permission, service persistence or blanket parity claim follows from this run.
All owned scratch was removed; driver reports zero surviving run directories.
Return idle with completed evidence, not a pending background command.
