# 6a — Independent lens, table and atom review

Status: **resumed narrow V008 hidden-projection repair approved independently**. Historical R1 below predates its exact-ID follow-up; this resumed review does not reopen the accepted header guard or broad implementation. Prior scoped approvals remain intact. Fresh combined runtime/V008 and parent acceptance remain separate.

## Independent atom-first findings

1. **Dropdown creation resolves display titles instead of source definition keys (P2).** Target `src/lens/atom_put.hpp:409–418` scans both IDs and titles; original `../gdocsmith-node/src/core/lens/dropdowns.ts:84–97` resolves an existing definition key first, otherwise an explicitly registered creation key, never an arbitrary display title. With `list-1` titled `Status`, target's focused test accepts `list=Status`; actual original `dropdownResolve` refuses it. More consequentially, if another list's title is `list-1`, target refuses the valid explicit `list=list-1` as ambiguous; actual original selects `list-1` and its own option ID. Repair should preserve key precedence and explicit new-definition aliases, not replace these with title guessing. Existing `tests/lens/test_dropdowns.hpp` asserts the unsupported title behavior and misses the key/title collision.

2. **Native same-document URL links are not rendered as file fragments (P2).** Target `src/lens/links.hpp:262–265` returns every URL verbatim. Original `../gdocsmith-node/src/core/lens/links.ts:20–33` recognizes same-document Google URLs, resolves the native heading/tab target, and returns no override for unrelated URLs. Actual original execution returned `#goals` for a current-document URL containing `?tab=main#heading=provider-heading`, and no override for an external URL. The target helper necessarily returns the raw URLs. Thus native URL-backed internal links retain opaque provider coordinates instead of source-supported readable fragments; heading rename/file-copy portability differs. Fix the helper and verify its eventual projection caller rather than claiming helper-only success establishes export parity.

3. **New footnote creation lacks the body-only precondition (P2).** Target `src/lens/atom_put.hpp:419–420` accepts a fresh labeled footnote for any `EditTarget`, including a synthetic header/footer/footnote tab. Original `../gdocsmith-node/src/core/lens/parse.ts:970–978` expressly refuses creation when the tab ID contains `::`. Actual original parser execution accepted `[^new]` in the body and returned `unsupportedSyntax` in `main::header`. The target creation helper has no corresponding check before producing an intent that `src/reconcile/paragraph.hpp:308–309` emits as `createFootnote`. Restore the early creation guard without rejecting legal existing-reference preservation. Final public segment transaction behavior remains lane 3's integration obligation; no live API consequence was executed or claimed.

4. **Default English date locale changes exported identity (P2).** Target `src/lens/atom_put.hpp:153` always exports stored `locale=en`; original `../gdocsmith-node/src/core/lens/project.ts:1367–1369` deliberately omits that provider default. Locale participates in target identity at `src/lens/atom_put.hpp:170–175`. A source-exported unchanged English date therefore differs from target projection/identity despite identical provider ID, timestamp and display settings, risking needless recreation when importing the raw source envelope. Actual original `tokenOrdinals` execution omitted `en` and retained `pt-BR`, preserving the same provider ID in both cases. Omit the same default during projection and add the source-envelope/no-op identity case, retaining non-default locales. This is a source-visible helper distinction; the changing envelope/glue path is not certified by this review.

These are source-examined target defects. Original behavioral probes were executed read-only; no new target probe binary or source/test modifications were authorized. The existing target suites do not cover the failing distinctions above.

## Parent-requested fresh HTTP content-URI check

The narrower helper difference is real: original `../gdocsmith-node/src/core/lens/images.ts:97–105` accepts any string, whereas target `src/lens/images.hpp:123–125` filters to non-empty HTTPS. **However, the original public move path does not support HTTP recreation:** actual `tabSectionWrite` execution reaches original `../gdocsmith-node/src/core/model/edit.ts:398–400` and refuses HTTP with `invalidAtom` before request planning. With the identical synthetic plain image, two paragraphs, provider identity and 120×60pt dimensions:

- Fresh HTTPS: original helper returns a source; actual move plans one `insertInlineImage` with that exact HTTPS URI and 120×60pt dimensions, reporting one recreation.
- Fresh HTTP: original helper returns a source, but actual move refuses `invalidAtom`; no planned request was claimed.
- Empty fresh URI: original helper also returns a source, but actual move likewise refuses `invalidAtom`.

