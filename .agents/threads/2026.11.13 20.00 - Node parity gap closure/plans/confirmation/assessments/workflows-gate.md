# Workflow coverage and repeatable offline gate

## Assessment result

Changes made to brief C through this owned refinement supplement, not to the shared brief: prioritize four missing shared journeys; retain existing adapters and runner; separate executable parity from qualification; make freshness, control reachability and raw exit status mandatory

Reviewed brief C / task 3 contract rev 1 as workflow-assessor. This is static assessment, not an implementation or fresh verification receipt. No builds, tests, differential runs, services, network or installation performed. Lead owns acceptance and later implementation authority

Explicitly loaded repository AGENTS.md, swarm common/refiner roles, confirmation rules and task contract, app-rewrite prepare-transfer/verify, and justfile conventions. Host model/effort metadata unavailable

## Existing boundary and evidence

- Original configured unit command: `node --test "src/**/*.test.ts" "scripts/**/*.test.ts"` with cwd `../gdocsmith-node` (`../gdocsmith-node/justfile`, test recipe). package.json has no npm test script. The separately configured live and MCP integration recipes are not part of this unit command
- Target collection: `tests/test_main.cpp` includes model, diff, emulator, lens, reconcile, engine and API suites. CMake registers one aggregate CTest with 120-second timeout and binary-directory TMPDIR (`CMakeLists.txt`, BUILD_TESTING block). A CTest count of one is not one application assertion
- Target parity adapter is EXCLUDE_FROM_ALL; ordinary aggregate build does not guarantee its rebuild (`CMakeLists.txt`, gdocsmith_parity_adapter target)
- Existing differential actually invokes original fileExport/fileImport and target exportModel/fileImport with each project's existing fake backend; it is not MCP or live provider conformance (`tests/parity/node-import-adapter.mjs:24-39`, `tests/parity/cpp-import-adapter.cpp:150-232`)
- Historical receipt: `../../../tasks/parity-evidence/resumed-final-recheck/final-summary.json`, freshGates[0], records differential exit 1. Its named assertions, known-ID variant, 70 controls, unit counts and raw differences are historical, not newly executed here. The vectors file's executed/state prose is also historical and is not gate authority
- Workspace history mentions a restricted export test. It was not opened or reconstructed. No claim about that file's assertions or complete export transfer is made

### Map of existing vectors

| Existing checks | Protected subjourney | Missing boundary, not implied by those checks |
| --- | --- | --- |
| V001 | Existing-document export/import no-op; zero mutations; existing export path | New document, repeated independent export freshness, public file management |
| V002 | Body edit plus document title; content before rename; second-tab preservation | Rename-only, lagged Docs title, failed rename after successful content |
| V003/V004 | Stale refusal, fresh recovery, preserved input; landed/lost write response and stale replay with one batch | Unknown create or comment identity; no automatic replay policy for those writes |
| V005/V006 | Omitted default versus nondefault font on readback | General style/layout/segment behavior |
| V007 | One cell-background edit and absence of unrelated writes | Table structure, merged/resized tables, broader declarations |
| V008 | Hidden suggested insertion, immutable snippet refusal, rejection; preserved synthetic comment/reply payload | Actual comment mutation, reply ownership, resolve/reopen, partial comment outcomes; broader review/atoms |
| V009 plus known-ID variant | Invalid image inputs before send; local/file/http inputs; upload/delete identity; existing-document image replay | New-document rollback and unknown create outcomes; failed actual throwaway deletion warning semantics |
| V010 | Foreign image recreation, one source read, invalid source errors, image ID/no-reinsert checks | General foreign links/atoms or live external fidelity |

`tests/parity/run-engine-differential.mjs:56-76,166-300,311-548` and `tests/parity/safety-assertions.mjs` are the current assertion/control boundaries. V009's backend leak control deliberately produces wrong behavior and checks assertion sensitivity; it is not a characterization of the application's cleanup-failure recovery/warning contract

## Four prioritized uncovered shared journeys

These are shared-boundary evidence gaps, not claims that target production lacks the feature. Retain each parameter row; do not reduce the family to one happy-path status check

### W1 — Create, rollback, recover and retry without duplicate creation

