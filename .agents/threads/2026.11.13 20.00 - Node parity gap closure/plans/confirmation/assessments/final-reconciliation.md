# Final Reconciliation — Cycle 1 Frozen Convergence Snapshot

Reviewer: `final-reconciliation-reviewer` (`reviewer2`, read-only)
Subject: `13-A` frozen convergence snapshot & `confirmation-03` parity receipts (`13-R` contract rev 1)

## 1. Frozen Tree & Binary Identity Verification

- **`HEAD`:** `20ed42e6a67341d71a13f404f149aee34b4c30bc` (verified)
- **Canonical Release binary (`build/bin/gdocsmith-mcp`):** SHA-256 `68809beff98e055f90d079014efe3df6f82a637ac9210c03a0ccb904cf1b14db` (verified)
- **`justfile`:** SHA-256 `e65a208fbb6186f04c739185b3356a1be3d66925c27198c792f76d42f3e9cf81` (verified; all `src/` headers hashed and matched against frozen tree)
- **`confirmation-01` / `confirmation-02` isolation:** Both directories under `tasks/parity-evidence/` contain only an empty `runs/` subdirectory and zero result artifacts; neither affected `confirmation-03` or any canonical/historical receipt.
- **`confirmation-03` input integrity:** `identityBefore == identityAfter` (`true`), `compiledInputsCurrent` (`true`), `allValuesAreSynthetic` (`true`), `onlyOwnedRunPrefixSanitized` (`true`).

## 2. Qualification Checks (`just test`, `just build`, `just lint`, `just test-mcp`)

| Check | Independent Verification Result | Notes |
| --- | --- | --- |
| Debug+ASan+UBSan (`ctest --test-dir build/cmake/Debug`) | **PASS** (63.60s) | `436` test cases (`436 passed | 0 failed | 0 skipped`), `6976` assertions (`6973 passed | 3 failed` — all 3 failures are inside the 3 `doctest::may_fail()` cases) |
| Release (`ctest --test-dir build/cmake/Release`) | **PASS** (1.90s) | `436` test cases (`436 passed | 0 failed | 0 skipped`), `6976` assertions (`6973 passed | 3 failed`) |
| `doctest::may_fail()` count | **3** cases (not 4) | `tests/engine/test_import_structure.hpp:147,164,190` (historical `6911` assertions / `4 may_fail` count predates Cycle-1 `10-B`/`10-C` additions; current frozen tree is `436` cases, `3 may_fail`, `6976` assertions) |
| `clang-tidy` (`src/main.cpp` with `build/cmake/Debug`) | **PASS** (exit `0`) | Verified via `/opt/homebrew/opt/llvm/bin/clang-tidy -p build/cmake/Debug src/main.cpp` |
| `test-mcp` (`node scripts/test-mcp.mjs build/bin/gdocsmith-mcp export,import,file,status`) | **PASS** (exit `0`) | `MCP 2026-07-28 stdio+http conformance passed: gdocsmith-mcp, 5 tools` |

## 3. Differential Parity Receipt (`confirmation-03`)

Source: `tasks/parity-evidence/confirmation-03/differential-results.json` & `differential-results-raw-diffs.json`

- **Cases / Variants:** `10` cases + `1` variant (`11` total evaluated)
- **`originalAssertionFailures`:** `0`
- **`targetAssertionFailures`:** `0`
- **`launchFailures`:** `0`
- **`controlsDetected` / `controlsTotal`:** `70` / `70` (`observationControls: 64`, `backendControls: 6`)
- **`ownedRunDirectoriesRemaining`:** `0`
- **`qualification.pass`:** `false` (nonzero exit `1` preserved; no raw waiver)
- **`qualification.reasons`:** `11` entries (`V001-noop` through `V010-cross-doc-image-copy` and `V009-image-upload-lifecycle/known-upload-error` each `"has raw differences or none were computed"`)

### Per-Case Raw Difference Counts