Thus there is **no demonstrated legitimate original HTTP recreation positive retired** by this filter; do not infer one from the standalone resolver. Target early refusal classification can differ, and source-only fallback remains a separate local contract. This is not parent approval for a new transport policy, nor blanket equivalence for every URI case. Other actionable findings above remain changes-needed. No source-visible fallback warnings/dimensions or broader copy fallback were authorized.

## Independently checked favorable distinctions

- `src/lens/images.hpp:173–194` keeps the source-only HTTPS fallback local to recreation, creates URI-only intent without dimensions or source-visible notes/loss reports, and owns its original document/tab/key provenance. The separate `imageResolverBuild` still uses `imageSourceOf`, not fallback. Drawing/chart/missing-source refusal and fresh-content precedence remain intact at this helper boundary.
- `atomRecreate` gives supported recreations fresh model keys without retaining provider raw identity; person/date/dropdown payload metadata and moved-footnote labels are preserved. Unknown atom/new hidden-chip creation refuses. Kept dropdown selection updates the selected provider option without replacing chip identity.
- Current image/person mixed-piece emission runs in reverse order at the same insertion index; exact dimensions, email/location and absent invented provider IDs are asserted by the focused suite.
- Apple CoreFoundation timezone/string objects have immediate owning RAII; resolver closures capture owning snapshots, not borrowed documents. Examined date winter/summer, DST/day-boundary and Unicode/punctuation cases remain green. Apple-only framework linkage is conditional through `app_options`; non-Apple timezone/Unicode qualifications remain, not a Linux/full-Unicode acceptance.

## Independent execution and reviewed identities

- Reference HEAD: `d00069dd188ca995bfda9828cc9eee5016603b63`; target HEAD: `20ed42e6a67341d71a13f404f149aee34b4c30bc`. Only the ready atom/helper seams and focused tests were reviewed, not the dirty branch.
- Existing focused executable SHA-256: `201b166813cfeb7675161e7247238bcb14e62e1dfe5bb045af0e746301be8561`. Before execution, recursively resolved local quoted includes had no file newer than the binary; owned source/test hashes matched the ready handoff and remained unchanged at the final check.
- Independent fatal-ASan/UBSan execution of `build/atoms-parity/atoms_test`: **22 cases / 222 assertions passed, zero failures/skips**. No canonical build, install or new test output cache was created.
- Target SHA-256: atom helper `f422aa6e3e0aee05c872938543e0891ee0ba0ca8e029d956371175810bed61a4`; images `612f3f5492d8b8f38d420ae2260a034a6ca6ce07c205d326d5a3856fbd7fd772`; links `fd86fca004ba95ca84f3bae3490b291b6c00d4640e10da54854ebfea3b6f694c`; dropdowns `94d2f706b5e61f1f4052e1195434b26408f052d4191133d356e5b4c1b0cf74ab`; hidden chips `a2b58e5bc7389eadadc0cede74b31daeff4df33ce8e0fb0e0d85f80b76cdd44c`; model request builders `a8557444a806109a564ecefeec8c4347840fff78647bbc51bbfc09938c614958`; paragraph emitter `e1dd68b499c0ea8e6baf9354d99caad1078714193dccb22f4678e88e7493823d`.
- Focused test SHA-256: images `f49e1aba20854cc7fbdd8420b734c4ef4ad016c797b7e83fdba2f15aa711754c`; dropdowns `ad4c2733e0c66ecc15aeda852cdcfe4886cb277fa7c4cf37cc172cc293e8f4ff`; hidden chips `030f386f2ec96db834288f7e02a5decf8d92b49ca636fe7199ac00fba98f0ad3`; links `f1d33cf3cbe7f4ac67c2ef95be23de78b8e1ca52a3e7b6859f9b20133da4d409`.
- Original read-only probes used the already supplied Node 24 runtime and actual source helper/parser imports, not a reimplementation. They covered two native/external URL distinctions, two dropdown key/title distinctions, body versus synthetic-segment footnote creation, and an NFC/NBSP slug. The worker's original 83-suite result and tidy result remain worker evidence, not independent reruns.

## Remaining gate

Parent must serialize repairs with the actual owners; reviewer made no production fixes and woke no peers. Retain this same 6a task for at most one bounded repair recheck and, only after lane 2 is ready, the dependent complete lens/table/style/envelope review. Engine provenance/source/loss permission enforcement, copy-versus-move and repeated-image replacement behavior, authoritative IDs/readback/upload cleanup, omission propagation, shared wrong-behavior controls and final source-bound runtime remain unaccepted integration gates. Typed fallback provenance is not authority to invent an image source or waive an actually missing safe authoritative URI.

