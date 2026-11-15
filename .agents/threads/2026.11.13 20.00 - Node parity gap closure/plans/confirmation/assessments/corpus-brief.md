## Brief

Author task: `12-B` (`corpus-brief-author`, level 0, read-only)
Independent refinement task/result: `12-B-R` (`corpus-brief-refiner`, level 1, medium effort — `changes made`)
Brief identity: `assessments/corpus-brief.md` SHA-256 `e7c7c7160158aaa280ad2d5d48af21d0fdda070edf7350ec6d7765925086bbb0` (`12-B` contract rev 1 / `SA-04` / `D5`)

This brief specifies a measurable, fail-closed corpus-execution recipe for `SA-04 / BF-M19` using synthetic project-local snapshots, explicitly separating unavailable/empty/nonempty corpus states and prohibiting zero-skips-as-coverage claims. Implementation remains blocked pending lead acceptance of `12-B-R` and separate implementation authorization.

Status: ✅ ready for execution (brief refined; implementation remains gated on separate lead task/contract)

Progress: ⬜ draft · 🔄 refining · ❓ decision or spike needed · ✅ ready for execution

### Why and canonical context
- Rationale: `../gdocsmith-node/src/core/model/corpus.test.ts:14–31` silently returns (`return;`) when `~/.cache/gdocsmith/db.sqlite` is absent or empty (`rows.length === 0`), so zero registered skips in an inherited baseline does not prove any corpus document or tab was parsed or laid out.
- Context: `plans/confirmation/tasks/12-B.md` (contract rev 1), `plans/confirmation/tasks/12-B-R.md` (contract rev 1), `plans/confirmation/assessments/source-obligations.md` (§SA-04 / BF-M19), `plans/confirmation/decisions/concurrency-plan.md` (D5), `../gdocsmith-node/src/core/model/corpus.test.ts:1–31`, `tests/model/test_layout.hpp`, `src/model/from_json.hpp:683`, `src/model/layout.hpp:91–98`, `src/model/invariants.hpp:52–69`, `tests/fixtures/conformance/` (149 JSON files), `tests/parity/seeds/` (6 JSON files), `justfile:27–48`, `CMakeLists.txt:57–73`, `vcpkg.json`.
- Brief mode: full brief (level-0 read-only native handoff refined by level-1 `12-B-R`).

### Observed current and target state
- Current:
  - `../gdocsmith-node` has no `tests/fixtures/` or `fixtures/` directory; its only corpus test (`src/core/model/corpus.test.ts:11,14–31`) targets `~/.cache/gdocsmith/db.sqlite` (`SELECT doc_id, data_json FROM doc_snapshots`), returns normally on open error (`:17–24`) or `rows.length === 0` (`:25`), and otherwise runs `docModelParse` + `layoutCompute(tab)` for every tab (`:27–30`).
  - `gdocsmith-mcp` has 149 JSON files in `tests/fixtures/conformance/` and 6 JSON files in `tests/parity/seeds/`, `tests/model/test_layout.hpp` has no corpus/sqlite/cache replay, `vcpkg.json` has no `sqlite3` dependency, and neither `justfile:27–48` nor `CMakeLists.txt:57–73` defines a corpus replay target.
  - Unlike Node's assumed invariant throw, C++ `layoutCompute(tab)` (`src/model/layout.hpp:91–98`) only computes `Layout` ranges and does not throw or assert on index mismatches; `tests/model/test_layout.hpp:55–74` verifies layout by comparing `layoutCompute(tab)` ranges against `rawRangesCollect(json)`.
  - Actual corpus execution counts in `gdocsmith-mcp` are `0` documents / `0` tabs.
