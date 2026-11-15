# 4 — Atom, chip, image and link parity

Status: four independent atom findings repaired and fresh checked; ready for parent-serialized independent recheck. Final integrated acceptance remains parent-owned.

## Implemented handoff

- `src/lens/atom_put.hpp`: owning-value `AtomPutReport`/`ImageSource`, `AtomPutOptions`, `atomTokenProject`, `atomTokenIdentity`, `atomTokenKeep`, `atomTokenCreate`, and `atomRecreate` overloads. Lens owner received exact contract before integration; engine owner consumes recreation losses/provenance.
- Creation/recreation: person email, rich-link URL, date display metadata/timezone/locale, dropdown definition/option identity, sized images, footnote create labels and page breaks. New keys replace old provider identity; kept image replacement/dropdown selection retain identity. New hidden chips and unknown/uncreatable atoms refuse.
- Images: `imageSourceOf` retains current content URI/dimensions and reports source-linked crop/rotation/adjustment/border/alt-text loss. Parent-refined `imageRecreationSourceOf` additionally permits an existing plain image's authoritative HTTPS `sourceUri` for local recreation when no fresh source exists. No actual recreatable URI, drawing/chart source, and unknown/ambiguous resolver sources still refuse; lossy approval cannot manufacture a source. `imageResolverBuild` owns immutable same-document/all-tab and supplied external-document source snapshots and deliberately remains fresh-content-only. Upload placeholders now match the existing transaction's `gdocsmith-upload:` scheme.
- Links: pure pending style construction and complete-intent resolution for same/other-tab headings/tabs, new heading keys, and explicitly supplied other-document snapshots; missing, ambiguous, malformed, and newly authored Google slug URL targets refuse. Engine/lens owners own calls and provider-ID resolution phases.
- Parent-approved Apple CoreFoundation hook is owned by lane 5. This helper uses RAII for IANA date offsets and Unicode NFC/case normalization, with no global timezone state, dynamic loading, external service, or dependency install.

## Fresh checks

- Isolated `build/atoms-parity/atoms_test` compiled from existing GapChecks test flags, explicit system framework linkage; final current fatal ASan/UBSan after independent-review repairs: **24 cases, 252 assertions, zero failures/skips**.
- Focused clang-tidy with isolated compilation database and owned-header filter: clean.
- Actual read-only original `put.test.ts`, `parse.test.ts`, `hiddenChips.test.ts`: **83 passed, zero skipped**.
- Additional identical vectors through actual original parser/slug helper and target tests: five date instants (winter/summer, Tokyo previous UTC day, DST spring/fall) and two Unicode/punctuation slugs passed.
- Subsequent focused repair: moved footnotes recreate their original label with fresh identity. Explicit original token label is retained in lens-owned snapshot; fallback derives the source reference order only while available and otherwise refuses instead of guessing. Real existing paragraph request emission is exercised with mixed text/image/person insertion, exact size/email/location payloads, reversed piece order, and absent invented provider IDs.
- Reference HEAD remained `d00069dd188ca995bfda9828cc9eee5016603b63`. Current SHA-256:
  - Atom helper `bcbbfc2f6f491ac6e506016e9dcee4258c8b548174cbcdf57508fd4b3fa59c8c`.
  - Dropdown helper `3a5541edaeffdc80d63baee1afeb69b9772190ae875fcf5a9d8c25fef933fb16`.
  - Images `d34f0bf6e0d6cde2c1408e6a9f59e56361c62802584e87759412df47751590e8`.
  - Links `d4a7b52c59c81f017ef8e08a61bd0989f83caeb47b939d702dab1a1ac35427bb`.
  - Image tests `51954823faf29925a6f635f8e4d256a750d8f97a2456d51948363a2391d75bf0`.
  - Dropdown tests `2b798a777f88b6625725df5c548a415eee302977f009cc987a88c17f5a255764`.
  - Hidden-chip/date/footnote tests `97273fbfd2ce77d60ce6b43d797377307e5f4e666b0d6a6561f8c69d4643b38a`.
  - Link tests `502d98a772270984651643c5b22703e7236a19a57c663580c960c8354bb03324`.
  Later changes reopen these checks.