## Narrow warning-hunk follow-up

Parent supplied a subsequent warning-only update. Independently examined only `src/lens/atom_put.hpp:405–408` and the corresponding focused assertions in `tests/lens/test_images.hpp:103–110` against original `../gdocsmith-node/src/core/lens/put.ts:1143–1147`. **Approve this hunk:** it constructs an owning note containing the quoted supplied alt label, matches the original literal exactly, and deduplicates equal notes within the report. The focused test asserts that literal and two identical creations leave one note. No new lifetime concern: both note and report entries own their strings.

Observed updated SHA-256: atom helper `8a431b7d9f3b9c975754d860867ffd4d372673398ca11feb889ff4231fb01979`; image tests `cd77155c8d10ba40531cf56e0556a7b6d347f11939cca13510c20e3fbdc12568`. Prior identities/counts above describe the earlier independent execution, not this new source. Parent reports worker fatal sanitizers 22/225 and clean tidy; reviewer did not repeat unaffected runtime checks or relabel that worker result as independent. Four substantive atom-stage findings remain open, and full revised lane-2 glue approval remains withheld.

## One bounded repair recheck — scoped approval

Parent reactivated this same task at lane 4's revised READY. Examined only the four reported repair seams, related focused regressions and invalid-fresh precedence:

- **Finding 1 closed:** `src/lens/dropdowns.hpp:128–150` prioritizes actual definition keys, then explicit registered creation aliases, never display titles; `atomTokenCreate` consumes its provider definition/option IDs. Regressions distinguish title refusal, colliding title, alias creation and existing-key precedence over an alias.
- **Finding 2 closed at helper boundary:** `src/lens/links.hpp:265–321` recognizes current-document native Google URL heading/tab targets and recursively renders the same fragments as original; unrelated/foreign URL targets return no override. Tests assert same-tab heading, other-tab heading, tab-only, external and foreign-document distinctions. Actual projection/export caller integration remains the lens owner's unrevised gate.
- **Finding 3 closed:** `src/lens/atom_put.hpp:417–421` refuses new footnotes in synthetic `::` segments with the source error, while existing-reference preservation remains legal. Body plus header/footer/footnote cases exercise both branches.
- **Finding 4 closed:** `src/lens/atom_put.hpp:154–155` omits provider-default English locale and retains nondefault locale. Regression matches the source-exported unchanged date token identity/provider ID and distinguishes `pt-BR`.
- **Invalid-fresh precedence corrected:** `src/lens/images.hpp:187–189` does not bypass a present invalid string-valued fresh URI using a different HTTPS source URI. Independently executed the actual original `tabSectionWrite` for HTTP and empty fresh strings, each accompanied by a valid HTTPS source URI: both refused `invalidAtom`. The target regression requires that exact refusal. Absent-fresh source-only fallback stays local, URI-only, owning-provenance-only and unavailable to the copy resolver.

Independent current targeted fatal-ASan/UBSan execution: **5 selected repair cases / 69 assertions passed**, zero failures. Four cases/59 assertions ran together, followed by the previously unmatched footnote case/10 assertions only; unrelated cases were explicitly filtered, not asserted to have executed. The existing binary's recursively resolved local quoted includes were checked for freshness before execution. No new build, production/test edit, helper wake or original write occurred. Worker full 24/252/zero-skips and tidy remain worker evidence, not an independently repeated full suite.

Reviewed repaired SHA-256 identities:
- Atom helper `bcbbfc2f6f491ac6e506016e9dcee4258c8b548174cbcdf57508fd4b3fa59c8c`.
- Dropdown helper `3a5541edaeffdc80d63baee1afeb69b9772190ae875fcf5a9d8c25fef933fb16`.
- Images `d34f0bf6e0d6cde2c1408e6a9f59e56361c62802584e87759412df47751590e8`.
- Links `d4a7b52c59c81f017ef8e08a61bd0989f83caeb47b939d702dab1a1ac35427bb`.
- Image tests `51954823faf29925a6f635f8e4d256a750d8f97a2456d51948363a2391d75bf0`.
- Dropdown tests `2b798a777f88b6625725df5c548a415eee302977f009cc987a88c17f5a255764`.
- Hidden/date/footnote tests `97273fbfd2ce77d60ce6b43d797377307e5f4e666b0d6a6561f8c69d4643b38a`.
- Link tests `502d98a772270984651643c5b22703e7236a19a57c663580c960c8354bb03324`.
- Executable `cc9f38f9f4331a07a6695a418e7a5bb20a095dbdf61dc015c7337d26f49d7d93`.

