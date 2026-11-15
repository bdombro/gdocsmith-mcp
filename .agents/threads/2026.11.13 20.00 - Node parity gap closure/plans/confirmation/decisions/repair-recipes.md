# Bounded repair decisions

Task 4, contract rev 1. Technical choices decided; implementation, contract changes and acceptance remain with the lead. Static evidence only: no reproduction, tests, builds or runtime checks performed. Two proposed production repair scopes, plus existing gate tooling. No assertion retirement, normalization or narrowed replacement claim

Inputs: [source obligations](../assessments/source-obligations.md), [workflow gate](../assessments/workflows-gate.md), [briefs](../briefs.md). Repository paths below are workspace-relative; sibling reference remains read-only

## D4-R1 — Faithful review carry, not blanket permission

Decision: characterize the three original import rows first, then repair the visible/model-symbol mapping and apply the original post-edit classification. Do not merely remove the pending-mark refusal or assign a neighboring character's marks to every insertion

Evidence:

- ../gdocsmith-node/src/core/engine/import.test.ts:352–408 specifies deletion-half replacement with unchanged hidden insertion, cross-edge refusal naming the pending ID followed by explicit reject, and style-span replacement retaining its ID and exact ordered runs
- ../gdocsmith-node/src/core/lens/put.ts:835–921 maps visible symbols onto model symbols before back-to-front splices. Hidden insertion handling is distinct from ordinary character alignment
- ../gdocsmith-node/src/core/engine/suggestionRule.ts:74–171 is the actual carry classifier, not paragraphSplice. It groups replacement hunks separated only by kept whitespace, carries identical nonempty marks across eligible spans of at least two characters, uses joinMarks for pure insertions, and refuses removed suggestions or replacements across hidden insertions. Fully deleted and one-character spans are not blanket carry cases
- src/lens/put.hpp:290–315 assumes equal visible/model symbol counts and rejects unmatched marked symbols before any carry stage. src/model/symbols.hpp:66–80,115–144 preserves hidden insertion symbols; these are not plain visible characters
- src/engine/session.hpp:731–789 applies rejects to a candidate, calls markdownPut, then publishes that candidate. src/engine/suggestion_rule.hpp currently provides rejection support, not the original post-edit carry classifier. src/engine/import.hpp:1338 reaches this write path

Proposed exclusive production ownership: src/lens/put.hpp, src/engine/suggestion_rule.hpp, src/engine/session.hpp as one coherent repair scope. Tests: tests/engine/test_import.hpp and tests/lens/test_put.hpp. Do not broaden model, reconcile, emulator or provider behavior without a new handoff

Implementable recipe:

1. Add the three source-grounded cases at the existing RemoteImportFixture seam, asserting absent errors and verification match where the original does, exact ordered text/marks and hidden insertion payload. Retain the source cross-edge identifying diagnostic and zero mutation checks before reject. Record actual failing boundary before repair; static mismatch is not a reproduced failure
2. Keep a post-reject/pre-write baseline in writeSegment. Align visible markdown symbols to actual model symbols so unchanged hidden insertions survive at their original gap; replacements ending beside a hidden insertion must not consume it. Preserve atom identity, newline metadata and unrelated review payloads
3. Implement the original classifier in suggestion_rule.hpp and invoke it on the candidate before publication or any remote send. Reuse existing diff and marks helpers. Preserve grouping across whitespace, strict-inside insertion semantics, mixed-mark/hidden-edge refusal and removed-ID detection. Preserve existing explicit-reject handling; no generic allow override
4. Replace the lens's unconditional marked-symbol refusal only where this checked engine path supplies equivalent protection. Standalone markdownPut must retain its safe refusal contract unless its caller contract is explicitly revised and tested. If that requires an explicit option, keep the default restrictive; do not make all lens callers permissive
5. Run affected existing lens/import selectors, then independent review and both actual adapters for any newly authorized shared cases. Current driver has no new case IDs; adding them requires explicit adapter/vector/assertion ownership from the lead

