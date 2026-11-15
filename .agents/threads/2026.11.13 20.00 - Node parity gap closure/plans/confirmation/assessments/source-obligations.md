# Bounded source-obligation supplement

Task 1, contract rev 1; independent refiner source-assessor. Result: changes made to brief A's assessment, ready for lead review, not acceptance. Static inspection only; no application, baseline, test, build, network or live execution.

## Scope and evidence vocabulary

Eight consequential families were inspected below. Stable inherited IDs remain authoritative; SA IDs identify this supplement, not a new denominator. N paths mean the read-only original `../gdocsmith-node`; C paths mean the current target repository. Line ranges refer to the inspected files, not historical ledger ranges.

- Direct: identical meaningful check runs on both implementations. None established here
- Translated candidate: current target assertions statically preserve the named source observations; runtime qualification remains unverified
- Equivalent replacement: requires justified invariant/observation and acceptance authority. None newly established here
- Unresolved: missing observation, divergent input set, conditional execution or behavioral mismatch candidate
- Retired: none; this assessment has no retirement authority

This supplements [obligations](../../parity/obligations.md) and [baseline transfer](../../parity/baseline-transfer.md), without editing or adopting their superseded missing-feature statements. Historical 846 cases, 398/6521 target results and shared-vector receipts remain historical, not executions by this task.

## Collection reconciliation and denominator limits

1. N `justfile` test recipe still selects `src/**/*.test.ts` and `scripts/**/*.test.ts`. A read-only file listing/count found 61 matching files, consistent with [inventory](../../../tasks/parity-evidence/inventory.json). The original package manifest has no test script. No runner was invoked, so this is discovery, not fresh collected/executed case count
2. N `src/core/testing.ts:11–81` confirms per-row registration, spread array arguments, undefined-object-property removal for equality, recursive subset matching and retained array-length distinctions. Those helper semantics are part of each translated obligation, not permission to normalize arbitrary output
3. N `src/core/engine/import.test.ts:24–27` registers cleanup after each case. Its `importClean:1205–1212` asserts both absent errors and verification match. Rerunning native original fixtures would use their original temporary-directory setup; later checks must explicitly contain artifacts inside an authorized project root
4. C `tests/test_main.cpp:1–55` includes the model, diff, engine and API counterparts inspected here. It additionally includes `model/test_foundation_assertions.hpp`; old claims that effective-style/invariant assertions are absent cannot be reused. C `tests/lens/test_lens_all.hpp:1–23` transitively includes `test_put.hpp`. C `CMakeLists.txt:60–75` registers one aggregate test with timeout 120, source working directory and build-local TMPDIR; it does not register one test per header
5. The 61-file discovery count agrees with the inherited inventory; current `tests/test_main.cpp` bytes differ from the inventory's initial digest and agree with confirmation workspace's newer identity. Therefore initial target header counts/digests are not a current collection freeze
6. This pass reads selected assertions in five original test files plus helpers/configuration and relevant production paths. It does not enumerate every assertion in those five files or inspect the other 56 original test files. All remaining command, scripts, emulator, lens, reconcile, model and engine families are uninspected here except the specifically cited counterparts. Even these eight families have residual subassertions
7. Original live/MCP integration recipes, platform-specific behavior, target-only branches, dependencies and external-provider fidelity are outside this assessment. The meaningful-obligation denominator remains unknown. No percentage, all-846 transfer claim or corpus execution count follows

## Eight-family dispositions

### SA-01 / BF-API-3 — HTTP response versus helper exception

- Original assertions: N `src/core/fetchWithRetry.test.ts:61–75` requires an injected 404 response to be returned with exactly one call. N `src/core/fetchWithRetry.ts:27–45` returns the final response rather than rejecting all error statuses
- Current target: C `src/api.hpp:927–996` retries transient responses but throws for status >=400. C `tests/api/test_api.hpp:1203–1226` asserts one call and an exception for 400, not returned 404
- Disposition: unresolved helper-contract difference, statically demonstrated branch difference; not a demonstrated public Docs workflow regression
- Correction to inherited qualification: N `src/core/gws.ts:195–219` shows default `googleApiFetch` also returns its response after the optional 401 refresh; it does not itself reject 404. Higher clients do reject non-ok responses, e.g. N `gws.ts:294–320`. The earlier suggestion that the default authenticated helper may itself account for rejection is not supported by this current source
- Cheapest next check: inert authenticated target transport and original injected fetcher both return synthetic 404; separately assert response status/body and one resource call, excluding token exchange from that counter. Keep public client error-format behavior separate. A control that throws instead of returning must fail the return assertion, not count as a launch failure

