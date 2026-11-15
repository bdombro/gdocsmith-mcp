# 10-D3 decision: raw follow-ups (group accounting refinement and follow-up packets)

Decider: raw-followup-decider (claude-opus-5.5/high, as assigned). Task 10-D3, contract rev 1. Read-only native return for `decisions/raw-followups.md`.

**Outcome:** **decided** on the accounting, the routing of every family, and new packet scopes and checks. **Human required** for Q-L5 to Q-L8 (§5). **Blocked report** for one content-exclusion denial (§0). This decision grants no waiver, no normalization, no new equivalence and no implementation authority. Raw stays RED.

## 0. Authority, sources and blocker

- **Explicitly loaded, in full:**
  - repository `AGENTS.md`
  - swarm `roles/common.md` and `roles/decider.md`
  - `plans/confirmation/rules.md`
  - `tasks/10-D3.md` rev 1, plus `tasks/10-T1.md` and `tasks/10-T2.md` for their acceptance notes
  - `map.md`
  - `decisions/concurrency-plan.md` (D5), `decisions/raw-repair-recipes.md` (10-D1), `decisions/raw-dispositions.md` (10-D2)
  - `assessments/raw-group-index.md` and `assessments/raw-differences.md`
  - `assessments/raw-c3-c7-triage.md`: SHA-256 re-hashed, equals c29c8c71d5aa13df6ffd19d698a0797cfddd120bad2e4ed212d88a48f77b27d2
  - `assessments/raw-c1-c2-clusters.md`: SHA-256 re-hashed, equals 9eea3d550588ee1ef30680bd916a0341403a409ca833bbbd796bf4e29ce50ec6
  - Nothing is claimed as inherited automatically.
- **Receipt R** = `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/resumed-final-recheck`.
  - `differential-results-raw-diffs.json` re-hashed: 8291aac1269b94683c6ca0cea88ccf9cbdbbdf422e5072209ee48b42ae4162b6, matches the index.
  - One read-only projection of path, case IDs, value kind and per-class counts, with no values copied. It confirmed 206 groups and 250 leaves, per-class 101/124, 27/31, 35/38, 3/3, 9/10, 26/39, 5/5, and `totalLeafDifferences` = 250.
- **Blocker (content exclusion):** the next read-only projection of the thread directory was denied by content policy. Per rules.md I did not retry or switch access route. Effects:
  - the "absent-marker" shape check in §2.3 was not done
  - no further bus intent rows or pointer send after that point
  - the lead must confirm thread-directory access before dispatching any packet below that reads R
  - the decision itself uses only evidence loaded before the denial
- **Helpers:** none. The assignment forbids nested helpers. The legwork is delegated as packets 10-T3 to 10-T5 below.
- **Not done:** no source edits, builds, tests, runtime, network, Google/auth or installs. `messages.md` was not accessed.

## 1. Refined group accounting (decided)

### 1.1 Whole-receipt reconciliation, traced vs untraced

"Traced" means one of raw-differences T1–T6 analyzes the group in substance, or C4's existing authority covers it. Being traced is **not** a disposition.

| Class | Traced or addressed (groups/leaves) | Untraced (groups/leaves) | Total |
|---|---|---|---|
| C1 | T1: G001–G006 → 6/22 | 95/102 | 101/124 |
| C2 | T2: G007, G009 → 2/5 | 25/26 | 27/31 |
| C3 | G008, G092, G125, G130–G133, G169, G171–G173, G175–G176 → 13/14 | 22/24 | 35/38 |
| C4 | existing layout authority G023, G032–G033 → 3/3 | 0/0 | 3/3 |
| C5 | T4/T5/T6 → 9/10 | 0/0 | 9/10 |
| C6 | T4/T5/T6 → 26/39 | 0/0 | 26/39 |
| C7 | G174 → 1/1 | 4/4 | 5/5 |
| **Total** | **60/94** | **146/156** | **206/250** |

