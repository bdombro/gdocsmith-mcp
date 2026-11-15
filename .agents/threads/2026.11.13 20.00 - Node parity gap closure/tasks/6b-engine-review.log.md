# Engine review log

- Parent prepared the bounded independent gate; implementation readiness is pending.

## 2026-11-13 independent review6b

Read contract, current parent map, lane3/lane7 exact ready states/logs and integrated-tail evidence. Examined actual immutable Node auth/import/transaction/opaque-token parsing and corresponding accessible C++ implementations. No duplicate atom/conformance review and no excluded export-test access.

Result: **CHANGES NEEDED**, specifically 6B-1 and 6B-2 in state. These are deterministic source-path counterexamples, not inferred from test names/counts. 6B-1 compares source per-tab creation semantics (`import.ts:227,245`) with target document-wide omission (`import.hpp:1165,1232`, `session.hpp:761–762`, `lens/put.hpp:78–94`). 6B-2 compares source known-created verification-failure cleanup (`transaction.ts:263–297`, `import.ts:472–493`) with target phase/inconclusive exclusions (`transaction.hpp:1043–1095`, `import.hpp:1515–1516,1543–1544`). Preserve unknown-ID safety and actual-mismatch retention; neither finding authorizes general retries or weaker guards.

Auth BF-API-1/2 independently **APPROVED** for the examined contract. Existing isolated AuthParity executable, fatal ASan/UBSan, selector `auth cache*,auth refresh*`: 2 cases/36 assertions passed (19 unrelated cases excluded). Read accessible tests' success/failure waiter enrollment, pinned credentials, forced refresh, recovery and unchanged private configuration assertions. Executable SHA256 `2413560a2ebff7999a8f53ef8ab0dd299d613b72cafd14f14ca60bb0d7f9cb80`. No live credential export/Google calls and no invented 404 policy requirement.

Positive static boundary evidence:
- Node upload preflight/cleanup `import.ts:562–619`; target `import.hpp:596–713`, planning hook `1227–1228`, `transaction.hpp:742–795,802–839`; actual app callbacks `app.hpp:505–522`; API durable temporary-ID error `api.hpp:1572–1660`.
- Accessible upload assertions `tests/engine/test_import.hpp:1551–1657` inspect exact temporary cleanup identity, no-send/preflight aggregation and canonical kept-local recovery. Foreign-source cache/provider replay `1659–1716`; actual app no-auth upload test `1718–1787`; typed known-ID error `1789–1811`. Target tests explicitly inspect actual inlineObjects identity, not merely image type/count.
- Segments/footnotes/settings `import.hpp:744–877`; typed segment operations `session.hpp:651–725`; footnote phase reload/revision/identity `transaction.hpp:937–996`; named-style mutations/unsupported pending-style guards in `engine/named_styles.hpp`. Suggestion unsupported distinctions and source-version/stale/partial/unknown paths examined through accessible import tests.
- Recovery canonical branch `import.hpp:1206–1218` requires existing target, unchanged revision and only imagesUpload attempts; other writes/conflicts use separate recovery placement. Cleanup cannot invent a temporary ID and does not turn an unknown upload into confirmed target success.
- Dropdown aliases/option identity/relabel model in `engine/dropdowns.hpp`, request widening/selection/narrowing `reconcile.hpp:643–741` inspected. Backend readiness is now reported in lane8; final integrated engine result remains deferred, not the entire stable review.

Engine runtime qualification: first selected existing EngineParity executable run showed 20 cases/266 assertions, 18 cases/262 assertions passing. Four failed assertions were path-alias-sensitive created-cleanup path equality and three upload fixture cardinalities; no production diagnosis inferred. A project-local TMPDIR rerun was launched, then the host explicitly moved that command to background and prohibited further access; no poll, duplicate run or completed-result claim. This is not a clean engine runtime receipt. No new test/probe files were written.