### SA-02 / BF-M12 — rich-link shape and omitted nesting

- Original: N `src/core/model/fromJson.test.ts:49–68` asserts one rich-link atom, owning key/raw object, exact remaining atom shape with no explicit style, and default nesting 0 when the JSON field is absent
- Current target: C `tests/model/test_from_json.hpp:48–85` now asserts key/raw ownership and empty native style, then explicitly erases `nestingLevel` from built JSON and checks missing-field precondition, list ID, zero nesting and absent textStyle. C `src/model/from_json.hpp:207–219` leaves the default-initialized bullet nesting unchanged when absent
- Disposition: translated candidate for both named checks. The old missing-field-fixture claim is stale. Native always-present empty style encodes JS absent style; this is a local typed-model translation, not permission to erase style from external observations
- Runtime and sensitivity unverified here. Cheapest future controls: supply a spurious atom style; change missing nesting's resulting value; remove raw/key ownership independently. Each must fail its matching assertion

### SA-03 / BF-D02, related BF-D03 — generated trial preservation

- Original: N `src/core/diff/myers.test.ts:39–50` uses seed 7, 500 trials, alphabet abc, lengths 0–13, with reconstruction, independent DP minimality and determinism assertions. Its draw order is a length, a characters, b length, b characters. Related `:61–72` uses seed 11, 300 trials, alphabet xyz, lengths 0–9, maximal/equal/strictly increasing pairs. N `src/core/model/testDocs.ts:131–143` defines the PRNG
- Target: C `tests/diff/test_myers.hpp:11–33,60–84,97–120` uses the corresponding PRNG and preserves the properties/bounds, but draws both lengths before either character sequence
- Disposition: translated candidate for properties and bounds; unresolved preservation of the original concrete generated sample. Equal seed/trial totals do not establish equal inputs. No implementation defect inferred
- Cheapest next check: preserve original draw order or pin the original synthetic input pairs and run both real diff implementations on them, retaining the target's existing generated suite. Assert input sequence identity before crediting transferred trials. A reordered-draw control must fail that identity check; a deliberately nonminimal yet reconstructing script must fail the independent minimality check

### SA-04 / BF-M19 — conditional corpus

- Original: N `src/core/model/corpus.test.ts:14–31` returns normally on inaccessible/empty corpus. Otherwise every cached snapshot is parsed and each tab laid out through invariant checks
- Target: no corpus replay counterpart established in the inspected target collection or corresponding model tests. Synthetic model/layout tests are not evidence that private cached snapshots were exercised
- Disposition: unresolved execution and transfer; no cache was opened. Zero registered skips in an inherited baseline does not prove a single corpus document ran
- Cheapest next check: explicitly qualify unavailable/empty/nonempty states using synthetic project-local snapshots and report document/tab counts separately. Preserve private-cache exclusion. A wrong layout must reach an invariant assertion; an empty/unavailable corpus must not yield a nonzero replay claim. This needs separately authorized fixture/runner scope, not access to personal cache data

### SA-05 / OB-012 — six tab-parent refusal rows

- Original: N `src/core/engine/import.test.ts:955–976` separately checks missing ID, new-title-as-ID, later parent, self parent, descendant parent and unknown ID after deletion. Every row also appends a new tab; status refused, first code, distinguishing message and zero mutation count are required
- Target: C `tests/engine/test_import.hpp:58–89` tests an invalid parent plus missing required tab, but only checks a lower bound on problem count, not the six import rows or row-specific first error. C `src/engine/import.hpp:156–243` implements unknown-ID, missing-parent, deleted-parent and ordering checks, so missing capability must not be inferred
- Disposition: unresolved assertion transfer at the import boundary. There is plausible implementation correspondence, not fresh behavioral proof
- Cheapest next check: port all six rows through current `RemoteImportFixture`, preserving nested parent identities, appended new tab, exact code/message and zero batch/create/delete/rename/comment mutations. Wrong first error or one extra create must fail independently of refusal status