Leaves are summed from the case lists in the group index. G001 has 10 cases and G002 has 8.

### 1.2 10-T1 reconciliation (7 vs 8 families, 26 vs 28)

- **26 vs 28:** 26 is the **group** count of untraced C3/C7 (22 + 4). The **leaf-occurrence** count is **28**, because G123 and G124 each carry V009 and V010. The saved line "40 = 14 + 26" is a group identity. The leaf identity is 43 = 15 + 28 (G008 carries two leaves).
- **7 vs 8:** the saved artifact defines exactly **seven families (F1–F7)**, and they cover all 26 groups once. The "8" belongs to F1, which has 7 groups and 8 leaf occurrences across 8 executions: V003, V004, V005, V007, V008, V009, V010 and V009K. That is 7 case IDs plus the V009K variant.
  - Q1's phrase "all 7 variants" is corrected to "7 groups / 8 executions (7 case IDs + V009K)".
  - Neither number creates an eighth family.
- **Per-family leaves:** F1 7/8, F2 1/1, F3 5/5, F4 4/4, F5 2/3, F6 2/2, F7 5/5. Total 26/28.
- **Traced groups with no packet**, attached here as cross-references only (primary accounting stays "traced", nothing is double counted):
  - G171 and G175 (`…errors.0.phase`, V010) → F3. P2 explicitly left phase out.
  - G174 (C7 message, V010) → F4.
  - G092 (`result.commentsLine`) → F2. T3 called it "related but distinct" and confirmed it as traced, but gave it no packet.
  - G125, G130–G133 (V009 IDs/URIs) → F7. T5 left the provider-ID differences unresolved.
  - G124's mention as an example in raw-differences item 3 is not a trace. It stays untraced in F5. 10-T1 was right.

### 1.3 10-T2 reconciliation (115 + 26 vs 128; K1 78 vs 80)

- **Leaf math:** subtracting 9 for G001–G006 was wrong. 9 is V001's whole per-execution total, which also includes G007, G008 and G009. G001–G006 carry **22** leaves (G001 = 10, G002 = 8, G003–G006 = 4).
  - Untraced C1 = 124 − 22 = **102** leaves.
  - Untraced C2 = 31 − 5 = **26** (correct as saved).
  - In-scope total **102 + 26 = 128**. The saved total was right, but its C1 term was not.
- **K1 label:** the table's list and the "corrected" list name the **same 78 groups**. "80" was a miscount. The G035/G057 "correction" changed no membership: G035 was never in K1's list and G057 already was.
  - Verified disjoint coverage: K1 78 + K2 13 + K3 4 = 95, exactly the untraced C1 set.
  - Leaves: K1 84 (G027, G028 carry 3 each; G057 and G069 carry 2 each), K2 13, K3 5 (G035 carries 2). Total 102.
  - C2-A 3 groups / 3 leaves and C2-B 22 / 23 (G134 carries 2). Total 25 / 26.
- **Group ranges that are named but not actually traced:**
  - T6's heading range "G041–G061" includes C1 G057 and C2 G058–G059.
  - T5's "G125, G130–G143" includes C2 G134.
  - Neither trace analyzes those groups, so they **stay untraced under 10-T2's clusters** (G057 in K1; G058, G059 and G134 in C2-B), with cross-references to T6/T5. 10-T2's line "T3–T6 groups are C3/C5/C6" is corrected to say this.
- **K3 is a write payload under a C1 label.** The R projection shows:
  - G034 and G035 are `callLog.4.args.1.1.updateTextStyle.textStyle.{fontSize, weightedFontFamily}` in V005/V006, the request side of T6/G041–G042.
  - G076 and G077 are `callLog.9.args.1.{1,2}` in V008, the request side of T4.
  - Their class labels stay as recorded.
- **Partial trace of G001/G002:** T1 analyzed only V001's call log. The other 16 leaves share path and values, so they are traced by identity only. FU-1 must project all executions.

