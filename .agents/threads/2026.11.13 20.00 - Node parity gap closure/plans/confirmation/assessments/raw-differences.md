# Raw differences assessment

Independent refiner: raw-assessor; task 2 contract rev 1; brief B reviewed

Result: changes needed to the proposed accounting units; three repair/decision packets below need lead selection and ownership. No production, test, adapter, comparator, receipt or original writes. No new equivalence accepted.

## Authority and scope

Explicitly loaded: repository AGENTS.md; swarm common/refiner roles; confirmation rules; task 2 rev 1; briefs.md B; workspace.md; app-rewrite skill and prepare-transfer/verify references. Initial missing-contract blocker resolved through corrected bus pointer. Host default model/effort metadata unavailable, not inferred.

Assessment is static: read-only JSON projections, full multiset reconciliation and bounded source/adapter/assertion reads. No build, test, differential run, network, installation, service, live action, commit, push or helper launched. Historical results remain historical.

Receipt root R = .agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/resumed-final-recheck

Group IDs resolve through [raw-group-index.md](raw-group-index.md), which accounts for every group and occurrence without copying observations.

## Refinement and accounting

1. R/differential-results-raw-diffs.json contains 206 groups and 250 leaf occurrences across ten cases plus the known-upload-error variant. The brief's 101/27/35/3/9/26/5 classification counts are groups, not leaves; occurrence counts are 124/31/38/3/10/39/5. Correct the brief's unit wording before using these figures in acceptance.
2. The case/path/original/target multiset reconstructed from all eleven R/differential-results.json differential arrays equals the grouped receipt's expanded multiset exactly: zero missing and zero extra entries. This validates accounting, not behavior. The array's 206 groups include variant in their key; 187 unique path/value triples alone would improperly merge 19 variant distinctions. Driver citation: tests/parity/run-engine-differential.mjs:497–505. A separate read-only projection of the written index found 206 covered entries, zero missing/duplicate entries and zero class/case mismatches against the raw receipt.
3. Classes are triage labels, not semantic dispositions. For example G008's origMarkdown difference is file bytes, despite its observable-contract class; G124's deletionEvidence is adapter-authored explanatory text, despite not being in the wording class. Neither observation establishes an engine defect by its class.
4. R/final-summary.json freshGates[0] records exitCode 1. R/differential-results.json summary records zero named assertion failures and 70/70 controls detected, while all eleven executions differ. These historical named passes do not waive raw RED.
5. The receipt binds frozen/current inputs at its historical execution; it does not establish current source freshness. Source changes after that receipt require new authorized qualification. No current identity certification is made here.

## Six consequential traces

### T1. Ordered external reads: G001, G002 and G003–G006, V001

Observation: R/differential-results.json cases[0].node/cpp.observation.callLog. Index 1 is commentsList on the original and getDocument on the target; the target also adds getFileMetadata reads. This is not merely renaming a call.

Creation/production trace: tests/parity/node-import-adapter.mjs:24–27,87–91 invokes the actual original fileExport. Original ../gdocsmith-node/src/core/engine/export.ts:168–179 performs getDocument then commentsList. Target tests/parity/cpp-import-adapter.cpp:267–270 directly invokes exportModel with seeded reviewComments, bypassing the original export wrapper's comments read. Its import uses src/engine/import.hpp:966–985, whose Docs::getDocument always requests authoritative Drive identity/title metadata as well as Docs JSON. Both real engine behavior and unequal adapter export boundaries therefore contribute to the ordered list.

Obligation/blind spot: original ../gdocsmith-node/src/core/engine/import.test.ts:451–463, named OB-001; driver tests/parity/run-engine-differential.mjs:178–185 checks unchanged/no writes/same export/body, not read order, read failures or an identity/title conflict. The raw comparison does retain this distinction.

Candidate disposition: unresolved boundary/schedule difference, not authorized equivalence. A later boundary-aligned characterization must preserve exact ordered calls and separately exercise failed/conflicting metadata reads. Do not remove the metadata guard merely to match the schedule.

### T2. Native bytes: G007, G008 and G009, V001

Observation: original exported markdown is 533 UTF-8 bytes; target is 440. Both end with one LF. Differences include missing target apiUser metadata and different named-style declaration representation/keys, not just trailing whitespace. Raw SHA-256 values differ; raw bytes remain solely in the existing receipt.

