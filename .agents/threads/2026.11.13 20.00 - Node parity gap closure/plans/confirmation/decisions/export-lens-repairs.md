# 11-D1 — Export/lens parity repairs (`styleBaseline`, `:style[]{}`, `::section{id=s1}`, `TAB_SETTINGS_DEFAULT`, and colon escaping)

Settles whether `styleBaseline` and `:style[]{}` should be removed/replaced to match Node and corrects/confirms the bounded repair recipes for Items 1, 2, 3, 4, and 5 so `11-A` (`tests/engine/test_import_structure.hpp`) and lens/export parity implementation can proceed without breaking existing tests.

Status: ✅ decided

Progress: ✅ decided (pending lead recording into `plans/confirmation/decisions/export-lens-repairs.md` and brief/task update)

## Request and context

Three observed-RED cases in `11-A` (`tests/engine/test_import_structure.hpp`) and two `src/lens/render.hpp` differences against `../gdocsmith-node` were analyzed in `plans/confirmation/briefs/11-B.author.md` (`14-L`). Two items (`styleBaseline` and `:style[]{}`) involved trade-offs with existing C++ test suites and internal `put.hpp` invariants, and two of the proposed repair recipes (`::section{id=s1}` in `document.hpp` and `TAB_SETTINGS_DEFAULT` / colon escaping test impacts) needed code verification before executor dispatch.

- Asking agent/task: `14-L` (`brief-export-lens-author`) via `11-D1` (`export-lens-decider` -> `coord-v2`/`lead`)
- Question:
  1. Item 3 (`styleBaseline`): whether to stop emitting `styleBaseline` by default (or gate behind an option) and how to keep `test_import.hpp` legacy/snapshot tests valid while unblocking `11-A` section-2 header edits.
  2. Item 5 (`:style[]{}`): whether to adopt Node's `blockInvisibleIs` filtering in `render.hpp`/`project.hpp` and update `test_render.hpp`/`test_document.hpp` assertions, or keep `:style[]{}` as a documented difference.
  3. Confirm bounded repair recipes for Items 1 (`::section{id=s1}`), 2 (`TAB_SETTINGS_DEFAULT`), and 4 (`render.hpp` colon escaping), noting all existing C++ tests affected.
- Context summary: C++ already defaults `ExportOptions::sourceVersion` to `"3.0.20"` (`GDOCSMITH_NODE_VERSION`), omitting `styleBaseline` and emitting `::section{id=s1}` in `exportDocumentFile`. However, `RemoteImportFixture::file()` (`tests/engine/test_import.hpp:361-365`) uses `ExportOptions{.sourceVersion = std::nullopt}` (legacy `gdocsmith: 1`), which caused `11-A`'s `structure::exportText` to hit legacy `styleBaseline` and missing `::section{id=s1}`. Separately, `render.hpp:17` over-escapes `:` and `export.hpp:265-299` emits default tab/page margin attributes.
- References/inputs:
  - `plans/confirmation/briefs/11-B.author.md` (`14-L`)
  - `src/lens/document.hpp`, `src/lens/render.hpp`, `src/lens/project.hpp`, `src/lens/put.hpp`, `src/lens/directives.hpp`, `src/model/invariants.hpp`
  - `src/engine/export.hpp`, `src/engine/import.hpp`
  - `tests/engine/test_import_structure.hpp`, `tests/engine/test_import.hpp`, `tests/engine/test_export.hpp`, `tests/lens/test_render.hpp`, `tests/lens/test_document.hpp`
  - Read-only Node reference: `../gdocsmith-node/src/core/lens/document.ts`, `../gdocsmith-node/src/core/lens/render.ts`, `../gdocsmith-node/src/core/lens/project.ts`, `../gdocsmith-node/src/core/engine/export.ts`, `../gdocsmith-node/src/core/engine/import.ts`

## Authority and evidence

