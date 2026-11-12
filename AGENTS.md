# AGENTS.md (`gdocsmith-mcp`)

Written for: AI coding agents working in the `gdocsmith-mcp` C++26 plugin repository.

`gdocsmith-mcp` is a C++26 MCP plugin (`2026-07-28` stdio + Streamable HTTP on `127.0.0.1:8087`) and CLI built on `slimr-cpp` for safe, revision-locked Google Docs editing through local extended Markdown and YAML files.


## Documentation Upkeep

When a change conflicts with `README.md`, update the affected README and its
linked user/developer documentation in the same change. Keep examples,
configuration, install/build commands and compatibility claims consistent
with implemented behavior; do not claim unverified release or client support.

## Commands

| Task | Command |
| --- | --- |
| Run unit, mock-API, MCP `2026-07-28` & HTTP tests (ASan + UBSan, no network) | `just test` |
| Build and test release binary (`build/bin/gdocsmith-mcp`) | `just build` |
| Analyze app and test translation units (`CLANG_TIDY` respected) | `just lint` |
| Verify isolated MCP `2026-07-28` stdio + ephemeral HTTP discovery | `just test-mcp` |
| Validate Homebrew formula Ruby syntax | `just check-formula` |
| Manage local Homebrew service lifecycle (`127.0.0.1:8087`) | `just install-local` / `start` / `stop` / `status` / `uninstall` |
| Clean build artifacts | `just clean` |

## Module Boundaries

| Path | Owns | Must Not |
| --- | --- | --- |
| `src/main.cpp` | Entrypoint calling `slimr::enable_http(app, {.host = "127.0.0.1", .port = 8087})` + `app.run(argc, argv)` | Business logic |
| `src/app.hpp` | `make_gdocsmith_app()`, per-document FIFO queue, `export`, `import`, `file`, `status` tool handlers, and `gdocsmith://guide` resource (including executable-relative `pkgshare` lookup) | Direct `curl` calls |
| `src/model/` | `DocModel` AST (`document`, `tab`, `table`, `paragraph`, `element`, `style`, `comments`, `suggestions`), Google Docs JSON parsing (`from_json.hpp`), `batchUpdate` object builders (`to_json.hpp` `RequestBuilder`), plus equivalence, invariants, layout, object-ID, key and error helpers | Network or filesystem I/O |
| `src/diff/` | O(ND) Myers shortest edit script (`myers.hpp`), hunk application (`patch.hpp`), tree-sequence diffing (`tree_diff.hpp`), and unified-diff formatting (`unified.hpp`) | Know about Google Docs JSON or Markdown envelopes |
| `src/lens/` | Extended Markdown envelope: YAML frontmatter, tab and rich-atom directives, inline parser (`parse.hpp`), renderer (`render.hpp`), and document-file projection/serialization (tables, comments, suggestions) | Emit `batchUpdate` requests |
| `src/reconcile/` | Back-to-front `batchUpdate` request building from model/lens intent (`reconcile.hpp`, `requests.hpp`, `container.hpp`, `context.hpp`, `lists.hpp`, `paragraph.hpp`, `tables.hpp`) | Perform HTTP or filesystem I/O |
| `src/emulator/` | In-memory batchUpdate simulator (`document_state`, `document_mutator`, `document_emulator`, `field_mask`, `google_docs_emulator`) and conformance fixture runner (`conformance.hpp`) | Call live Google APIs |
| `src/engine/artifacts.hpp` | Work-root precedence (`GDOCSMITH_WORK_DIR` -> `<ws>/.gdocsmith` -> `/tmp/gdocsmith-<uid>`), `0700` dirs / `0600` files, atomic `atomic_write`/`fileReplace`, SHA-256 snapshots, `WriteTracker`, and keeping a locally edited `export.md` on re-export | API calls |
| `src/engine/load.hpp`, `guard.hpp`, `plan.hpp` | Loading a document into a `DocModel` (`docLoad`) and resolving comment anchors, request-origin guards (`allow` gates), emulator-checked plans, and per-tab unified diffs | Direct HTTP I/O |
| `src/engine/session.hpp`, `transaction.hpp`, `flush.hpp` | In-memory document session (`DocHandle`/`TabHandle`), phased create/update/comment transactions, and flushing pending requests | Markdown parsing or rendering |
| `src/engine/export.hpp`, `import.hpp` | Export orchestration (`exportModel`) and the import pipeline (stale-base check, reconcile, write, verify, re-export) | Own network transport |
| `src/api.hpp` | `DocsClient` & `DriveClient` over `curl.hpp`, `exec_gws` RAII fd/spawn handling, in-memory OAuth token exchange via `gws auth export --unmasked` | Persist OAuth tokens to disk |
| `tests/` | Header-only suites under `tests/{model,diff,lens,reconcile,emulator,engine,api}/` combined by `tests/test_main.cpp`; conformance fixtures live in `tests/fixtures/conformance/` | Hit live Google Docs APIs in `just test` |
| `scripts/test-mcp.mjs` | Isolated stdio (`mcp`) + ephemeral HTTP (`http --port 0 --allow-ephemeral-port`) MCP `2026-07-28` test | Hit live Google APIs |
| `Formula/gdocsmith-mcp.rb` & `scripts/dev-formula.mjs` | Homebrew formula (`service do` on `127.0.0.1:8087`, `pkgshare` docs, Homebrew `PATH`) and local formula staging | Embed secrets or run `git add -A` |
| `.cursor-plugin/` & `.claude-plugin/` | Cursor and Claude HTTP plugin manifests (`http://127.0.0.1:8087/mcp`) | Include shell scripts or credentials |

## Safety & App Conventions

1. **Reverse-index batchUpdate invariant:** Every Docs `batchUpdate` request list generated by `reconcile` must be ordered from highest document index to lowest (back-to-front) so earlier edits never invalidate the coordinates of later edits.
2. **Revision locking:** `import` must compare the frontmatter `revision` against the live document's `revisionId` (and `base.md`). If stale (`staleBase`), refuse the write and export the fresh remote revision.
3. **Sharing & deletion guards:** `file` action `delete` requires `allow: ["delete"]`. `file` action `grant` requires `allow: ["share"]`, and must unconditionally refuse `anyone` (public), `domain`, or `owner` permission grants.
4. **Preserve local edits on re-export:** If `exports/<docId>/<revId>/next.md` already exists and differs from `base.md`, `export` must not clobber the user/agent's unimported edits in `next.md`.
5. **Loopback auth boundary:** Keep `oauth.enabled = false` (`127.0.0.1:8087` loopback single-user mode); never add a persistent HTTP Google token broker database or mix MCP bearer tokens with Google access tokens.

## C++ Memory Safety

- Immediately wrap owned C handles, allocations, descriptors, and temporary resources in RAII. Make owning wrappers noncopyable unless copying creates independent ownership; moves must leave the source safely destructible.
- Borrow views, pointers, and references only for the current call by default. Store owning values unless lifetime/invalidation guarantees are explicit. Deferred callbacks require explicit ownership-safe captures; no escaping reference captures without a demonstrated lifetime guarantee.
- Initialize scalar/pointer members at declaration and local values before use. Compiler auto-initialization is defense in depth, not permission for uninitialized reads.
- Check range/overflow before narrowing lengths/counts at C API boundaries; casts alone are not validation. Report invalid inputs using existing error mechanisms.
- Never let C++ exceptions escape C callbacks. Translate them into the C API's error/abort mechanism and propagate failures at the C++ boundary; do not silently swallow them.
- Preserve hardening, zero auto-initialization, ASan/UBSan with fatal reports, and required lint/tests. These checks do not prove memory safety.