Retain this same task idle for the directly dependent full lens/table/glue stage after revised lane-2 READY. The initial findings above and intermediate warning-follow-up statuses are historical. Scoped helper approval does not discharge creation-alias wiring, projection/export caller behavior, raw-envelope no-op identity, engine source/loss guards, provider readback/cleanup, shared controls or final current-source runtime.

## Full ready lens/table/glue examination — changes-needed

Parent provided NP-E042 source readiness and authorized this same task's dependent full stage without waiting for final tidy/strict-original-parse reporting. Examined current source-supported table/layout/style/reset/envelope, atomic putback and atom/review integration seams; did not audit unrelated dirty-branch changes.

### Actionable full-stage findings

**A1 — Downward merge intent becomes literal cell text instead of a merge (P1).** Target `src/lens/tables.hpp:22–23` and `src/lens/parse.hpp:579–641` recognize only left/up markers; `:merge{down}` falls into ordinary `parseInlines` text. Original `../gdocsmith-node/src/core/lens/parse.test.ts:282–299` and `put.test.ts:908–910` require downward chains and movement of the region's sole content to its top-left cell. Independently executed the actual original parser on the source's two-column Name/Ada/Eng vector: it produced a two-row merge in column 1, moved Eng into the top-right cell, and emptied the lower covered cell. Current target instead keeps a literal marker and no corresponding merge. This is changed geometry/content, not an honestly unsupported refusal. Port region/content-cell resolution, including down chains and escaping/cycle/rectangle guards; add the original positive through real putback/planning.

**A2 — Raw hidden-chip character creation bypasses the uncreatable guard (P1).** Target `src/lens/put.hpp:360–362` creates an ordinary character symbol for newly authored U+E907; the putback path has no hidden-character guard. Refusing new `:chip` atoms in the approved helper does not cover this path. Original `../gdocsmith-node/src/core/lens/put.ts:1152–1155` explicitly refuses insertion or movement of this character because the provider drops it. Independently executed original `tabSectionWrite` adding the character to plain text: `unrecreatableMove`. Target source instead reaches ordinary text insertion. Restore a guard on newly inserted/moved hidden characters while preserving legal weighted within-paragraph retention and deletion; test the actual putback path, not only atom creation.

**A3 — Every existing heading is undeletable, even with no inbound links (P2).** Target `src/lens/put.hpp:206–210` unconditionally refuses a paragraph with `headingId`, including table row/column deletion through the same helper. Original `../gdocsmith-node/src/core/lens/put.ts:243–247` deletes projected blocks through the model; link-damage decisions belong to the separate guards. Actual original `tabSectionWrite` deleting an unlinked provider-ID heading succeeded and retained the unrelated paragraph. Remove the blanket heading-ID prohibition without weakening genuinely protected/review or inbound-link guards; verify deletion requests/tombstones and an actually linked-heading guard separately.

**A4 — Ordinary edits beside retained review metadata are still refused (P2).** Target `src/lens/put.hpp:196–203,289–291` refuses any changed paragraph carrying deletion/style suggestion metadata, not merely edits to a joined suggested paragraph break. Original `../gdocsmith-node/src/core/lens/put.ts:827–842` aligns and splices the actual changed spans. Independently executed two original positives: change an ordinary prefix while a different run retains a pending suggested deletion; repeat with a pending suggested text-style change. Both changed successfully while retaining the pending text and its suggestion metadata. Target's unconditional `review(paragraph)` refusal prevents both. Narrow protection to the actual conflicting edit regions, retaining the already proven suggested-newline refusal. Existing target tests cover conservative refusal or unrelated paragraphs, not these same-paragraph source positives.

**A5 — The repaired native URL helper is bypassed by its projection caller (P2).** Target `src/lens/project.hpp:27–31` returns a `link.url` directly before calling the repaired `documentLinkHref`; thus ordinary exported native same-document URL links still expose provider URL coordinates rather than source fragments. Original `../gdocsmith-node/src/core/lens/links.ts:20–33` and this review's original executable probe yield `#goals` for the same-document heading URL. The approved helper now does this correctly, but integrated projection cannot reach that branch. Wire URL targets through the helper when document context exists, preserving external URL rendering; add a `projectionBuild`/document export assertion rather than repeating helper tests.