Creation/production trace: node-import-adapter.mjs:205–229 and cpp-import-adapter.cpp:399–417 read actual export.md/export.orig.md. Original export.ts:199–213 passes apiUser into documentFileExport. Target src/engine/export.hpp:312–324 initializes file frontmatter without apiUser; :355–375 builds per-tab ns-prefixed named-style sets with a different declaration projection. Neither adapter fabricates these file bytes.

Obligation/blind spot: OB-001 checks document text, no mutations and path identity, not exact envelopes. Original import.test.ts:115–149 additionally makes inherited-font meaning consequential; a shorter declaration is not proven equivalent by successful parsing. The strict original-parser interchange recorded in R/differential-results.json normalization.rawFileInterchange establishes parser acceptance only, not preservation of every metadata/default-style obligation.

Candidate disposition: unresolved native representation, with an observable metadata omission candidate. Exact byte equality versus authorized schema-level equivalence remains a lead/decider question; no trimming, key rewriting, default removal or version translation is proposed.

### T3. Missing error navigation: G169, G171–G176, V010

Observation: R/differential-results.json cases[9].node.observation.images.imageErrors[0].result.errors[0] includes line/tab; target lacks both. The second unreadable-source error likewise loses navigation. Status remains refused and no writes occur, so matching status does not preserve the result contract.

Creation/production trace: both adapters collect actual fileImport results for unknown-image and missing-source edits: node-import-adapter.mjs:189–196; cpp-import-adapter.cpp:350–361. Original import.ts:239–256 collects writeMarkdown failures and converts relative/body positions to document lines while adding section.title. Target import.hpp:1338 calls writeMarkdown directly; atom_put.hpp:398–402 throws CoreError for an unresolved image. The CoreError result branch import.hpp:1529–1533 constructs code/message/phase without line/tab, unlike its FileProblems branch.

Obligation/blind spot: original import.test.ts:1811–1844 covers owning-image recreation and missing-source refusal. Driver run-engine-differential.mjs:305–315 checks refusal, no mutations and message patterns, not error location. Proposed navigation preservation is grounded in actual original execution/production behavior, not an assertion falsely attributed to that source test.

Related but distinct: G092's commentsLine is 27 versus 19 because native layouts differ. Both point to the actual comment marker in their own exported files. Source export.ts:218–225 derives navigation from envelope fields; target export.hpp:485–496 scans markers. Target V008 native frontmatter omits commentsLine/suggestionsLine entirely even though its result supplies commentsLine. Absolute line numbers cannot be compared as meaningless noise without separately verifying each native file's navigation contract.

Candidate disposition: observable diagnostic omission; repair packet P2. Correct per-file commentsLine offsets are not proven defects merely because their integers differ.

### T4. Review rejection adds writes: G084–G091, V008

Observation: R/differential-results.json cases[7] records one original deleteContentRange request; target sends that deletion plus updateParagraphStyle and updateTextStyle. Target's paragraph update spans the remaining newline and resets a broad style field mask; provider direction/namedStyleType differences remain in G088–G091.

Creation/production trace: both adapters record actual outgoing requests and actual backend replies (node-import-adapter.mjs:104–119; cpp-import-adapter.cpp:226–251). Original import.ts:215–260 passes rejected suggestion IDs through writeMarkdown and tracks applied rejects; target import.hpp:1229–1235 and :1338–1339 supplies its rejected-suggestion state and consumes the putback report. Original import.test.ts:330–350 specifies hidden insertion, immutable snippet refusal and rejection removing content/review. Target src/reconcile/paragraph.hpp:508–552 can emit full paragraph/newline restyles; container.hpp:424–438 can select forceFullStyle when merging into an empty kept paragraph. Original reconcile/container.ts:121–130 and paragraph.ts:100–127 have corresponding full-style machinery: its existence alone does not explain the divergent requests. This identifies the relevant intent/emission seams, not a runtime-proven branch diagnosis.

Obligation/blind spot: driver :238–251 checks review removal and remote text but not unrelated style writes. safety-assertions.mjs:67–90 strengthens full review payload/grammar but contains no review-only-write assertion. Contrast its :47–65 cell-style-only-write guard, which would reject unrelated operations for V007.