## 2. Follow-up decisions by family

### 2.1 FU-1: T1 read schedule and adapter alignment (G001–G006, K1, K2, K4, K5)

Decided:
1. **No schedule normalization of the comparison.** An alignment projection is evidence only. The metadata guard (`src/engine/import.hpp:966–985`) must not be removed to match the original's schedule (raw-differences T1; AGENTS.md safety rules 2 and 7).
2. **Static alignment projection** goes to new **10-T3** (§3).
   - Over all 11 executions, project method sequences and classify each K1/K2 group and G001–G006 as one of:
     - explained by an inserted target `getFileMetadata`
     - explained by the adapter-only Node `commentsList`
     - **residual**
   - Same output also covers:
     - the V009K call log, which was never opened, so 10-T2's V009K placement is unverified
     - K5: the **role** of the document read at each early V010 read (copy source vs destination), recorded as roles, never ID values
     - the F7 receipt-ordinal ID mapping in V009K
3. **K4, the final `textExport` missing on the target in V008, is triaged first.** A skipped verification read would be a safety-rule-7 gap, not a scheduling detail.
   - 10-T3 statically reads the target's post-write verification/re-export path and whether Node's final `textExport` feeds hidden-insertion verification.
   - If the target lacks an authoritative readback the original performs, 10-T3 stops and escalates as a candidate defect to **10-D4**.
4. **Adapter boundary.** The target export adapter calls `exportModel` directly with seeded comments (`tests/parity/cpp-import-adapter.cpp:267–270`). The Node adapter calls the real `fileExport`.
   - 10-T3 first identifies statically the target's public export entry used by the `export` tool, and whether it reads comments through the Drive client.
   - **If it does:** the boundary fix is adapter-only, in conditional **9-H** (§3).
   - **If it does not:** the missing comments read is an engine difference. Route it to 10-D4. 9-H must not paper over it.
5. **Metadata-read obligation.** The failed or conflicting metadata read from T1 has never been exercised.
   - Assign to conditional **10-E**, a target-only characterization with no production edits.
   - Ask 14-F/14-FR for an original-side contained row, for 6-B, covering what the original does when the corresponding read fails or conflicts. That source behavior is unverified here and must not be assumed.
6. **Contract question.** Whether the target's extra identity/title read is an accepted schedule divergence is **Q-L8**. Until it is answered, all K1/K2 groups and G001–G006 stay unresolved.

### 2.2 FU-2: native files (C2-A, C2-B, F1, G092 file side)

Decided:
- 10-D2 §2 applies unchanged to **every** C2 group and every F1 group:
  - the bytes stay RED
  - no trim, rekey, default stripping or translation
  - the `apiUser` omission is classified under Q-L2
  - the declaration representation goes through P4 under Q-L3
- **P4 scope extension** (conditional, still needs author and refiner gates; proposal to the lead). Add per-fixture rows:
  - V007 `cellStyles` (G063, G065–G068 and F1 G064), checked against OB-027
  - V008 review frontmatter (G078, G080–G083 and F1 G079), including T3's note that target V008 frontmatter omits `commentsLine`/`suggestionsLine`
  - the V005 re-export shrink (G036, G038–G040): Node grows by 8 bytes, the target shrinks by 52
- **Static key-set projection goes to new 10-T4.**
  - For each execution: frontmatter key sets, named-style and cellStyles declaration key sets, and the per-file navigation check for F2 (G093, G092).
  - The navigation check asks whether each side's reported line lands on the referenced snippet or comment marker **in its own exported file**.
  - No line normalization. Integers are never compared across engines.

### 2.3 FU-3: added `phase` field (F3: G094, G127, G128, G199, G200; cross-reference G171, G175)