**A6 — Dropdown declaration frontmatter is accepted then discarded; public definition editing remains blocked (P2, cross-owner integration).** Target `src/lens/frontmatter.hpp:86–102,159–162` admits `dropdowns` but has no corresponding stored/validated field; `src/lens/document.hpp:507–525` consequently cannot round-trip it. Existing engine seam `src/engine/import.hpp:718–722` additionally labels it read-only. Original `../gdocsmith-node/src/core/lens/frontmatter.ts:210–216` stores/validates it, and `lens/dropdowns.ts:31–47,129 onward` explicitly supports existing-definition edits and creation keys. Consequently the repaired `createdDropdownIds` helper map alone is not a public new-list workflow. Parent must serialize the lens representation/export hook and engine definition-application/alias wiring with their actual owners. At minimum do not silently erase source declarations or claim full dropdown-definition parity; preserve source list/option IDs, rename/recolor semantics and authoritative newly assigned IDs in the integrated repair.

All target consequences above are source-examined; no unauthorized production/test edits or newly compiled probe binaries were used. Actual original behavior was executed where stated. These are current ready-scope discrepancies, not a request to weaken safeguards for source-unsupported operations.

### Favorable examined behavior and current runtime

- Candidate-only typed putback keeps the original tab/tombstone state untouched on refusal; source-linked image repeated-ID replacement refusal and lossy-copy versus lossy-move distinction are exercised.
- Declaration reset fields, missing-property removal, default padding reset/shared-border mirroring, fixed-to-auto width masks, retained table geometry keys, section settings and impossible clears have focused real-request/emulator checks. The narrow `src/reconcile/tables.hpp:399–406` change explicitly clears width with automatic distribution.
- Exact source version `3.0.20` plus preserved legacy integer envelopes, opening s1/settings, intentional blank paragraphs, source review quote/unescape/reply spelling and creation-only omission/default unknown-ID refusal are covered by the examined focused tests. Explicit `gdoc:` sources are not silently omitted.
- Joined suggested-newline edits are separately refused with `suggestionConflict`, while outside-paragraph edits retain the joined original paragraphs. This does not close A4's distinct nonconflicting same-paragraph cases.
- Independently verified the existing `build/cmake/SwarmFidelity/parity_lens` had no newer recursively resolved local quoted dependency, then executed it with fatal ASan/UBSan: **64 cases / 510 assertions, zero failures/skips**. This observed current count supersedes neither the historical worker 60/466 nor parent-reported earlier 64/501; they are different executions. No canonical build or new output cache was created.
- Updated final lane-2 handoff was read after this examination: source frozen at 20:48Z, affected app/test tidy clean, worker 64/510 zero-skips, and actual strict original Node parsing of an unmodified C++ synthetic review export passed. These previously pending worker gates are now evidenced, not outstanding omissions. They do not address A1–A6 or replace independent/public workflow acceptance; this reviewer does not relabel worker strict-parse/tidy checks as independently rerun.

### Full-stage identities

These were unchanged between opening and closing source checks:
- Put `b7f4e3e6a2c6f1cd6fdef88b7ea01038acb0cd4a526177b173865be473bb188e`.
- Document `45b1e0c8311982013ae49afd98517a2a9b7ad07170da5c7663368bd684e00781`.
- Parse `c82e0d3b03bf865db09e8bc7d813c5a6a10c66d669b7b116e014fdc0c54b1232`.
- Project `9cf6917752fdf8d548b836c3cc81ecd264c0f59a93b59c47dbe2ee62d5220ef0`.
- Frontmatter `497722ce350da7417770263c22daee9f6593509b5bcb1a122fe6e847e5118fb5`.
- Render `747525edaf2c942da1952fac3b199f150efc73a76c7b6aadc5459af6dc8243de`.
- Tables `d794ec35734961b91d993d95ff4c7e56f1bb8f248ec25529eaadba352678749a`.
- Sections `0f8f637ea1895433813c7250c3daa0056a218a687adfb61258bfe8862405a5a3`.
- Model table edits `68131f9a4425aa4d41414218ff9e4e3e9fbe1f98a961e39b0f7982d2a0655136`.
- Table reconcile `8da0956b3dfeae4907c2d71949b215b4934a4d910e347edb7c721fefd9d6a8da`.
- Focused executable `c76dbab5a2e3bbf1e49f5909474da140eee4f080d1730b60f32d1293ef41c725`.
- Put tests `1bdd85977f3e062359a5c5f1400769f68bdb71484ee932eaeb595fc34085d53d`; document tests `9ef030fe259187325d677d46ae479b5721506188f92b42dcbf29f3b4e310990c`.