| Case / Variant | Raw Leaf Diffs |
| --- | ---: |
| `V001-noop` | 9 |
| `V002-body-and-title` | 13 |
| `V003-stale` | 8 |
| `V004-landed-lost-response-replay` | 13 |
| `V005-font-default` | 27 |
| `V006-font-nondefault` | 23 |
| `V007-cell-style-edit` | 10 |
| `V008-reject-hidden-insertion` | 18 |
| `V009-image-upload-lifecycle` | 52 |
| `V010-cross-doc-image-copy` | 35 |
| `V009-image-upload-lifecycle/known-upload-error` | 30 |
| **Total (`confirmation-03`)** | **238** |

- **Historical comparison:** `238` raw leaf differences across `194` `groupedExactPathAndValues` groups in `confirmation-03`, down from `250` raw leaf differences across `206` groups in historical `resumed-final-recheck` / `resumed-final-qualification`.
- **Raw group classification breakdown (`groupedExactPathAndValues`):**
  - `external-call-schedule-or-payload-difference-unresolved; ordered calls retained`: 99 groups (122 leaves)
  - `observable-contract-difference-unresolved`: 33 groups (36 leaves)
  - `native-file-format-difference-unresolved; no trim/drop/paragraph normalization; lens owner must prove source-linked equivalence or fix`: 27 groups (31 leaves)
  - `provider-state-difference-potential-defect`: 22 groups (35 leaves)
  - `write-request-payload-or-order-difference-potential-defect`: 5 groups (6 leaves)
  - `wording-variance-proposal-unaccepted; retain exact instructions and semantics`: 5 groups (5 leaves)
  - `intentional-equivalent-recovery-directory-layout-parent-approved; raw path retained; fresh/distinct/contained/no-clobber checks remain required`: 3 groups (3 leaves)

## 4. Explicit Scope Boundaries & Unresolved / Cycle-2 Items

1. **Unknown whole-source denominator:** `assessments/source-obligations.md` inspected selected assertions across 5 of 61 discovered original `.test.ts` files plus helpers/config; the total meaningful-obligation denominator across `../gdocsmith-node` remains unknown. No percentage or full-parity closure claim is made.
2. **Remaining raw RED differences (no waiver):** All `238` raw leaf differences (`194` groups) across all `11` cases/variants remain RED (`qualification.pass: false`); no normalization or raw waiver is applied.
3. **Unexecuted live Google & non-macOS platform checks:** All verified runs (`ctest`, `test-mcp`, `confirmation-03`) are offline macOS (`arm64-osx`) executions against synthetic/mocked backends. Live Google Docs/Drive API behavior and non-macOS platform parity remain unexecuted.
4. **Cycle-2 and open items preserved for follow-up:**
   - **`9-B`..`9-F`:** Shared workflow journeys (`W1`–`W4`: partial-rename `9-B`/`9-C`, create/rollback/retry `9-D`/`9-DR`, nested-tab lifecycle/refusal `9-E`/`9-ER`, authorized comment partial outcomes `9-F`/`9-FR`) remain blocked/unselected for Cycle 2.
   - **`11-A` 3 target REDs:** `tests/engine/test_import_structure.hpp:147,164,190` (`rtl keeps pageless tab pageless`, `new pageful landscape tab writes LETTER size`, `second section header and footer lifecycle`) remain `doctest::may_fail()` with 3 failing target assertions.
   - **`src/lens/render.hpp:126`:** Paragraph colon escaping logged separately during `7-R` review remains open.
   - **`Q-L1`..`Q-L3` (product/lead-owned decisions):**
     - `Q-L1`: Effective-style sameness vs explicit/inherited stored-font structure (`V005`/`V006` O5 inheritance difference).
     - `Q-L2`: Restore `apiUser` frontmatter field (adds identity read) vs formally drop and amend docs.
     - `Q-L3`: Acceptance of named-style/cell-style declaration-edit refusal narrowing and native-file equivalence criterion (A/B/C).
   - **`12-B` corpus execution:** Refined brief (`assessments/corpus-brief.md`, SHA-256 `e7c7c7160158aaa280ad2d5d48af21d0fdda070edf7350ec6d7765925086bbb0`) is accepted, but corpus execution remains unimplemented (`0` documents / `0` tabs executed).