Decided:
- This is an **observable contract addition**, neither equivalence nor defect. P2 (10-A) must keep phase unchanged per 10-D1 §3.2, so this is not added to 10-A.
- New **10-T5** gathers the static evidence:
  - the original result builder: does it ever emit `phase`, and does its schema or any consumer reject or ignore extra fields?
  - the target emitters (e.g. `import.hpp:1529–1533`)
  - OB-029 and OB-036 dependencies
- **Unverified:** whether the receipt's original value is truly absent. My projection reported its JSON kind as an object, not undefined or null, which suggests a structured absence marker. The denial stopped the shape check, so 10-T5 must confirm it before claiming "absent".
- Acceptance of the field is **Q-L5**.

### 2.4 FU-4: wording vs detail loss (F4: G062, G126, G170, G198; cross-reference G174)

Decided:
- **Split each item:**
  - **Wording only**, which is C7 proper and human-owned: whatever remains after removing detail loss.
  - **Detail loss**, a diagnostic content omission candidate and not wording:
    - G062 drops the expected/actual values and the tab suffix
    - G170 drops the source directive token
    - G126 and G198 drop the OS error text
    - G174: 10-T5 must decide which half it falls in
- **No C7 item is accepted here.** Q-L6 decides.
- **Ownership:** 10-A's P2 asserts the *current* message text, so adding detail would conflict with an accepted recipe. No detail repair is merged into 10-A.
  - Any repair selected after Q-L6 becomes its own packet on `src/engine/import.hpp`.
  - It is serialized after 10-A and 10-AR release that file (D5 single-owner rule).
- **Privacy constraint:** any restored OS error text in tests or artifacts uses synthetic paths only and is sanitized.

### 2.5 FU-5: adapter observation schema (F5: G124, G196; F6: G129, G201)

Decided:
- These are adapter-boundary observation differences (raw-differences item 3, T5). They are neither engine defects nor equivalent, and the comparator is not normalized.
- **Fix route:** both adapters must observe the same backend facts: whether the temporary document exists after deletion, and structured deletion evidence instead of explanatory prose. Owner: conditional **9-H**, together with FU-1 item 4.
- Cleanup parity stays unclaimed until both sides record an equal actual-delete observation.

### 2.6 FU-6: created IDs and URIs (F7: G197, G202–G205; cross-reference G125, G130–G133)

Decided:
- Same status as T5. `createdIdBijections` stays `appliedToComparison=false`.
- 10-T3 records whether the receipt-ordinal mapping holds in V009K (structure only, no ID values).
- Using a bijection in comparison is a comparator authorization, which is **Q-L7**.

### 2.7 K3 (G034, G035, G076, G077)

Decided: **no new packet.**
- G034 and G035 join P3/10-C. They are already covered by 10-D2 O1 "record only".
- G076 and G077 join P1/10-B. R-P1-0 already compares the V008 request lists.
- Add all four to 13-A's fresh raw gate list.

## 3. New or changed packets: ownership and checks

All packets are proposals for the lead. None authorizes implementation.

**Brief and refinement gates:**
- 10-T3, 10-T4 and 10-T5 are read-only L0 assessments, like 10-T1 and 10-T2. They need no brief/refiner gate, but each needs a lead-issued contract.
- 9-H and 10-E need L0 authored briefs plus separate L1 refinements, per rules.md (e.g. 14-J/14-JR and 14-K/14-KR).
- Every packet that reads R needs thread-directory access restored or confirmed first (§0).

