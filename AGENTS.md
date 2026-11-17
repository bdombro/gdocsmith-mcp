# AGENTS.md (`gdocsmith-mcp`)

Written for: AI coding agents working in the `gdocsmith-mcp` C++26 plugin repository.

`gdocsmith-mcp` is a C++26 MCP plugin (`2026-07-28` Streamable HTTP on `127.0.0.1:10007`) and CLI built on `slimr-*` for safe, revision-locked Google Docs editing through local extended Markdown and YAML files.

<!-- cpp-templates:begin -->
## Tooling alignment

Align this repository via the `cpp-templates` skill (`just check <template> .`). Keep repository-specific rules outside `<!-- cpp-templates:begin -->` and `<!-- cpp-templates:end -->`. Do not hand-edit verbatim template files or template recipes (repositories may add extra recipes or variables).

## Edit-don't-rewrite

- Make targeted edits and move files with `git mv`; never regenerate files from scratch or retype them from memory.
- Template files arrive through `just new` or `just sync`.
- Never delete, skip, or loosen tests to pass; if a test must change, explain why in the commit message.

## C++ Memory Safety

- Immediately wrap owned C handles, allocations, descriptors, and temporary resources in RAII. Make owning wrappers noncopyable unless copying deliberately creates independent ownership; moves must leave the source safely destructible.
- Borrow views, pointers, and references only for the current call by default. Store owning values unless lifetime/invalidation guarantees are explicit. Deferred callbacks require explicit ownership-safe captures; no escaping reference captures without a demonstrated lifetime guarantee.
- Initialize scalar/pointer members at declaration and local values before use. Compiler zero-initialization is defense in depth, not permission for uninitialized reads.
- Check range/overflow before narrowing lengths/counts at C API boundaries; casts alone are not validation. Report failures through existing error mechanisms.
- Never let C++ exceptions escape C callbacks. Translate them into the C API's error/abort mechanism and propagate failures at the C++ boundary; do not silently swallow them.
- Preserve libc++ hardening, zero auto-initialization, ASan/UBSan with fatal reports, and required lint/tests. These checks do not prove memory safety.

## C++ style

- Favor modern C++26 and the standard library over legacy patterns; write for readability; comment what is not obvious and why.
- Prefer values over pointers, `const` by default, and no raw `new`/`delete`.
- Use `std::expected` for expected failures; use exceptions only at process and callback boundaries.
- Structure code with one folder per feature under `src/`, mirrored under `tests/`. Keep one test TU per executable (`tests/test_main.cpp`).
- Name what you use (`using slimr::Json;` or `slimr::Json`, never `using namespace`). Use PascalCase for types and camelCase for functions and variables (review guidance, not lint).
- Put file-local helpers in an unnamed namespace in `.cpp` files, or in `detail::` in headers.
- Follow KISS: inline single-use helpers, avoid speculative abstractions, keep diffs lean.
- Apply style rules to new and touched code; ports do not rewrite working code just for style.
- `.clang-tidy` enforces bug-finding checks (`clang-analyzer-core.*`, `clang-analyzer-cplusplus.*`, and bug-risk `bugprone-*` checks; pending legacy findings are temporarily disabled with per-check counts in `.clang-tidy`); fix findings or suppress inline with a reason.
- `INTERFACE_SOURCES` caveat: linking a header-only library that ships an implementation TU (such as `slimr-yaml`) into both a library and an executable that links that library compiles the TU twice and causes duplicate symbols.

## Testing

Iterate with `just test "<case>"` (Debug build with ASan + UBSan, `BUGSINK_DSN=""`), then finish with a full `just test`. `just build` compiles Release only and runs no tests.

## Recipes

- Core: `build`, `clean`, `configure`, `deps`, `doctor`, `fmt`, `lint`, `release`, `test [filter]`, `upgrade-deps`
- `homebrew-*`: `install`, `uninstall`
- `homebrew-service` and `homebrew-mcp`: `daemon-logs`, `daemon-restart`, `daemon-start`, `daemon-status`, `daemon-stop`
- `homebrew-mcp`: `test-mcp`
- Dropped tooling (do not re-add): foreground runner, local tap/overlay symlinking, GitHub-branch/local-publish/dry-run helpers, standalone formula check, and JS formula helpers