- Target:
  - A fail-closed corpus runner/test scope (to be separately authorized after `12-B-R` acceptance) that loads a synthetic project-local JSON snapshot corpus, explicitly classifies `unavailable`, `empty`, and `nonempty` states, reports `docs_executed` and `tabs_executed` separately, compares `layoutCompute(tab)` ranges against raw JSON `startIndex`/`endIndex` ranges for every tab, and fails closed when a required non-empty run executes `0` documents or `0` tabs.
  - Non-goals: opening or reading personal `~/.cache/gdocsmith/db.sqlite`, claiming synthetic model/layout tests prove private cache coverage, adding a `sqlite3` C++ dependency without separate authorization, or authorizing implementation in `12-B` / `12-B-R`.

### Instructions, conventions and capabilities
- Required sources: `AGENTS.md`, `roles/common.md`, `roles/brief-author.md`, `roles/refiner.md`, `templates/task-brief.md`, `plans/confirmation/rules.md`, `plans/confirmation/tasks/12-B.md` (rev 1), `plans/confirmation/tasks/12-B-R.md` (rev 1), `plans/confirmation/assessments/source-obligations.md` (§SA-04), `plans/confirmation/decisions/concurrency-plan.md` (D5), `plans/confirmation/assessments/corpus-brief.md` — all explicitly loaded.
- Inheritance: none claimed; all sources explicitly loaded.
- Applicable checks: preserve private-cache exclusion; no zero-skips-as-coverage claims; report document and tab counts separately; C++ memory safety rules in `AGENTS.md` if C++ runner code is later authorized.
- Capability preflight: read-only inspection and bus logging available and used; no file edits, builds, or test runs performed.

### Decisions and alternatives
- Established:
  - `D5` (accepted graph decision): `12-B` is a read-only brief-authoring task for `assessments/corpus-brief.md`, followed by `12-B-R` (medium-effort refiner); no implementation is authorized.
  - `plans/confirmation/rules.md`: private cache exclusion and no zero-skips-as-coverage claims.
- Proposed:
  - Use synthetic project-local JSON snapshots (e.g., derived from sanitized `tests/fixtures/conformance/` or `tests/parity/seeds/` shapes) rather than `~/.cache/gdocsmith/db.sqlite` (avoiding unconfigured C++ `sqlite3` dependencies).
  - Report `{ state: "unavailable" | "empty" | "nonempty", docs_executed: N, tabs_executed: M }` and fail closed if `nonempty` is required but `docs_executed == 0 || tabs_executed == 0`.
- Alternatives:
  - Direct `~/.cache/gdocsmith/db.sqlite` replay: rejected by `SA-04` and `rules.md` (personal cache data / non-reproducible).
  - Node `node:sqlite` synthetic corpus runner: viable only if separately authorized as a script target; C++ JSON-snapshot test in `tests/model/` integrates directly with `just test` / `ctest` without new dependencies.
- Assumptions:
  - Future authorized implementation will target `tests/model/` (e.g., a dedicated header included in `tests/test_main.cpp` under its one-include-at-a-time lease) unless the lead selects a standalone test/script target.

### Implementation recipe
- Steps/prior art:
  1. Define a synthetic project-local JSON corpus fixture set (at least one multi-tab document, plus explicit `empty` and `unavailable` path/fixture cases) without touching `~/.cache/gdocsmith/db.sqlite`.
  2. Implement a corpus loader returning an explicit state (`unavailable`, `empty`, `nonempty`) and separate `docs_executed` / `tabs_executed` counters.
  3. For each document in `nonempty`, parse via `docModelParse(json, DocModelParseOptions{.docId = ..., .keys = ...})` (`src/model/from_json.hpp:683`), increment `docs_executed`, and for each tab run `layoutCompute(tab)` (`src/model/layout.hpp:91–98`), compare computed `blockRanges`/`rowRanges`/`cellRanges` against raw JSON `startIndex`/`endIndex` ranges (following `tests/model/test_layout.hpp:55–74`) and optionally `containerValidate` (`src/model/invariants.hpp:52–69`), incrementing `tabs_executed`.
  4. Assert fail-closed behavior: `unavailable` and `empty` must report `0` docs / `0` tabs and must fail any non-empty coverage gate; `nonempty` must assert `docs_executed > 0 && tabs_executed > 0` and exact expected counts.