| ID | Status / level | Depends on | Unique writes | Scope and required checks |
|---|---|---|---|---|
| **10-T3** | F prep, L0 read-only, low | this decision; R access | `assessments/raw-schedule-alignment.md` (native return) | FU-1 items 2–4 plus the F7 mapping. **Checks:** <ul><li>R SHA-256 recorded</li><li>each of the 95 K-groups plus G001–G006 classified exactly once (explained-metadata / explained-adapter / residual)</li><li>in-memory negative control: one synthetic non-metadata call inserted into a copy of one projected sequence must show up as residual</li><li>no values or IDs copied</li><li>K4 escalation trigger as in §2.1 item 3</li></ul> |
| **10-T4** | F prep, L0 read-only, low | this decision; R access | `assessments/raw-native-projection.md` | FU-2 key sets for all 25 C2 and 7 F1 groups, plus F2 per-file navigation (G092, G093). **Checks:** <ul><li>per-group coverage</li><li>keys and lengths only, no content</li><li>in-memory control: shifting a reported line by one in a copy must fail the navigation predicate</li><li>the V005 shrink explained or recorded as unknown</li></ul> |
| **10-T5** | F prep, L0 read-only, low | this decision | `assessments/raw-diagnostic-contract.md` | FU-3 and FU-4 static reads of original and target result builders. Confirm the absence representation in R (§2.3). Classify each F4 item as wording vs detail loss, G174 included. **Checks:** every claim cites file:line; no wording judged acceptable. |
| **10-D4** | C, L2 high | 10-T3, 10-T4, 10-T5 | `decisions/raw-followups-2.md` | Settles residual schedule groups, the K4 outcome, the engine-vs-adapter comments read, and repair selection after Q-L5 to Q-L8. Same no-waiver rule. |
| **9-H** | C, L0 executor plus **9-HR** L2 review | 10-T3 shows the boundary is adapter-only; lead selection; exclusive lane-9 adapter lease, serialized after 9-B/9-C if selected (D5 lane-9 write conflict); BUILD for the adapter, outside canonical dirs | `tests/parity/cpp-import-adapter.cpp`, `tests/parity/node-import-adapter.mjs` (observation-schema and export-entry changes only) | Route the target export through its public export entry with backend-seeded comments. Make cleanup observations equal on both sides. **Checks:** <ul><li>exact ordered calls recorded before and after</li><li>no driver or comparator changes</li><li>control: a deliberately unequal observation field must still surface as a raw difference</li><li>fresh affected raw rerun only in 13-A</li></ul> |
| **10-E** | C, L0 executor plus **10-ER** L2 review | lead selection; brief 14-K/14-KR; `tests/test_main.cpp` lease after 10-B (order 11-A → 10-A → 10-C → 10-B → 10-E); BUILD | new `tests/engine/test_import_metadata_reads.hpp`, one include line | Target-only characterization of a failed or conflicting metadata read (identity/title). **Checks:** <ul><li>no production edits</li><li>red results recorded, never pinned or skipped; Q-L4 applies</li><li>ordered calls and no-write outcome asserted</li><li>controls: a fault-injected conflicting title must reach and fail the guard assertion</li><li>original-side row requested from 14-F for 6-B</li></ul> |
| **P4** (10-D2) | C, scope extended | Q-L2, Q-L3 | as 10-D2 | Adds the §2.2 per-fixture rows |
| **13-A** | gate list change | n/a | n/a | The fresh raw gate must list **all 206 groups** as RED unless dispositioned. It explicitly re-observes K3 (G034, G035, G076, G077) together with P1/P3, and the FU-1/FU-5 groups if 9-H is selected. |
| **14-F** | brief request | n/a | n/a | Add original-side rows for the metadata-read failure/conflict (10-E) and the V008 final `textExport` role (K4) |

**Scheduling:**
- 10-T3, 10-T4 and 10-T5 have disjoint outputs and can run concurrently within the four-agent limit. 10-T3 goes first because of the K4 safety trigger.
- None of them blocks 13-A. They stay on 13-R's pending list.

## 4. Rejected alternatives

- **Normalize away `getFileMetadata`/`commentsList` in the comparator.** Rejected: it manufactures parity (rules.md) and hides the K4/K5 ordering risks.
- **Fold F3 phase or F4 detail into 10-A.** Rejected: it contradicts 10-D1's accepted P2 assertions and widens 10-A without a gate.
- **Accept the C7 wording, F3 phase or F7 bijection as technically equivalent.** Rejected: these are product contract choices (§5).
- **Re-trace all 146 untraced groups by hand.** Rejected: cluster projections plus residual escalation are cheaper and fault-sensitive.