## Changelog

Add one line under `## Unreleased` in `CHANGELOG.md` for every change (use `Internal: ...` for internal changes).

## Paths, env vars, and README

- `<name>` in Homebrew paths is always the formula name, never a short XDG name: app data lives in `$(brew --prefix)/var/<name>`, config in `$(brew --prefix)/etc/<name>/`, and service secrets in `$(brew --prefix)/etc/<name>/env` (`chmod 600`, never in the public formula).
- Secrets are env vars only.
- Every `README.md` is end-user first: what it does, install, usage, env var table (name, required or optional, purpose), then links into `docs/` (`docs/development.md`, `docs/architecture.md`) for contributor workflow and architecture.

## Remote tests

Never run `test.yml` manually before `just release` (because `release` runs `test.yml` first). Run `test.yml` manually only when asked or to reproduce a Linux-only failure.
<!-- cpp-templates:end -->

## Documentation Upkeep

When a change conflicts with `README.md`, update the affected README and its
linked user/developer documentation in the same change. Keep examples,
configuration, install/build commands and compatibility claims consistent
with implemented behavior; do not claim unverified release or client support.

## Extra Commands

| Task | Command |
| --- | --- |
| Offline Node-vs-C++ engine parity with a prebuilt ParityChecks adapter, a fresh run ID and an explicit Node; raw differences exit 1 | `just parity <run_id> <node>` |

## Module Boundaries

| Path | Owns | Must Not |
| --- | --- | --- |
| `src/main.cpp` | Entrypoint handling `--log-file <path>`, calling `slimr::enable_http(app, {.port = 10007})` + `app.run(...)` | Business logic |
| `src/log_file.hpp` | Generic `app_log::LogFileGuard` stdout/stderr redirection and 10 MiB rotation (`<path>` -> `<path>.1`) | `gdocsmith` dependencies |
| `src/app.hpp` | `make_gdocsmith_app()`, per-document FIFO queue, `export`, `import`, `file`, `status` tool handlers, and `gdocsmith://guide` resource (including executable-relative `pkgshare` lookup) | Direct `curl` calls |
| `src/model/` | `DocModel` AST (`document`, `tab`, `table`, `paragraph`, `element`, `style`, `comments`, `suggestions`), Google Docs JSON parsing (`from_json.hpp`), `batchUpdate` object builders (`to_json.hpp` `RequestBuilder`), plus equivalence, invariants, layout, object-ID, key and error helpers | Network or filesystem I/O |
| `src/diff/` | O(ND) Myers shortest edit script (`myers.hpp`), hunk application (`patch.hpp`), tree-sequence diffing (`tree_diff.hpp`), and unified-diff formatting (`unified.hpp`) | Know about Google Docs JSON or Markdown envelopes |
| `src/lens/` | Extended Markdown envelope: YAML frontmatter, tab and rich-atom directives, inline parser (`parse.hpp`), renderer (`render.hpp`), and document-file projection/serialization (tables, comments, suggestions) | Emit `batchUpdate` requests |
| `src/reconcile/` | Back-to-front `batchUpdate` request building from model/lens intent (`reconcile.hpp`, `requests.hpp`, `container.hpp`, `context.hpp`, `lists.hpp`, `paragraph.hpp`, `tables.hpp`) | Perform HTTP or filesystem I/O |
| `src/emulator/` | In-memory batchUpdate simulator (`document_state`, `document_mutator`, `document_emulator`, `field_mask`, `google_docs_emulator`) and conformance fixture runner (`conformance.hpp`) | Call live Google APIs |
| `src/engine/artifacts.hpp` | Work-root precedence (`GDOCSMITH_WORK_DIR` -> `<ws>/.gdocsmith` -> `$(brew --prefix)/var/gdocsmith-mcp`), `0700` dirs / `0600` files, atomic `atomic_write`/`fileReplace`, SHA-256 snapshots, `WriteTracker`, and keeping a locally edited `export.md` on re-export | API calls |
| `src/engine/load.hpp`, `guard.hpp`, `plan.hpp` | Loading a document into a `DocModel` (`docLoad`) and resolving comment anchors, request-origin guards (`allow` gates), emulator-checked plans, and per-tab unified diffs | Direct HTTP I/O |
| `src/engine/session.hpp`, `transaction.hpp`, `flush.hpp` | In-memory document session (`DocHandle`/`TabHandle`), phased create/update/comment transactions, and flushing pending requests | Markdown parsing or rendering |
| `src/engine/export.hpp`, `import.hpp` | Export orchestration (`exportModel`) and the import pipeline (stale-base check, reconcile, write, verify, re-export) | Own network transport |
| `src/api.hpp` | `DocsClient` & `DriveClient` over `cpp-httplib`, `exec_gws` RAII fd/spawn handling, in-memory OAuth token exchange via `gws auth export --unmasked` | Persist OAuth tokens to disk |
| `tests/` | Header-only suites under `tests/{model,diff,lens,reconcile,emulator,engine,api}/` and `tests/test_log_file.hpp` combined by `tests/test_main.cpp`; conformance fixtures live in `tests/fixtures/conformance/` | Hit live Google Docs APIs in `just test` |
| `scripts/doctor.sh` | Required tools, `LLVM_BIN`, `VCPKG_ROOT`, C++26 compile check, and Linux `enable-linger` check | Modify host state |
| `scripts/test-mcp.ts` | Ephemeral HTTP (`--skip-checks http --host 127.0.0.1 --port 0`) MCP `2026-07-28` test | Hit live Google APIs |
| `scripts/release.ts`, `scripts/formula-shared.ts` | CI release version bump, changelog promotion, formula update, tag, and GitHub release | Embed secrets |
| `Formula/gdocsmith-mcp.rb` | Homebrew formula (macOS/Linux `on_macos`/`on_linux`, `service do` sourcing `etc/gdocsmith-mcp/env` + `--log-file`, `pkgshare` docs, `--version` + `discover` `test do`) | Embed secrets or run `git add -A` |
| `.cursor-plugin/` & `.claude-plugin/` | Cursor and Claude HTTP plugin manifests (`http://127.0.0.1:10007/mcp`) | Include shell scripts or credentials |

