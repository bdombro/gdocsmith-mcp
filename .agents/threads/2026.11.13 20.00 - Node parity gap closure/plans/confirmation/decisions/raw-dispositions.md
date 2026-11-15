# 10-D2 decision: stored-font characterization (P3/T6) and native envelope (T2)

Decider: raw-disposition-decider (claude-opus-5.5/high, as assigned). Task 10-D2, contract rev 1. Read-only native return for `decisions/raw-dispositions.md`.

Outcome: **decided** for what to characterize and how to classify each question. **Human required** for Q-L1 to Q-L4 (equivalence choices the product owner makes). No waiver, no normalization, no new equivalence, no implementation authority.

## 0. Authority and sources

- **Loaded explicitly:**
  - repository `AGENTS.md`, in full
  - swarm `roles/common.md` and `roles/decider.md`
  - `plans/confirmation/rules.md`
  - `tasks/10-D2.md` contract rev 1
  - `map.md`
  - `decisions/concurrency-plan.md` (D5)
  - `assessments/raw-differences.md` (T2, T6, P3), plus `tasks/10-C.md` rev 1 as the consumer of this decision
- **Inherited instructions:** none claimed; anything not listed above is unverified.
- **Helpers:** none. The assignment forbids nested helpers, and the work only needed bounded checks of a few seams.
- **What I did:** static read-only spot checks (listed in §4). No build, test, runtime, network, Google or auth action. No audit of the 206 groups was repeated.
- **Evidence vs product boundary:**
  - Technical (decided here): which observations are needed, which checks are fault-sensitive, and how each difference is classified.
  - Product (lead/user): whether any difference that does not change meaning is acceptable as parity. Any such acceptance is a product equivalence, not a technical finding.

## 1. P3/T6: stored-font characterization recipe for 10-C

**Disposition of T6 (G041–G061, V005/V006):** unresolved. G060 (the 22pt run stored explicitly in the original but missing on the target) is a durable-formatting defect candidate until 10-C has observed it. The matching 11pt match / 22pt mismatch verdicts do not show the two engines store the same provider state.

### 1.1 Fixed inputs (keep as is)

- Same vectors as the original `import.test.ts:115–149`:
  - seed run style is explicit `fontSize` 11 or 22 PT plus `weightedFontFamily` Arial/400
  - edit "Hello world" → "Hello revised world"
- The readback-omission fault stays exactly as it is: `fontSize` and `weightedFontFamily` are deleted from text runs in the readback copy only, after a write.
- These existing assertions stay unchanged:
  - 11pt: `errors` absent, `verification` "match", `status` "applied"
  - 22pt: `verification` "mismatch", `status` "failed", summary prefix "Every write landed, but the readback did not match the file"
- The 11pt and 22pt outcomes are separate test cases and must never be merged.

### 1.2 Observation point (the discriminating requirement)

Read stored character and newline style from authoritative backend storage, not from `getDocument` output. Take it after the batchUpdate is applied and before any readback transform runs.

- The target adapter already separates the transformed readback copy (`tests/parity/cpp-import-adapter.cpp:146–163`) from backend storage (`:422–425`, per T6).
- The 10-C test header must use the equivalent storage accessor of the in-memory emulator/backend. The refined brief names the exact accessor; 10-C must not invent one.

### 1.3 Observations, split into "assert" and "record only"

| ID | Observation | Treatment |
|---|---|---|
| O1 | Exact ordered outgoing requests: `updateTextStyle` range, `fields` mask, `textStyle` body | **Record only.** Pinning the target's current requests would hide G041/G042. |
| O2 | Per stored character index over the whole edited paragraph, including the trailing newline: explicit `fontSize` and `weightedFontFamily`, present/absent plus value | **Record only** for inserted characters (explicit vs cleared is Q-L1) |
| O3 | Per index: effective style. Rule: the run's explicit field, else the paragraph's `namedStyleType` text style in the tab's stored `namedStyles`, else the stored NORMAL_TEXT text style. Record which tier supplied each value; if no tier has one, record "unresolved" and do not assume 11pt. | **Assert** (A1–A3) |
| O4 | Run segmentation: (start, end, explicit style) | **Record only.** Classified separately from character semantics (G043–G056). |
| O5 | Inheritance probe: after the write, change only NORMAL_TEXT `fontSize` in stored `namedStyles`, then re-resolve effective size for inserted and surviving characters | **Record only.** Tells explicit-pinned text apart from inherited text, which is the real meaning gap behind Q-L1. |

### 1.4 Source-grounded assertions (fault-sensitive, product-neutral)

- **A1.** Every character of "Hello revised world" (inserted and surviving) has effective `fontSize` equal to the seed size and effective family Arial/400. Checked separately for 11 and for 22.
- **A2.** The paragraph newline's explicit and effective style after the write equals its stored state before the write.
- **A3.** Surviving characters ("Hello " and " world") have the same effective style before and after the write.
- **A4 (separation).** For 22pt, one `getDocument` call that applies the omission transform leaves the O2/O3 storage observation unchanged. This shows the readback fault and the storage check are independent.

