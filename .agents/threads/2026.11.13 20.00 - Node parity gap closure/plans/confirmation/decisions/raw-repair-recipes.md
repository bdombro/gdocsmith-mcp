# D-10-D1: Review-restyle (P1) and image-diagnostic (P2) repair recipes

Task 10-D1, contract rev 1. Decider: raw-repair-decider (claude-opus-5.5/high, as launched). Read-only native return for the lead to record verbatim. Outcome: **decided**. This authorizes no implementation and accepts no new equivalence. 10-A and 10-B still need the lead to accept this decision, and per rules.md they also need an authored brief and a separate refinement (see §4).

## 0. Sources and authority

- Explicitly loaded: repository AGENTS.md; swarm roles/common.md and roles/decider.md; plans/confirmation/rules.md; tasks/10-D1.md rev 1; map.md; decisions/concurrency-plan.md (D5); assessments/raw-differences.md (T3, T4, P1, P2); tasks/10-A.md, 10-B.md and 10.md. Nothing was inherited automatically.
- Bounded spot-checks (read-only):
  - src/engine/session.hpp:731–788
  - src/engine/suggestion_rule.hpp:138–259
  - src/reconcile/paragraph.hpp:480–570
  - src/reconcile/container.hpp:395–450
  - src/engine/import.hpp:1220–1240, 1290–1345, 1500–1548, 1596–1608
  - src/lens/atom_put.hpp:385–405
  - src/model/errors.hpp:73–120
  - tests/parity seed suggestion-v1.json (synthetic) and the V008/V010 adapter and driver blocks
  - tests/engine/test_import.hpp:180–382
  - tests/reconcile/test_paragraph.hpp:63–146
  - original ../gdocsmith-node/src/core/engine/import.ts:213–300 and import.test.ts:1813–1844
- Not done: no build, test, runtime, network, install, helper launch or source edit. Historical receipt values were cited by location, not copied.
- Helpers: none. The assignment forbids nested helpers, and the refined packets plus the spot-checks above were enough to decide.

## 1. Seam and conflict decision (decided first)

**P1 does not need src/engine/session.hpp. The default P1 recipe adds no 7-R dependency.**

Facts:
- session.hpp:731–788 (`TabHandle::writeSegment`) only orchestrates. It copies the tab, calls `suggestionRejectsApply`, parses Markdown, drops rejected review markers, calls `lens::markdownPut`, and stores the candidate. It contains no paragraph or text style emission or field-mask logic.
- The style requests come from reconcile:
  - paragraph.hpp:509–531: the `forceFullStyle` mask, and the `restyleAll` newline/text mask.
  - container.hpp:424–438: the `forceFullStyle=true` override for an empty kept paragraph.
- The candidate-shaping code that might also cause the defect is outside 10-B's provisional files, and 7-A owns it under D5:
  - suggestion_rule.hpp:169–243 strips rejected symbols, restamps the paragraph, and in the join path copies the next paragraph's newline.
  - src/lens/put.hpp is the Markdown putback.

