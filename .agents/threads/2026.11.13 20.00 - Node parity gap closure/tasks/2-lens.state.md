# 2 — Lens, layout, style and table parity

Status: resumed narrow hidden-suggestion projection repair READY for runtime requalification; all lens ownership released. No parent acceptance or complete Node-suite transfer claimed.

## Resumed projection handback (22:47Z; before absolute 22:49Z stop)

- Diagnosed actual V008 upstream projection, not header/comparator: target retained zero-range markers for a first paragraph with only suggested inserted content and an ordinary current newline. Exact suggestion-ID matching correctly exposed this underlying omission.
- Implemented the actual original `project.ts` `blockHiddenIs` branch (1025–1040): nonbullet, nonempty inlines, every text/atom has nonempty insertion IDs. Original blocks remain unchanged. Kept genuinely blank and mixed-visible paragraphs and bullets. Previous-visible paragraph receives insertion point markers in source first-seen ID order, including newline IDs, matching `blocksProject`/`insertionPoints` (370–380, 697–715).
- Positioned-object behavior was checked by executing actual original public `projectionBuild`: all-hidden paragraph with positioned-object IDs projects zero blocks while the model remains exactly unchanged. Target regression now requires that same projection and retained model; typed unrelated-edit preservation covers positioned IDs with ordinary and inserted newline variants. No blind model deletion or blank trimming.
- Fresh isolated `SwarmFidelity/parity_lens` fatal ASan/UBSan: **78 cases / 657 assertions, all pass, zero skip**. Focused test translation-unit clang-tidy clean. One intermediate test-only spelling error (`Bullet` versus `BulletRef`) was corrected before this fresh successful build; earlier 78/643 result is superseded.
- No V008/control/full-suite success claim: actual fresh adapter/driver and integrated runtime gates remain parent/runtime-owned. No canonical builds, Node writes, live calls, installs or nested agents.
- SHA256: `src/lens/review_projection.hpp` `7f7fd3cd4923fcd97ce144b5d70af6e78e7c850bab6c955c4d472b0e9a62ef55`; `tests/lens/test_project.hpp` `9c8c1469c0f176bb2801fc1a758e3f1b7833cef8259494f83c5194e051274a68`; `tests/lens/test_put.hpp` `5b4d9774a0a3dbbf125dcf28757445c47b963f487850c0ee7c4eab193db01460`; executable `1273d5d2c7cbc1f5729c2f137540819887a4bbda3d3e58ec8990f2e941d19d35`.
- All narrow lens ownership released; no commands running.

## Post-qualification handback (absolute stop 21:47Z)

Final authorized minimal follow-up: matched suggestion IDs now require the exact source delimiter (` ` or `}`), so visible `s10` cannot suppress hidden `s1` context hints. Added a focused existing-file collision regression. **Source-only receipt: not rebuilt or executed**, per short parent deadline; whitespace check passed. Released ownership immediately. New SHA256 document `becedcb9d9ab6330baa627e257024ff1587baee6c66978ae67f29242f2804695`; document tests `03cfe81b7f6baa10ac7694f7eaebb6e34c766054fd4050fa47f6cbc74f9b24a4`. Earlier executable/counts below are historical for this final hunk; final runtime must compile it.

Released all lens ownership. Final focused fatal ASan/UBSan **76 cases / 622 assertions, all pass, zero skip**; final focused test tidy clean, earlier affected app tidy clean. No commands remain running. Full suites/V008 still require parent requalification, not claimed.

Actual allowed review failure reproduced as missing s1 in its hand-built source fixture; fixture now explicitly carries required opening and uses public typed envelope, retaining all immutable snippet/context/no-op assertions. Production fixes preserve source suggestion tab/context hints and exact single-final-newline grammar, split adjacent suggestions, and supply opening/body separators for edited parsed bodies. Fully hidden inserted paragraphs do not falsely join an unrelated tail. Raw document body spacing and explicit blank paragraphs are preserved. Actual original export/parser grammar and unmodified fresh C++ review parsing passed with comment/reply/quote data intact.