Snapshot identities (SHA256; production stable through final capture):
- Target HEAD `20ed42e6a67341d71a13f404f149aee34b4c30bc` (dirty shared working tree; HEAD alone is not input identity).
- `src/api.hpp`: `76e84a3a2b5f32edbb20ec84c56e106eca91ad8f7cf0e91b782d0d32059ed2a5`
- `tests/api/test_api.hpp`: `66e4c004c3222861084468b47f111231f9f09b322fbba95ed864f14ad7745d07`
- `src/app.hpp`: `23a0fb2e7f189beeff597ee3613560437cf8cdc51f6a22269d347bd833c2f3d2`
- `src/engine/import.hpp`: `caa458de038d8620e4f9ce238e67b7cd43a94720c359154fa5f5c7dfa9d3ece3`
- `src/engine/session.hpp`: `fca4c4d708d3f6d1b36c9ae6f0517f8b8f0fed07e35a32032d54643ed14658f1`
- `src/engine/transaction.hpp`: `58fcbd4d93fdc7578d4543e72a62c7aa7ed9c0dcf2135bddddfd9efe7e0fa489`
- `src/engine/flush.hpp`: `221b297e9443ab4838d8b048a1bd2ba1f58a7fa239a58187912e0406d6360d2a`
- `src/engine/plan.hpp`: `21eb21a022877a4743ac748f87b479ba84d94bc5c0c8fa7aae9c6b07a7941c9e`
- `src/engine/guard.hpp`: `5d5408059f6f80e409b4a3350e5e47237dfc483f5b728795f38dbe5efd3c4493`
- `src/engine/dropdowns.hpp`: `5be9978febd2e834ee5987c633db68622d9fcd1d8f1c38d4e051539030b9aa01`
- `src/engine/named_styles.hpp`: `dc0e7a49a53c976ec3ae26084a22b6d5038896176ebf768a755f1fed0685a3ee`
- `src/reconcile/reconcile.hpp`: `9457dd43a7c7ccfdafcad87cf8bd2c4a8f8500ecc6ebd45816db00f926790e9a`
- `src/lens/put.hpp`: `edb19b2e1e26f763661de3fe6a1cd5b1c99038686b3424dc2ec0e9882119d248`
- `tests/engine/test_import.hpp`: `7664793b65e10b42624365b3497806e64ba43643882263a3aa671b420b9ed4e6`
- Original HEAD `d00069dd188ca995bfda9828cc9eee5016603b63`
- Original `src/core/auth.ts`: `a5053961c9e1987d4382e748b84a6b0886d9150b1692a2ff11e6d50c5b7089d6`
- Original `src/core/engine/import.ts`: `cf2907f0f43e59dc23226ce8699c8d5a9b63834ec931b2d8db8f35b3ba186ad7`
- Original `src/core/engine/transaction.ts`: `d4dbe51f7b2ec8f284d2e2917e7a636fdc0e9f7e6dac3c243d61d140b8559cde`

One bounded repair recheck remains available. Parent owns acceptance and final frozen runtime; historical shared receipt and raw differing leaves are not promoted to full parity.

## Final engine handoff / dropdown conclusion, 21:16Z

Consumed engine's final fresh handoff (21:15:40Z): owner reports 74 broad cases/971 assertions, one remaining failure in the policy-excluded rich-export test; all dropdown/upload/replay cases reportedly pass. Did not access, reconstruct or probe that excluded test. This owner receipt is not a full-pass independent gate.

Cleared only the earlier dropdown deferral after comparing actual original `lens/dropdowns.ts:138–219`, `engine/session.ts:446–461`, `reconcile/reconcile.ts:631–692`, and backend relabel semantics (`emulator/emulate.ts:1155–1156`) with target declaration/alias/options/request ordering and backend absence/relabel repairs. Current independent actual public-import selector `import dropdown parity*`, project-local fixture root, fatal ASan/UBSan: **3 cases/75 assertions passed**, 389 unrelated cases excluded. These assertions inspect unchanged raw replay, distinct same-title lists, untitled default title, provider linkage, name/position-preserving option identity, recolor/relabel, alias collision-followup and zero-send invalid selections/cardinality/removals. This bounded dropdown gate is approved; it does not resolve 6B-1 or 6B-2.