### SA-06 / OB-015 and OB-016 — rollback versus failed cleanup

- Original: N `src/core/engine/import.test.ts:657–690` distinguishes confirmed deletion with failed/reverted/no doc/one delete and later successful second create, from failed deletion with known doc, same canonical input/export path, parsed doc identity and identifying warning
- Current target: C `tests/engine/test_import.hpp:499–542` exercises both branches through actual import/emulator services, including one create/delete/batch, unchanged intent and second create after confirmed cleanup; failed cleanup checks same path, parsed identity, unknown write state and warning. Fixture delete callback exists at `:328–331`; implementation at C `src/engine/import.hpp:1547–1584` tracks deletion and recovery. The old “no deleteFile callback” claim is stale
- Disposition: translated candidate for the original named assertions, with stronger target state checks. No fresh runtime evidence here. Exact provider-ID nonreuse is not directly asserted by the original second-create count; do not silently relabel that count as an identity inequality
- Cleanup fidelity limit: target callback marks the emulator document deleted; this case asserts callback counts, not directly the deleted flag. Later fault control should acknowledge deletion without removing state and require authoritative provider-state inspection; count-only assertions would not detect that fault. The original fixture's afterEach filesystem cleanup also needs contained-run cleanup evidence separately

### SA-07 / OB-023 and OB-026 — page-mode and section identity

- Original page cases: N `src/core/engine/import.test.ts:1142–1171` requires rtl editing to preserve PAGELESS mode and native directive output; newly added landscape pageful tab must have exact 792×612 PT pageSize while the original tab header remains pageful
- Original section sequence: N `import.test.ts:1274–1321`, including `importClean:1205–1212`, exports second-section header identity, edits it, creates section-2 footer without default-footer aliasing, deletes header and empties remote headers, then rejects nonexistent section 3 with identifying diagnostic
- Target: C `tests/engine/test_import.hpp:617–655` checks A4 landscape inequality, selected margins/named styles, verified application and no-op reimport, not the original PAGELESS/new-LETTER-tab pair. C `tests/engine/test_export.hpp:85–105` checks rendered section-2 identities, not the import lifecycle. Production C `src/engine/import.hpp:742–775,821–852,1330–1342` now handles section identity/create/delete and page setup
- Disposition: partial translated candidates for related geometry/export observations; the named original import sequences remain unresolved. Do not copy the ledger's broad missing non-body/layout feature claims
- Concrete diagnostic difference candidate: current missing-section branch says the requested section does not exist, whereas the source assertion requires the specific section number. Runtime ordering may introduce an earlier error; characterize rather than declare the exact public result from this branch alone
- Cheapest next check: port the exact two page cases and second-section sequence, retaining absent-errors/match helper checks and remote collection state. Faults: force pageful during rtl, substitute merely landscape-but-wrong dimensions, alias section 2 to default, or preserve a deleted header; each must reach and fail its named assertion

### SA-08 / OB-030 — edits inside review spans versus crossing edges

- Original: N `src/core/engine/import.test.ts:352–408` distinguishes deletion-half replacement retaining deletion ID and untouched hidden insertion half, crossing edge refusal naming the pending ID then explicit rejection yielding plain runs, and within-style-span replacement preserving its style suggestion ID. Exact ordered run arrays and no-error/match helper outcomes are separate obligations
- Target: C `tests/lens/test_put.hpp:503–532` edits text beside reviewed symbols and checks retained symbols/self-check; C `tests/engine/test_import.hpp:718–737` tests explicit deletion/insertion rejection. Neither substitutes for inside-span replacement and cross-edge parameter distinctions
- Static mismatch candidate: C `src/lens/put.hpp:290–315` rejects any unmatched original symbol with nonempty suggestion marks. C `src/model/symbols.hpp:66–74` includes deletion/style marks. Replacing a character inside a marked word can therefore take the rejection branch even when original source permits the edit. The write path calls `target.writeMarkdown` at C `src/engine/import.hpp:1338`; this is not merely an unused helper
- Disposition: unresolved transfer plus high-confidence static behavioral mismatch candidate; not an executed regression. Existing supported rejection is not parity for supported source in-span edits
- Cheapest next check: three actual original/target import cases matching the source distinctions, comparing ordered text and unchanged synthetic suggestion IDs without normalization. Controls must drop deletion ID, alter hidden insertion half, drop style ID, or permit cross-edge loss without reject; verify the relevant state assertion rejects each reached fault

