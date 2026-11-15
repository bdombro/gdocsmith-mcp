# Task 8 — READY after approved raw-style repair

## Approved follow-up, superseding historical blocker below

Parent authorized narrow document_state.hpp repair at 21:12Z. Atom parsing now represents omitted style as null instead of an object. Explicit empty and populated styles remain objects; actual style edits create objects, so legitimate clears remain explicit {}. No serializer filtering, blanket empty stripping, engine workaround or expected-value relaxation.

Three additional real-emulator cases cover all three input shapes for no-request roundtrip and definition rename, plus both absent→explicit-clear and bold→empty-clear with no-op replay. Current focused result: **53 cases / 242 assertions passed, zero skips**, same hardened fatal ASan/UBSan command. The original Node runtime recorded in workspace.md independently passed **12 assertions** across three shapes for no-op, rename, clear and replay. No Google/auth/network operations.

Focused clang-tidy is clean with repository checks. LLVM22 selects doctest's line-based anonymous registrations, which collide across suite headers; test-only `DOCTEST_COUNTER=__COUNTER__` restores the unique registration scheme used by the actual test compiler. No product diagnostics disabled. Engine owner c3d86a20-ff3c-4a50-b239-7654fc3a76cb notified with exact READY hashes; final engine/full current-source runtime and parent acceptance remain pending.

Superseding SHA-256:

- src/emulator/document_state.hpp: `5dfabc59218e40eeb38cccab1846a5bfaf9cefd7f4eb7d783cdea8442d94b2b9`
- src/emulator/document_emulator.hpp (unchanged): `6e963e34e314b486fe68f6ec608e1b0d1848114fa7930c281185c08f6d1efe9b`
- tests/emulator/test_dropdown_definition_update.hpp: `59361bd75c486eb9c4075803053128bd0d572077a480d42d520ff9447c382cca`
- /tmp/gdocsmith-task8-foundation-final: `a9d952c2b6b10409feb44f2b4a55c863d903aa19e7e777fa5d824d77c88b4990`

Previous 14 foundation-case transfer dispositions remain unchanged and bounded. Historical evidence below is retained, not counted as current failing behavior.

Completed 2026-11-13T21:09:34Z; launch 10. Parent alone accepts. No end-to-end NP-E051/053 closure or full parity claim.

## Verification

- Old dropdown implementation: 5 cases / 17 assertions, 3 failed cases / 5 failed assertions (rename, selected-option disappearance, 0/1/51 option counts).
- Final isolated TU: **50 cases / 229 assertions, all passed, zero skips**. Fourteen new cases; other assertions strengthen existing cases.
- Existing compiler command from `build/cmake/Debug/compile_commands.json`, substituting only stdin test TU and `/tmp/gdocsmith-task8-foundation-final` output. Seven included headers are listed by the fingerprints below, excluding test_main and production header.
- `/usr/bin/clang++`, C++26, libc++ FAST hardening, `-ftrivial-auto-var-init=zero`, `-fsanitize=address,undefined`, `-fno-sanitize-recover=undefined`; runtime ASAN_OPTIONS halt_on_error=1:abort_on_error=1 and UBSAN_OPTIONS halt_on_error=1:print_stacktrace=1. Existing includes/dependencies only.
- Independent no-request counterexample: 1 case / 1 equality assertion, failed as expected; other five dropdown cases filtered out in that probe. This is diagnostic evidence, not passing parity evidence.
- `git diff --check` clean for owned code/test paths. No fixture paths appear in the worktree diff.
- No canonical test/app executable, CMake, fixture, original source, dependency installation, Google/auth/network/service or commit operation.

## Exact parameter disposition

