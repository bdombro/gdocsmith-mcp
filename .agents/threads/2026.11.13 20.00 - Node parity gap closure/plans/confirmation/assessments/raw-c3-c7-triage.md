# Raw C3/C7 triage (10-T1)

Read-only static triage under contract rev 1. These are candidate families only, not dispositions. Nothing is classed as equivalent or as a defect. Group IDs use [raw-group-index.md](raw-group-index.md); traces T1–T6 and packets P1–P3 refer to [raw-differences.md](raw-differences.md).

## Authority and loading

- **Explicitly loaded:** repository `AGENTS.md`; swarm `roles/common.md` and `roles/brief-author.md`; confirmation `rules.md`; `tasks/10-T1.md` rev 1; `map.md`; accepted `raw-group-index.md` and `raw-differences.md`. I don't claim any instructions were inherited automatically.
- **Receipt:** R = `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/resumed-final-recheck`. The file `R/differential-results-raw-diffs.json` matches the accepted SHA-256 `8291aac1…62b6`. I only read projections of path, cases, values and sourceConstraints. No observation values are copied here beyond path names and value kinds.
- **Not done:** no source reads beyond the accepted assessments, no builds, tests, network, live access, writes, helpers or commits. `messages.md` was not accessed.

## Scoped set

| Set | Groups | Count |
| --- | --- | ---: |
| C3, all | G008, G022, G031, G037, G064, G079, G092–G094, G123–G125, G127–G133, G169, G171–G173, G175–G176, G194, G196–G197, G199–G205 | 35 |
| C7, all | G062, G126, G170, G174, G198 | 5 |
| Excluded: in accepted traces | G008 (T2); G092 (T3, related); G125, G130–G133 (T5); G169, G171–G173, G175–G176 (T3); C7 G174 (in T3's G171–G176 range) | 14 |
| **Untraced C3** | G022, G031, G037, G064, G079, G093, G094, G123, G124, G127, G128, G129, G194, G196, G197, G199, G200, G201, G202, G203, G204, G205 | **22** |
| **Untraced C7** | G062, G126, G170, G198 | **4** |

40 = 14 + 26. Each untraced group appears exactly once in the families below.

## Candidate behavior families (untraced only)

| Family | Groups (variant) | Path | Observed kind | Closest trace | Candidate (unresolved) |
| --- | --- | --- | --- | --- | --- |
| F1 Native file bytes under C3 label | G022 (V003), G031 (V004), G037 (V005), G064 (V007), G079 (V008), G123 (V009,V010), G194 (V009K) | `$.exported.origMarkdown` | Original frontmatter has `apiUser` and a different metadata/style-declaration layout; target lacks `apiUser` and starts directly with `namedStyleSets`/`cellStyles` | T2 (G007–G009) | Same native-envelope question as T2, repeated in 7 variants. G064 adds a `cellStyles` declaration, which needs its own check against OB-027. G079 has review sections, so check the V008 comments/suggestions frontmatter (see T3 note on commentsLine). |
| F2 Snippet-refusal line | G093 (V008) | `$.snippetResult.errors.0.line` | Integer differs | T3 related (G092) | Native layouts differ, so the integers can't be compared directly. Unknown whether each line points to the refused snippet in its own file. |
| F3 Added `phase` field | G094 (V008); G127, G128 (V009); G199, G200 (V009K) | `…errors.N.phase` | Original absent; target `"planning"` | Near T3 (P2 lists "adjacent phase" as separate) | Target adds a result field. Additive change versus contract change is undecided. |
| F4 Error-message wording | G062 (V006); G126 (V009); G198 (V009K); G170 (V010) | `…errors.0.message` | G062: same text up to `(fontSize)`; the original adds expected/actual detail and a tab suffix. G126/G198: OS error string versus generic wording. G170: the original prefixes the source directive token; the target gives only the core phrase. | T3 (G174 already traced), T6 | Proposed wording change, not accepted. G170 and G062 drop diagnostic detail (directive, expected value), not just phrasing. |
| F5 Adapter observation text | G124 (V009,V010); G196 (V009K) | `$.images.deletionEvidence` | Adapter-written explanatory string | T5 (cleanup observation schemas) | Difference in the adapter's observation schema; raw-differences item 3 says it is not engine behavior. Needs adapter-boundary confirmation, not an engine fix. |
| F6 `backendExists` absent vs false | G129 (V009); G201 (V009K) | `$.images.temporaryDocuments.0.backendExists` | Original absent; target `false` | T5 (unequal schemas :199–217 vs :363–377) | Adapter observes different things. Cleanup failure not shown. |
| F7 Created-ID/URI scheme (known-error variant) | G197, G202, G205 (`…id` / `documentId`); G203, G204 (`contentUris.0/1`), all V009K | IDs and URIs | Fake-backend ID formats differ | T5 (createdIdBijections, appliedToComparison=false) | Same ID/URI question as T5 but in V009K, not traced there. Mapping one ID onto the other is not authorized. |

**Coverage check:** F1 7 + F2 1 + F3 5 + F4 4 + F5 2 + F6 2 + F7 5 = 26.

## Minimal follow-up packets (proposals)

| Packet | Covers | Missing evidence | Proposed owner |
| --- | --- | --- | --- |
| Q1 Native-envelope fold-in | F1 | Whether 10-D2's native-envelope decision for G007–G009 applies to all 7 variants. A separate per-variant check of the `cellStyles` declaration (G064, OB-027) and of review frontmatter (G079). Add `apiUser` omission to the T2 metadata candidate. | 10-D2 decider (decision only); no new executor |
| Q2 Per-file navigation check | F2 (+ G092) | Whether each engine's refusal line points to the same snippet in its own exported file. Needs a narrow characterization under an existing receipt/driver run; the integers alone can't settle it. | 10-D1/P2 diagnostic owner, as a sibling assertion; no line normalization |
| Q3 Result-field contract for `phase` | F3 | Whether the original result schema allows extra fields, and whether any consumer or obligation (OB-029, OB-036) depends on `phase` being absent. | 10-D3 decision; source read of the original/target result builders |
| Q4 Wording and detail decision | F4 | Lead/decider decision on accepting the C7 wording. Separately decide whether dropping detail (G062 expected value and tab; G170 directive token) blocks a source-equivalent diagnostic contract. | 10-D3; route any G170 detail issue to P2's owner |
| Q5 Adapter observation schema | F5, F6 | Confirm at the adapter boundary that the strings and `backendExists` reflect what the adapter observes, not engine output. Equal observation of the actual delete is still needed before cleanup parity can be claimed. | 9-series adapter owner (9-B…9-G lease); no comparator normalization |
| Q6 ID-scheme question in V009K | F7 | Same decision as T5's createdIdBijections for the known-error variant. Check that the upload-receipt ordinal mapping holds in V009K. | 10-D3 decision; no normalization unless explicitly authorized |

## Limits

- Families are inferred only from receipt paths, value kinds and sourceConstraints. I didn't read any original or target engine source for these 26 groups, and neither engine was run.
- Excluding G092 relies on T3 calling it "related but distinct". 10-D3 should confirm it counts as traced.
- G124 appears in raw-differences item 3 only as an example; I treat it as untraced.
- Historical receipt only; whether the current source still matches it is not verified.
- No group gets a semantic disposition. All 26 keep their C3/C7 unresolved or unaccepted labels.