Final handoff hashes matched on disk:
- `src/engine/import.hpp`: `caa458de038d8620e4f9ce238e67b7cd43a94720c359154fa5f5c7dfa9d3ece3`
- `src/engine/transaction.hpp`: `58fcbd4d93fdc7578d4543e72a62c7aa7ed9c0dcf2135bddddfd9efe7e0fa489`
- `src/engine/dropdowns.hpp`: `5be9978febd2e834ee5987c633db68622d9fcd1d8f1c38d4e051539030b9aa01`
- `src/engine/export.hpp`: `8bac9a9bc68b4a4db87be740b7af1ca4a2e9c87bd07a1a5b7269d70aa71b8413`
- `src/emulator/document_state.hpp`: `5dfabc59218e40eeb38cccab1846a5bfaf9cefd7f4eb7d783cdea8442d94b2b9`
- `src/emulator/document_emulator.hpp`: `6e963e34e314b486fe68f6ec608e1b0d1848114fa7930c281185c08f6d1efe9b`
- Current EngineParity executable: `d25eeff73968b25d8390b2c64701d42f52d1d3247cf45102c859173353458e12`

## Single bounded repair recheck, 21:26–21:28Z

Parent authorized exactly one current-binary recheck, no builds/source edits, absolute stop 21:30Z. Read only precise repaired production branches and accessible new public cases. Did not access either excluded `tests/engine/test_export.hpp` or excluded policy inbox, nor reconstruct/probe either.

Current source/binary hash identities match engine's recovered final native receipt:
- import `51a17b306db24890713e5602baca3882eb630089d2ded97fab33a0fb3b2dcbb5`
- import tests `c738629b82c750cb223abdaaeb7712047c288c9ea15e8786bb3f3dd7af07e1d1`
- binary `f87031a2c4e6a3e7de3e9212bf8fbb5023898b87ee252b8d5048570bfd09021d`

Existing Ninja dependency metadata: 133 accessible project source/test dependencies, none newer than binary, current accessible digest `95d20d9241f6103404e5425ca3f7dddd5b0d72dc98d518b6404d624aefc4469f`. Excluded export dependency deliberately not read/stat/hashed; no whole-TU/excluded-test freshness or approval claim.

Exactly one execution of existing binary, fatal ASan/UBSan, project-local fixture root; selected names:
- unavailable opaque tokens omitted only in newly created tabs;
- known created document reverted after first content verification read fails;
- unknown document creation identity never triggers cleanup/replay;
- creation with unknown identity never invents/retries an ID;
- mismatched authoritative readback never becomes verified success.

Result: **5 cases/61 assertions passed**, 389 unrelated cases excluded. 6B-1 body omission now resets per section (`import.hpp:1301`) and existing-tab refusal remains. Original 6B-2 verification-read rollback plus failed-cleanup known-ID recovery pass. Existing mismatch and unknown-ID controls pass. No builds or production/test edits.

However the same repaired rollback predicate exposes **6B-2R**, fully described in state: post-transaction comment-read failure deletes a successfully transacted new document because target completion remains false through its final comment read. This contradicts actual source `import.ts:327–328,472–493` and explicit source test `import.test.ts:712–731`; target `import.hpp:1467–1490,1543`. No extra probe/test was executed. This is a consequential repair-boundary defect, not an unrelated audit expansion or speculative style request. Overall result remains **CHANGES NEEDED**; only 6B-1 and original 6B-2 reproduction are closed. No further repair/recheck authorized.

Native owner receipt subsequently recovered focused 66/66/872 and broad 75/76/1011 of1012; hashes match. Restricted rich-export structural gate and foreign-image tab/line/phase metadata remain explicit residuals. The earlier host-backgrounded command still has no reviewer-claimed completion. Return via native parent completion only; no policy inbox access.

## Explicitly authorized additional 6B-2R gate — 21:32:34–21:33Z

Parent expressly authorized one additional boundary-only ≤2-minute recheck, absolute stop 21:35Z, because prior repair introduced deterministic document deletion. No inferred expired authorization, new builds or source edits.

Compared actual source `engine/import.ts:327–328,476–484` with target owning `transactedCreation` declaration/assignment (`import.hpp:1189,1392–1399`) and catch/cleanup (`1510–1519,1547`). Owning receipt is assigned only after nonrefused successful transaction and actual provider identity resolution, distinguishing a successful transaction from a tracker-known failed-transaction creation. Existing-document global completion remains unchanged. Actual mismatch remains explicitly excluded from cleanup; unknown identity cannot enter it.