## Safety & App Conventions

1. **Reverse-index batchUpdate invariant:** Every Docs `batchUpdate` request list generated by `reconcile` must be ordered from highest document index to lowest (back-to-front) so earlier edits never invalidate the coordinates of later edits.
2. **Revision locking:** `import` must compare frontmatter `revision` against the live document's `revisionId` and validate `export.orig.md` when present. If stale (`staleBase`), refuse writes and export fresh remote state. The public factory defaults to `remoteImportEnabled=true`; scoped reviews and final runtime gates are parent-accepted. The synthetic live smoke covered create/body-edit/export/no-op/stale refusal/delete, not every rich-content or review capability.
3. **Sharing & deletion guards:** `file` action `delete` requires `allow: ["delete"]`. `file` action `grant` requires `allow: ["share"]`, and must unconditionally refuse `anyone` (public), `domain`, or `owner` permission grants.
4. **Preserve local edits on re-export:** The canonical files are `exports/<docId>/<revId>/export.md` (working copy) and `export.orig.md` (recorded original), matching the available read-only `../gdocsmith-node` reference. Re-export must not clobber edited `export.md`. Recovery must return fresh remote content, not relabel an edited working copy as authoritative. No migration or cleanup of user files.
5. **Auth boundary:** Keep `oauth.enabled = false` (`127.0.0.1:10007` single-user mode); never add a persistent HTTP Google token broker database or mix MCP bearer tokens with Google access tokens.
6. **Capability boundary:** Supported body-rich edits, tab operations, `namedStyleSets` / `cellStyles` declaration edits, and Drive comment actions do not imply atom mutation, merged/resized-table editing, non-body editing, or protected review edits. Preserve unchanged metadata and refuse unsupported changes before sends. `allow` cannot override an unsupported operation.
7. **Unknown outcomes:** Never replay creates, copies or comments blindly. Preserve edited inputs, report confirmed/partial/unknown writes honestly, and require authoritative readback before verified success. Offline tests are not live Google proof or full-parity evidence.