- Original obligations: `../gdocsmith-node/src/core/engine/import.test.ts:617-744,775-785`: null-ID creation reuses initial tab; returned provider ID reaches result/file; repeated updated file creates only once; known post-create failure deletes once and permits retry only after confirmed rollback; failed deletion retains ID/export and warning; mismatch or posttransaction failure keeps landed content; unknown create outcome reports searchable title without invented ID; invalid syntax sends no create
- Target evidence seam: `tests/engine/test_import.hpp:408-564` already has known-created rollback, posttransaction comment-read failure, cleanup-failure and unknown-create unit cases; `src/engine/import.hpp:987-996,1519-1582` tracks identity/cleanup. Existing parity adapter explicitly rejects createDocument (`tests/parity/cpp-import-adapter.cpp:169-174`)
- Minimal extension: one authored synthetic null-document file with generic baseline and two distinct body sections. Add bounded case branches to both existing adapters using their existing create/delete methods, record returned IDs, per-phase calls, remote deletion flag, authored-input bytes and parsed output identity. No new provider abstraction
- Required sequence rows: success then unchanged replay; known create then content failure with confirmed deletion then explicit retry; same failure with deletion rejected; acknowledged content then mismatched readback; create response lost without known ID. Invalid syntax must produce zero creates
- Preserve source result/error/warning fields and call order. Confirm rollback permits a second explicit create only in that row. Unknown identity must never provoke cleanup of a guessed document or an automatic retry
- Reached wrong-behavior control: create succeeds, injected content failure is reached, but delete callback acknowledges without deleting. Actual backend deletion state must reject the false rollback. Separately corrupt result/file provider identity to demonstrate identity-assertion sensitivity

### W2 — Rename-only, delayed title readback and partial content/rename failure

- Original obligations: `../gdocsmith-node/src/core/engine/import.test.ts:476-554`: title-only emits only updateFile and completes; confirmed Drive title wins over lagged Docs title; a failed rename after content reports phase rename, non-none write state and landed content while export keeps old title
- Target seam: `tests/engine/test_import.hpp`, case `import - confirmed content then rejected title reports partial and refreshes`; `src/engine/import.hpp:1418-1421`; adapter rename hook `tests/parity/cpp-import-adapter.cpp:221-227`
- Minimal setup: reuse two-tab text seed. Three explicit rows: title-only; title-only with post-rename Docs read returning old title; body-and-title with rename callback failing after real content mutation
- Assertions: exact mutation kinds/order; no content batch for title-only; unchanged protected tab; returned warning/error fields and rename phase; raw remote content, remote title and parsed export title; input/existing-original preservation where failure recovery requires it. Do not equate delayed Docs title with failed rename
- Reached control: acknowledge rename without changing authoritative metadata, or erase landed content after rename failure. Record reached callback and reject the appropriate title/content assertion. The planned synthetic rejected-write error is not permission to contact an authenticated API

### W3 — Nested tab reparent/order/delete and stable identity

- Original obligations: `../gdocsmith-node/src/core/engine/import.test.ts:849-952`: no-op hierarchy export; sibling reorder; parent add/change/remove including subtree; only updateDocumentTabProperties for moves; new child placement; deletedTabs payload; orphan refusal before writes; delete-all clears one tab; comment-anchor deletion requires commentChanges
- Target seam: `tests/engine/test_import.hpp:31-89,1482` covers validation and provider-created tab revision chaining; `src/engine/import.hpp:1297-1335` applies tab structure through the existing session. These do not establish shared nested state transitions
- Minimal setup: five synthetic tabs, one parent with two children, distinct stable IDs and distinguishable bodies. Three rows: reorder/reparent then no-op; delete parent while one child remains, expecting refusal/no send; explicit subtree deletion then delete-all, expecting one cleared tab. Keep anchored-tab deletion as a separate guarded parameter if selected
- Assertions: exact flattened tree/order/parent IDs; unchanged moved bodies and IDs; only structural request kinds for move; exact deletedTabs payload; final kept-tab title and warning; revision chain; second import zero writes
- Reached control: backend acknowledges a move but leaves parent/order unchanged. Require both callback reach and final hierarchy assertion failure; generic applied status cannot pass. Corrupting a body separately must fail preservation, not be normalized away

### W4 — Authorized comment actions, ownership and unknown partial outcomes

- Original obligations: `../gdocsmith-node/src/core/engine/import.test.ts:1539-1581,1600-1660`: reply/resolve/edit-own/create require allow; final remote comments and exported reply identity/text; other users' text/ownership/read-only fields protected; own reply edit/delete; resolved inclusion/reopen
- Target seam: `tests/engine/test_import.hpp:1018-1044,1171-1214,1229-1306` has approval, partial comments and seven authoritative-action unit rows. Shared adapter currently throws on all commentSend calls and returns static seeded comments (`tests/parity/cpp-import-adapter.cpp:210-220`)
- Minimal extension: existing synthetic review seed gains one own thread, one other-owned thread, one own reply and one resolved thread; use existing fake comment methods, not a bespoke provider model. Rows: no allow refuses before all writes; allowed reply/resolve/edit/create then export; immutable/other-owned field change refuses; reopen included resolved thread
- Add target-only safety characterization, not invented source parity, for second action failing after first lands: preserve edited intent and original, fresh remote comments, confirmed first identity, unknown/partial result, no blind replay. Original partial/unknown behavior must first be observed through the actual original adapter; until then this row remains unresolved
- Reached control: callback returns a plausible receipt without changing authoritative comments; assert post-read state and reject success. For partial failure count sends and retained first identity, never auto-replay a create/reply to obtain a green result