Only 6a state/log were written. No peer/helper wake or source changes. Parent owns repair serialization, any further explicitly bounded reactivation and acceptance. Complete A remains changes-needed despite approved standalone atom helpers.

## Parent-authorized A1–A6 bounded repair recheck (21:10–21:12Z)

**Disposition: approve the reviewed lens-owned repair scope; no consequential residual defect identified in these repair hunks.** This does not certify the still-separate dropdown backend workflow or claim complete source-suite transfer.

Read revised lane-2 READY/frozen state/log; inspected only the six consequential repair seams, their accessible lens-owned regressions and tightly coupled controls. Did not repeat approved atom/conformance review or inspect the prohibited export test.

- **A1 closed:** `src/lens/parse.hpp:270–329,641–668` now resolves directional connected regions, validates sole content/rectangle/cycle/outside conditions, moves content and referenced blocks/style identity to the rectangular head, and recognizes down markers rather than literalizing them. `tests/lens/test_parse.hpp:8–31` checks the original chained-region example, longer down chain, escaped literal and malformed-region controls; `tests/lens/test_put.hpp:462–477` checks actual down-merge content/span plus real request/emulator selfcheck.
- **A2 closed:** `src/lens/put.hpp:302–308,372–375` weights existing raw hidden symbols above ordinary text and refuses newly inserted U+E907 with the original error. `tests/lens/test_put.hpp:480–501` covers new insertion refusal, within-paragraph weighted retention, legal deletion and cross-paragraph refusal with atomic original-tab preservation.
- **A3 closed:** `src/lens/put.hpp:206–210` removes only the heading-ID blanket prohibition. `tests/lens/test_put.hpp:569–603` exercises real unlinked deletion/tombstones/request selfcheck and repeats with a retained actual inbound link, asserting the existing blocking `headingLink` guard only for the linked case.
- **A4 closed for the reproduced retained-deletion/style cases:** `src/lens/put.hpp:290–317` permits nonconflicting edits, retains aligned original reviewed symbols and blocks removal of marked symbols. `tests/lens/test_put.hpp:503–534` proves both original same-paragraph positives preserve complete marked-symbol equality and pass actual request/emulator selfchecks. Protected/review deletion and joined suggested-newline restrictions remain; no assertion here broadens approval to all source suggestion operations.
- **A5 closed:** `src/lens/project.hpp:27–40` now consults approved native-URL rendering with document context and retains ordinary external URLs as fallback. `tests/lens/test_project.hpp:9–30` exercises actual `projectionBuild`, not just the previously approved helper, asserting `#goals` versus unchanged external URL.
- **A6 lens portion closed:** `src/lens/frontmatter.hpp:98,241–269` owns/validates source nested declarations with nonempty option names/keys and normalized valid colors; `src/lens/document.hpp:522–524` serializes them. `tests/lens/test_document.hpp:93–119` verifies roundtrip, malformed declarations, and typed putback using explicitly supplied actual definition/option IDs through the registered alias map. This test supplies definitions/IDs deliberately; it is **not** backend definition creation, selection/relabel or authoritative readback evidence. Those remaining obligations belong to the engine/emulator owners and their separate integration gate.

### Independent targeted execution

Verified all supplied frozen hashes against actual source/tests/executable, then checked recursively resolved accessible local quoted dependencies for binary freshness (75 files, none newer). Independently executed only the seven named A1–A6 repair/control cases under fatal ASan/UBSan: **7 selected cases / 68 assertions passed, zero failures**. The other 64 cases were explicitly filtered, not silently skipped or claimed executed. Worker full 71/578/zero-skips and app/test tidy remain worker-reported evidence.

Current SHA-256 identities matched at opening and closing:
- Put `edb19b2e1e26f763661de3fe6a1cd5b1c99038686b3424dc2ec0e9882119d248`.
- Parse `47f5b341ba97c1537f62a338a093835e785e1be390ef31fa1db7b92913aa2e0d`.
- Project `2025f45a028404a3a153cc5aef5161c8b2b051e765f13078b9c7d7b91d66a3db`.
- Frontmatter `6be909143bad1d65deab46474dd8370acecccc042b2081a7eb8873f8ae7ceda8`.
- Document `aa1e39ec9a9d4807e84433c6f2fe97c0f865b7cb0897cf9b2351d820fe6f34fa`.
- Tables `3d72c15c7477a14c9937b1836ab0b1b44cbf8947c63d5d775635e123a1b1b3b0`.
- Put tests `7b605d91ff69238d9a540c4a4b6e3ba040510143b38db8333511e1d3bbbfaebc`.
- Parse tests `096a98f6f14801e5e69c0242c16589902658957dfdbef96eff8b7e31a41b2473`.
- Projection tests `d2e783ce5fa3bc17f345a9452d0cfe6b44add2d9e82ee093dbae70398618a50d`.
- Document tests `9760de93b6487c52b093860379beea84a67b1d99d0c8ea85700c3d55772f8dc1`.
- Focused executable `83462e46d2498d0cd39a9e3f1622e59be8755457f0f6eb057a6879617a497957`.