## 5. Human-required questions (lead/user)

Existing Q-L1 to Q-L4 from 10-D2 still apply.

- **Q-L5:** Is the target's added `phase` field on error results an accepted, documented contract extension? Or must the target match the original's result shape? Affects G094, G127, G128, G171, G175, G199 and G200.
- **Q-L6:** Accept the target's generic or shorter diagnostic wording? And separately, must dropped detail be restored (expected/actual values, tab suffix, directive token, OS error text)? Affects G062, G126, G170, G174 and G198. If detail is required, a repair packet follows after 10-A.
- **Q-L7:** May created-ID/URI bijections be applied in comparison for fake-backend IDs? Affects G125, G130–G133, G197 and G202–G205. If not, these stay RED by definition.
- **Q-L8:** Is the target's always-on identity/title metadata read, and the resulting call schedule, an accepted divergence? It would be conditional on 10-E's failure/conflict characterization and K4 showing no lost verification. The alternative is that the original's read schedule is required. Affects G001–G006 and K1/K2. Removing the guard is not an option this decider can recommend (AGENTS.md safety rules 2 and 7).

## 6. Limits

- Static only. No group gets a semantic disposition here. All class labels and the historical RED status are unchanged.
- The content-exclusion denial blocked the absent-marker shape check and further bus use. Packets that read R need access confirmed first.
- The 10-T2 V009K placement, the K4 cause, K5 roles and the F7 V009K mapping are unverified until 10-T3 runs.
- The historical receipt does not establish that the current source still matches it.

---

**Summary**

- **Accounting:**
  - **10-T1:** 26 untraced groups make 28 leaf occurrences (G123 and G124 are double-case). There are 7 families. The "8" is F1's execution count (7 groups across 8 executions), not an extra family.
  - **10-T2:** the C1 term should be 124 − 22 = 102, not 115, because G001–G006 carry 22 leaves and 9 was V001's whole total. 102 + 26 = 128. K1's list always held 78 groups; "80" was a miscount, and the G035/G057 "fix" changed nothing.
  - G057, G058, G059 and G134 are inside the T5/T6 heading ranges but were never analyzed there, so they stay untraced.
  - Whole receipt: 60 groups / 94 leaves traced or addressed, 146 / 156 untraced, 206 / 250 in total.
- **Decisions:**
  - No schedule normalization. Three new read-only assessments:
    - 10-T3: schedule alignment, K4 verification-gap triage first, V009K, K5 roles, F7 mapping
    - 10-T4: native key sets and per-file navigation
    - 10-T5: phase and wording/detail contract
  - Conditional executors:
    - 9-H: adapter export boundary and cleanup observation schema, under the lane-9 lease
    - 10-E: metadata-read failure/conflict characterization, in a new header with the next include lease
  - 10-D4 settles whatever is left after those.
  - K3 joins P1/P3 and 13-A's gate list. P4 gains rows for V007 cellStyles, V008 review frontmatter and the V005 shrink.
  - The human-owned narrowing questions are Q-L5 to Q-L8, with nothing waived.
- **Sources:** all required sources were explicitly loaded, both triage hashes matched, and R's hash matched.
- **Unresolved:**
  - The content-exclusion denial on the thread directory stopped one spot-check, further bus logging and the pointer send. The lead must restore or confirm access before the R-reading packets run.
  - Whether the original's phase/line values are really absent is unverified.
  - Everything is static and historical. Raw stays RED.

10-D3 status=review rev=1 file=.agents/threads/2026.11.13 20.00 - Node parity gap closure/plans/confirmation/tasks/10-D3.md (decision: this native return, for decisions/raw-followups.md; bus pointer not sent because of the content-exclusion denial)