### Deliberately not selected

Independent export freshness (`import.test.ts:762-773`), file create/copy/share/delete public management, additional rich atoms/images/review/segments, conditional corpus execution and non-Apple date/Unicode remain outside this four-journey selection. This is not a denominator or retirement decision. Current adapters cannot prove public MCP mutation behavior, live Google fidelity or non-Apple platform qualification

## Refined minimal gate implementation brief

Proposed future ownership: `scripts/test-parity.mjs`, one alphabetically placed `parity` recipe in `justfile`, and a focused wrapper test beside existing script tests if the lead grants that scope. Existing engine adapters/driver remain unchanged for the first gate-only implementation. No provider framework, normalization, fixture redesign, implicit installs or broad dependency/configure prerequisite

### Interface, inputs and commands

Proposed recipe interface (not currently runnable):

```text
just parity confirmation-01 node
```

`parity run_id node_executable="node"` invokes the small script with `--run-id confirmation-01`; the selected node executable starts the wrapper and its original-adapter children. Require an explicit already-installed reviewed runtime supporting the original TypeScript runner; record executable digest/version. Do not install or substitute a runtime on failure

The script maps that fresh run ID to project-relative `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/confirmation-01/`. Reject an existing destination, invalid ID, symlink ancestor, path traversal, foreign/nonprivate output, absent reviewed executable/cache/dependencies or stale adapter. Do not overwrite or clean old receipts

Existing command to rebuild only the adapter, under a separately granted sole runtime owner, with no configure/install:

```sh
cmake --build build/cmake/ParityChecks --target gdocsmith_parity_adapter --parallel 2
```

The wrapper then launches this existing driver with exactly these argv pairs, using its own selected node executable:

```sh
node tests/parity/run-engine-differential.mjs \
  --original-root ../gdocsmith-node \
  --target-adapter build/cmake/ParityChecks/gdocsmith_parity_adapter \
  --vectors '.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/proposed-vectors.json' \
  --case-ids V001-noop,V002-body-and-title,V003-stale,V004-landed-lost-response-replay,V005-font-default,V006-font-nondefault,V007-cell-style-edit,V008-reject-hidden-insertion,V009-image-upload-lifecycle,V010-cross-doc-image-copy \
  --owned-run-root '.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/confirmation-01/runs' \
  --results '.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/confirmation-01/differential-results.json'
```

The `node` token denotes the same selected executable, not an additional PATH lookup. The current driver automatically executes the known-upload-error variant and controls; there is no variant/control flag. W1-W4 need later adapter/case additions and cannot be selected by inventing IDs now

### Separate qualification commands, not parity dependencies

These are currently configured command shapes, deferred to authorized execution:

| Outcome | Exact command and cwd | Scope |
| --- | --- | --- |
| Original unit | `node --test "src/**/*.test.ts" "scripts/**/*.test.ts"`; cwd `../gdocsmith-node` | Configured original unit files, not integration |
| Debug target unit | `cmake --build build/cmake/GapChecks --target gdocsmith_mcp_test --parallel 2`, then `ctest --test-dir build/cmake/GapChecks --output-on-failure --no-tests=error`; cwd target | Aggregate target assertions, not source-transfer proof |
| Release target unit | Same two commands with `GapRelease` | Separate optimized build qualification |
| Target MCP discovery | `node scripts/test-mcp.mjs build/bin/gdocsmith-mcp export,import,file,status`; cwd target, project-contained TMPDIR required | Existing isolated stdio plus ephemeral loopback HTTP; no Google calls or mutation parity |

No `just test`, `just build` or `just test-mcp` dependency in the proposed parity recipe: current configure recipes install dependencies, and test-mcp rebuilds canonical output. Do not implicitly run original test-all/live/MCP recipes. Restricted evidence remains restricted; inability to qualify affected coverage is reported, not bypassed