## Source-linked obligations and limits

- `lens/put.test.ts:215–232,304–323,1093–1110`: person relabel refusal/recreation; full date move metadata; changed/unknown chip IDs create using supported attributes.
- `lens/parse.test.ts:121–189`: create payloads, image sizing/negative dimensions, date-only noon/floating/zoned date/locale/invalid-zone distinctions.
- `lens/hiddenChips.test.ts:66–130`: genuinely uncreatable chip creation/cross-paragraph movement versus legal deletion/within-paragraph weighted preservation. Helper tests cover creation refusal; general hidden-character putback stays with lens owner.
- `engine/import.test.ts:1717–1757,1782–1889,1917–1946,2106–2265`: image reject clone, copy/current URI/size, lossy move permission, upload lifecycle, and dropdown selection/definition IDs. Pure helper payload/identity/source tests are translated; actual upload cleanup/provider-ID/readback lifecycle remains engine-owner evidence, not a lane-4 pass.
- `lens/links.ts` and `lens/put.test.ts` link positives: exact native IDs/new keys/external URL plus missing/ambiguous/malformed errors tested. General rendering/glue, unchanged Google slug URL preservation, and authoritative readback remain integrated checks.
- Non-Apple date support requires real standard-library timezone support or explicitly refuses; non-Apple Unicode currently has only ASCII case normalization. No Linux/full Unicode parity claim.
- Lens owner received two concrete source distinctions for integration: a repeated image ID carrying `src` must refuse even though a single moved image may replace `src`; lossy copies retain their source and warn, whereas lossy moves require explicit permission. These are not accepted from helper-only checks.
- Independent wrong-behavior mutation controls and complete import/provider-ID/cleanup workflows remain parent/lane-5 integrated gates; passing negative refusal tests are not mislabeled as mutation evidence.
- External document authorization/read-error propagation and source creation-only omission policy remain integration obligations, not pure-helper passes.

## Bounded source-fidelity repair

- Original `lens/put.ts:1526–1537` proves a separate local-recreation fallback after `imageSourceOf`: existing embedded `imageProperties.sourceUri` must start with `https://`; resulting create has only type/URI, not dimensions. Original fresh-image path alone records `recreated` and losses.
- Executed six actual original `tabSectionWrite` vectors: fresh content URI prefers fresh data and keeps size/report; HTTPS source-only fallback has no size, no recreated report and no notes; missing both, HTTP source, drawing and chart moves refuse `unrecreatableMove`. An initial probe had an invalid synthetic document title/provider identity; corrected harness inputs before claiming these results.
- Target preserves the fallback request payload and no-warning distinction. It additionally retains owning provenance in internal `report.recreated` with `ImageSource.sourceUriFallback=true`; this marker is not a copy warning or permission waiver. Engine owner must match the authoritative source URI and independently apply actual loss/provenance guards. A source-only image is still absent from `imageSourceOf`/cross-tab resolver; no copied/foreign-source fallback expansion.
- Parent's safe-URI refinement does not authorize bypassing an invalid fresh URI with a different source URI. Subsequent original public-path proof confirmed HTTP/empty fresh URI plus valid HTTPS source URI still refuses `invalidAtom`; target now preserves that precedence. Source-only fallback qualifies only when no string-valued fresh URI was provided. Fresh helper validation remains distinct from original `imageSourceOf`'s mere string-type check.
- Follow-up upload-tail source fidelity: original `lens/put.ts:1145–1146` emits the owning label and deduplicates notes. Target now uses the exact note `alt text "Logo" was dropped: Google can't set an inserted image's alt text`; focused tests assert exact string and duplicate-image note deduplication. Cross-document copy warnings remain engine-owned, not duplicated in helpers.
- Original `lens/put.ts:99,134–139` and `lens/parse.ts:1387–1410` prove omission is explicit creation-only `allowTokenOmission` → `onOmission`. Five actual original parser checks confirmed: default unknown opaque ID refuses; explicit option omits with exact note; unknown image ID may omit; unresolved explicit `gdoc:` source still refuses; supported person attributes create without omission. Exact note contract published to lens/engine owners. No general parser/engine writes were made.