Conditional conflict rule:
- 10-B's provisional ownership stays as is: paragraph.hpp, container.hpp, tests/reconcile/test_paragraph.hpp, plus the new test header in §2.3.
- If characterization places the divergence in suggestion_rule.hpp, src/lens/put.hpp or session.hpp, 10-B must stop before any repair edit and escalate. The lead would then append the exact file scope to 10-B, add an accepted-7-R dependency (D5's session.hpp edge, extended to these three 7-A files), and have the P1 brief re-refined.
- 10-B must never edit a 7-A-owned file while 7-A holds it.

P2 has no conflict with 7-A. Its repair seam is src/engine/import.hpp, which 10-A owns under D5.

## 2. P1 recipe: review-only rejection must not restyle unrelated content

### 2.1 Claim, scope and non-goals

- Blocked claim: V008 request and provider-state parity, G084–G091.
- In scope: rejecting a suggested **text insertion inside a paragraph whose own newline survives**. This is the V008 shape: seed suggestion-v1.json has one paragraph, "Hello world" carries insertion s1, the newline is not suggested, and `paragraphStyle` is empty.
- Out of scope: rejecting a suggested paragraph *break* (the newline-insertion join path). Merged-paragraph style semantics there are not characterized, and the "no style write" invariant is not asserted for it. A style write in that path is neither declared a defect nor waived.
- Non-goals: request filtering or post-hoc pruning, treating the extra writes as an equivalent decomposition, changes to adapters, driver or comparator, and new equivalences.

### 2.2 Exact reproducer

- **R-P1-0, historical, both engines.** In receipt R (tasks/parity-evidence/resumed-final-recheck), compare differential-results.json cases[7] node and cpp `observation.mutations`. The original sends a single deleteContentRange over the inserted run. The target adds updateParagraphStyle over the surviving newline with a broad mask, plus updateTextStyle. Cite the pointers; don't copy the values. This is static evidence, not a fresh run.
- **R-P1-1, target engine, exact V008 shape.**
  - Seed the target emulator with a synthetic document equivalent to suggestion-v1.json.
  - Export, add ` reject` to the `::suggestion{id=s1…}` header, and import.
  - Record:
    - (a) the ordered sent requests
    - (b) stored post-write `paragraphStyle`, newline `textStyle` and remaining runs
    - (c) result status, errors and the review/comment payload.
  - Expected after repair: exactly one batch with exactly one deleteContentRange whose range equals the inserted run (original indices 1–12 for this seed), no updateParagraphStyle or updateTextStyle, and stored paragraph and newline style equal to the pre-import state.
  - Required first observation: this test fails on the current tree. Record the red output before any repair.
- **R-P1-2, target engine, nondefault surviving style.** Same flow, but:
  - The surviving paragraph has nondefault style: explicit `namedStyleType` NORMAL_TEXT, `direction` RIGHT_TO_LEFT, `alignment` CENTER.
  - Its newline `textStyle` has `bold: true`.
  - A second, unaffected paragraph has an italic run.
  - Insertion s1 is mid-paragraph ("Keep [inserted] keep") and also at paragraph start.
  - Expected: only the deletion is sent, and all surviving style fields and unaffected runs are unchanged in storage.
- **R-P1-3, layer localization, test-only, target.** Using public functions only, apply the stages to a copy of the parsed seed model:
  1. `suggestionRejectsApply`
  2. `lens::markdownPut` with the exported tab Markdown
  3. the reconcile entry point (original vs candidate)

  After each stage, record the paragraph key, `style`, `newline.style` and stamp. The first stage where the paragraph's identity or style differs from "original minus rejected symbols" is the localized layer. That decides the seam: reconcile means 10-B continues; suggestion_rule.hpp, put.hpp or session.hpp means stop and escalate (§1). Record the content hashes of those three 7-A-owned files with the result, because 7-A may be editing them at the same time. A localization verdict stays provisional until it is rechecked against the 7-A tree accepted by 7-R.
- **Original side, nondefault variant.** The R-P1-2 nondefault/mid-paragraph shapes are not in any receipt. Do not assume the original's behavior for them. Proposal: 14-F/14-FR adds a contained original row (original fileImport on the R-P1-2 synthetic seed) for 6-B. 10-BR needs either that row or an explicit lead decision that the target-only invariant is enough, justified by R-P1-0 plus AGENTS.md safety rule 6 (preserve unchanged metadata).

### 2.3 Safe minimal source and test seams

- **Engine-level test.** Put it in a new 10-B-owned header, tests/engine/test_import_review_writes.hpp.
  - It may *use* `RemoteImportFixture` from tests/engine/test_import.hpp read-only, provided its include line comes after engine/test_import.hpp.
  - It must not edit test_import.hpp, which 7-A owns.
  - If 7-A's in-flight changes make that fixture unstable at check time, define a minimal local fixture in the new header instead.
  - It needs a one-line include in tests/test_main.cpp under the existing lease. Lease order becomes 11-A → 10-A → 10-C → 10-B, skipping unselected tasks.
- **Reconcile-level test.** If localization is reconcile, add a focused unit case in tests/reconcile/test_paragraph.hpp. It uses the existing `roundTripParagraph` harness to remove symbols from a kept paragraph with nondefault style, and asserts request kinds = {deleteContentRange} only and `compare.diffs` empty.
- **Repair seam, only if localized to reconcile:**
  - paragraph.hpp: the `forceFullStyle`/`restyleAll` decision and the `styleFieldsChanged` inputs for a kept paragraph whose only change is a deletion.
  - container.hpp:424–438: when the empty kept-paragraph override applies.
  - Fix the decision that creates the extra fields. Don't drop requests after they are built.
  - Existing tests must keep passing, including "a word change … restyled with the full mask" (inserted text legitimately gets the full mask) and "empty last body paragraph restyles directly".

### 2.4 Fault controls (each must be reached and must fail)

- **C-P1-a, injected unrelated reset.** A test-only `batchUpdate` wrapper appends an `updateParagraphStyle{direction: LEFT_TO_RIGHT}` (and, separately, an `updateTextStyle` clearing bold on the newline) before forwarding to the emulator. Both the exact-request assertion and the stored-style assertion must fail.
- **C-P1-b, storage-only fault.** After a clean import, change the stored emulator paragraph `direction`, or the newline bold, directly. The stored-style assertion alone must fail, which shows it doesn't depend on the request assertion.
- **C-P1-c, review payload.** Remove the synthetic comment or reply from the exported state in a test copy. The review-payload preservation assertion must fail.
- Production code gets no fault switches.

### 2.5 Disconfirming results (stop and escalate, no repair)

- The original also restyles the surviving paragraph for the same shape (via the R-P1-0 pointers or a fresh original row).
- The target's extra writes are shown to be necessary while preserving exactly the same observable formatting.
- Localization lands in a 7-A-owned file (§1).

### 2.6 Review gate

- **10-BR (L2, high) verifies:**
  - red-before-green output for R-P1-1 and R-P1-2
  - R-P1-3 localization, with the recorded 7-A file hashes
  - no request filtering
  - exact single-delete request list and range on the V008 shape
  - stored style equality
  - C-P1-a, C-P1-b and C-P1-c each reached and failing
  - existing reconcile and engine suites pass, plus lint, using the commands fixed by the refined brief (`just test`, `just lint` per AGENTS.md)
  - review/comment payload preserved
- Then a fresh affected raw gate in 13-A for G084–G091. Those groups stay unresolved until that rerun; nothing here waives them.

## 3. P2 recipe: preserve line and tab for image refusals

### 3.1 Claim and faithful target semantics

- Blocked claim: V010 image-refusal error contract (G169, G172, G173, G176). Phase and wording differences are separate and not in scope.
- Original behavior (import.ts:243–256, :300):
  - Each tab's `writeMarkdown` runs under `problemCollectAsync`.
  - Each collected problem gets `tab = section.title`.
  - Its line becomes `section.line` when the problem has no line, otherwise `section.bodyLine + problem.line − 1`.
  - The two image errors are content-code CoreErrors with no line, so the original reports the edited tab's title and the line of that tab's directive.
- Target behavior today:
  - import.hpp:1338 calls `target.writeMarkdown` directly.
  - atom_put.hpp:399–402 (unknown image, `anchorNotFound`) and import.hpp:1249 (unreadable source, `anchorNotFound`) throw CoreError.
  - The CoreError branch at import.hpp:1529–1533 emits code, message and phase only.

### 3.2 Exact reproducer

- **R-P2-0, historical.** Compare receipt R differential-results.json cases[9] node vs cpp `observation.images.imageErrors[0..1].result.errors[0]`: node has line and tab, cpp has neither. The values stay in the receipt.
- **R-P2-1, target.** In the new 10-A-owned tests/engine/test_import_errors.hpp:
  - Seed a two-tab synthetic target document and a synthetic source document. Leave the "missing-source" document unseeded, so reading it fails.
  - Export, then insert `:image{src="gdoc:synthetic-source/unknown-image"}` into the **second** tab's body and import. Repeat with `gdoc:missing-source/unknown-image`.
  - Assert:
    - `status == "refused"`
    - error `code == "anchorNotFound"`
    - message matches the current text (`no single image with that id in that document` / `document missing-source couldn't be read`)
    - phase unchanged from the pre-repair observation
    - zero batches and zero uploads
    - `tab ==` the second tab's title
    - `line ==` the 1-based line of the second tab's `::tab` directive, found by searching the written file text, not hard-coded.
  - Record the red output first.
- **R-P2-2, header offset.** Repeat R-P2-1 after adding lines before the edited tab: an extra first-tab paragraph, plus frontmatter or comment-block growth if available. Assert the reported line moves to the new directive line.
- **R-P2-3, regression.** An existing FileProblems path must still pass its errors through with their own line and tab. Use the tab-title validation at import.hpp:1291 that reaches the 1526 branch.

### 3.3 Safe minimal source and test seams

- **Repair (10-A, src/engine/import.hpp only):**
  - Wrap the import.hpp:1338 `writeMarkdown` call using the existing `model::problemCollect` semantics (errors.hpp:95–120), with an empty location.
  - Map each collected problem: `tab = section.title`; `line = problem.line ? section.bodyLine + *problem.line − 1 : section.line`.
  - Rethrow as `model::FileProblems`. That type derives from CoreError, so the 1545/1606 `refused` classification and no-send behavior are kept.
  - Non-content CoreErrors and non-CoreError exceptions must still propagate unchanged, as `problemCollect` already does.
- **Line-basis guard.** Apply the `bodyLine` offset to FileProblems that come out of `writeMarkdown` *with* a line only if the executor first shows those lines are tab-body-relative. Otherwise leave them unchanged and record that, so lines are never offset twice.
- **Collection order.** Decided: throw right after the failing tab (the minimal option). Rejected: the original's collect-all-tabs-then-throw (import.ts:300), because it changes control flow for later tabs and segment/footnote writes, and V010 doesn't exercise it.
- **Remaining gap.** Multi-tab, multi-error aggregation and dropdown-step problem locations are unresolved parity differences. They are not claimed here.
- **Docs.** docs/images.md changes only if it documents the image-refusal error fields. A bounded grep found no such documentation.
- **Include line.** One tests/test_main.cpp include line, after engine/test_import.hpp, under the lease.

### 3.4 Fault controls

- **C-P2-a, missing tab.** Run the localization predicate on a copy of the observed error with `tab` removed. It must fail.
- **C-P2-b, wrong tab or shifted line.** Run the predicate with the first tab's title or directive line, or with `line ± 1`. It must fail. Also check that the two tabs' directive lines differ, so the test can tell them apart.
- **C-P2-c, no-send.** Inject an extra `batchUpdate` or upload count into the observation copy. The no-send assertion must fail.

### 3.5 Disconfirming results

- The lead or user explicitly authorizes missing navigation as a replacement contract.
- A fresh original row (6-B) shows a different line basis, for example a token position instead of `section.line`. In that case, characterize before repairing.

### 3.6 Review gate

- **10-AR (L2, high) verifies:**
  - red-before-green output
  - R-P2-1, R-P2-2 and R-P2-3 assertions
  - C-P2-a, C-P2-b and C-P2-c reached and failing
  - status, code, message and phase unchanged
  - FileProblems branch unchanged
  - full `just test` and `just lint`, per the refined brief
  - original-context evidence from 6-B, or the historical R-P2-0 pointers if the lead accepts those
- Then a fresh V010 raw rerun in 13-A.
- Do not normalize away optional fields.

## 4. Changes this decision makes to briefs, tasks and gates (proposed; the lead decides)

1. 10-B: add the new header tests/engine/test_import_review_writes.hpp, with read-only use of RemoteImportFixture, and a fourth include-lease slot after 10-C. Add the stop-and-escalate trigger from §1, which extends the 7-R edge rule to suggestion_rule.hpp, put.hpp and session.hpp.
2. 10-A: semantics as in §3; ownership unchanged.
3. Per rules.md "Planned task contracts", this recipe is not a brief. 10-A and 10-B each still need a level-0 authored brief plus a separate level-1 refinement (for example 14-H/14-HR for P2 and 14-I/14-IR for P1). That refinement fixes exact commands and selectors.
4. 14-F/14-FR and 6-B: add an original-side P1 nondefault-style rejection row, and an optional V010 two-tab context row.
5. 13-A: G084–G091 and G169/G172/G173/G176 must be in its fresh raw gate. Raw RED stays until then.

## 5. Unresolved limits

- The P1 root cause is not proven, because nothing was run. R-P1-3 decides the seam, and the session.hpp conclusion covers only the files that emit requests.
- Paragraph-break rejection semantics, P2 multi-error aggregation and dropdown-step locations, and the P2 line basis for FileProblems that already carry lines are all unresolved.
- Original-side evidence for the new variants is missing until 6-B runs.
- The historical receipt doesn't establish that current source is fresh.

## §3 P2, rev-2 addendum (supersedes §3.2–§3.4 where they differ)

Trigger: 10-AR returned `changes needed` on the §3.5 disconfirming result. The original reports the failing **top-level block's** line, and counts it from the top of the file, frontmatter included. It does not report the `::tab` line. P1 is unchanged and was not reviewed again.

### Facts (from bounded reads; values reported by the reviewer are labeled)
- Original, line attachment: lens/parse.ts:228–232 wraps each top-level token in `problemCollect(problems, { line }, …)`.
- Original, line basis: lens/document.ts:283–306 starts its line counter after the frontmatter envelope's line count, so `line` and `bodyLine` are file-absolute. import.ts:252 maps a problem to `section.bodyLine + line − 1`.
- 10-AR reports that the original returns line 24 for V010 (the failing block). I did not re-derive that value.
- Target, line basis: `documentFileParse` (src/lens/document.hpp:133–194, 294, 324, 370) counts every tab, comment, reply and suggestion line from `split->body`, after the frontmatter. Every import validator that reports these lines (import.hpp:142–545, 1449) is therefore off by the frontmatter line count compared with the original.
- Target, where the failure happens: parsing (src/lens/parse.hpp) and putback (src/lens/put.hpp) are separate steps. Image resolution fails inside putback (put.hpp:372 and :384 call `atomTokenCreate`, which throws at atom_put.hpp:399–402; the unreadable-source error comes from the resolver at import.hpp:1249). Only put.hpp knows which top-level block failed.
- Equality: `ProjBlock`, `TokenRef` and `Projection` use defaulted `operator==` (tokens.hpp:36–101). A plain line field on any of them would change how blocks compare and diff.
- Composed input: `documentTabMarkdown` (document.hpp:631–646) adds generated lines before `tab.markdown`: a declarations frontmatter and an optional `::section` opener. `tab.markdown` comes last, unchanged. `markdownParse` also removes leading blank lines (parse.hpp:336–337).
- No production code compares whole `DocumentFile`, `DocumentFileTab` or `SuggestionBlock` objects. Import compares individual fields (bounded grep). The existing line assertions in tests/engine/test_import.hpp:37–132 set synthetic lines directly and don't depend on parsing.

### Decision 1: carrying the block line
1. **src/lens/parse.hpp and tokens.hpp (no current owner):**
   - `markdownParse` records a 1-based source line for each top-level block, counted from the start of its input after the frontmatter. The blank lines it removes still count.
   - The lines live on `Projection` as a member whose type always compares equal (for example `struct SourceLines { std::vector<int> lines; bool operator==(const SourceLines&) const { return true; } };`). This keeps the defaulted equality, rendering and diffs unchanged.
   - Nested cell sub-parses record no lines. Problems inside a table take the line of the enclosing top-level block, as in the original.
2. **src/lens/atom_put.hpp (no current owner):** add `AtomPutOptions::locateBlockProblems`, default false, so every other caller behaves as before. import.hpp sets it to true only around the tab-body `writeMarkdown` call. It resets it to false before `segmentsWrite`/`footnotesWrite`, so segment- and footnote-relative lines never reach the result.
3. **src/lens/put.hpp (owned by 7-A, so this needs an accepted 7-R edge):**
   - When `locateBlockProblems` is set, the top-level `containerPut` step for each parsed block runs with `model::problemCollect` semantics, attaching `where.line = blockLines[j]`. Here `j` is that block's index in the input `Projection`.
   - If `tokenOmissionsApply` (put.hpp:867) removes top-level blocks, the line table must be trimmed in step with it.
   - Throw `FileProblems` at the first failing block. Collecting every failing block, as the original does, is rejected because putback changes the candidate as it goes. That difference stays open.
   - No session.hpp change: `writeSegment` already passes the parsed `Projection` straight through. Its alias pass changes span tokens only and doesn't change block order or count; check this.
4. **src/engine/import.hpp (owned by 10-A):** around the body `writeMarkdown`, map each collected problem to `tab = section.title` and a line as follows:
   - Compute `prelude` = the number of newlines in the composed parse body (after its frontmatter) before the `tab.markdown` suffix.
   - Then `line = p.line && *p.line > prelude ? section.bodyLine + (*p.line − prelude) − 1 : section.line`.
   - Assert that `tab.markdown` really is a suffix of the composed body. If it isn't, fall back to `section.line` and record that.
   - Code, message, phase, the `refused` status and no-send behavior stay unchanged.
- Rejected: an import-level check that resolves images before putback. It would resolve tokens that are kept, which could cause false refusals and extra reads, and it duplicates logic. Also rejected: putting a line field on `TokenRef` or `ProjBlock`, because it changes equality.

### Decision 2: line basis, made file-absolute at the source
- **Change `documentFileParse` (src/lens/document.hpp)** so all recorded lines are file-absolute. Compute `fmLines` once: the number of newlines in the input before the body begins, or 0 when there's no frontmatter. Use `markdown.size() − body.size()` only after checking that `body` is a suffix of `markdown`; otherwise count the frontmatter delimiter lines directly. Add `fmLines` to:
  - `tab.line` and `tab.bodyLine`
  - comment block, reply and suggestion lines
  - any line-bearing problems raised inside `documentFileParse`
- Rejected: adding the offset only in import.hpp. That would mix two line bases in the same `errors[].line` field, or else require changing about 30 call sites.
- **Observable impact, a deliberate move to the original's behavior:** `tabTitleTaken`, the tab-line, comment, reply and suggestion-header problems, and the `section.line` fallback all move by `fmLines`. List each of these in 10-A's acceptance and the 10-AR review. Also check docs that describe error line numbers, per the documentation-upkeep rule in AGENTS.md.
- Coordination: tell 11-A in case its transferred assertions read parsed tab lines.

### Decision 3: updated expectations
- **R-P2-1:**
  - Expected `line` = the 1-based line, counted over the **full written file text including frontmatter**, of the first line of the top-level block that contains the failing token. Expected `tab` = the second tab's title.
  - Replace the current `tabDirectiveLines` helper in tests/engine/test_import_errors.hpp, which counts after the frontmatter.
  - V010-shaped oracle: the original's reported 24, cited by receipt pointer.
  - Add a multi-line paragraph case with the image on its second line. The expected line is the paragraph's first line.
- **R-P2-2:**
  - Vary the frontmatter length through the **seed**, not by editing an export, because import.hpp:718–729 refuses guard and variable edits and a title edit triggers a rename. For example, add named-style declarations so the export carries more frontmatter lines.
  - Also vary earlier-tab body length, with and without declarations.
  - In every variant, the reported line must equal the full-file scan.
- **C-P2-b, mutants that must fail:**
  - (i) the body-relative line (absolute − `fmLines`); first assert `fmLines > 0`
  - (ii) the `::tab` directive line; first assert it differs from the block line
  - (iii) line ± 1
  - (iv) the first tab's title
  - C-P2-a and C-P2-c are unchanged.
- **R-P2-3:** a tab-title FileProblem, plus at least one comment or suggestion-header problem, must report the file-absolute line from the full-text scan. This shows the basis moved consistently. Any parse-derived line assertion the full suite exposes is updated only with original evidence; expectations are never loosened.

### Ownership, sequencing and gates (proposed; the lead decides)
- Add to 10-A's scope: src/lens/document.hpp, src/lens/parse.hpp, src/lens/tokens.hpp, src/lens/atom_put.hpp, and src/lens/put.hpp. put.hpp is 7-A's file, so it is added only after an accepted 7-R, with the exact hunk named.
- Phase A, can start now: the basis change, line table, flag, import mapping and tests.
- Phase B, after 7-R: the put.hpp attachment.
- The tree after phase A is an interim state only. Image errors then report `section.line`, not the original's block line, so it must not be reviewed or accepted.
- 10-AR reviews both phases together: red-before-green output, every mutant reached and failing, existing suites and lint per the refined brief, and original evidence for one variant without declarations and one with an opener (add these 6-B rows). Then a fresh V010 raw rerun in 13-A.
- The authored and refined brief for 10-A must be re-refined for this scope.

### Open limits
- The original reports every failing block in a tab; the target stops at the first.
- The original's line basis when the composed input has a `::section` opener or no declarations is not verified. If the original has a quirk there, record it as a packet; don't copy or "fix" it silently.
- Other content-code putback errors also gain block lines; this goes beyond the image cases, so 10-AR must review it.
- Nothing was run; 24 is the value 10-AR reported.

## §2 P1, rev-2 addendum (supersedes §2.3–§2.6 where they differ)

Trigger: 10-B evidence on the accepted 7-A2/7-R tree, with all 7-A file hashes matching. R-P1-2 passes. R-P1-1 fails. R-P1-3 places the cause in stage 3: `markdownPut` with `carryReview` gives the emptied paragraph a new key (o1-t-0 → n1). Reconcile then treats it as a new paragraph and fully restyles it. Stages 1 and 2 keep the key. P2 is unchanged.

### Facts (bounded reads)
- session.hpp:761–762 passes `carryReview = suggestionRuleApply`.
- `suggestionRuleApply` (suggestion_rule.hpp:413–437) only classifies and refuses. It creates no keys.
- put.hpp creates keys only through `create(...)`, `containerNormalize` (when the block structure changed), `keys.next` for lists, sections and ranges, and the empty-tab fallback `paragraphEmptyCreate` after the callback.
- In `put_detail::containerPut`, a kept paragraph whose visible projection is empty is skipped when building the old side of the diff (the `visible.blocks.empty()` `continue`). Fully inserted paragraphs are also skipped there, which is 7-A's hidden-insertion rule. The new side comes from the parsed tab blocks, after session.hpp has removed the rejected review markers.
- Inference, not traced: the V008 paragraph is empty after the rejection, so it is skipped on the old side. If the parsed tab still has a block for it, the diff sees that block as an insertion, `create()` gives it a new key, and the leftover empty block triggers `containerNormalize`. This fits n1 and fits R-P1-2 passing, because paragraphs that still have visible text are paired normally.

### Decision 1: the put.hpp seam
1. **Trace first, as 10-B's first logged step.** Record which key-minting call produced n1: `create()` in an insertion or replacement hunk, `containerNormalize`, or the `paragraphEmptyCreate` fallback. Record the hunk (a/b ranges) and the skipped block indices. The repair below is authorized **only** if the trace shows `create()` handling an inserted new-side block with no symbols, where a skipped kept paragraph sits at the same visible position. For any other site, stop and return the trace for another bounded addendum.
2. **Repair, inside `put_detail::containerPut` only, gated on `state.carryReview`:**
   - When an insertion hunk inserts a paragraph-type block with no symbols, look in the model range that hunk replaces for a skipped kept paragraph. It must be visibly empty, not fully inserted, and not protected.
   - If there is one, reuse it instead of calling `create()`. Run `paragraphPut` on that block against an empty old-side block, which keeps its key, `style` and `newline.style`.
   - Match one-for-one, in order. Never reuse a block across hunks.
   - All other paths stay as they are.
3. **Not changed:**
   - Strict standalone `markdownPut`: the flag is off, so code and output are identical.
   - The skip rule for fully inserted paragraphs (7-A's hidden insertions).
   - The `carryReview` callback.
   - session.hpp and suggestion_rule.hpp.
   - The reconcile files. Once the key survives, reconcile's full restyle of a genuinely new paragraph is correct behavior.
   - No request filtering.
- Rejected: always adding a visibly empty kept paragraph to the old side. That would turn the cases where the parsed tab has no matching block into delete hunks, then tombstones and new keys, and it would change carry-mode behavior beyond the V008 shape. Also rejected: fixing this in reconcile, which would mask the identity loss instead of fixing it.

### Decision 2: 10-B ownership (the lead appends this to the contract)
- **Writes:**
  - src/lens/put.hpp: the `containerPut` seam and a helper in `put_detail` if needed.
  - tests/lens/test_put.hpp: put-level unit cases. 7-A owned this file, and the lead must confirm the handoff now that 7-R is accepted.
  - tests/engine/test_import_review_writes.hpp: R-P1-1, R-P1-2 and R-P1-3 plus the controls.
  - One tests/test_main.cpp include line, under its lease.
- **Removed from scope:** src/reconcile/paragraph.hpp, container.hpp and tests/reconcile/test_paragraph.hpp. If R-P1-1 still sends style requests after the key fix, stop and escalate; don't reach into reconcile.
- **Dependencies:** add accepted 7-R as a prerequisite. If 7-A files change after 10-B's characterization, recheck the hashes before the check step.

### Decision 3: controls (each must be reached and must fail)
- **C-P1-a, injected unrelated reset (unchanged):** a test-only `batchUpdate` wrapper appends a direction reset, or a newline bold reset. The request assertion and the stored-style assertion must both fail.
- **C-P1-b, storage-only fault (unchanged):** change the stored emulator paragraph style or newline style after a clean import. The storage assertion alone must fail.
- **C-P1-c, review payload (unchanged):** remove the synthetic comment or reply in a test copy. The payload assertion must fail.
- **C-P1-d, key identity (new):**
  - The put-level case asserts the emptied paragraph keeps its original key.
  - Mutant: give a test copy of the candidate a new key. The key assertion must fail.
  - Feeding that re-keyed copy to reconcile must reproduce the extra style requests. This ties the guard to the actual cause.
- **C-P1-e, no over-reuse (new):**
  - Under carry mode, add a **genuinely new** empty paragraph where no skipped kept paragraph sits. It must still get a new key.
  - A fully inserted (hidden) paragraph must never be reused.
  - If Markdown can't express an added empty paragraph, record that and use a model-level test case instead.
- **C-P1-f, strict mode unchanged (new):**
  - Standalone `markdownPut` without `carryReview` on the R-P1-1 and R-P1-2 inputs must produce the same candidate model and request list before and after the change.
  - Capture the pre-change baseline on the unmodified tree first.

### Decision 4: 10-BR review requirements (L2, high)
- **Trace and red-before-green:**
  - the key-minting trace from step 1 and its exact site
  - R-P1-1 red output before the change
- **After the change:**
  - R-P1-1 sends exactly one `deleteContentRange`, with the range from the original receipt (indices 1–12 for this seed)
  - stored paragraph and newline style equal the pre-import state
  - R-P1-2 still passes
- **Controls and diff hygiene:**
  - C-P1-a through C-P1-f each reached and failing
  - the diff touches only `containerPut`'s carry-gated path
  - no request filtering
  - reconcile, session.hpp and suggestion_rule.hpp unchanged
- **7-A invariants:** all accepted 7-A2/7-R test cases (tests/lens/test_put.hpp, tests/engine/test_import.hpp) pass with no changes to their expectations.
- **Suites:** the full test suite and lint, using the commands fixed by the refined brief.
- **Original-side evidence:** either the 6-B row for the nondefault variant, or an explicit lead decision on the target-only invariant (carried over from rev 1).
- **Then:** a fresh 13-A raw rerun for G084–G091. They stay RED until then.
- 10-B's brief must be re-refined for this scope before the source edit.

### Open limits
- The mechanism is inferred; step 1's trace confirms it or sends the work back.
- Rejecting a suggested paragraph break (join path) stays out of scope.
- No runtime evidence was produced here.

## §2 P1, rev-3 addendum (supersedes rev-2 Decision 1; rev-2 ownership amended below)

### Basis
- 10-B trace (accepted 7-A2/7-R tree): in V008, `o1-t-0` is emptied by rejecting the insertion. It still projects 1 visible block (a style-only directive line). The parsed desired Markdown has 0 blocks.
- So `put_detail::containerPut` gets a pure deletion hunk (a=0..1, b=0..0). The unpaired-old branch deletes and tombstones `o1-t-0`, which empties the container. The structural check then calls `model::containerNormalize`, which mints `n1`. Reconcile treats `n1` as new and restyles it. `create()` and `paragraphEmptyCreate` are never called.
- Node (`../gdocsmith-node/src/core/lens/put.ts:195-275`, `model/invariants.ts:43-104`):
  - An empty NORMAL_TEXT paragraph with no bullet (`blockInvisibleIs`) is not a projected block. It is "owned" by the previous block (or by `leadingOwned`).
  - `containerPut` deletes owned invisible paragraphs only together with their owner. It deletes `leadingOwned` blanks only when content is created into an empty projection.
  - In V008, p0 and p1 are both empty, so there are no edits, the key is kept, and the only request is `deleteContentRange`.
  - Node's `containerNormalize` has the same mint rule as C++. The difference is that Node never deletes the paragraph in the first place.

### Decision 1: seam (carry-gated, keyed to rejection)
1. **`src/engine/session.hpp` `writeSegment` (~731–788)**
   - Just before `suggestionRejectsApply`, record the keys of paragraphs that have ≥1 inline.
   - After rejection, keep the set `rejectionEmptied`: keys that are still paragraphs and now have 0 inlines.
   - Pass the set into `markdownPut` through a new `PutOptions` field.
   - Make no other session change.
   - This supersedes the rev-1 verdict that P1 needs no `session.hpp` change. `put.hpp` alone cannot tell "emptied by this rejection" apart from "empty and styled before the write".
2. **`src/lens/put.hpp`**
   - Add `PutOptions::rejectionEmptied` (a set of keys, empty by default) and copy it into `PutState`.
   - In `containerPut`, touch only the unpaired-old (deletion) branch. Keep the block in place (no `deletable()`, no tombstone, key and style untouched) when all of these hold:
     - `state.carryReview` is set;
     - the block is a paragraph whose key is in `state.rejectionEmptied`;
     - it is not `is_protected`;
     - deleting it would leave the container with no paragraph (the sole-paragraph / V008 shape).
   - The structural check then sees no change in size or key order, so `containerNormalize` mints nothing.
   - The `markdownPut` empty fallback (~955) is not reached because the candidate is non-empty.
   - Do not change paired, insert, `create()`, `paragraphPut`, `markdownPut` short-circuit, or reconcile code.
3. **Out of scope for v1**
   - Rejection-emptied paragraphs that are not alone in their container. Node keeps those as owned invisibles, but current C++ deletion of them does not re-key. Leave their behavior unchanged; this is a residual limit.
   - Any change to `project.hpp` (why C++ projects a style-only directive where Node treats the paragraph as invisible).

### Ownership (amends rev-2)
- 10-B owns:
  - `src/lens/put.hpp`
  - `src/engine/session.hpp` (hunk limited to `writeSegment`)
  - `tests/lens/test_put.hpp`
  - `tests/engine/test_import_review_writes.hpp`
  - one `test_main.cpp` include (lease order 11-A→10-A→10-C→10-B)
- Prerequisites: accepted 7-A2 and 7-R. The lead must confirm the 7-A→10-B handoff for `session.hpp` and `test_put.hpp`.
- Reconcile files are still excluded.
- Conflict: 10-A Phase B also edits `put.hpp` (P2 line attachment). The lead must serialize 10-B and 10-A Phase B on a `put.hpp` lease. Neither may start while the other holds it.

### Decision 2: controls (C-P1-a/b/c unchanged; d/e/f revised; g added)
- **C-P1-d (key identity):** after the V008-shape write, `o1-t-0` still exists, no `n*` key is minted, and the requests are exactly one `deleteContentRange`. Mutant: remove the keep condition, which must bring back `n1` plus the restyle requests and fail the test.
- **C-P1-e (no over-keep):**
  - (i) A carry write where the user deletes a paragraph that was *not* emptied by rejection still deletes and tombstones it.
  - (ii) A rejection-emptied paragraph that is not alone in its container behaves exactly as at baseline.
  - (iii) When desired Markdown puts content where the emptied paragraph was, the paired/insert path is unchanged.
  - (iv) A paragraph that was already empty before rejection is never in the set.
- **C-P1-f (strict invariance):** with the default empty set or no `carryReview`, strict `markdownPut` gives byte-identical results to baseline, including delete-all-content of a tab (deletion plus `containerNormalize` mint). All 7-A2/7-R tests pass without edits.
- **C-P1-g (session gate):** a unit check that `rejectionEmptied` holds exactly the paragraphs that went from ≥1 inline to 0 inlines in this write's rejection. Mutant: compute the set after rejection only, which must fail the check.

### 10-BR review gate (adds to rev-2)
- The trace record (deletion hunk shape; `containerNormalize` mint before the fix, none after) is attached.
- The `session.hpp` diff is limited to `writeSegment`. The `put.hpp` diff is limited to the new option, the state copy and the unpaired-deletion branch. Reconcile is untouched.
- R-P1-1 sends exactly one `deleteContentRange`. R-P1-2 still passes. C-P1-d/e/f/g pass, and the mutants are killed.
- No 7-A2/7-R test is weakened or edited. The `put.hpp` lease is serialized with 10-A Phase B.
- The 13-A raw rerun of G084–G091 is required for acceptance.

### Unresolved limits
- C++ projects a style-only directive for the emptied paragraph, while Node's `blockInvisibleIs` hides it. As a non-blocking first step, 10-B records which declaration field causes this. Any divergence is filed as a separate parity finding, not fixed here.
- Non-sole rejection-emptied paragraphs keep current C++ behavior. Revisit only if the 13-A rerun shows remaining G084–G091 differences.