- Behavior contract:
  - `unavailable` (missing/unreadable corpus path) -> `docs_executed == 0, tabs_executed == 0`, never reported as passing non-empty coverage.
  - `empty` (`0` documents) -> `docs_executed == 0, tabs_executed == 0`, never reported as passing non-empty coverage.
  - `nonempty` -> every document parsed and every tab laid out and range-checked against raw JSON; `docs_executed == expected_docs`, `tabs_executed == expected_tabs`.
- Failure/lifecycle contract:
  - Any `ParseError` (`src/model/from_json.hpp:18–21`), layout/JSON-index mismatch, container invariant failure, or `docs_executed == 0` / `tabs_executed == 0` under a non-empty requirement fails the check immediately.
- Local decision boundary:
  - Exact synthetic snapshot selection and helper naming within the separately authorized implementation task.
- Escalate:
  - Any request to read `~/.cache/gdocsmith/db.sqlite` or any `tests/test_main.cpp` / `CMakeLists.txt` / `justfile` ownership conflict -> route to `lead/coord-v2`.

### Uncertainty and suggested spikes
- Question: Should the separately authorized corpus check live in `tests/model/` (via `just test`, using JSON snapshots and no `sqlite3`) or as a standalone `ctest` / Node script target?
- Spike: None required for `12-B` / `12-B-R`; lead scheduling assigns exact file/target ownership and `tests/test_main.cpp` lease if/when implementation is selected.
- Graph proposal: Keep `12-B` -> `12-B-R` -> (optional future implementation task if selected by lead, sequenced on `tests/test_main.cpp` one-include-at-a-time lease after `11-A`, `10-A`, `10-C`); do not block `13-A` freeze.

### Acceptance and refinement
- Criteria/checks (cheapest discriminating checks for future authorized implementation; if placed in `tests/model/`, run via `just test` or `ctest --test-dir build/cmake/Debug --output-on-failure --no-tests=error`):
  1. **Unavailable/empty fail-closed check:** Run loader against (a) nonexistent path and (b) empty corpus; assert state is `unavailable` / `empty`, `docs_executed == 0`, `tabs_executed == 0`, and non-empty coverage gate rejects both.
  2. **Non-empty count check:** Run loader against synthetic multi-tab corpus; assert `docs_executed > 0`, `tabs_executed > docs_executed`, and both counts match expected fixture totals.
  3. **Wrong-layout fault check:** Inject a synthetic snapshot with a deliberate `startIndex`/`endIndex` or length mismatch; assert it reaches the `layoutCompute(tab)` vs. raw JSON range-equality assertion and fails (not swallowed or skipped).
- Author: `corpus-brief-author` (`12-B`, `af477fe0-26d3-4c3d-bf3a-d442429b0a29`), read-only native handoff; inputs `12-B` rev 1, `SA-04`, `D5`, `../gdocsmith-node/src/core/model/corpus.test.ts`, `tests/fixtures/conformance/`, `tests/parity/seeds/`, `tests/model/test_layout.hpp`, `justfile`, `CMakeLists.txt`.
- Independent refiner: `corpus-brief-refiner` (`12-B-R`), `changes made`; verified `assessments/corpus-brief.md` SHA-256 `e7c7c7160158aaa280ad2d5d48af21d0fdda070edf7350ec6d7765925086bbb0`, all cited paths/line numbers/fixture counts, `vcpkg.json` absence of `sqlite3`, and `src/model/layout.hpp` lack of internal invariant throws.
- Decision/spike gates: `D5` accepted; `12-B-R` refinement complete; lead acceptance and separate implementation contract required before any implementation.
- Release evidence: Pending lead acceptance of `12-B-R` native handoff.
- Invalidation: Changes to `SA-04`, `D5`, or `src/model/{from_json,layout,invariants}.hpp` reopen `12-B-R`.