| Obligation | Translated and verified here | Still unverified / limits |
|---|---|---|
| Dropdown definition update | Existing target option a renamed Alpha→Renamed; same-definition option b chip unchanged; other definition sharing option a ID unchanged; provider chip/definition/option IDs retained. Recolor uses option textStyle.foregroundColor.color.rgbColor and leaves all chips/unrelated definition unchanged. Title-only mask ignores an options patch. Missing selected ID refused; cardinalities 0/1/51 refused. | Raw-absence fidelity for seed chips fails independently (below); no claim of full original dropdown cardinality/creation/deletion matrix or end-to-end import acceptance. |
| BF-E02 | Source previous inheritance remains covered; newly next-character italic at paragraph start, empty paragraph empty style, source Hello World split both HEADING_1 styles, source bullet kix.l1 depth 1 split at index 1 copied to both halves. | No exhaustive insertion positions or arbitrary styles. |
| BF-E03 | Source AAA/BBB deletion 1..6 gives BB and HEADING_2; existing 3..6 gives AABB/HEADING_1. Source bold+italic seed, mask bold,underline with underline patch retains italic and applies underline. | Do not infer bold absence: original projected source assertion does not assert it. |
| BF-E10 | Separate real insertTableColumn-right and deleteTableColumn from original 2x2 input; expected 3/1 columns and 3/1 cells in **both** rows, rows stay 2. | Source table fixture adaptation uses actual insertTable request and derives resulting table start index, not a hardcoded independent fixture. Full default styles/175pt width/bold newline/21pt row height/rowSpan and explicit unmerge defaults remain untransferred. No claim that source's title alone checks copied cell style. |
| BF-M05 | Equal scalar key set, a=1 versus a=2, false equality. | Other existing style helper tests unchanged. Exact RGB tolerance transfer BF-M04 remains open. |
| BF-M07 | Canonical remaining text c has exactly empty style after adjacent bold a/b merge. | Other symbol parameters inherited from existing suite, no expanded Unicode claim. |
| BF-M11 | Leading section style equals exact empty object. | No broad parser-retirement claim. |
| BF-M12 | Remove nestingLevel from actual raw bullet before parse, assert truly absent, parse to list ID kix.list1/depth 0/no textStyle. Rich-link owning key nonempty, raw object present, no nonempty explicit native style. | JS optional absent style maps to C++ Inline's mandatory empty style object; parent must accept representation adaptation, not literal JS shape identity. |
| BF-M14 | Exact effective style: NORMAL_TEXT 11pt, HEADING_1 overrides to bold/20pt, run bold=false overrides heading, empty paragraph style uses normal 11pt; TITLE=0, SUBTITLE=1, HEADING_3=3, normal/absent no level; headingStyleIs true/false. | Absent optional string maps to empty string_view because native API is non-optional. Expected numerical dimensions use double variants while preserving exact source values 20/11. |
| BF-M15 | End table V1; adjacent table diagnostic identifies block 2; valid paragraph/table/paragraph/section/paragraph; exact P/T/P/T/P normalization; inserted new paragraph keys/stamps/content and original table values; already-valid full values/stable keys unchanged; empty new paragraph; invisible empty positive and heading/list/text/table negatives. | Native deep-copy value semantics: unchanged identity asserted by full values/stable keys, not JS object address. No new inCell matrix. |

BF-E04 through E09/E11, broader model edit/list primitives BF-M16/M17, generated diff sample identity BF-D02, remaining lens bold-null/default reset BF-P04 and other baseline-transfer rows are **not newly certified**. No lens tests/source were edited. Existing helper calls are not counted as transfer.

## Precise out-of-scope counterexample

Construct a one-index dropdown atom with dropdownId `emu.chip.1`, definition `def.target`, selected option `opt.a`, displayValue `Alpha`, and **no textStyle key**. Execute an empty request vector through `requests_emulate`. The readback preserves these identities/properties but appends `textStyle: {}` to the dropdown payload. Thus no-op raw-atom equality fails without any engine/frontend operation.

Cause: `src/emulator/document_state.hpp:129` defaults absent atom style to an object; `:654–655` reinserts that object. Original `src/core/emulator/tape.ts:241–255,477–478` keeps absent style absent. That production file was not authorized here and was not edited. Parent must route/authorize this narrow fix; source 3 should not add selection requests or weaken rename/no-op expectations.

The passing dropdown fixture uses real request-created chips, so this absence bug cannot be silently claimed closed by its identity assertions.

## SHA-256, current files

| File | SHA-256 |
|---|---|
| src/emulator/document_emulator.hpp | 6e963e34e314b486fe68f6ec608e1b0d1848114fa7930c281185c08f6d1efe9b |
| tests/emulator/test_dropdown_definition_update.hpp | f8f852fc332a13bdd0996d1ecf991fce3d4d3aa5d60ae7c3d2139488f511840a |
| tests/emulator/test_emulate.hpp | 3d65e25c18c60c273c7bc15d5dd5e17554792689a147ccae6ffe90801b1cdbc0 |
| tests/emulator/test_emulate_tables.hpp | e1cb9692fe134a7be873cd7e1a5119c312c75e4075546af184d21f3470a75282 |
| tests/model/test_foundation_assertions.hpp | b52fc6419756f6c5762e40725c1b5cadae25b63b9e958ac5bbc8f8681d2e9cbb |
| tests/model/test_from_json.hpp | a38d30538e1fe739927e8324843a07b72f4d979d687d472d4764543145b659f9 |
| tests/model/test_style_values.hpp | d3298f99b786fb7d2e17c85cd03ff9c4eadd68907323735ca070855bd407fcbe |
| tests/model/test_symbols.hpp | 02a9293b435ba762a211b7e7c8aa70a6984e950e50de649d9982bd52fa1e82df |
| tests/test_main.cpp | 9f90997fa990e547a536e20044ec0919b8967c3febe0e7894cb76d5e21127e2c |
| /tmp/gdocsmith-task8-foundation-final | 97426218cf443ba59d3dc3c2043015504828d7d540cdf0c80154dd523d2048b1 |
| /tmp/gdocsmith-task8-raw-counterexample | 9cb9c019bed6c232efa46adeb36cc52d2c757c7bd7235bd1e9718df33eb835bf |

Read-only original HEAD: d00069dd188ca995bfda9828cc9eee5016603b63. Original backend emulate.ts SHA-256 32f84ce15e48f5faeb08ba0b5ceb6f3ac3c3f5937034eb66125f974047ca102d; lens dropdowns.ts 9aeffd4b6d14e05340cc49d94b80dc24f004f71d05add8b294b4695909cd7851. No source code bodies or raw execution transcripts are included in this receipt.