Final SHA256: suggestion_blocks `41bd6d49760522e177fde1208b399622de29477321a3d8b6aa2578d3f13b7c81`; document `4364a44d52021cf93f9a1205790ff9394350601e542a57878d059dd2cff26d09`; parse `8454289624ecff29f8d8142c6531e8840642d9e54426d5d6bc1ac61554873564`; put `596dad38997865cafbe15f61971dc01d7372b92d27eb727895e0efe99fda7913`. Review tests `1ec049d8b7920bef619329501bb538cba6a028e23e3a4a834e7a5b042a6a692b`; document tests `cb1259d539db90df3b6184e57f56c3a55b19304d91fdab3068deb3db85b46efa`; put tests `a92dce8495bd7260e377630a89fa51c44eaa7c75051fdbd005bf3ade9383101c`; focused binary `52fa69e65bb834a32b04929cbab368c286ab486de8ce97dc8c0e734dfbae591a`.

## Independent six-finding repair (21:08Z)

Read the exact 6a state/log findings and source-executed reproductions. All six owned seams repaired; dependent engine workflow acceptance remains separate.

- **A1:** Directional merge markers now resolve connected regions and their sole content cell, including downward chains. Content/referenced blocks/style markers move to top-left; outside-table, cycles, nonrectangles, mixed literal/reference content and escaped marker distinctions are covered. Source pointers: `parse.test.ts:282–299`, `put.test.ts:908–910`. New actual typed-put/request/emulator regression verifies downward merge geometry and retained content.
- **A2:** Newly inserted raw U+E907 throws source `unrecreatableMove`; existing raw hidden characters use weighted alignment so within-paragraph retention outranks ordinary text, and deletion stays legal. Cross-paragraph reinsertion refuses atomically. Source `put.ts:1152–1155`.
- **A3:** Removed only the blanket `headingId` deletion prohibition. Protected/review restrictions remain. New actual deletion plan/tombstone/selfcheck passes for an unlinked provider-ID heading; the same workflow with a retained real inbound link produces existing `headingLink:block` guard. Source `put.ts:243–247`.
- **A4:** Paragraph protection caused by review metadata no longer forbids unrelated text-prefix edits. Alignment retains original reviewed symbols and their metadata; deleting actual marked symbols still conflicts, and the existing joined suggested-newline conflict remains. Both pending-deletion and pending-style same-paragraph positives now pass actual request/emulator selfchecks. Source `put.ts:827–842`.
- **A5:** URL-backed projection now calls approved `documentLinkHref` with document context before falling back to the original URL. Integrated projection asserts same-document native heading URL becomes `#goals`, unrelated URL remains unchanged. Source `links.ts:20–33`.
- **A6:** Added owning `DocumentFrontmatter.dropdowns` JSON, exact source nested declaration validation/hex normalization and document serialization. Roundtrip and malformed declarations tested; typed put proves registered new-list alias reaches actual supplied definition/option IDs. Engine owner consumes this field before put and owns actual definition operations/IDs/selection validation. Source `frontmatter.ts:210–216`, `dropdowns.ts:8–47,129 onward`.

Fresh final fatal ASan/UBSan: **71 cases / 578 assertions passed, zero failures/skips**. Fresh affected app and lens/reconcile tests clang-tidy both clean. Owned patch whitespace check clean. Earlier 64/510 below is historical.

### Frozen identities

- `put.hpp`: `edb19b2e1e26f763661de3fe6a1cd5b1c99038686b3424dc2ec0e9882119d248`
- `parse.hpp`: `47f5b341ba97c1537f62a338a093835e785e1be390ef31fa1db7b92913aa2e0d`
- `project.hpp`: `2025f45a028404a3a153cc5aef5161c8b2b051e765f13078b9c7d7b91d66a3db`
- `frontmatter.hpp`: `6be909143bad1d65deab46474dd8370acecccc042b2081a7eb8873f8ae7ceda8`
- `document.hpp`: `aa1e39ec9a9d4807e84433c6f2fe97c0f865b7cb0897cf9b2351d820fe6f34fa`
- `tables.hpp`: `3d72c15c7477a14c9937b1836ab0b1b44cbf8947c63d5d775635e123a1b1b3b0`
- Put tests: `7b605d91ff69238d9a540c4a4b6e3ba040510143b38db8333511e1d3bbbfaebc`
- Parse tests: `096a98f6f14801e5e69c0242c16589902658957dfdbef96eff8b7e31a41b2473`
- Projection tests: `d2e783ce5fa3bc17f345a9452d0cfe6b44add2d9e82ee093dbae70398618a50d`
- Document tests: `9760de93b6487c52b093860379beea84a67b1d99d0c8ea85700c3d55772f8dc1`
- Focused executable: `83462e46d2498d0cd39a9e3f1622e59be8755457f0f6eb057a6879617a497957`

