# gdocsmith-mcp

```
 ▄▄▄  ▄▄█  ▄▄   ▄▄▄  ▄▄▄ ▄▄▄▄  ▀ ▄█▄  █▄▄  
█  █ █  █ █  █ █    ▀▀▄▄ █ █ █ █  █   █  █ 
 ▀▀█  ▀▀▀  ▀▀   ▀▀▀ ▀▀▀  ▀ ▀ ▀ ▀   ▀▀ ▀  ▀ 
 ▀▀                                        
```

An MCP for Google Docs editing through local, extended Markdown + YAML files. Agents edit content; gdocsmith calculates minimal paragraph- and character-level changes, guards protected content, revision-locks writes, and verifies the result.

Compared with generic Google connectors and raw API wrappers, the file-based workflow gives agents room to do complex edits while keeping changes focused: less context bloat, fewer round trips, and fewer accidental overwrites.

## Features

- Export documents into readable, editable Markdown, including tabs, comments, and suggestions.
- Import focused edits with revision checks, explicit safety guards, and post-write verification.
- Copy complex documents losslessly when unsupported objects must be preserved.
- Small footprint: a standalone ARM64 binary; no Node/V8 runtime.

## `.mcp.json` Configuration

In the `assistant2` workspace, `gdocsmith-mcp` is mounted into the unified `build/bin/assistant-mcp` gateway under the `gdocs_` prefix (`gdocs_export`, `gdocs_import`, `gdocs_file`, `gdocs_status`).

To run `gdocsmith-mcp` as a standalone MCP server in `.mcp.json`:

```json
{
  "mcpServers": {
    "gdocsmith-mcp": {
      "command": "/absolute/path/to/gdocsmith-mcp/build/bin/gdocsmith-mcp",
      "args": ["mcp"],
      "env": {
        "GWS_PROFILE": "default"
      }
    }
  }
}
```

## App Architecture

The standalone MCP and JSON CLI are implemented in native C++26 on `slimr-cpp`; Google API access uses `libcurl`.

```text
plugins/gdocsmith-mcp/
├── vcpkg.json                # Independently resolved dependencies
├── CMakeLists.txt            # App/test targets and transitive dependency linking
├── justfile                  # Thin CMake/Ninja orchestration
├── README.md                 # User & operator documentation
├── AGENTS.md                 # Maintainer & AI agent conventions
├── src/
│   ├── engine.hpp            # Google Docs JSON AST <-> Extended Markdown/YAML converter & Myers diff (`reconcile_json`)
│   ├── api.hpp               # Docs v1 & Drive v3 REST client over `curl.hpp`, in-memory OAuth token manager, `WriteTracker`
│   ├── storage.hpp           # Work-root resolution, `0700` dir / `0600` `.gitignore`, atomic `file_replace`, SHA-256 snapshot checks
│   ├── app.hpp               # `gdocsmith::make_gdocsmith_app()`, per-doc FIFO queue, `gdocsmith://guide` resource, Bugsink DSN
│   └── main.cpp              # Standalone CLI & stdio MCP entrypoint
├── tests/
│   ├── test_engine.hpp       # 25 AST, Markdown, YAML, and Myers diff test cases
│   ├── test_storage.hpp      # 13 hashing, storage, permissions, mock API and app lifecycle test cases
│   └── test_main.cpp         # Combined test runner (38 test cases, 279 assertions)
└── build/bin/
    └── gdocsmith-mcp         # Standalone release binary
```

### Libraries Used

- **`<mcp.hpp>` & `<reflect.hpp>`:** Typed MCP tool definitions, compile-time JSON Schema generation, `gdocsmith://guide` resource registration, and narrow JSON CLI.
- **`<yaml.hpp>` (`rapidyaml`):** Frontmatter parsing (`parse_frontmatter`) and `comments.yaml` / `suggestions.yaml` serialization (`to_yaml`, `parse_yaml`).
- **`<curl.hpp>` (`-lcurl`):** Direct HTTPS calls to Google Docs API v1, Drive API v3, and Google OAuth2 token endpoint.
- **`<json.hpp>` (`glaze`):** Google Docs JSON AST parsing and `batchUpdate` payload serialization.
- **`<bugsink.hpp>` (`-lcurl`):** Optional error reporting configured with `BUGSINK_DSN`.
- **`<test.hpp>` (`doctest`):** Unit test suite compiled with `-fsanitize=address,undefined`.
- **OpenSSL / C++26 Standard Library:** Portable SHA-256 file integrity checks (`<openssl/evp.h>`), `<filesystem>`, `<mutex>`.