Required controls: drop deletion ID; mutate the hidden insertion text or ID; drop style ID; permit the crossing replacement without reject. Each must reach its relevant ordered-state/refusal assertion and fail. Add cheap boundary characterizations for insertion exactly at an edge, inside a span, a one-character replacement, whole-span deletion and whitespace-separated cross-edge replacement. Preserve adjacent-edit and explicit-reject regressions

Unresolved before implementation acceptance: exact first failing stage for replacement fixtures, target projection behavior with hidden halves, and backend realization of carried marks. Original/target execution is required; if more source files are necessary, stop and request a revised scope. No claim that this recipe alone proves full rich-review parity

Rejected: blanket pending-review rejection as parity; globally permitting marked edits; reconstructing hidden payloads from visible text; moving the rule into transport/reconcile

## D4-R3 — Raw helpers, checked clients, unchanged retries

Decision: return final HTTP responses from raw authenticated/retrying helpers; translate error responses at checked client boundaries. Keep a small reusable checked authenticated helper for existing public operations rather than duplicating status checks across every caller

Evidence:

- ../gdocsmith-node/src/core/fetchWithRetry.test.ts:61–75 requires returned 404 and one call; fetchWithRetry.ts:27–45 returns the final response, including exhausted transient responses
- ../gdocsmith-node/src/core/gws.ts:195–219 returns raw authenticated responses after one optional 401 refresh; getDocument at :345–361 performs its own non-ok check and formatting
- src/api.hpp:900–916 translates authenticated responses; :927–996 translates retry responses; DocsClient::getDocument at :1033–1049 immediately parses the helper body. Simply removing the throw would expose error JSON as document data
- All src callers of both helper names are in src/api.hpp. fetch_with_retry is used by getDocument; google_api_fetch is used by Docs create/batch and Drive read/write/comment/upload operations, including ignored-response DELETE and folder PATCH paths. Those callers must not silently accept errors
- tests/api/test_api.hpp:1203–1226 protects permanent-400 one-call behavior but currently also expects the raw helper to throw

Proposed exclusive production ownership: src/api.hpp only. Tests: tests/api/test_api.hpp. Do not change curl, tokens, engine or write scheduling

Recipe:

1. First characterize raw 404 status/body/headers and one resource call, and public getDocument 404 rejection with original formatted error context. Exclude synthetic token exchange from the resource-call counter. Observe original behavior through its actual injected-fetch boundary, not canned source output
2. Make google_api_fetch raw; retain its current status-to-GoogleApiError behavior in one checked helper. Route every existing public caller that previously relied on that behavior through the checked helper, including discarded responses. Do not put that checked helper inside fetch_with_retry
3. Make fetch_with_retry return the final response; getDocument explicitly rejects unsuccessful responses with existing formatting before JSON parsing. Preserve existing public exception compatibility. Treat raw helper status/body behavior and public client errors as distinct obligations
4. Keep retry counts/backoff, transient transport exceptions and one 401 refresh unchanged. No write operation is moved onto the retry helper. Preserve permanent-400 no-retry by asserting returned response and one call at the raw boundary, plus throw and one call at the checked boundary; this relocates the exception assertion, not its retirement

Focused checks: raw 404 and 400 return once; successful GET unchanged; exhausted 429/503 returns final response at configured count; 501 not retried; public GET rejects error JSON before parsing; representative checked Docs write and Drive DELETE reject errors without replay. Retain existing transient-success and write no-retry tests

Controls: throw at the raw 404 boundary must fail the response assertion; accept an error JSON object as a document must fail public rejection; one extra permanent-error resource call must fail count; a discarded DELETE error must fail checked-client observation. No generic error normalization or retry-policy redesign

Rejected: fixing only the 404 test, leaving public parsing unsafe; preserving the combined helper discrepancy by silently declaring it internal; duplicating error checks throughout all Drive methods

## D4-G — Reuse existing driver with a direct recipe