### 1.5 Negative controls

Each control is injected into stored state after the batchUpdate and before the storage observation. Each must be shown to reach its assertion: it fails with the fault and passes without it.

- **N1.** Clear explicit 22pt from one surviving character. A1 or A3 must fail, even though the existing 22pt status check still passes.
- **N2.** Style the newline (e.g. explicit 22pt or bold in the 11pt case). A2 must fail.
- **N3.** The readback-only omission is the existing fault. It must keep 22pt as "mismatch" and must not trip A1–A3.
- **Known limit:** clearing an explicit 11pt is deliberately not a failing control for A1. At 11pt, cleared and explicit give the same effective value now, and only O5 tells them apart. That is exactly Q-L1, so it is not settled by a test.

### 1.6 If a source-grounded assertion fails on the current tree

10-C must not weaken, pin, skip, mark as expected-failure, or repair. It returns **blocked** with the failing assertion and the O1–O5 observations, sanitized.

Whether a red include stays in the shared `tests/test_main.cpp` test binary or is withdrawn until a repair task is selected is a lead process decision (Q-L4). The one-line include lease rules in D5 still apply.

### 1.7 Original-side counterpart

10-C runs on the target only. The original's stored state is known only from the historical receipt (G060). For a fair comparison, the lead should ask the 14-F author (not this decider) to add an original-side row for the same vectors to 6-B. That row would capture stored explicit/effective style per character before the readback transform.

### 1.8 Decision paths after 10-C

- **A1–A3 fail on the target:** demonstrated defect. Propose a repair packet at the `src/reconcile/paragraph.hpp:153–175,537–552` emission seam (owner and brief to be assigned), with independent review and fresh affected gates.
- **A1–A3 pass, only O1/O2/O4/O5 differ:** the evidence supports "same effective style at characterization depth" only. O5 showing explicit-pinned vs inherited text is a durable difference that would show up under later named-style edits. Accepting it is Q-L1.

## 2. T2: native envelope (G007, G008, G009, V001)

The three questions below are separate. Answering one does not settle another.

### 2.1 apiUser omission: a classified technical mismatch

**Facts:**
- The original writes `apiUser` (`../gdocsmith-node/src/core/engine/export.ts:199–213`; `lens/document.ts:520`).
- The target's own docs promise it:
  - `docs/mechanics.md:47` says the frontmatter `apiUser` is the authenticated user's email
  - `docs/markdown.md:75` lists it as a read-only key that export regenerates
- The target parser accepts the key (`src/lens/frontmatter.hpp:161`).
- Target import requires the exported value to be preserved exactly (`src/engine/import.hpp:727–730`). With the key absent on both sides, that check passes trivially today.
- `FileServices::apiUser` exists (`src/engine/export.hpp:61`), but `exportDocumentFile` never sets the frontmatter field (`export.hpp:312–318`).
- An identity source exists (`src/api.hpp:1117–1123`).

**Disposition:** observable omission and a docs/behavior conflict. It is not representation noise and not an equivalence candidate.

**Product-owned direction (Q-L2):**
- (a) Restore `apiUser` from the authenticated identity. This adds an identity read, which changes the T1 call order and must not be normalized away. It also writes personal data into the file.
- (b) Formally drop `apiUser` and amend the README and linked docs in the same change, as `AGENTS.md` "Documentation Upkeep" requires.

**Technical requirements either way:**
- Tests and artifacts use a synthetic placeholder identity. Real values are never recorded.
- If (a): exact preservation on import is retained, and a check proves the no-op import does not refuse.

### 2.2 Native declarations: semantic characterization needed before any equivalence

**Facts:**
- **Original:**
  - `namedStyleSets` keys are "1", "2", …, and identical sets are de-duplicated (`lens/document.ts:426–439`)
  - each declaration is only the difference from built-in defaults, in `NAMED_STYLE_TYPES` order (`lens/namedStyles.ts:34–45`)
  - a property dropped from the declaration on edit returns to its default (`namedStyles.ts:57–59`)
- **Target:**
  - one set per tab, keyed `ns<n>`, built from `paragraphDeclarationExport` / `textStyleExport` (`src/engine/export.hpp:355–375`)
  - I saw no step comparable to the original's default-diffing at the seam I read. Which fields actually appear in the receipt is not proven here.
- **Capability difference:** `AGENTS.md` safety rule 6 says target style-declaration changes are unsupported and are refused before any send. The original applies them.
- **cellStyles:** in principle the question applies. The original omits an empty map (`lens/document.ts:527`); the target serializes per entry (`src/lens/document.hpp:519`). T2 reports no cellStyles difference for V001, so V001 is neither evidence nor equivalence for tables. Any cellStyles raw groups (e.g. the V007 cell-style vector) go to 10-T2/10-D3. Nothing is inferred from V001.

