# Raw C1/C2 clusters (10-T2, contract rev 1)

Read-only level-0 triage. Static only: no build, test, run, network or writes. These are proposals for 10-D3, not dispositions.

Receipt R = `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/resumed-final-recheck`. Authority is `differential-results-raw-diffs.json`, SHA-256 `8291aac1269b94683c6ca0cea88ccf9cbdbbdf422e5072209ee48b42ae4162b6` (re-hashed and matches). Group numbers follow [raw-group-index.md](raw-group-index.md). No raw values are copied here, only paths, method labels, value types and byte lengths.

Explicitly loaded: repository AGENTS.md; swarm `roles/common.md` and `roles/brief-author.md`; confirmation `rules.md`; `tasks/10-T2.md` rev 1; map entries 10-T2/10-D3; `assessments/raw-group-index.md`; `assessments/raw-differences.md`. Model/effort metadata: unverified.

## Scope

- Untraced C1: 95 groups — G010–G017, G019–G020, G024–G029, G034, G035, G057, G069–G077, G095–G121, G144–G167, G177–G192.
- Untraced C2: 25 groups — G018, G021, G030, G036, G038–G040, G058, G059, G063, G065–G068, G078, G080–G083, G122, G134, G168, G193, G195, G206.
- Excluded traced groups: G001–G006 (T1) and G007, G009 (T2). G008 is C3 and stays out of scope. T3–T6 groups are C3/C5/C6.
- Total: 120 groups. Leaf occurrences = 128 (C1 124 − 9 for G001–G006 → 115; C2 31 − 5 for G007/G009 → 26; the 9/5 figures are derived from the case lists in the group index).
- A read-only projection gave 120 distinct in-scope IDs out of 206 groups. Each group appears in exactly one cluster below.

## Observed call-method sequences (method labels only)

Node vs C++, from R/`differential-results.json` `cases[*].{node,cpp}.observation.callLog`:

- **V001–V007 (target pattern):** each target `getDocument` that serves an import/export read is followed by `getFileMetadata`. The target starts with two `getDocument` calls where Node has `getDocument,commentsList`. This is the pattern T1 already traced.
- **V008:** Node has 4 `textExport` calls; the target has 3. The target omits the final post-write `textExport`.
- **V009:** both sides have `imagesUpload`, `createDocument`, `batchUpdate`, `getDocument` and `deleteFile`. The argument shapes at the matching method are identical (inspected at indices 6–10). The target places those calls 1–3 positions later because of the inserted `getFileMetadata`.
- **V010:** the target opens with `getDocument×3` where Node has `getDocument,commentsList,getDocument,getDocument`. The target has 27 calls; Node has 23.

## Clusters

| Cluster | Groups (count) | Kind | Candidate owner(s) |
| --- | --- | --- | --- |
| K1 Index shift from inserted metadata reads | G010, G012–G017, G019, G020, G024, G027–G029, G057, G069, G070, G072–G075, G095–G103, G108–G121, G144–G152, G157–G167, G177–G181, G183–G192 (80) | Call scheduling | `src/engine/import.hpp:966–985` (`Docs::getDocument` + metadata); `tests/parity/cpp-import-adapter.cpp:267–270` (export boundary) |
| K2 Trailing extra calls on the longer side | G011, G025, G026, G071, G104–G107, G153–G156, G182 (13) | Call scheduling / count | Same as K1; V008 also involves `textExport` (cluster K4) |
| K3 Real batchUpdate payload differences | G034, G035 (V005/V006 index 4), G076, G077 (V008 index 9) (4) | Write payload | `src/reconcile/paragraph.hpp` and `container.hpp` seams; links to T6/P3 and T4/P1 |
| K4 V008 textExport count | Shown in K1/K2 paths (G072–G075, G070, G071); no separate group (0) | Scheduling difference with possible semantic effect | Target post-write verification and re-export path in `import.hpp` (unread) |
| K5 V010 cross-document read order | G144, G146, G150, G152, G158, G160 (`args.0` = document-ID position) — counted in K1 (0 extra) | Scheduling plus which document is read first | `import.hpp` copy/source-document read; adapter `cpp-import-adapter.cpp:350–361` |
| C2-A Shared fixture export envelope (V002–V004) | G018, G021, G030 (3) | Native file | `src/engine/export.hpp:312–324,355–375` (frontmatter / named styles per T2) |
| C2-B Initial plus orig/working tabs, per fixture | G036, G038–G040, G058, G059, G063, G065–G068, G078, G080–G083, G122, G134, G168, G193, G195, G206 (22) | Native file | Same export seam; the readers are `node-import-adapter.mjs:205–229` and `cpp-import-adapter.cpp:399–417`, which do not fabricate bytes (T2) |

