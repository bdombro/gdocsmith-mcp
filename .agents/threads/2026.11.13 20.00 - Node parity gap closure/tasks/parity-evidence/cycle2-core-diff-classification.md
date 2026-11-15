# All raw differences in the last completed core run

Provenance: `cycle2-core-results.json`, four actual original/target engine scenarios. This is **historical** while source owners repair integration; not a current frozen acceptance result. Exactly 22 differing leaf observations, exhaustively enumerated below using aliases for repeated literal values. All payloads are synthetic. `<RUN>` replaces only each owned run-root prefix; internal recovery paths/nonces are retained. No variances accepted or differences normalized away.

## Literal values

- `W`: target `"Rich body content is editable in-place; non-body sections, style baselines, merged-table structure and review context are read-only. This is not a recreation format."`; original property **absent**.
- `M`: original `"the cloud revision changed; reapply the edit to a fresh export, without replacing the revision in this file"`; target `"The cloud revision changed; reapply edits to a fresh export without changing this file's revision."`
- `S`: original `"The document changed in the cloud; nothing was sent. Reapply this file's edits to the fresh export at exportPath and import that."`; target `"Import refused because the revision is stale; nothing was sent. Reapply edits to the fresh export."`
- `U`: original `"Some writes may have landed; the fresh export at exportPath shows the document now. Reapply only the edits it lacks there, and don't import this file again."`; target `"Import failed; preserve this file and reapply only outstanding changes to a fresh export."`
- `P`: original `"<RUN>/work/exports/synthetic-doc/rev-2/export.md"`.

## Exhaustive leaf index

| Case | Exact raw path | Original → target literal values | Classification / source constraint |
|---|---|---|---|
| V001 | `$.exported.tabs.0.markdown` | `"\n\nHello world\n\n"` → `"Hello world\n"` | Native-format difference unresolved; OB-001 source I:451–463 asserts unchanged/match/path/zero mutations, not exact whitespace. Lens owner must prove bounded semantic/exact-output compatibility or fix, never trim blank paragraphs. |
| V001 | `$.exported.tabs.1.markdown` | `"\n\nProtected by omission\n"` → `"Protected by omission\n"` | Same unresolved native-format class; OB-001 plus retained second-tab state. |
| V001 | `$.result.warnings.0` | W | Obsolete capability-warning defect assigned export owner. Original case does not assert exact warnings, but actual source output is empty and target claim is now false for editable features. |
| V002 | `$.exported.tabs.0.markdown` | `"\n\nHello revised world\n\n"` → `"Hello revised world\n"` | Native-format difference unresolved; OB-002 source I:512–533 asserts included body text and title/order, not byte-equal tab markdown. |
| V002 | `$.exported.tabs.1.markdown` | `"\n\nProtected by omission\n"` → `"Protected by omission\n"` | Native-format difference unresolved; second-tab raw provider state matches, but that alone does not retire Markdown formatting obligations. |
| V002 | `$.result.warnings.0` | W | Obsolete capability-warning defect; no blanket variance approved. |
| V003 | `$.exported.tabs.0.markdown` | `"\n\nHello world\n\n"` → `"Hello world\n"` | Native-format difference unresolved; OB-006 source I:569–583 verifies fresh revision, not exact whitespace. |
| V003 | `$.exported.tabs.1.markdown` | `"\n\nProtected by omission\n"` → `"Protected by omission\n"` | Native-format difference unresolved; same caveat. |
| V003 | `$.result.errors.0.message` | M | Proposed wording variance **unaccepted**. OB-006 constrains code staleBase, refused/none, input preservation, distinct fresh export and zero batches; exact message is observed source behavior, not asserted verbatim in that case. Preserve safety instruction semantics. |
| V003 | `$.result.exportPath` | P → `"<RUN>/work/recoveries/99985-177380401335083-3/exports/synthetic-doc/rev-2/export.md"` | Proposed native recovery-layout variance **unaccepted**. Source requires distinct fresh export/current revision, not exact directory recipe. Keep path role/content/existence assertions and raw paths. |
| V003 | `$.result.summary` | S | Proposed wording variance **unaccepted**, subject to preserving no-send/reapply instructions; same OB-006 source constraints. |
| V003 | `$.result.warnings.0` | W | Obsolete capability-warning defect. |
| V004 | `$.exported.tabs.0.markdown` | `"\n\nHello revised world\n\n"` → `"Hello revised world\n"` | Native-format difference unresolved; OB-008 source I:410–429 asserts lost-response state/replay safety, not exact whitespace. |
| V004 | `$.exported.tabs.1.markdown` | `"\n\nProtected by omission\n"` → `"Protected by omission\n"` | Native-format difference unresolved. |
| V004 | `$.replay.errors.0.message` | M | Proposed wording variance **unaccepted**; source replay error must be staleBase and total batches one. |
| V004 | `$.replay.exportPath` | P → `"<RUN>/work/recoveries/99987-177380784910750-6/exports/synthetic-doc/rev-2/export.md"` | Proposed recovery-layout variance **unaccepted**; raw path retained and content/identity not normalized. |
| V004 | `$.replay.summary` | S | Proposed wording variance **unaccepted**, replay safety semantics retained. |
| V004 | `$.replay.warnings.0` | W | Obsolete capability-warning defect. |
| V004 | `$.result.exportPath` | P → `"<RUN>/work/recoveries/99987-177380779936458-3/exports/synthetic-doc/rev-2/export.md"` | Proposed recovery-layout variance **unaccepted**. OB-008 requires a distinct refresh after unknown write. |
| V004 | `$.result.summary` | U | Potential consequential wording defect/variance, **not approved**: source explicitly warns not to import the old file again and to reapply only missing edits; target currently loses that specific instruction. Source I:410–429 protects replay safety; production import catch constructs this observed message. |
| V004 | `$.result.verification` | `"inconclusive"` → `"notApplicable"` | Actual outcome-contract defect assigned engine owner, not wording/native format. Original production `engine/import.ts:546` and fresh original execution establish inconclusive; added a named characterization assertion to the next driver run. |
| V004 | `$.result.warnings.0` | W | Obsolete capability-warning defect. |

Other compared result/state/mutation fields matched in that completed run. The later driver additionally retains/compares full raw synthetic exports, stored vs readback provider state, and ordered callback logs, and performs strict original Node parsing of **unmodified** C++ initial/postimport exports. This may surface additional differences; each future result writes a complete `*-raw-diffs.json` with all exact values, source constraints and unaccepted classifications. This historical index cannot stand in for that frozen rerun.

Original `lens/document.test.ts:102–122` requires the first opening section line to exist and remain first and strips it from parsed tab markdown. It does not by itself authorize dropping meaningful blank paragraphs. Broader exact-output/roundtrip/empty-paragraph obligations remain lens-owner investigation; this lane does not retire them from the above bounded observations.