## Workflow

1. **Lossless copy (when needed):** Call `file` with `action: "copy"` when duplicating a document that contains opaque/unsupported objects that must be preserved verbatim.
2. **Export:** Call `export` with a document ID or Google Docs URL. Read or search the returned Markdown file (`exportPath`).
3. **Edit locally:** Modify the Markdown file using your standard file-editing tools.
4. **Import:** Call `import` with the absolute file path. Check `status`, `writeState`, `verification`, `warnings`, and `errors`, and use the newly returned `exportPath` as the base for any subsequent edits. Unchanged imports are verified no-ops (`0` API write requests).

## Tools & CLI Commands

Every tool is exposed over MCP (`gdocsmith-mcp mcp` or via `assistant-mcp` under the `gdocs_` prefix) and on the CLI (`gdocsmith-mcp <tool> ['{"arg":"val"}']`). The MCP server also serves the complete syntax and safety specification as the `gdocsmith://guide` resource (readable via `mcp_resource_read`).

| Tool | Gateway Name | Mutating | Description |
| --- | --- | --- | --- |
| `export` | `gdocs_export` | No | Export a Google Doc by ID or URL into `<workDir>/exports/<docId>/<revisionId>/` (`base.md`, `next.md`, `comments.yaml`, `suggestions.yaml`), preserving any unimported local edits in `next.md`. |
| `import` | `gdocs_import` | Yes | Reconcile an edited Markdown file against its revision-locked `base.md`, enforce safety guards (`allow` flags), apply minimal `batchUpdate` + Drive comment operations, and write a fresh post-import export. |
| `file` | `gdocs_file` | Yes | Manage Drive files: `copy`, `trash`, `delete` (requires `allow: ["delete"]`), or manage sharing permissions (`grant` requires `allow: ["share"]`; public `anyone`, `domain`, and ownership transfers are unconditionally refused). |
| `status` | `gdocs_status` | No | Returns plugin version (`3.0.20`) and active workspace storage directory without calling Google APIs. |

### CLI Examples

```bash
# Export a Google Doc to local Markdown
./build/bin/gdocsmith-mcp export '{"doc":"1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms"}'

# Import an edited Markdown file back into Google Docs
./build/bin/gdocsmith-mcp import '{"file":"/absolute/path/to/workspace/.gdocsmith/exports/example-doc/rev1/next.md"}'

# Copy a Google Doc losslessly in Drive
./build/bin/gdocsmith-mcp file '{"action":"copy","doc":"1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms","title":"Copy of RFC"}'

# Check status
./build/bin/gdocsmith-mcp status
```

## Extended Markdown & Multi-Tab Format

```markdown
---
gdocsmith: "3.0.20"
doc: 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms
revision: ALm37BV...
title: Architecture RFC
---
::tab{id=t.0 title="Overview"}

## Proposal

Draft content with **bold**, *italic*, ~~strikethrough~~, `inline code`, and [links](https://example.com).

- Bullet item
  - Nested bullet item
1. Numbered item

| Service | Latency |
| --- | --- |
| Native C++26 | 1.5 ms |
```

- **Document creation:** Set `doc: null` and `revision: null` in the YAML frontmatter to create a new Google Doc on `import`.
- **Tabs (`::tab{...}`):** Existing tabs are addressed by `id` (`t.0`, `t.1`, …), not title. Editing `title="..."` renames the tab; `id=""` creates a new tab at that position; adding `delete` to the directive deletes the tab (when `allow` permits).
- **Rich inline atoms:** Supports inline images (`![alt](url)`), person chips (`:person{email="..." name="..."}`), date chips (`:date{...}`), rich link chips (`:richLink{...}`), horizontal rules (`---`), and page breaks.
- **Review artifacts:** Open Drive comments and pending Docs suggestions are exported alongside the Markdown into `comments.yaml` and `suggestions.yaml` via `yaml.hpp`.

## Safety Guards, Storage & Concurrency