### Restrictions and remaining acceptance

`tests/engine/test_export.hpp` is restricted by organization content-exclusion policy. It was not accessed, reconstructed, requested through surrogates or read through another tool; evidence from that file is unavailable to this reviewer. No conclusion about its contents or assertions is claimed.

Only 6a state/log edited; no production/test modifications, installs, builds, live/auth/network calls, commits or peer/agent wakes. The parent-authorized eight-minute recheck stayed bounded. Parent acceptance, actual dropdown engine/emulator lifecycle, broader export/segment application evidence, shared wrong-behavior controls and final source-frozen runtime remain separate.

## Explicit narrow post-qualification lens recheck (21:48–21:50Z)

**Changes-needed R1; owner: lens serialization.** `src/lens/document.hpp:597–599` decides whether an inline suggestion is marked using a prefix-only substring. For hidden suggestion `s1` and a visible `s10` marker, it falsely treats `s1` as marked and omits its required `exportLine`. Actual original `src/core/lens/document.ts:568–584` checks the complete ID followed by space or closing brace. This is a read-hint fidelity defect, not evidence of destructive import behavior.

Independently executed actual original public `docJsonBuild` → `docModelParse` → `documentFileExport` with a fully inserted first paragraph/newline carrying `s1`, followed by text carrying deletion `s10`. Assertions confirmed: no complete `s1` body marker, a complete `s10` marker, and source exports `s1` with `exportLine`. The current target substring condition necessarily suppresses that hint for this same body. Repair only the complete-ID boundary check and add this coupled control; do not broaden scope.

Otherwise reviewed the requested seams: suggestion tab/context hints and before/after newline grammar; adjacent suggestion split; opening/body separator after replacing parsed body; parser-internal leading-blank rebasing without erasing raw document spacing/explicit blank paragraph nodes; hidden inserted paragraph/newline versus genuine joined breaks; and allowed immutable-snippet fixture using explicit source opening/public envelope without weakened assertions. No other consequential residual established within this bounded review.

Independent controls: existing focused binary ran **4 selected cases / 43 assertions, all passed**, with 72 unrelated cases filtered. This covers immutable table/nonbody snippets, source review envelope, opening/raw blank spacing, and unrelated edit beside the fully hidden inserted paragraph. Actual original parser independently accepted the supplied unmodified C++ review-export fixture and preserved quote, reply, literal escapes and exact suggestion tail. An initial proof assertion used the wrong original result shape (array rather than raw suggestion-section object); corrected against the actual public API and reran successfully.

Scope identities matched lane-2 final handback:
- suggestion blocks `41bd6d49760522e177fde1208b399622de29477321a3d8b6aa2578d3f13b7c81`
- document `4364a44d52021cf93f9a1205790ff9394350601e542a57878d059dd2cff26d09`
- parse `8454289624ecff29f8d8142c6531e8840642d9e54426d5d6bc1ac61554873564`
- put `596dad38997865cafbe15f61971dc01d7372b92d27eb727895e0efe99fda7913`
- suggestion tests `1ec049d8b7920bef619329501bb538cba6a028e23e3a4a834e7a5b042a6a692b`
- document tests `cb1259d539db90df3b6184e57f56c3a55b19304d91fdab3068deb3db85b46efa`
- put tests `a92dce8495bd7260e377630a89fa51c44eaa7c75051fdbd005bf3ade9383101c`
- existing focused binary `52fa69e65bb834a32b04929cbab368c286ab486de8ce97dc8c0e734dfbae591a`

Worker 76/622 and tidy evidence remains attributed to the worker. **No whole-binary dependency-freshness certification**: unrelated atom/person validation may change independently. Final fresh combined runtime and V008 remain parent-owned. Excluded `tests/engine/test_export.hpp` and `messages.md` were neither read nor reconstructed. Only owned state/log written; no builds, production edits, installs, live operations or nested agents.

## Resumed independent boundary review — 22:48–22:49Z