**Disposition:** unresolved native representation. Parsing success, recorded as `rawFileInterchange`, shows only that the file is accepted.

**Proposed characterization packet P4** (conditional, test-only, executor L0 with a separate brief and refinement; not part of 10-C):
1. A no-op import of each engine's own native file produces zero named-style or cell-style writes and no refusal.
2. Each engine's native file is fed to the other engine; record outcome and writes.
3. One declaration property is edited and one is removed. Record the original's writes and the target's refusal-before-send.
4. Negative control: changing one declaration value must not be reported as a no-op.

The capability difference in step 3 is product scope (Q-L3), not a defect this decider can decide.

### 2.3 Exact bytes vs meaning

**Decision:**
- The raw byte comparison stays RED as recorded: 533 vs 440 bytes, different SHA-256.
- No trimming, key renaming, default stripping, version translation or other byte normalization is authorized.
- Self-consistent line navigation (T3/V008) stays its own check per native file.

**Product-owned criterion (Q-L1/Q-L2/Q-L3):** choose one.
- **A. Exact bytes:** the target adopts the original's naming, delta form, order and keys. Large change, but reversible.
- **B. A listed contract of fields and meaning:** apiUser presence, P4 results, navigation consistency, no-op stability. Only if the lead adopts it explicitly, and only for the listed fields.
- **C. Accept documented divergence:** a user-level waiver. Not granted here.

Technical recommendation: B is the smallest criterion that stays fault-sensitive, but only after the 2.1 direction is chosen and P4 has evidence. Until then G007–G009 remain unresolved.

## 3. Human-required questions for the lead

- **Q-L1.** Is effective-style sameness now (A1–A3 pass) enough for V005/V006 parity, or must explicit vs inherited structure match? The 11pt cleared vs explicit case and the extra 22pt `weightedFontFamily` behave differently under later named-style edits (O5).
- **Q-L2.** Restore `apiUser` (an extra identity read, personal data in the file) or drop it and amend the docs?
- **Q-L3.** Is the refusal of named-style/cell-style declaration edits an accepted capability narrowing compared with the original? And which native-file criterion (A, B or C) applies?
- **Q-L4.** If a source-grounded 10-C assertion fails, does the include stay registered as red, or is it withdrawn pending a selected repair?

## 4. Spot checks behind this decision (static)

- Original `import.test.ts:115–149`: confirms the two vectors, the readback-only deletion and the three-part verdicts.
- Target `cpp-import-adapter.cpp:146–163`: the transform touches the JSON copy only.
- Target `export.hpp:61,312–375` and original `lens/document.ts:405–445,520–528` plus `namedStyles.ts:34–59`: confirm 2.1 and 2.2.
- Target `frontmatter.hpp:161`, `import.hpp:727–730`, `api.hpp:1117–1123`, `docs/mechanics.md:47` and `docs/markdown.md:75`: confirm the docs/behavior conflict.

## 5. Effects on tasks and map (proposals for the lead)

- **10-C:** release on this recipe (§1). It stays characterization only: one new header plus one include under its lease, no production edits.
- **New conditional P4 envelope characterization:** needs author/refiner gates; selected after Q-L2/Q-L3.
- **6-B / 14-F:** proposal to add an original-side stored-style row (§1.7).
- **Possible repair packets:** a font repair if A1–A3 fail, and an `apiUser` repair or docs change per Q-L2. Each needs an owner, refined brief, independent review and fresh gates.
- **13-A/13-R:** T2/T6 stay listed as pending. Raw stays RED.

## 6. Remaining limits

- Static only. G060 has no fresh stored-state observation on either engine.
- The effective-style rule's emulator accessor and how the emulator fills `namedStyles` are unverified.
- Which declaration fields appear in the V001 receipt was not re-read (no duplicate audit).
- cellStyles has no evidence from V001.

---

**Summary**
- **What I decided:** 10-C gets the §1 recipe. It reads stored per-character and newline style from backend storage before any readback transform. A1–A4 are asserted; requests, explicit fields, run segmentation and an inheritance probe (O5) are recorded only. Controls N1–N3 cover a cleared surviving 22pt, a styled newline, and the readback fault alone. The 11pt and 22pt outcomes stay separate.
- **T2:**
  - `apiUser` is an omission that contradicts the target's own docs, not representation noise.
  - Named-style declarations need P4 characterization, including the target's refusal of declaration edits.
  - The bytes stay RED with nothing normalized.
- **Instructions:** every source listed in the brief was loaded explicitly, plus `tasks/10-C.md`. None was missing.
- **Open for the lead:** Q-L1 to Q-L4. G060 has no fresh evidence, the emulator storage accessor is unverified, and nothing about cellStyles can be concluded from V001.

10-D2 status=review rev=1 file=.agents/threads/2026.11.13 20.00 - Node parity gap closure/plans/confirmation/tasks/10-D2.md