- **Explicit `allow` gates:** Potentially destructive or lossy operations refuse until explicitly authorized via `allow`:
  - `commentChanges` — resolving/deleting/editing comments or removing text anchored by an open comment
  - `brokenLink`, `namedRangeDamage`, `lossyImageMove`, `lossyListRebuild`
  - `delete` (on `file` permanent deletion) and `share` (on `file` permission grants; `anyone`/`domain`/`owner` grants are always blocked)
- **Revision locking (`staleBase`):** If the remote Google Doc revision has advanced since `base.md` was exported, `import` refuses to overwrite concurrent changes and exports the latest remote revision so edits can be cleanly reapplied.
- **Work directory precedence (`src/storage.hpp`):**
  1. `$GDOCSMITH_WORK_DIR` if set
  2. `<workspace>/.gdocsmith` (created `0700` with a `0600` `.gitignore` containing `*\n`)
  3. `/tmp/gdocsmith-<uid>` (`0700`)
  All file writes use atomic temporary-file-and-rename (`file_replace`) and verify regular-file ownership and SHA-256 integrity.
- **Per-document FIFO serialization:** Concurrent tool calls targeting the same `docId` are serialized in-process while different documents run in parallel.

## Authentication & Environment Variables

Authenticates via `gws auth export --unmasked` (using the active `gws` profile) and exchanges refresh tokens in memory against `https://oauth2.googleapis.com/token` over `slimr::curl`. Access tokens stay in process memory and are never written to disk.

| Variable | Default | Purpose |
| --- | --- | --- |
| `GDOCSMITH_WORK_DIR` | `<workspace>/.gdocsmith` | Directory for exported Markdown revisions (`exports/<docId>/<revId>/`) |
| `GWS_PROFILE` | Active `gws` profile | Profile selected by `gws` when exporting OAuth credentials |
| `GWS_BIN` | `gws` | Path or name of the `gws` CLI executable |
| `BUGSINK_DSN` | Unset (disabled) | Error reporting DSN (`""` disables) |

## Building & Testing

Use Clang with C++26 and libc++ FAST hardening, CMake 3.31+, Ninja, [just](https://just.systems), a bootstrapped [vcpkg](https://vcpkg.io), Node for Inspector, and a compatible clang-tidy (LLVM 23 locally). Set `VCPKG_ROOT` and put vcpkg and the tools on `PATH`. Linux additionally needs libc++ and libc++abi; select libc++ with `CXXFLAGS=-stdlib=libc++`.

Dependencies are declared in `vcpkg.json`. During local development, `(cd /absolute/path/to/slimr-cpp && just publish --local --all)` publishes ports into vcpkg's local overlay; export `VCPKG_OVERLAY_PORTS="$VCPKG_ROOT/local-ports"` in your shell. `just deps` installs into `build/deps/` for the detected native triplet. `configure` always runs it, with CMake manifest installation disabled. CMake links installed `slimr::mcp`, `slimr::yaml`, `slimr::test` and `OpenSSL::Crypto` targets, never source-header overrides.

```bash
just test      # Debug ASan/UBSan AST, diff, isolated-storage and mock-API tests
just build     # Optimized IPO Release app and tests; build/bin/gdocsmith-mcp
just lint      # App and test translation units using the Debug compilation database
just test-mcp  # Pinned Inspector 2.14.0: initialize and tools/list only
just run status
just clean     # Remove build/ only; leaves legacy dist/ and user data untouched
```

Debug and Release graphs live in `build/cmake/Debug/` and `build/cmake/Release/`; only Release apps are emitted to `build/bin/`. Set `CXX` before the first configure to choose a compiler; CMake retains its absolute path even when lint uses a different `PATH`. Both configurations enable libc++ hardening and zero automatic-variable initialization; Debug uses fatal UBSan reports. Inspector isolates HOME/XDG directories under `build/`, clears credentials, disables error reporting and external CLI access, and cleans up afterward; it never invokes a tool or accesses Google APIs.

CI runs the same commands on native Linux/macOS. Configure `VCPKG_REF` to the tested full vcpkg commit SHA; slimr packages resolve from the public registry pinned in `vcpkg-configuration.json`, without a dependency token. Local macOS validation is not Linux execution evidence.