### Genuine integration qualifications

Dropdown definition creation/reselection targeted engine positive is worker-reported, not lane-2 acceptance; emulator automatic relabel/readback and unchanged-definition workflow are currently engine/delegated-owner repair gates. No helper, API or emulator writes made here.

Organization policy denied access to `tests/engine/test_export.hpp`; this lane did not retry through other tools or reconstruct it. Engine owner must handle that file's assertions. An independent public source-model probe established a paragraph with bold run+image followed by a one-cell table parses back as two blocks without embedded terminal LF, but actual original produces three paragraphs when those run/cell texts contain embedded terminal LF. No LF trimming, blank-paragraph deletion or whole-fixture-equivalence claim was introduced; header-specific fixture behavior remains engine-owned.

## Live implementation

- Stable engine hook: `documentTabMarkdown(file, tab, doc, tabId)` prefixes declaration YAML; `markdownParse` consumes declaration-only tab frontmatter. `markdownPut(EditTarget, Projection)` stays atomic.
- Stable atom hook agreed with lane 4: `lens/atom_put.hpp` token identity/create/keep and recreation APIs. Lane 2 owns sole putback wiring.
- Implemented: declaration null/reset preservation, source-version preservation for exact Node `3.0.20` plus existing integer `1`, identity-preserving table alignment/geometry/settings/merges, editable section layout.
- Validation uses isolated `build/cmake/SwarmFidelity/parity_lens` test-only harness, not canonical application outputs.
- Source-linked translated obligations: OB-027/OB-040 table/layout/style putback; put table settings at 969, section settings at 1060, creation/copy cases at 1093; OB-043 raw exact-version envelope and declarations. Original Node cases have not all been transferred.

## Latest verified evidence (20:48Z)

- Fresh isolated fatal ASan + UBSan: **64 cases / 510 assertions, all passed, zero skipped**. Includes actual reconcile request/emulator checks, new-table fixed/automatic width clearing, table-only cell-style mutation without invented paragraph anchors, merges and identity-preserving geometry, declarations and null resets, review-carry default opening section, settings-only mutation, image copy versus lossy move and repeated-ID replacement refusal, creation-only omission controls, joined-suggestion unrelated-edit positive and exact refusal, source-mode review round trips.
- Fresh affected app translation unit and isolated lens/reconcile test translation unit clang-tidy: both clean, exit 0.
- Actual original Node selected put/document/frontmatter/table-alignment suites: **86 pass, zero failures or skips**.
- Lane 5 independently reports strict original Node parsing of unmodified C++ pre/post-import V001–V004 exports passes. Exact body separators are now source-faithful, not adapter-normalized. Explicit blank paragraph nodes remain represented and tested.
- Direct actual strict original Node `documentFileParse` consumed an unmodified C++ source3 review export: synthetic comment/reply/quote, escaped literal directive text, and suggestion before/after section text all asserted intact. Source mode uses double directives; numeric1 legacy remains triple-delimited.
- Actual original runtime controls confirmed arbitrary joined pending-break edits refuse with exact `suggestionConflict`, while an unrelated paragraph edit succeeds. Target now preserves both underlying joined paragraphs during that positive edit.

## Stable integration contracts