Frozen receipt identities matched on disk:
- import `13b9c65659979a373e2e78654eed11eb28445b13b70cb5a949130d1f6e891b22`
- accessible import tests `25d97e7660fc06741da157b7febf070b1b57f5aca28bd002f893b08e27039b8e`
- executable `e5224840b0bc55321f387d7e5af2f9ff85a8c8dab8d64401401b384a9a2196f1`

Exactly one current-binary execution, fatal ASan/UBSan and project-local fixture root: posttransaction commentsList failure; first transaction verification-read failure (successful/failed cleanup); unknown document creation identity; creation missing provider identity; existing-document authoritative mismatch. **5 cases/66 assertions passed**, 390 unrelated cases excluded. Command3047 completed exit0 at21:32:47Z; all commands launched for this additional gate completed, no new background command.

Result: **APPROVE scoped 6B-2R root repair**, closing the prior scoped transaction-boundary finding. Original6B-1/2 passing recheck remains recorded. Created-mismatch keep was source-checked via explicit exclusion; existing-document mismatch was executed, not mislabeled as created-document runtime evidence.

No unaffected auth/dropdown/lens review, excluded file/inbox access, reconstruction or surrogate probe. No whole-engine/full-suite/full-parity approval: foreign-image tab/line/phase metadata and restricted export structural gate remain unaccepted. No invented completion of the earlier host-backgrounded command. Parent owns overall acceptance.

## Final permitted fixture review, 21:48–21:49Z

New explicit parent scope: test-only public fixture corrections now; only the additional person-validation guard hunk after actual owner-ready, no builds/source edits, deadline21:52Z. Reviewed precise accessible public fixture and atomicity assertions, not unaffected6B2R/auth/dropdown/lens or excluded export/inbox.

Statically approve API fixture `73640281a107c9d649dbec0f57745cf177b5757c6265ef7db4ba3ba81f051046`: actual exported/parsed envelope and required context before editing (`test_api.hpp:172,246–254,300–304`), explicit creation `.id=""` at323, while outcome/readback/write counts/local input preservation remain asserted at455–537. No softened expected outcomes.

Statically approve transaction atomicity fixture `7f2d839a98edf82b3ccd74b9b0822a1f6068da12ff850495ec2b21a9fdd9e5a8` at156–175: replace obsolete image blanket refusal with source-unsupported missing equation and invalid person email. Retains whole-model equality and old-image presence after failed putback. Actual Node `model/edit.ts:387–390` rejects the invalid person email before edit; source `lens/parse.ts:1064–1066` merely creates intent before that model validation, so inspecting parser intent alone would incorrectly imply acceptance.

Actual owner-ready for the person guard has not yet arrived at this checkpoint; no guard approval, no polling or fabricated runtime. Parent-supplied81/82 result remains pre-fix evidence; final current-source runtime remains required. Return idle until actual guard-ready/deadline.

## Final ready guard-only conclusion, 21:51Z

Actual owner-ready/release arrived21:50:58Z. Verified helper SHA256 `1045efdf348dab15807544ebb12a75ea7dc6e79f80f7f1bf654b966adf925fbc`; inspected only new `personEmailValid` and its person-create call placement. **APPROVE scoped guard statically**.

Actual source `src/core/model/edit.ts:387–390` requires non-whitespace/non-@ pieces, exactly one separator @, a dot with nonempty suffix; target `src/lens/atom_put.hpp:22–30` uses corresponding regex and explicitly rejects ECMAScript Unicode whitespace not covered by narrow C++ regex whitespace. Includes NBSP, OGHAM space, U+2000–U+200A, line/paragraph separators, narrow no-break/medium mathematical/ideographic spaces and BOM; no arbitrary RFC/domain/address policy added. Call at353–357 precedes key allocation/atom construction at441, preserving failure before committed putback mutation.

No builds/new cases/execution or unaffected review performed. Owner-supplied actual Node6invalid/3valid, fatal2/31, real public transaction1/6 and clean tidy remain supplied runtime evidence, not independently rerun. Supplied atom-test SHA `03d7484099ea8f3fb95aac57efb14daf0d7cd22e294f5b26024d062de9aa5cb8` retained as owner-provided identity, not falsely matched to an unspecified file. Fixture approvals at hashes73640281/7f2d839a stand. Final source freeze/runtime and overall acceptance remain parent-owned; no excluded content/inbox access. Completed before21:52Z.