Candidate disposition: observable extra-write/provider-state defect candidate, not an equivalent request decomposition. P1 must establish whether the newline style reset is necessary and preserve unrelated paragraph/text formatting.

### T5. Upload aliases are already deduplicated: G125, G130–G143, V009

Observation: R/differential-results.json cases[8] shows one upload on each side, two images/two receipt URIs and three insertions. On both sides, ordered insertion URI receipt ordinals are [0,1,1]. The local relative path and file URL share the same uploaded image; the HTTP image is the other entry. The two uploaded image byte digests differ. This is evidence against an upload-alias-duplication defect in this receipt.

Creation/production trace: node adapter :154–164 and target cpp-import-adapter.cpp:301–311 inject the same local/file/HTTP forms. Original lens/images.ts:29–45 resolves file URLs and relative paths to the upload scheme; target lens/images.hpp:55–90 performs the corresponding local path resolution. Original engine/import.ts:577–617 uploads the collected placeholders and maps receipts back to them; target import.hpp:645–677 does likewise. Generated throwaway IDs come from different backend creation schemes: cpp-import-adapter.cpp:190–205 constructs URIs using its returned ID; node-import-adapter.mjs:63–69 retains the original FakeGoogle receipt.

Obligation/blind spot: original import.test.ts:1917–1932 requires one upload, three insertions, deletion and no warnings. Driver :288–299 checks one upload and three inserted URI memberships, but does not assert local/file alias sharing or exactly two distinct uploaded sources. Thus a future duplicate-upload regression could pass that named assertion.

Candidate disposition: alias sharing is statically observed as preserved for this input; provider-ID/URI byte differences remain unresolved. R/differential-results.json normalization.createdIdBijections records proposed ID correspondences with appliedToComparison=false. No ID or URI normalization is authorized. Cleanup evidence prose/backendExists differences additionally originate in unequal observation schemas (:199–217 versus target :363–377), not demonstrated cleanup failures.

### T6. Provider state differs despite matching font verdicts: G041–G061, V005/V006

Observation: V005 and V006 both emit insertText plus updateTextStyle but differ in explicit style payloads. For default 11pt the original clears explicit font fields; target writes explicit 11pt/Arial. For 22pt both set size, but target additionally writes weightedFontFamily. G060 reports stored explicit 22pt in an original run where the corresponding target run lacks it. Text-run boundaries also differ (G043–G056); concatenated text alone hides these details.

Creation/production trace: original source assertion import.test.ts:115–149 deliberately strips font metadata from post-write readbacks, requiring 11pt match and 22pt mismatch. Adapter node-import-adapter.mjs:122–140 mirrors that readback transform; original emulator/fakeGoogle.ts:289–293 returns a structured clone, so the transform does not alter stored provider state. Target cpp-import-adapter.cpp:146–163 similarly transforms its JSON copy, while :422–425 separately exposes backend storage. Thus G060 is not explained by the deliberate readback stripping alone. Actual outgoing payloads remain in cases[4/5].node/cpp.observation.mutations[0].requests[1]. Target paragraph.hpp:153–175,537–552 is a style-request emission seam; a full segmentation/root-cause proof was not performed.

Obligation/blind spot: driver :215–225 preserves distinct match/mismatch statuses and text but not exact stored style/range consequences. The raw comparison does detect these. Identical failure verdicts for 22pt do not establish provider-state parity.

Candidate disposition: unresolved explicit-style/segmentation difference with potential durable formatting consequence. P3 requests the smallest discriminating characterization, not removal of the readback fault or provider-state normalization.

## Existing recovery-layout authority only

G023/G032/G033 correspond exactly to V003 result.exportPath and V004 result/replay.exportPath. R/raw report acceptedVariances[0] records only recovery-directory layout authority; it does not authorize metadata, diagnostics, IDs, extra writes or general byte differences.

Driver :197–213 checks distinct returned export, current revision, preserved input and no duplicate content send; safety-assertions.mjs:23–28 checks actual parsed fresh body. Its known-ID :30–47 separately protects canonical-existing-path, warning, unchanged edited bytes and fresh original; that is not permission to extend the V003/V004 waiver.