- `DocumentFrontmatter.sourceVersion` defaults to exact string `3.0.20`; parsed numeric `1` resets it to null and stays legacy. Engine owns export/import propagation.
- `DocumentFileTab.openingSettings` retains parsed Node opening `s1`; its line is removed without trimming remaining body separators, matching actual Node. `documentTabMarkdown` reinstates it for typed putback and prefixes declarations.
- `markdownPut(EditTarget, Projection|string_view, AtomPutOptions={}, PutOptions={})` stays atomic; reports `recreatedImages` owning provenance and losses. `PutOptions.allowTokenOmission` is explicit, default false; engine threads it only for document creation. Known objects never silently omit; foreign explicit image sources still refuse. Resolver results are owned/cached per put call, so omission classification does not duplicate resolver work. Foreign lossy copies retain warning provenance without requiring move permission.
- `projectionBuild(tab, const DocModel* = nullptr)` propagates document context to links; engine resolves pending links after all tab changes. Engine review normalization is called at projection completion.

## Explicit remaining gaps / qualifications

- Creation-only omission is implemented and focused source-grounded cases pass; whole `doc:null` import acceptance remains engine/independent-runtime owned.
- Arbitrary edits across joined suggested paragraph breaks are genuinely source-refused, not a unilateral target restriction; exact source error and unrelated-edit success are verified. No complete source suggestion-suite transfer is claimed.
- Source-mode review/comment helpers are engine-owned and sole document glue/parser integration is verified locally plus direct strict Node parsing. Full workflow V008 acceptance remains parent/runtime owned.
- Non-Apple Unicode/time-zone qualifications belong to the atom helper lane and remain explicit.
- This is a focused lane result, not a fresh full-suite claim. Parent owns acceptance, differential disposition and obligation closure.

## Contract

- Objective: carry source-editable declarations, section/page layout, table geometry/merges and rich body intent through typed putback and real request planning. Refusal or read-only export is not a replacement for successful source behavior.
- Own production: `src/lens/put.hpp`, `frontmatter.hpp`, `document.hpp`, `project.hpp`, `render.hpp`, `parse.hpp`, `tokens.hpp`, `text_styles.hpp`, paragraph/cell/named-style/table/section lens helpers; `src/model/edit_tables.hpp`; `src/reconcile/tables.hpp`. Own directly related existing lens/table tests and this state/log.
- Exclude: atom/image/dropdown/hidden-chip/suggestion/review helpers reserved for lanes 3/4; engine/session/import/app/API, `model/to_json.hpp`, other reconcile/model headers, `tests/test_main.cpp`, fixtures, docs, map/inbox and Node writes.
- Integration ownership: sole `put.hpp`/general lens glue writer. Atom lane supplies standalone helper/API for your serialized integration; engine lane owns session/import and other reconcile hooks. Request precise shared-file changes rather than concurrent edits.
- Inputs: real Node lens put tests, named/style/layout/table behavior and independent fixture expectations. Preserve full before/after state, identity, default/null/reset distinctions, back-to-front requests and atomic refusals for genuinely unsupported source cases.
- Added obligation: investigate Node file-envelope version versus target integer 1. Preserve raw-file interchange as an explicit obligation; implement faithful compatibility with focused cases where feasible, or report a concrete protocol decision rather than hiding it in differential normalization. Preserve existing local edits/artifacts.
- Required deliverables: actual supported mutation workflows, source-case-linked translated regressions with meaningful payload/state/order assertions, representative negative controls, stable helper/engine handoff and precise remaining gaps.
- Validation: smallest existing lens/table sanitizer selectors, lint affected app/tests, then fresh integrated full checks at freeze. Use isolated/reused test-only caches, never canonical app builds or installs.
- Budget: 45 minutes/three repair cycles within 20:00:36–22:00:36Z, 20 total launches/four executing turns; source-freeze target 21:40Z.
- Gate: worker checks → independent examination → fresh dependent runtime/obligation evidence → parent acceptance.
- Unexpected issues: native coordinator `2bd0d79e-d1ee-4f5f-87c3-90ca1a101052` or parent completion; publish shared API needs promptly, never simply remove safety gates.
- Safety: preserve dirty work and read-only source/tests; no commits, Google/auth/services/installs, nested agents or content-exclusion bypass. Sanitize all artifacts: no credentials, sensitive personal data, confidential source bodies or raw transcripts.