## Refined brief A and prioritized follow-up briefs

Refinement outcome: changes made. Keep original brief A's disjoint assessment ownership and assertion-first approach. Replace “unresolved families imply missing capability” with the current distinctions above; count runner discovery separately from conditional execution; include the corrected default-fetch boundary. No map changes are made here.

The following are proposals, not source/test write authority. One owner should serialize changes to `tests/engine/test_import.hpp`; review/lens implementation ownership requires a separate handoff. No new normalizer or wholesale source-test rewrite is justified.

### R1 — characterize and decide OB-030 in-span review carry

- Priority: first; affected claim is faithful import of edits inside a pending review span
- Scope: add the three SA-08 source cases at the existing import seam; retain source parameter distinctions and full ordered run observations. Use existing synthetic fixtures and a fresh contained evidence destination
- Stop/decision packet: if target refusal reproduces, choose faithful carry repair versus explicitly narrowing replacement scope. A blanket permission to edit/reject protected content is not an authorized correction. Refiner recommends faithful carry; lead routes a consequential capability change to the assigned decider and implementation owner
- Acceptance: actual original passes its assertions, target either reproduces the gap or passes all same assertions after separately authorized repair; reached missing-ID/hidden-half/cross-edge controls fail. Adjacent-edit and explicit-reject regressions remain intact
- Proposed later validation: existing engine differential driver with reviewed new case IDs and both actual adapters, followed by the affected import/lens test selectors. Adapter extension and exact new IDs/commands must be reviewed before running; no invented current command is claimed

### R2 — translate hierarchy and native page/section sequences

- Priority: second; bounded assertion-transfer work, not presumed implementation repair
- Scope: one import-test owner ports six SA-05 rows and the exact SA-07 page/section cases using current services. Preserve first-error ordering, complete no-write counters, exact dimensions, canonical native output, section identity and remote deletion state
- Stop/decision packet: first run characterizes discrepancies before changes. A different missing-section diagnostic is an observable-contract disposition, not a style nit; retain original expectation and route a proposed change/exception for acceptance rather than weakening it
- Acceptance: parameter-specific assertions plus importClean's absent-errors/match checks; faults changing only error code, allowing one create, using wrong landscape dimensions or aliasing sections are independently detected. Do not substitute current generic geometry test or export-only check
- Proposed later validation: contained original/target cases through existing import adapters and a targeted `import parity` test selector under the accepted hardened build. Canonical runtime owner supplies exact invocation/output roots; no build or execution authorized by this report

### R3 — settle BF-API-3 at the correct transport boundary

- Priority: third; bounded characterization/decision, no blind public behavior change
- Decision question: should C's combined authenticated/retrying helper preserve N's raw response contract while higher clients translate errors, or is a narrower boundary adaptation accepted? Current source establishes both the raw-helper return and higher-client rejection; neither may be silently retired
- Scope: API owner adds an inert 404 response/body/one-call characterization, plus an existing public-client 404 error case to protect translation context. Do not weaken the permanent-400 no-retry assertion or change write retry behavior to satisfy a helper test
- Acceptance: explicit helper/public-client observations on both sides, a reached throw-versus-return control, and independent one-call count. Record any intentionally changed boundary as authorized correction/equivalent replacement with rationale, never parity by generic “error” normalization
- Proposed later command after isolated build ownership is granted: the existing hardened test binary's `--test-case="*404*,*permanent 400*"` selector, with no-test selection treated as failure. The original exact helper case needs a contained project-local characterization harness or approved original-test execution; its execution is currently unverified

BF-D02 sample alignment and corpus availability accounting remain explicit residuals, not fourth/fifth priority briefs. SA-02 and SA-06 need fresh qualification and sensitivity, not new missing-feature implementations.

## Static input identities