**Approve the new narrow hidden-projection repair. No unresolved consequential defect identified in this patch.** Reviewed only `src/lens/review_projection.hpp:13–56` and focused new controls in `tests/lens/test_project.hpp:9–60`, `tests/lens/test_put.hpp:1093–1119`, against actual original `src/core/lens/project.ts:370–380,697–714,1025–1037`. No broad implementation, seven-file diff, accepted export-fixture change or header-guard rereview.

- New predicate matches the source's missing second branch: nonbullet, nonempty inline sequence, every text/atom carrying nonempty suggested-insertion IDs. The existing whole-paragraph-inserted branch remains an OR; ordinary versus inserted newline does not incorrectly retire either source path.
- Empty paragraphs, bullets with current newline, and mixed visible content are not swallowed by the new branch. Fully inserted bullets still follow the existing whole-paragraph branch, as source does.
- Hidden-first paragraphs produce no bogus inline point; subsequent hidden paragraphs append zero-width points only to a preceding projected paragraph-like block, not a table or structural token. IDs retain source first-seen run/atom order, deduplicate within that block, then append previously unseen newline IDs. Appending preserves end-of-previous-visible placement and order.
- Positioned-object IDs do not waive the source hidden predicate. Original models are borrowed const during projection, and typed unrelated-edit controls assert exact retained hidden paragraph—including positioned IDs—and no tombstones for both current and inserted newline variants. No model deletion/blank trimming introduced.

### Independent evidence

Executed actual read-only original public model/projection APIs with hidden text plus a marked person atom, positioned-object IDs, both current/inserted newline, and a visible prefix. Assertions verified source omission, preceding-point IDs/order/dedup, no points when first hidden, exact model preservation, and blank/bullet/mixed predicate boundaries. All passed.

Independently ran only the two focused existing repair regressions under fatal ASan/UBSan: **2 cases / 39 assertions passed, zero failures**; 76 unrelated cases explicitly filtered. Did not repeat the worker's full 78/657 run or tidy, which remain worker evidence. No build or canonical output was produced.

Current SHA-256 identities match the released worker handback:
- `src/lens/review_projection.hpp`: `7f7fd3cd4923fcd97ce144b5d70af6e78e7c850bab6c955c4d472b0e9a62ef55`
- `tests/lens/test_project.hpp`: `9c8c1469c0f176bb2801fc1a758e3f1b7833cef8259494f83c5194e051274a68`
- `tests/lens/test_put.hpp`: `5b4d9774a0a3dbbf125dcf28757445c47b963f487850c0ee7c4eab193db01460`
- existing focused binary: `1273d5d2c7cbc1f5729c2f137540819887a4bbda3d3e58ec8990f2e941d19d35`

Approval is bound to this repair and these identities, not a whole-binary freshness certificate or real-driver green claim. Prior source/target/control counts remain historical; fresh combined build/full tests/actual V008 driver belong to the retained runtime and parent acceptance gate.

Only owned 6a state/log edited. No production/source writes, builds, live/network/auth calls, installations, services, commits, new agents or peer messages. Excluded export test and `messages.md` not read or reconstructed. Completed within the explicit three-minute boundary.

Depends on: lane 4's source-ready handoff for the independent atom-first stage; the complete gate additionally requires lane 2's source-ready handoff and integrated glue examination. Do not hold the ready independent stage behind the unfinished lens tail.

Scope: independently examine their reported current feature seams and focused tests against the read-only Node behavior, not the entire pre-existing dirty branch. Cover table geometry/merges, declarations/layout/style resets and no-ops, raw envelope compatibility, supported atom identity/create/move/delete/link semantics, IANA date boundaries and loss guards. Check the Apple-only framework hook without taking CMake ownership.

Own writes: this state and matching log only. Production source, tests, map and inbox are read-only; return actionable findings to the actual implementation owners.

Acceptance: explicit approve/changes-needed with source/target locations, observable consequences, source-linked parameter distinctions and reviewed input identities. Do not equate helper existence or a green test count with a verified public workflow. Record platform-qualified or genuinely unsupported source cases honestly.

Budget: 15 active review minutes, one review and at most one bounded repair recheck, within the shared 22:00:36Z deadline. Return atom findings first if lens is not ready; retain the same task/context for its directly dependent integration follow-up. Parent accepts; reviewers never accept their own source changes.

Safety: no Google/auth/network/services, installs, commits, Node edits or nested agents. Sanitize all artifacts; no secrets, sensitive personal data, confidential source bodies or raw transcripts.