- Delegated authority: Level-2 technical decision over `11-D1` items 1–5 within `gdocsmith-mcp` (`../gdocsmith-node` is read-only; no source edits, builds, or tests in this task).
- Human boundary: None crossed; no scope expansion, live API calls, or commits requested.
- Verified facts:
  - **Item 3 (`styleBaseline`)**:
    - `ExportOptions::sourceVersion` in `src/engine/export.hpp:36` already defaults to `lens::GDOCSMITH_NODE_VERSION` (`"3.0.20"`).
    - `src/engine/export.hpp:395` already gates `styleBaseline` behind `!options.sourceVersion`: `if (!options.sourceVersion) docFile.frontmatter.styleBaseline = std::move(baseline);`. Default `3.0.20` exports do **not** emit `styleBaseline`.
    - Node (`../gdocsmith-node/src/core/engine/export.ts`, `import.ts`, `lens/document.ts`) has no `styleBaseline` frontmatter field.
    - `RemoteImportFixture::file()` (`tests/engine/test_import.hpp:361-365`) passes `ExportOptions{.sourceVersion = std::nullopt}` (legacy `gdocsmith: 1`). `tests/engine/test_import_structure.hpp:18` (`structure::exportText`) calls `fixture.file()`, so all `11-A` structure tests inadvertently exported legacy `gdocsmith: 1` files containing `styleBaseline` (including `headersFooters` raw JSON with `"content": "Draft\n"` via `src/engine/export.hpp:233`). `structure::replaceFirst(text, "Draft", "Final")` (`tests/engine/test_import_structure.hpp`) replaced `"Draft"` inside frontmatter `styleBaseline` before the `::header{kind=default section=2}` body, tripping `readonlyValidate` (`src/engine/import.hpp:714-721`).
    - Existing tests explicitly verify legacy `styleBaseline` behavior under `ExportOptions{.sourceVersion = std::nullopt}` (`tests/engine/test_export.hpp:118-138`, `tests/engine/test_import.hpp:680-693`, `tests/engine/test_import.hpp:1395-1402`).
  - **Item 1 (`::section{id=s1}`)**:
    - `src/engine/export.hpp:336-349` **already** inserts `::section{id=s1}` at the start of `projection.blocks` whenever `options.sourceVersion` is set (`"3.0.20"`), leaving `dft.openingSettings` as `std::nullopt`.
    - `tests/engine/test_import.hpp:685-686` explicitly verifies this: `exportDocumentFile(...)` with default options produces `CHECK(file.tabs[0].markdown.starts_with("::section{id=s1}"));`.
    - `src/lens/document.hpp:545-549` only prepends `::section{...}` from `tab.openingSettings` (populated when `documentFileParse` splits `::section{id=s1}` on `3.0.20` files). Unconditionally emitting `::section{id=s1}` in `document.hpp:545-549` when `!tab.openingSettings` (as proposed by `11-B.author.md`) would **double-emit** `::section{id=s1}` on every `exportDocumentFile` -> `documentFileExport` roundtrip and on programmatic `DocumentFile` construction (`tests/lens/test_document.hpp:13, 45, 76, 98, 174`), and would break legacy `gdocsmith: 1` tests (`tests/lens/test_document.hpp:138-152, 167-212`).
  - **Item 2 (`TAB_SETTINGS_DEFAULT`)**:
    - Node (`../gdocsmith-node/src/core/lens/document.ts:153-167`, `pageAttributes`) omits: `background: "#FFFFFF"`, `footerMargin: 36`, `headerMargin: 36`, `pageNumberStart: 1`, and `margins: 72,72,72,72` (when all 4 sides are `72`).
    - C++ `exportTabSettings` (`src/engine/export.hpp:265-299`) emits all five defaults unconditionally whenever present in `documentStyle` (as in `emulator::blank_tab_template()`), producing `margins=72,72,72,72 pageNumberStart=1 headerMargin=36 footerMargin=36 background=#FFFFFF` between `title=...` and `pageful` (`tests/engine/test_import_structure.hpp`).
    - No existing C++ test asserts emission of those default values (`tests/engine/test_import.hpp:886-892` asserts non-default `headerMargin=24`, `footerMargin=20`, `background=#FFF4CC`, `pageNumberStart=5`).
  - **Item 4 (Colon escaping)**:
    - `src/lens/render.hpp:17` escapes every `:` (`\\`*~[]!:#|{}`), turning `"Launch date:"` into `"Launch date\\:"`.
    - Node (`../gdocsmith-node/src/core/lens/render.ts:459`) only escapes `:` matching `(?<![A-Za-z0-9\\]):(?=:(?:pagebreak|section|toc)(?![A-Za-z0-9]))`.
    - Contrary to `11-B.author.md`, an existing C++ test **does** assert `\\:`: `tests/engine/test_import.hpp:769-773` checks `"Launch date\\: March 3 ok"` and `"Launch date\\: :suggestion{id=s1 start}March 3:suggestion{id=s1 end} ok"` with a comment noting Node source asserts the unescaped form.
  - **Item 5 (`:style[]{}`)**:
    - Node (`../gdocsmith-node/src/core/lens/project.ts:246-254`) filters `blockInvisibleIs(block) || whitespaceOnlyIs(block)` during projection and tracks hidden paragraphs in `owned` / `leadingOwned`.
    - C++ defines `blockInvisibleIs` (`src/model/invariants.hpp:36-45`) but does not implement `owned`/`leadingOwned` tracking in `src/lens/project.hpp` or `src/lens/put.hpp`.
    - Critically, `src/lens/put.hpp:614-615` (`tablePut`) projects a single cell paragraph via `markdownParse(markdownRender(projectionBuild({cell.blocks[0]}, target.tab)))` and calls `if (projected.blocks.empty()) refuse();`; `src/lens/project.hpp:384-393` (`inlineCell`) and `src/lens/put.hpp:760-762` (`containerPut`) likewise rely on empty paragraphs projecting to a visible `:style[]{}` block. Suppressing `:style[]{}` in `render.hpp:126` or filtering `blockInvisibleIs` in `project.hpp` without porting Node's full `owned`/`leadingOwned` lifecycle would break empty table cells and structural roundtrips.
    - Two lens tests (`tests/lens/test_render.hpp:43`, `tests/lens/test_document.hpp:78`) directly assert `:style[]{}` behavior; `tests/engine/test_import.hpp:688-689` already handles both presence and absence of `:style[]{}`.
- Uncertain: None affecting these decisions (all code paths inspected directly; runtime behavior remains to be verified by executor `just test` after edits).

## Options

### Item 3 (`styleBaseline`)
- ✅ **Keep `styleBaseline` gated behind `!options.sourceVersion` (legacy `gdocsmith: 1` only) and switch `11-A` (`structure::exportText`) to default `ExportOptions{}` (`sourceVersion = "3.0.20"`):** Matches Node (`3.0.20` emits no `styleBaseline`), keeps `readonlyValidate` and `RemoteImportFixture::file()` intact so all `test_import.hpp` / `test_export.hpp` legacy snapshot tests remain valid without weakening, and directly unblocks `11-A`'s section-2 header edit and pageless `::section{id=s1}` tests. Low risk, fully reversible.
- ⬜ **Remove `styleBaseline` entirely from `export.hpp`/`import.hpp`:** Would break `tests/engine/test_export.hpp:118-138` and `tests/engine/test_import.hpp:691, 1395-1402` and require deleting intentional legacy `gdocsmith: 1` snapshot protection. Rejected.
- ⬜ **Relax `readonlyValidate` (`src/engine/import.hpp:714-721`) to ignore `styleBaseline`:** Leaves legacy `styleBaseline` bloated in frontmatter and breaks `test_import.hpp:1395-1402` (`raw settings snapshot change` expects `unsupportedSyntax` refusal). Rejected.

### Item 5 (`:style[]{}`)
- ✅ **Keep `:style[]{}` as a documented structural difference for this phase (do not change `render.hpp:126` or `project.hpp`):** Preserves `put.hpp:614-615` (`tablePut`), `project.hpp:384-393` (`inlineCell`), `put.hpp:760-762` (`containerPut`), and the existing `test_render.hpp:43` / `test_document.hpp:78` contracts without risking regressions across table/cell/review-rejection putback. Matches `10-B`'s scoping disposition (`decisions/raw-repair-recipes.md:413`).
- ⬜ **Adopt `blockInvisibleIs` filtering in `render.hpp`/`project.hpp` and update `test_render.hpp` / `test_document.hpp`:** Unsafe as a localized edit because `put.hpp:614-615` (`tablePut`) immediately `refuse()`s when an empty cell paragraph renders empty, and `project.hpp:384-393` (`inlineCell`) assumes `!child.blocks.empty()`. Would require a full `owned`/`leadingOwned` projection + putback rewrite. Rejected for this bounded repair batch.

## Decider outcome

Item 3 keeps `styleBaseline` gated behind `options.sourceVersion == std::nullopt` and unblocks `11-A` by exporting Node-3 (`ExportOptions{}`) in `structure::exportText`; Item 5 retains `:style[]{}` as a documented C++ difference; Items 1, 2, and 4 have confirmed bounded repair recipes below.

- Decider: `export-lens-decider` (`11-D1`, `decider2`, `gemini-4-hydrogen` / high)
- Outcome: **decided** (all 5 items resolved within delegated authority)
- Decision/rationale:
  1. **Item 3 (`styleBaseline`) — Keep gated behind `!options.sourceVersion`; update `structure::exportText` to Node-3 defaults:**
     - **Production code (`src/engine/export.hpp:395`, `src/engine/import.hpp:714-721`)**: No change needed (`styleBaseline` is already omitted when `sourceVersion` is `"3.0.20"`).
     - **Test helper (`tests/engine/test_import_structure.hpp:17-19`)**: Change `structure::exportText(RemoteImportFixture& fixture)` from `lens::documentFileExport(fixture.file())` to export with default `ExportOptions{}` (`sourceVersion = "3.0.20"`, matching `exportModel` / `test_import.hpp:685`):
       ```cpp
       auto json = fixture.backend.get_doc("synthetic_doc").json;
       json["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
       return lens::documentFileExport(exportDocumentFile(
           model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc"})));
       ```
     - **Preserves**: `RemoteImportFixture::file()` (`tests/engine/test_import.hpp:361-365`) and all legacy/snapshot tests in `tests/engine/test_import.hpp` and `tests/engine/test_export.hpp` remain untouched and valid.
  2. **Item 5 (`:style[]{}`) — Keep as a documented difference:**
     - Do **not** alter `src/lens/render.hpp:126` or add `blockInvisibleIs` filtering to `src/lens/project.hpp` in this repair batch; do **not** modify `tests/lens/test_render.hpp:43` or `tests/lens/test_document.hpp:78`.
     - Record `:style[]{}` rendering of empty `NORMAL_TEXT` paragraphs (and empty-paragraph preservation in `render.hpp`/`project.hpp`/`put.hpp`) as a documented C++ vs Node lens difference; if future parity requires removing `:style[]{}`, open a separate scoped task that also ports `owned`/`leadingOwned` handling across `project.hpp` and `put.hpp` (`tablePut`, `inlineCell`, `containerPut`).
  3. **Item 1 (`::section{id=s1}`) — Bounded recipe (CORRECTED from `11-B.author.md`):**
     - Do **not** edit `src/lens/document.hpp:545-549` (`if (tab.openingSettings)`). Unconditional emission there would emit a duplicate `::section{id=s1}` on `3.0.20` exports (since `src/engine/export.hpp:336-349` already inserts `::section{id=s1}` into `projection.blocks` when `options.sourceVersion` is set) and break `tests/lens/test_document.hpp` and `tests/engine/test_import.hpp:686`.
     - Instead, switching `structure::exportText` in `tests/engine/test_import_structure.hpp:18` to default `ExportOptions{}` (`sourceVersion = "3.0.20"`) exercises `src/engine/export.hpp:336-349` directly, which already emits `::section{id=s1}` on PAGELESS tabs.
     - **Affected C++ tests**: `tests/engine/test_import_structure.hpp` (`rtl keeps pageless tab pageless`: remove `* doctest::may_fail()` once verified). No existing tests break.
  4. **Item 2 (`TAB_SETTINGS_DEFAULT`) — Confirmed bounded recipe (EXPANDED beyond `11-B.author.md`):**
     - In `exportTabSettings` (`src/engine/export.hpp:271-299`), omit **all five** Node `TAB_SETTINGS_DEFAULT` / `PAGE_LAYOUT_DEFAULT.margins` defaults (Node `document.ts:153-167`, `pageAttributes`):
       1. `margins` (when `pageful`): omit when all 4 sides equal `72` (`std::array<double, 4>{72, 72, 72, 72}`, or `pair.second == "72,72,72,72"`).
       2. `pageNumberStart`: omit when `pageSettings.pageNumberStart == 1` (`pair.second == "1"`).
       3. `marginHeader` -> `headerMargin`: omit when `lens::dimensionPt(style["marginHeader"]) == 36.0`.
       4. `marginFooter` -> `footerMargin`: omit when `lens::dimensionPt(style["marginFooter"]) == 36.0`.
       5. `background`: omit when `*color == "#FFFFFF"`.
     - **Affected C++ tests**: Unblocks `tests/engine/test_import_structure.hpp` (`new pageful landscape tab writes LETTER size`: remove `* doctest::may_fail()` once verified). `tests/engine/test_import.hpp:886-892` uses non-default values (`24`, `20`, `#FFF4CC`, `5`, `36,40,44,48`) and remains green.
  5. **Item 4 (`src/lens/render.hpp:17` colon escaping) — Confirmed bounded recipe (with test impact):**
     - In `src/lens/render.hpp:17` (`markdownTextEscape`), remove `:` from the unconditional escape set `\\`*~[]!:#|{}` -> `\\`*~[]!#|{}`.
     - Escape `:` **only** when matching Node's `/(?<![A-Za-z0-9\\]):(?=:(?:pagebreak|section|toc)(?![A-Za-z0-9]))/g` rule (i.e., `:` not preceded by `[A-Za-z0-9\\]` and followed by `:(pagebreak|section|toc)` not followed by `[A-Za-z0-9]`).
     - **Affected C++ tests (CORRECTION to `11-B.author.md`)**: `tests/engine/test_import.hpp:769-773` **is affected** and must be updated alongside `render.hpp:17` to assert the unescaped Node form `"Launch date: March 3 ok"` and `"Launch date: :suggestion{id=s1 start}March 3:suggestion{id=s1 end} ok"` (removing the comment about target escaping `:`). Add targeted unit coverage in `tests/lens/test_render.hpp` confirming `Launch date:` stays unescaped while `::section` / `::pagebreak` / `::toc` literal text escapes as `\::...`.
- Human referral: None.
- Remaining uncertainty: None.

## Integration and acceptance

- Impact:
  - Update `plans/confirmation/briefs/11-B.author.md` (or produce a refined `plans/confirmation/briefs/11-B.md` via level-1 refiner) incorporating this decision's corrections:
    - Do **not** edit `src/lens/document.hpp:545-549`.
    - Do **not** edit `src/lens/render.hpp:126` or `src/lens/project.hpp` for `:style[]{}` (document it as an intentional difference).
    - Do **not** edit `src/engine/import.hpp:714-721` or `RemoteImportFixture::file()` for `styleBaseline`; update `structure::exportText` in `tests/engine/test_import_structure.hpp:17-19` to use default `ExportOptions{}` (`sourceVersion = "3.0.20"`), and remove `* doctest::may_fail()` from the three `11-A` tests in `tests/engine/test_import_structure.hpp` once verified green.
    - Filter all five defaults in `exportTabSettings` (`src/engine/export.hpp:271-299`): `margins=72,72,72,72`, `pageNumberStart=1`, `headerMargin=36`, `footerMargin=36`, `background=#FFFFFF`.
    - Fix `markdownTextEscape` in `src/lens/render.hpp:17` and update `tests/engine/test_import.hpp:769-773` + add unit test in `tests/lens/test_render.hpp`.
  - Update `README.md` / `gdocsmith://guide` only if any user-facing doc explicitly claims `:style[]{}` suppression or `Launch date\:` escaping (verify during implementation).
- Gates to reopen: None of `11-A` or `11-R` need invalidation; after the bounded repair task lands, `11-A`'s three `* doctest::may_fail()` annotations in `tests/engine/test_import_structure.hpp` are removed cleanly.
- Delivery/acceptance: Delivered natively to `coord-v2`/`lead`; bus pointer sent (`export-lens-decider` -> `coord-v2`).
- Supersedes: Corrects the `document.hpp:545` recipe, missing `margins`/`pageNumberStart` defaults in `export.hpp:271-273`, and missed `test_import.hpp:769-773` impact in `plans/confirmation/briefs/11-B.author.md`.
```

---

### Concise Summary

1. **Item 3 (`styleBaseline`):** Keep `styleBaseline` gated behind `options.sourceVersion == std::nullopt` (`src/engine/export.hpp:395`) and preserve `readonlyValidate` (`src/engine/import.hpp:714-721`) and `RemoteImportFixture::file()` (`tests/engine/test_import.hpp:361-365`) so all existing legacy/snapshot tests stay intact. Unblock `11-A` by switching `structure::exportText` (`tests/engine/test_import_structure.hpp:17-19`) to default `ExportOptions{}` (`sourceVersion = "3.0.20"`).
2. **Item 5 (`:style[]{}`):** Keep `:style[]{}` as a documented C++ difference (do **not** filter `blockInvisibleIs` in `src/lens/render.hpp:126` or `src/lens/project.hpp`, and keep `tests/lens/test_render.hpp:43` and `tests/lens/test_document.hpp:78` unchanged), because `src/lens/put.hpp:614-615` (`tablePut`), `src/lens/project.hpp:384-393` (`inlineCell`), and `src/lens/put.hpp:760-762` (`containerPut`) depend on empty paragraphs projecting to a non-empty block until full `owned`/`leadingOwned` tracking is ported.
3. **Confirmed & corrected bounded repair recipes (Items 1, 2, 4):**
   - **Item 1 (`::section{id=s1}`):** Leave `src/lens/document.hpp:545-549` untouched (`exportDocumentFile` at `src/engine/export.hpp:336-349` already emits `::section{id=s1}` under default `sourceVersion = "3.0.20"`, and changing `document.hpp` would double-emit `::section{id=s1}` and break `test_document.hpp`). Switching `structure::exportText` to default `ExportOptions{}` resolves the PAGELESS RED.
   - **Item 2 (`TAB_SETTINGS_DEFAULT`):** In `src/engine/export.hpp:271-299` (`exportTabSettings`), omit all five defaults: `margins=72,72,72,72` (for `pageful`), `pageNumberStart=1`, `headerMargin=36`, `footerMargin=36`, and `background=#FFFFFF`. No existing C++ tests break.
   - **Item 4 (colon escaping):** In `src/lens/render.hpp:17`, remove `:` from the unconditional escape set and escape `:` only matching Node's `(?<![A-Za-z0-9\\]):(?=:(?:pagebreak|section|toc)(?![A-Za-z0-9]))` rule. **Affected test:** update `tests/engine/test_import.hpp:769-773` from `"Launch date\\:"` to `"Launch date:"` (matching Node) and add a unit test in `tests/lens/test_render.hpp`.