## Independent 6a repair dispositions

1. **Dropdown key/title collision — repaired.** New pure `dropdownResolve` matches actual definition keys first, then explicitly registered `AtomPutOptions.createdDropdownIds`, never display titles. Replaced the incorrect title-positive test with title refusal, collision precedence and registered-alias cases. Source: `lens/dropdowns.ts:84–97`.
2. **Native same-document URL rendering — repaired.** `documentLinkHref` converts same-document native Google URL heading/tab targets to readable file fragments and returns no override for unrelated URLs. Existing public/helper signatures remain unchanged. Caller export integration remains lens-owner evidence. Source: `lens/links.ts:20–33`.
3. **Body-only new footnotes — repaired.** `atomTokenCreate` refuses fresh references in synthetic `::` segment tabs with the source error. Existing-reference keep path is not prohibited. Tests cover body creation plus header/footer/footnote refusals and preservation. Source: `lens/parse.ts:970–978`.
4. **Provider-default English date identity — repaired.** Projection omits `locale=en`, preserves nondefault `pt-BR`, and matches the unchanged source token identity/provider ID. Source: `lens/project.ts:1367–1369`.
5. **Invalid fresh URI precedence — repaired after original proof.** Two actual original `tabSectionWrite` vectors (HTTP and empty fresh URI, each with valid HTTPS source URI) returned `invalidAtom`; target regression now requires that exact refusal, not positive fallback.

Fresh actual original helper/parser execution additionally passed nine shared review vectors: dropdown key/title/registered alias; native/external URL; body/segment footnote; default/nondefault date locale. Owned target current checks passed 24 cases/252 assertions under fatal ASan/UBSan with clean affected-header clang-tidy. No engine/glue/API/reviewer source or state was edited, and no peer/reviewer was woken during the four-active capacity constraint.

## Contract

- Objective: port source-supported atom/chip/image/link creation, update, move/delete and resolution semantics with correct identity, source guards and partial/unknown handling. Preserve genuinely uncreatable source cases and exact source distinctions.
- Own: a focused standalone lens atom putback helper; `src/lens/{images,dropdowns,hidden_chips,links}.hpp`; `src/model/to_json.hpp`, `src/reconcile/paragraph.hpp`; corresponding existing dedicated atom/image/dropdown/link tests and this state/log.
- Exclude: `put.hpp`/general lens glue and table reconcile (lane 2), engine/session/import/app/API and other reconcile/model files (lane 3), suggestion/review helpers, test entrypoint, docs/map/inbox, Node writes and fixture-data changes.
- Handoff: publish a minimal owning-value typed helper API and cases to lane 2, the sole putback integration writer. Engine/image-upload service needs go to lane 3; request explicit cross-owner hooks, not duplicate implementations or bypasses.
- Evidence: source-case-linked inputs, full metadata/identity outcomes, request ordering, provider-assigned IDs, allowed loss versus unwaivable unknown movement, links and cleanup. Ordinary success/refusal and representative known-wrong controls must reach the real helper and integrated path.
- Validation: focused existing sanitizer suites and affected lint in isolated test-only cache, then lane 2/3 integrated and final current-source checks. No actual Google access or uploads.
- Budget: 40 minutes/three repair cycles within shared 20:00:36–22:00:36Z, 20 launches/four executing turns; source-freeze target 21:40Z.
- Gate: worker checks → independent examination → fresh source/target obligation/runtime evidence → parent acceptance.
- Unexpected issues: native coordinator `2bd0d79e-d1ee-4f5f-87c3-90ca1a101052` or completion; report concrete source/API/ownership gaps promptly.
- Safety: preserve dirty work and read-only source/tests; no commits, Google/auth/services/installs, nested agents or content-exclusion bypass. Sanitize all artifacts: no credentials, sensitive personal data, confidential source bodies or raw transcripts.