Driver :163 additionally checks returned result.exportPath containment under the owned run prefix; its historical shared:export-contained assertions pass on both V003/V004 sides. Driver ownedPath and adapter work-root checks constrain destinations independently. The receipt's replay checks cover stale refusal/single batch, not a separate full replay-export body/containment assertion. Distinctness/current content/containment/no-clobber must remain explicit conditions in any later requalification; do not broaden authority to compensate for an unobserved condition.

## Three prioritized packets — proposals, not implementation authority

### P1. Review-only rejection must not silently restyle unrelated content

- Claim blocked: V008 review behavior/request/provider-state parity; G084–G091
- Evidence: T4; original sends one delete, target adds broad style resets and changes surviving paragraph metadata
- Proposed owner/scope: a separately assigned reconcile owner, paragraph/container seams and one focused existing review regression file; no adapter/comparator changes
- Minimal experiment: preserve nondefault surviving paragraph direction and newline style in a synthetic suggested-insertion rejection; compare putback/reconcile intent, actual ordered requests and authoritative post-write style on both engines before choosing the repair seam
- Expected protection: remove only the rejected insertion, preserve comment/review payload and untouched formatting; explicitly justify any necessary style request
- Disconfirming result: original also resets the same surviving style for the same pinned fixture, or target's additional writes are shown necessary while preserving exact observable formatting
- Negative control: add an unrelated direction/text-style reset; the focused assertion must reject it after reaching the backend
- Acceptance dependencies: scoped review and fresh affected shared/raw gate after source changes; no request filtering

### P2. Preserve actionable line/tab diagnostics for image refusals

- Claim blocked: full V010 error contract, G169/G172/G173/G176; adjacent phase/wording differences remain separate
- Evidence: T3; original attaches document-relative context; target CoreError branch drops it
- Proposed owner/scope: separately assigned engine import error-context owner, tab writeMarkdown boundary and one focused image-refusal test; preserve existing structured FileProblems handling
- Minimal repair recipe: attach the failing section's title and source-equivalent document-relative location before converting an image-resolution failure into the public result, retaining the original section-line fallback when no token position exists; preserve code/message/status/no-send behavior
- Expected observation: both unknown-image and unreadable-source errors identify the actual edited tab and source-equivalent location in each native file, including after a header offset change
- Disconfirming result: missing navigation is explicitly authorized as a replacement contract, or the source position points at unrelated content and needs separate source characterization
- Negative control: omit tab or shift line into another section; the localization assertion must fail
- Acceptance dependencies: source-linked diagnostic assertion, affected V010 controls/raw rerun and independent review; do not normalize away optional fields

### P3. Characterize stored font semantics before accepting style decomposition

- Claim blocked: V005/V006 durable provider-style equivalence, especially G041/G042/G060
- Evidence: T6; different request payloads and stored runs survive identical match/mismatch verdicts
- Proposed owner/scope: separately assigned model/reconcile characterization owner; existing font readback fixture and focused style assertion only until cause is established
- Minimal experiment: keep current 11pt/22pt vectors and readback omission fault; independently compare authoritative stored character/newline effective style and explicit fields at insertion boundaries before any omission transform
- Expected observation: classify run segmentation separately from character/newline style; retain distinct 11pt pass/22pt mismatch outcomes and exact request observations
- Disconfirming result: all character/newline semantics and subsequent edits agree, with source-linked evidence that explicit-style presence is irrelevant; such evidence supports a narrowly proposed decision, not an automatic waiver
- Negative control: clear 22pt from a surviving unaffected character or style the newline; a storage-focused assertion must reject the fault even if the existing status check still passes
- Acceptance dependencies: decider on any equivalence proposal; source ownership and fresh affected gates if a repair is chosen

## Remaining limits and handoff

Complete accounting is not complete semantic adjudication: all groups outside the narrow existing recovery authority retain their original unresolved/potential labels. Six representatives are traced, not 206 root causes proven. No new normalization, obligation retirement, raw-RED waiver, release acceptance or live-provider fidelity claim is made.

Future execution must use the exact driver flags recorded in R/final-summary.json freshGates[0], newly owned result/run destinations and the assigned canonical runtime owner. Do not overwrite this historical receipt. This assessment supplies refinement and repair proposals; lead owns acceptance.
