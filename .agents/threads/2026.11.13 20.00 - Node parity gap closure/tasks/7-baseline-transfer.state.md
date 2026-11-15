# 7 — Foundation test-obligation transfer reconciliation

Status: delivered for parent examination; not accepted. Launched 2026-11-13T20:46Z (launch 9), completed 20:53Z after one bounded evidence pass and citation clarification, before 21:06Z. No application runtime or canonical builds executed.

## Auth repair follow-up — completed, handback ready

Initial static report accepted **as a report only**, not full certification. At 21:03Z parent explicitly forwarded source3 release NP-E049; both released hashes matched before edits. BF-API-1/2 repair is complete and ready for independent 6b review, not self-accepted. **This worker now hands both API files back to source3/parent and will make no further source/test writes without a new handoff.**

Changed only auth-cache/refresh functions, needed standard concurrency includes, and two focused source-linked API cases in those two files. No 404-policy/uploader/other API change. Completed inside 12 active minutes/two validation cycles and unchanged global freeze/deadline.

- Owning shared refresh result/exception, work outside auth cache lock, all-waiter wakeup, clear-completed state; credential pinning and reuse retained; offline transport error wrapped with actual source context.
- Fatal ASan/UBSan isolated API TU: auth-only **2/36 passed**, auth plus existing adjacent API checks **6/54 passed**. Excluded unrelated cases honestly recorded (19 and15).
- Cache-only reached behavioral controls: disabling single-flight produces 11 assertion failures/exit1; disabling context produces exact message failure/exit1; no faults gives2/36 pass. No fault switches in production.
- Targeted tidy clean on isolated API test TU and actual main TU; scoped whitespace clean. First compiler invocation's archive-language flag mistake was corrected; not a product failure.
- Handback hashes: `src/api.hpp` SHA256 `76e84a3a2b5f32edbb20ec84c56e106eca91ad8f7cf0e91b782d0d32059ed2a5`; `tests/api/test_api.hpp` SHA256 `66e4c004c3222861084468b47f111231f9f09b322fbba95ed864f14ad7745d07`.
- Build cache `build/cmake/AuthParity`, separate from canonical binaries; no real auth/network/services, installs, commits or original writes. Independent 6b/parent acceptance required; other foundation transfer gaps remain unchanged.

## Result

- Artifact: `../plans/parity/baseline-transfer.md` (relative to this tasks directory).
- Foundation source assertion/parameter-linked dispositions cover model, diff, existing parser, emulator, API and artifact families. Translated-existing means static correspondence awaiting final runtime; partial, missing and unverified portions remain explicit. No all-846 assertion/case-transfer claim, inferred retirement, or helper/count/name proxy.
- Consequential source-supported static counterexamples: API refresh lacks concurrent single-flight; offline refresh transport exceptions lose the source diagnostic context. Both belong to current sole API writer, lane 3 / task `3-engine`. Callback-only synthetic reproducer specifications and exact source/target citations are in BF-API-1/2. Neither was executed here.
- Separate unresolved helper-boundary difference: injected source retry wrapper returns 404, target wrapper throws; source default authenticated dependency may itself throw, so this is not mislabeled a demonstrated public workflow defect.
- Concrete test-transfer holes include a “default nesting” target fixture that always emits explicit zero; a row/column target test that actually only mutates rows; omitted style inheritance/retention, parser refusal/parameter and emulator round-trip assertions. Missing tests are not automatically missing implementations.
- Accepted parent conformance 4/174/all149/process controls reused without rerun or fixture comparison; worker's older “not accepted” state is superseded by parent acceptance. UNMODELED conservatism is not full API coverage.
- Source configured-test digest freshly matches initial inventory; target static digest binds the explicitly listed 22-file scope. Concurrent source changes invalidate affected static citations and still require parent-owned final runtime.

## Gate / next owner

Parent owns acceptance/map and BF-API-3 boundary adaptation; BF-API-1/2 are now separately repaired under the explicit handoff above, awaiting independent review. Final runtime owner may execute existing translated checks, but a green suite cannot close absent source parameter assertions. No peer/helper wake or nested delegation occurred. Original pass wrote only report/state/log; follow-up additionally wrote its explicitly released auth/API-test scope and isolated test cache.

Depends on: accepted initial 43-group source inventory; foundation comparison is independent of the still-changing rich-import implementation.

Objective: reconcile the existing model/diff/lens parsing/emulator/API/artifact foundation obligations that ten shared import vectors cannot certify. Use the existing inventory and original configured test files, inspect corresponding existing C++ tests and relevant implementation only where necessary, and produce source-assertion/parameter-linked dispositions rather than infer equivalence from test totals, file names or shared descriptions.

Own writes: `plans/parity/baseline-transfer.md`, this state and matching log only. Original and target code/tests, the task-5 ledger/evidence, map, inbox and thread roots are read-only. No nested agents.

Required output: bounded, explicit coverage of the source foundation families with exact original and target assertion locations, important parameter distinctions and evidence type. Reuse accepted 149-fixture accounting without calling it universal emulator coverage. Mark every genuinely missing/unverified assertion family honestly; prioritize concrete counterexamples that would invalidate replacement behavior. Do not duplicate the active rich-feature/adapters investigation. Report a newly found implementation defect with a reproducible synthetic case and precise current owner.

Acceptance: parent examines the dispositions and residuals. An actual source-supported behavior cannot be retired merely because C++ refuses it; a matching test name, aggregate count or inferred helper availability is not an executed transfer. This audit alone cannot mark unexecuted tests passed or certify all 846 original test cases.

Budget: 20 minutes, one evidence pass plus one bounded clarification, within the unchanged 22:00:36Z deadline. Stop at concrete evidence and residuals rather than pursue an unbounded whole-branch review. Never launch merely to fill a slot.

Safety: no Google/auth/network/services, installs, commits, code/test edits or original writes. Sanitize all artifacts; no secrets, sensitive personal data, confidential source bodies or raw transcripts.