Decision: no scripts/test-parity.mjs wrapper, separate process supervisor or duplicate summary framework. Prefer one parameterized just parity recipe calling the existing driver. Keep evidence validation and its tests beside that driver, extending only missing fail-closed checks. This is a proposed tooling scope, not an edit authorization

Proposed ownership: justfile, tests/parity/run-engine-differential.mjs and one focused adjacent driver-contract test file. Existing engine adapters, safety assertions and vectors remain unchanged for this gate-only work

Evidence: tests/parity/run-engine-differential.mjs:1–55 already validates arguments, private owned paths and five hardening markers; :111–153 fingerprints original/target inputs, executable/runtime/dependencies/build; :478–548 retains detailed/raw reports and exits nonzero for differences, assertion/launch/control failures or stale inputs. justfile configure/test/build recipes install or rebuild broader outputs, so parity must not depend on them

Interface: explicit fresh relative evidence destination/run ID and reviewed Node executable, with the exact existing six driver flags from the assessment. Use its ten existing IDs and automatic variant/controls unchanged. No new case IDs, runtime lookup fallback, implicit configure/install, unit or MCP invocation. A separately authorized runtime owner rebuilds the isolated ParityChecks adapter first

Required small driver changes:

1. Reject existing result/raw-report destinations and reused run scopes, including unsafe ancestors, before writing. Retain raw leaves and exact child exits in existing reports
2. Fail closed on incomplete expected case/variant identities, missing side observations, missing required controls, false baselineWasPassing, missing requiredAssertion in rejectedBy, or unreached backend controls. Do not accept an empty control set or merely compare detected count with total count
3. Include owned-run cleanup in the final nonzero predicate. Keep original/target assertion outcome, controls, raw differential, freshness and cleanup distinct; unit/MCP/live/platform qualifications remain unrun or unverified, not inherited
4. Extend the existing identity manifest only for operative inputs the new recipe adds: justfile/driver-contract inputs and relevant fixture identities. Retain before/after equality, binary/build receipt and runtime identity; mtime is not provenance. Separate aggregate unit qualification stays outside this engine gate

No separate gate-summary.json is needed: the existing detailed report and stdout summary are the durable report/console surfaces. Success requires complete validated reports and driver exit 0. Raw differences remain RED and exit 1 even when named assertions pass; preserve any other infrastructure nonzero/signal as failure. Never mask child failure with a later successful command. Existing per-child timeout/output limits remain; a new arbitrary overall 300-second timeout adds process-tree complexity without evidence and is deferred

Targeted tooling controls: missing case, empty/missing named control, reached=false backend control, cleanup remainder, malformed/incomplete report, stale input and raw difference must each produce nonzero; complete valid synthetic gate metadata may pass only with zero raw differences. Test pure final qualification logic cheaply; do not replace application executions with these mechanism tests

Rejected: a new wrapper owning paths, spawning, fingerprints, aggregation and deadlines already partly handled by the driver; bare direct invocation without closing vacuous-control/cleanup/fresh-destination gaps; rebuilding canonical runtime through just test/build

## Handoff and limits

Lead should create/refine two disjoint production contracts above and a separate tooling contract; serialize shared adapter/vector edits if later authorized. R1 needs characterization before claiming repair; R3 can proceed independently. Gate execution depends on reviewed tooling, reviewed affected source and sole runtime ownership, not unrelated workflow expansion

R2 tab/page/section transfers, BF-D02 generated sample alignment, conditional corpus and W1–W4 adapter journeys remain unresolved follow-ups, not retired obligations. Existing raw-difference RED and external/platform limitations remain. No human-owned scope trade-off was selected

Instruction sources explicitly loaded: repository AGENTS.md; swarm common/decider and Copilot host guidance; confirmation rules and task rev 1; app-rewrite prepare-transfer, port-reconcile and verify; user justfile conventions. Automatic inheritance and model/effort metadata remain unverified/unavailable. No helpers permitted or launched. Validation consisted of static source/caller/assertion reads only