Read-only SHA-256 snapshots, not a source freeze or runtime receipt. These identities permit later invalidation checks; all selected files may still change concurrently.

| Input | SHA-256 |
|---|---|
| N justfile | a8db68ced0af14fdc981e784847b02362d9bc44130e1ec12ea0f5c99fd5caa24 |
| N src/core/testing.ts | 8a6d686d8da4a09aff62e41c43d2fd0141bde80352ccead063fd957476712e73 |
| N src/core/fetchWithRetry.test.ts | 779e36047d223b655338584a30b5c14042079f875f75d15c334c9e526c881e93 |
| N src/core/fetchWithRetry.ts | 365781224acf09ea38da8363c3b3d72d29876ad7681d4861c6a842bde5d201cb |
| N src/core/gws.ts | 64eb77243670bc619d6b5c136c7d3bb7bc3e6fb3ccdbfd46d24d4e11ac84a203 |
| N src/core/model/fromJson.test.ts | 1cb9a9d06cd80d5f61dc61185bcd1357fe55ef164cd643b8bade952d9c31b73d |
| N src/core/model/corpus.test.ts | dc15b1a4da26222d7d5478ef3fd7272c245ba87110fb31141a14e90d48619357 |
| N src/core/diff/myers.test.ts | 4f29eede9ad4f65e4336aeb2118f13a67596165528f45e5445b11820d1a7a220 |
| N src/core/engine/import.test.ts | de70991877e2d7d65a23a02d3fd4c9916ab30bd87e6cecd1ece01b50beb26d82 |
| C tests/test_main.cpp | 9f90997fa990e547a536e20044ec0919b8967c3febe0e7894cb76d5e21127e2c |
| C tests/model/test_from_json.hpp | a38d30538e1fe739927e8324843a07b72f4d979d687d472d4764543145b659f9 |
| C tests/diff/test_myers.hpp | a66b6c56ed4d1ff13e1637f640c34b47bf1494bfb22e9f9548e9f06de3c67475 |
| C tests/engine/test_import.hpp | 25d97e7660fc06741da157b7febf070b1b57f5aca28bd002f893b08e27039b8e |
| C tests/lens/test_put.hpp | 5b4d9774a0a3dbbf125dcf28757445c47b963f487850c0ee7c4eab193db01460 |
| C tests/engine/test_export.hpp | c7d04d2718f193f5fc74388604e9fa1c94d5ee634b9ce843842712317bb6e863 |
| C tests/api/test_api.hpp | 73640281a107c9d649dbec0f57745cf177b5757c6265ef7db4ba3ba81f051046 |
| C src/lens/put.hpp | 596dad38997865cafbe15f61971dc01d7372b92d27eb727895e0efe99fda7913 |
| C src/engine/import.hpp | 13b9c65659979a373e2e78654eed11eb28445b13b70cb5a949130d1f6e891b22 |
| C src/api.hpp | 76e84a3a2b5f32edbb20ec84c56e106eca91ad8f7cf0e91b782d0d32059ed2a5 |
| C CMakeLists.txt | 2a901a2c3045a76f3e45593d0fc33ca921a5fd2406ee54c43a9ef7235f567d5e |

Fresh commands were limited to bus protocol, read-only source/configuration searches, `rg --files ... -g "*.test.ts" | wc -l` and `shasum -a 256` over the listed inputs. The file-count command returned 61; selected original justfile/helper hashes match the inherited inventory; target entrypoint/configuration hashes match confirmation workspace. Citation inspection and collection reconciliation completed. No claimed test pass, control detection, full inventory rehash or live evidence.

## Instruction loading and acceptance limits

Explicitly loaded repository AGENTS.md; swarm common/refiner roles and Copilot host guide; confirmation rules, task 1 contract rev 1, workspace and brief A; app-rewrite prepare-transfer and verify. Automatic inheritance unverified; actual model/effort unavailable. Host file-edit tools were available and used only for this report and task-1 worker fields/Notes.

No nested agents, excluded inbox access/reconstruction, original writes or other owned-path writes. Lead owns acceptance, behavioral dispositions and subsequent permissions. Unknowns block only the affected claim; the eight-family result is sufficient to launch separately authorized bounded characterization, not to certify replacement parity.