Totals: K1 80 + K2 13 + K3 4 = C1 97. That is 2 too many, so for exact single coverage:

- **G035** goes to K3 only. It is a payload path at index 4 in both V005 and V006.
- **G057** (V006/V008 index 6 method) goes to K1.

Corrected K1 list for exact coverage: G010, G012–G017, G019, G020, G024, G027–G029, G057, G069, G070, G072–G075, G095–G103, G108–G121, G144–G152, G157–G167, G177–G181, G183–G192. Count = 1+6+2+1+3+1+1+1+4+9+14+9+11+5+10 = 78.

Final coverage: K1 78 + K2 13 + K3 4 = 95 C1; C2-A 3 + C2-B 22 = 25 C2; total 120. K4 and K5 are cross-cutting labels with no exclusive groups.

## Byte-length facts for C2 (UTF-8, not content)

- **Initial export, Node vs target:** V001–V004 533/440; V005 443/319; V006 499/319; V007 529/359; V008 702/479; V009 443/259; V010 443/259.
- **Final export, Node vs target:** V002/V004 541/448; V005 451/267; V007 529/359; V008 576/386; V009 491/307; V010 459/275.
- **Notable conflict:** in V005 the Node re-export grows by 8 bytes while the target shrinks by 52 bytes. The likely cause (envelope omission or lost style) is unknown.

## Ownership boundaries kept separate

1. **Adapter boundary.** The target export adapter calls `exportModel` directly with seeded comments (`cpp-import-adapter.cpp:267–270`). The Node adapter calls the real `fileExport` (`node-import-adapter.mjs:24–27,87–91`). Part of K1/K2 is therefore made by the adapter, not by the engine.
2. **Engine call scheduling.** `import.hpp:966–985` always reads metadata (the identity/title guard from T1).
3. **Native file semantics.** C2 bytes come from the real export on each side (T2). Byte-length differences say nothing about whether meaning is preserved.

## Discriminating packets (proposals for 10-D3)

- **D-K1/K2 — normalize the schedule only for evidence, never for comparison.** Align calls by method subsequence with `getFileMetadata` and the adapter-only `commentsList` removed, then report any leftover mismatch per case.
  - Missing evidence: an alignment projection over R. Also a boundary-aligned target adapter run that goes through the real export wrapper (needs a separately granted execution).
  - Unknowns: whether any K1 group survives alignment; V009K was checked only from paths.
- **D-K3 — merge into P3 (G034/G035) and P1 (G076/G077); don't open new packets.**
  - Missing evidence: the P1/P3 experiments already proposed in raw-differences.md.
- **D-K4 — explain the missing final `textExport` in target V008.**
  - Missing evidence: a static read of the target's post-write verification/re-export path, and whether Node's `textExport` feeds hidden-insertion verification.
  - Ordered-failure risk: if the target skips a verification read, that could be a verification gap rather than a performance difference.
- **D-K5 — V010 cross-document read order.**
  - Missing evidence: which document ID each early read targets (G144/G146/G150/G152/G158/G160 `args.0`), and whether the target checks the copy source before the destination's comments. The ordered-failure behaviour under a source-read fault is unexercised.
- **D-C2 — compare at schema level, per fixture.** Parse both files' frontmatter key sets and named-style declarations without rewriting either file. Separately, explain V005's target shrink on re-export.
  - Missing evidence: a key-set projection of the receipt files, and the original import.test.ts:115–149 inherited-font obligation applied to V005/V006 (T2/T6).
  - Exact-bytes versus schema-equivalence remains a lead/decider question.

## Preserved unknowns, conflicts and limits

- No new source reads for K4 (`textExport` path) or for K5 beyond the call paths.
- The V009K call log was not opened directly; its K1/K2 placement comes from path and value types only.
- The V005 shrinking target re-export (C2-B) is unexplained.
- T1's unexercised failed/conflicting metadata-read obligation still applies to K1.
- No group is moved toward equivalence or defect. The class labels and the historical RED status are unchanged.