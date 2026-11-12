# B4g log — fuzz test OOB crash (`tab.tape[i]`)

- Reproduced: Release + Debug test binaries both crash (exit 133 = SIGTRAP, EXC_BREAKPOINT) under `-tc=*fuzz*` / `-ts=reconcile::fuzz`, no buffered output.
- SIGTRAP is libc++ `_LIBCPP_HARDENING_MODE_FAST` bounds-check trap on OOB `std::vector`/`std::string` `operator[]` (confirmed via crash report: EXC_BREAKPOINT).
- Built a standalone `-O0` repro (`repro_fuzz.cpp`) driving the fuzz loop with per-seed/op logging → crash is SEED 17, first request (APPLY 0): `updateTextStyle` range [81,83] while the emulator tape is only 81 cells.
- Dumped seed 17 origins: model thinks original doc spans [0,84]; emulator tape = 81. 3-unit discrepancy.
- Root cause: `src/model/test_docs.hpp` `paragraphJsonBuild` computes JSON `endIndex` with `str.size()` / `t.text.size()` (UTF-8 BYTES) instead of UTF-16 code units. "émoji😀" = 10 bytes but 7 UTF-16 units (overcount 3) → JSON indices are internally inconsistent → model origins (byte-based) disagree with emulator tape (UTF-16) → reconcile emits OOB ranges.
- Verdict: TEST bug (synthetic doc generator emits invalid UTF-16 indices). Emulator/reconcile are correct to treat indices as UTF-16.

## Resolution

- Fixed 5 source spots that used `std::string::size()/length()` (UTF-8 BYTES) for Google Docs indices (which are UTF-16 code units):
  - `src/model/symbols.hpp`: added `utf16CodeUnitLength(std::string_view)` helper.
  - `src/model/layout.hpp` `paragraphLength`: text length now UTF-16 (was `inlineItem.text.size()`).
  - `src/model/test_docs.hpp` `paragraphJsonBuild`: synthetic JSON `endIndex` now UTF-16 (was `str.size()`/`t.text.size()`).
  - `src/reconcile/paragraph.hpp` `textStylesEmit`: char-symbol length now `codePointUtf16Units(sym.ch)` (was `sym.ch.length()`).
  - `src/reconcile/reconcile.hpp` `rangeEdgeIndex`: char-symbol sum now `codePointUtf16Units(...)` (was `.ch.size()`).
- Fixed 2 fuzz-helper test bugs (`tests/reconcile/test_fuzz.hpp`): `fuzzRowsInsert`/`fuzzColumnsInsert` now copy the reference cell's style (matching `cellLike`/`rowsInsert`/`columnsInsert` and the emulator's row/column insert), instead of `make_table_cell_style_default()` — this removed the `backgroundColor` mismatch on inserted cells (seeds 11, 28).
- Verified: standalone `-O0` repro runs all 50 seeds with no crash; only seed 21 throws `unrealizableList` (benign — test treats it as `unrealizable++`, not a failure).
- Verified with the rebuilt Release binary: `-ts=reconcile::fuzz` → 1 passed; full suite → 207/207 passed, 3941/3941 assertions.
