# 8 — Repair dropdown backend fidelity and foundation assertions

Status: READY after approved narrow document_state.hpp root-cause repair on 2026-11-13T21:14Z. Focused suite 53 cases/242 assertions passes; actual original Node 12/12 synthetic assertions and focused tidy pass. Engine owner notified with exact hashes; final integration acceptance remains parent-owned.

Depends on: the accepted bounded task-7 report. This work is independent of active import/lens/API source changes.

Own writes: `src/emulator/document_emulator.hpp` only for the narrowly identified dropdown-definition/chip-relabel behavior; focused model/diff/emulator test headers, and `tests/test_main.cpp` solely for new test includes; this state/log and a concise disposition receipt. Exclude API/auth, engine and lens tests, conformance controls/fixtures, other production source, CMake, parent map, coordinator inbox and other owners' artifacts.

Approved same-task scope extension at 21:12Z: exclusive narrow `src/emulator/document_state.hpp` writes for absent atom textStyle fidelity, preserving explicit empty style and legitimate clears. Completed as a single initialization change plus three real-emulator regression cases; no serialization stripping.

Objective: transfer meaningful missing source assertions identified in `plans/parity/baseline-transfer.md`, not chase aggregate coverage. Prioritize the misleading row/column test that never mutates columns; unrelated italic retention; heading split state; absent list nesting rather than explicit zero; effective-style precedence/classification; direct container invariants/normalization; and scalar-only style inequality/default/reset preservation.

Highest-priority concrete blocker: the emulator's `updateDropdownDefinitionProperties` currently updates definitions but does not relabel raw chips whose selected option was renamed. Match the actual original backend effect, preserve definition/option/provider IDs, update only matching chips and leave unrelated lists unchanged. Add a focused regression through the real request emulator, not an engine extra-selection workaround. Source 3 owns the engine dropdown implementation and awaits this independently writable backend fix.

After that blocker, use remaining bounded time for the explicitly identified emulator parameter gaps above; do not broaden into an unbounded full foundation rewrite. Report uncompleted matrices honestly.

Use the actual original parameter rows and existing real target seams, with fixture equivalence checked explicitly. A helper call, title, relaxed comparison, production-emulator self-comparison or locally recreated implementation is not an assertion transfer. Preserve source distinctions and use native stable identity where JS object identity is inapplicable; document the adaptation. Do not copy raw source bodies into evidence artifacts.

Keep the focused command hardened C++26, zero initialization, fatal ASan/UBSan; use an isolated test-only translation unit/cache, never canonical app output or dependency installers. Add a few coherent data-driven cases in existing files where possible; source tests remain read-only. Report any actual product failure with a minimal synthetic reproduction and current owner instead of silently adjusting expected results or broadening production writes.

Budget: 12 minutes/two bounded cycles within the unchanged 22:00:36Z deadline and 21:40Z source-freeze target. Produce exact translated versus still-unverified parameter dispositions and fresh focused counts/hashes. Parent accepts; final current-source full runtime must include these assertions. No other production repair is authorized without a precise parent handoff.

Safety: no Google/auth/network/services, installs, commits, original writes or nested agents. Sanitize all artifacts; no secrets, sensitive personal data, confidential source bodies or raw transcripts.

## Delivered

- Receipt: `parity-evidence/8-foundation-disposition.md` (relative to this tasks directory).
- Five real request-emulator dropdown cases cover renamed selection, recolor using provider textStyle schema, unrelated definition/chips, title-only mask, selected-option removal and invalid cardinalities 0/1/51. Rename/selected-removal/cardinality assertions failed the old implementation (5 cases, 17 assertions; 3 cases/5 assertions failed).
- Current focused isolated TU: **50 cases, 229 assertions, zero failures/skips**, fatal ASan/UBSan, C++26, libc++ FAST hardening, zero auto-initialization; whitespace check clean. Fourteen new cases plus strengthened existing cases. No canonical output/CMake/dependency changes.
- BF-E02: next/empty inheritance, both heading split styles and both copied bullets now asserted. BF-E03: start-of-A retains B heading and unrelated italic retained under bold/underline mask. BF-E10: actual independent 2x2 column insertion-right/deletion, both rows and column count.
- BF-M05 scalar-only inequality; BF-M07 empty style of remaining c; BF-M11 exact leading empty section style; BF-M12 truly omitted nesting default, rich-link raw/key/empty native style; BF-M14 effective-style precedence/false override/normal 11pt and heading classification; BF-M15 direct validation, exact normalization, stable keys, empty paragraph and all invisibility negatives.

## Historical integration blocker (repaired after explicit approval)

Source 3's no-op dropdown counterexample is confirmed independently: a raw dropdown with omitted textStyle acquires `textStyle: {}` after `requests_emulate(doc, {})`. Provider IDs/properties remain identical, but model raw equality changes. Dedicated isolated counterexample fails 1/1 expected assertion; it is not included as a weakened passing test.

Cause was `src/emulator/document_state.hpp:129` initializing absent atom style to an empty object. Approved repair initializes absence as null; explicit {} stays an object, and actual text-style updates still materialize objects including legitimate clears. Serializer unchanged. Actual original Node confirms all three style shapes through no-op, rename, clear and replay (12 independent assertions). NP-E051/053 end-to-end acceptance remains pending engine-owner and parent current-source runtime.

Remaining untranslated matrices and precise native adaptations are in the receipt. No model production API defect was found: initial effective-style expected objects used integer variants for canonical double dimensions; expected literals were corrected to the same numerical 20pt/11pt values, not relaxed or derived from actual results.