Original collection is the actual two configured globs and discovered file identities, recorded separately from execution/assertion results; do not infer collection from historical 846. Target collection manifest records tests/test_main.cpp and included allowed files; `ctest --test-dir build/cmake/GapChecks -N --no-tests=error` lists aggregate registration only, not internal cases. Do not claim a runner-level original collection-only command without implementing/verifying one

### Output, failure and control contract

1. Retain existing `differential-results.json` and `differential-results-raw-diffs.json` unchanged. Add one sanitized `gate-summary.json`: exact argv/cwd, exit/signal/timing, input identities, and independent fields for original execution, target execution, named assertions, controls, raw differential, freshness, cleanup, unit qualification, MCP discovery, live fidelity and platform qualification
2. Unit/MCP outcomes default `unrun`, never inherited from historical receipts. Live fidelity and other-platform qualification default `unverified`; no fallback network or service activation. Overall output explicitly scopes itself to offline engine parity
3. Expect exactly the ten selected cases once and the known-ID variant once, completed original and target observations, no launch failures and zero named assertion failures. Verify every expected control exists by stable name/side/case/variant and that its intended baseline assertion passed. Require detected=true and exact requiredAssertion in rejectedBy; backend controls additionally require reached=true. Do not use 70 as a substitute for identity/presence checking
4. Preserve separate observation-corruption and reached-backend controls. Launch failure, malformed JSON or timeout cannot count as detecting wrong behavior. Missing observations or an empty controls list fail closed
5. Raw parity is GREEN only with zero differences on all selected cases/variants. Any retained raw difference remains RED and preserves child exit 1, even if named checks and controls pass. Classification or narrowly accepted recovery layout is not a new comparator normalization or a green raw result
6. Require frozenInputs=true, compiledInputsCurrent=true, and ownedRunDirectoriesRemaining=0. Existing driver checks five compile-hardening markers, gives each child 120 seconds/8 MiB stdout, isolates temporary directories and deletes only marked child-owned run directories (`run-engine-differential.mjs:35-50,311-350,500-548`). Keep those checks and unmodified ordered observations
7. Proposed wrapper: one 300-second driver deadline; on expiry terminate only its own child tree, record timeout/nonzero and partial evidence, never resume/replay writes. Do not delete unknown or unmarked leftovers. A missing complete report or cleanup failure is RED, not a retry invitation. Implementation must test process-tree termination rather than merely timing out the parent
8. Success requires both child exit 0 and valid complete report. Child exit 1 stays 1; other nonzero/signal/timeout becomes a nonzero infrastructure result with cause retained. Never append `|| true`, suppress differingCases, trim Markdown, remap IDs/errors or rewrite expected output

### Freshness beyond existing driver checks

Before any authorized build, record revision plus dirty content identities of original src/scripts/config/lockfiles; target src, tests/parity, selected fixtures, CMakeLists/justfile; proposed wrapper/operative contract; selected runtime and installed relevant dependency versions. Record cache, compile_commands, toolchain/build mode and executable digests after build. Match build receipt input identities to this run

Current driver fingerprints src/tests-parity/CMake, vectors, executable, original src/scripts/config and four installed dependency versions; it compares before/after content and checks source mtimes against binary. Mtime alone cannot establish provenance, and its identity excludes target aggregate tests and the proposed wrapper/justfile. The wrapper must bind those omitted operative inputs in its own before/after manifest without copying raw source or secrets

Any relevant source, assertion/control, adapter, seed/vector, normalizer, runtime/dependency, build configuration or operative scope decision change invalidates dependent outcomes. A changed target unit suite invalidates unit qualification; an unrelated report wording change need not invalidate engine execution. Unknown impact reopens the dependent claim

## Implementation graph and decision boundary

1. Lead accepts this refinement and chooses coverage/dispositions with assessments A/B. No change to existing contracts or shared brief by this worker
2. Assign one gate wrapper/justfile owner; first implement only fail-closed reuse of the current driver. Assign independent static/control review before execution
3. W2 is the smallest adapter extension; W1/W3/W4 follow only when individually selected and owned. All share the two adapters/driver, so serialize those edits rather than create concurrent writers. New fixture files can be prepared independently if separately granted
4. One later canonical runtime owner rebuilds adapter, freezes identities and runs affected checks into fresh destinations; no current assessment has runtime permission
5. Full integrated acceptance depends on lead-accepted source-obligation and raw-difference dispositions as well as fresh selected evidence. A narrow wrapper implementation can proceed independently, but must remain RED while raw differences remain

No architectural decider needed for the minimal wrapper approach. Lead decision required only on which journey rows to implement and the scope of any parity claim; partial comment behavior without original observation remains explicitly unresolved. No new permission, normalization waiver, source retirement or release recommendation requested
