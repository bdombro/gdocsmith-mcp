# gdocsmith-mcp

```
 ▄▄▄  ▄▄█  ▄▄   ▄▄▄  ▄▄▄ ▄▄▄▄  ▀ ▄█▄  █▄▄  
█  █ █  █ █  █ █    ▀▀▄▄ █ █ █ █  █   █  █ 
 ▀▀█  ▀▀▀  ▀▀   ▀▀▀ ▀▀▀  ▀ ▀ ▀ ▀   ▀▀ ▀  ▀ 
 ▀▀                                        
```

An MCP server and CLI for Google Docs editing through local, extended Markdown + YAML files. Agents edit content; gdocsmith calculates minimal paragraph- and character-level changes, guards protected content, revision-locks writes, and verifies the result.

Compared with generic Google connectors and raw API wrappers, the file-based workflow gives agents room to do complex edits while keeping changes focused: less context bloat, fewer round trips, and fewer accidental overwrites.

## Features

- Export documents into readable, editable Markdown, including tabs, comments, and suggestions.
- Import focused edits with revision checks, explicit safety guards, and post-write verification.
- Copy complex documents losslessly when unsupported objects must be preserved.
- Small footprint: a standalone ARM64/x64 binary exposing CLI, stdio MCP, and Streamable HTTP (`127.0.0.1:8087`); no Node/V8 runtime.

## Transports & MCP `2026-07-28`

`gdocsmith-mcp` implements the MCP **`2026-07-28`** specification over both stdio and Streamable HTTP:

- **Breaking change (`2026-07-28`):** Session initialization (`initialize`), `notifications/initialized`, `MCP-Session-Id` headers, and SSE `GET`/`DELETE` streams are removed. Clients discover capabilities via `server/discover`, list tools via `tools/list`, read `gdocsmith://guide` via `resources/read` (or the fallback `mcp_resource_read` tool), and invoke tools via `tools/call`. On HTTP `POST /mcp`, callers must send `MCP-Protocol-Version: 2026-07-28` (and `Accept: application/json`), plus `Mcp-Method` (and `Mcp-Name` on `tools/call`). Calling `initialize` returns `-32601 Method not found`.
- **Streamable HTTP (default port `8087`):**
  ```bash
  ./build/bin/gdocsmith-mcp http --host 127.0.0.1 --port 8087
  # Endpoint: http://127.0.0.1:8087/mcp
  ```
- **Stdio MCP (`mcp`):**
  ```json
  {
    "mcpServers": {
      "gdocsmith-mcp": {
        "command": "/absolute/path/to/gdocsmith-mcp",
        "args": ["mcp"],
        "env": {
          "GWS_PROFILE": "default"
        }
      }
    }
  }
  ```
- **HTTP client configuration:**
  ```json
  {
    "mcpServers": {
      "gdocsmith-mcp": {
        "type": "http",
        "url": "http://127.0.0.1:8087/mcp"
      }
    }
  }
  ```
- **Cursor / Claude plugins:** `.cursor-plugin/plugin.json`, `.claude-plugin/plugin.json`, and `.claude-plugin/marketplace.json` configure the HTTP server at `http://127.0.0.1:8087/mcp`. Note that host clients using these manifests must support MCP `2026-07-28` Streamable HTTP.

## App Architecture

The standalone MCP server (stdio + Streamable HTTP) and JSON CLI are implemented in native C++26 on `slimr-cpp`; Google API access uses `libcurl`.

```text
plugins/gdocsmith-mcp/
├── vcpkg.json                      # Independently resolved dependencies (slimr-mcp[http])
├── CMakeLists.txt                  # App/test targets and transitive dependency linking (slimr::mcp_http)
├── justfile                        # CMake/Ninja, lint, test-mcp, and Homebrew service lifecycle
├── Formula/gdocsmith-mcp.rb        # Homebrew formula + launchd service definition (127.0.0.1:8087)
├── scripts/
│   ├── dev-formula.mjs             # Local tarball formula staging helper (binary + docs)
│   └── test-mcp.mjs                # Stdio + ephemeral HTTP MCP 2026-07-28 conformance check
├── .cursor-plugin/plugin.json      # Cursor plugin manifest (HTTP 127.0.0.1:8087/mcp)
├── .claude-plugin/                 # Claude plugin & marketplace manifests (HTTP 127.0.0.1:8087/mcp)
├── src/
│   ├── engine.hpp                  # Google Docs JSON AST <-> Extended Markdown/YAML converter & Myers diff (`reconcile_json`)
│   ├── api.hpp                     # Docs v1 & Drive v3 REST client over `curl.hpp`, in-memory OAuth token manager, `WriteTracker`
│   ├── storage.hpp                 # Work-root resolution, `0700` dir / `0600` `.gitignore`, atomic `file_replace`, SHA-256 snapshot checks
│   ├── app.hpp                     # `gdocsmith::make_gdocsmith_app()`, per-doc FIFO queue, `gdocsmith://guide` resource, Bugsink DSN
│   └── main.cpp                    # Standalone CLI, stdio (`mcp`), and Streamable HTTP (`http`) entrypoint
├── tests/
│   ├── test_engine.hpp             # AST, Markdown, YAML, and Myers diff test cases
│   ├── test_storage.hpp            # Hashing, storage, permissions, CLI/MCP 2026-07-28, and HTTP concurrency test cases
│   └── test_main.cpp               # Combined test runner
└── build/bin/
    └── gdocsmith-mcp               # Standalone release binary
```

### Libraries Used

- **`<mcp.hpp>`, `<mcp_http.hpp>` & `<reflect.hpp>`:** Typed MCP `2026-07-28` tool definitions, compile-time JSON Schema generation, Streamable HTTP transport (`127.0.0.1:8087`), `gdocsmith://guide` resource registration, and narrow JSON CLI.
- **`<yaml.hpp>` (`rapidyaml`):** Frontmatter parsing (`parse_frontmatter`) and `comments.yaml` / `suggestions.yaml` serialization (`to_yaml`, `parse_yaml`).
- **`<curl.hpp>` (`-lcurl`):** Direct HTTPS calls to Google Docs API v1, Drive API v3, and Google OAuth2 token endpoint.
- **`<json.hpp>` (`glaze`):** Google Docs JSON AST parsing and `batchUpdate` payload serialization.
- **`<bugsink.hpp>` (`-lcurl`):** Optional error reporting configured with `BUGSINK_DSN`.
- **`<test.hpp>` (`doctest`):** Unit, CLI, and `McpHttpServer` test suite compiled with `-fsanitize=address,undefined`.
- **OpenSSL / C++26 Standard Library:** Portable SHA-256 file integrity checks (`<openssl/evp.h>`), `<filesystem>`, `<mutex>`.

## Workflow

1. **Lossless copy (when needed):** Call `file` with `action: "copy"` when duplicating a document that contains opaque/unsupported objects that must be preserved verbatim.
2. **Export:** Call `export` with a document ID or Google Docs URL. Read or search the returned Markdown file (`exportPath`).
3. **Edit locally:** Modify the Markdown file using your standard file-editing tools.
4. **Import:** Call `import` with the absolute file path. Check `status`, `writeState`, `verification`, `warnings`, and `errors`, and use the newly returned `exportPath` as the base for any subsequent edits. Unchanged imports are verified no-ops (`0` API write requests).

## Tools & CLI Commands

Every tool is exposed over MCP (`gdocsmith-mcp mcp` or `gdocsmith-mcp http`) and on the CLI (`gdocsmith-mcp <tool> ['{"arg":"val"}']`). The MCP server also serves the complete syntax and safety specification as the `gdocsmith://guide` resource (readable via `resources/read` or the `mcp_resource_read` tool). When installed via Homebrew, `docs/agent-guide.md` is installed into `pkgshare` and resolved relative to the executable.

| Tool | Mutating | Description |
| --- | --- | --- |
| `export` | No | Export a Google Doc by ID or URL into `<workDir>/exports/<docId>/<revisionId>/` (`base.md`, `next.md`, `comments.yaml`, `suggestions.yaml`), preserving any unimported local edits in `next.md`. |
| `import` | Yes | Reconcile an edited Markdown file against its revision-locked `base.md`, enforce safety guards (`allow` flags), apply minimal `batchUpdate` + Drive comment operations, and write a fresh post-import export. |
| `file` | Yes | Manage Drive files: `copy`, `trash`, `delete` (requires `allow: ["delete"]`), or manage sharing permissions (`grant` requires `allow: ["share"]`; public `anyone`, `domain`, and ownership transfers are unconditionally refused). |
| `status` | No | Returns plugin version (`4.0.0`) and active workspace storage directory without calling Google APIs. |

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
gdocsmith: "4.0.0"
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

## Authentication & Security Boundary

`gdocsmith-mcp` runs in **loopback single-user mode** (`127.0.0.1:8087`) protected by loopback binding plus `Host`/`Origin` DNS-rebinding checks in `slimr::mcp_http`:

- Credentials are obtained locally via `gws auth export --unmasked` (using your host `gws` CLI profile) and exchanged in memory against `https://oauth2.googleapis.com/token` over `slimr::curl`. Access tokens stay in process memory and are never written to disk.
- `gdocsmith-mcp` does **not** run a persistent HTTP Google token broker database, does **not** accept external Google OAuth tokens over HTTP, and does **not** enable or claim standard MCP OAuth (`oauth.enabled = false`).
- Because `brew services` runs under `launchd` with a minimal environment, [`Formula/gdocsmith-mcp.rb`](Formula/gdocsmith-mcp.rb) sets `PATH` to `#{HOMEBREW_PREFIX}/bin:/usr/bin:/bin` so `gws` resolves automatically when installed via Homebrew. Set `GWS_PROFILE`, `GWS_BIN`, or `GDOCSMITH_WORK_DIR` in your service/environment as needed.

| Variable | Default | Purpose |
| --- | --- | --- |
| `GDOCSMITH_WORK_DIR` | `<workspace>/.gdocsmith` | Directory for exported Markdown revisions (`exports/<docId>/<revId>/`) |
| `GWS_PROFILE` | Active `gws` profile | Profile selected by `gws` when exporting OAuth credentials |
| `GWS_BIN` | `gws` | Path or name of the `gws` CLI executable |
| `BUGSINK_DSN` | Unset (disabled) | Error reporting DSN (`""` disables) |

## Homebrew & Service Lifecycle

`Formula/gdocsmith-mcp.rb` installs `gdocsmith-mcp` + `docs/` and registers a background service listening on `http://127.0.0.1:8087/mcp` (logs at `$(brew --prefix)/var/log/gdocsmith-mcp.log`):

```bash
just install          # Install formula (or install-local / reinstall-local from a local build)
just start            # Start brew service and wait until http://127.0.0.1:8087/mcp responds
just status           # Check brew services status
just restart          # Restart service and wait for readiness
just stop             # Stop brew service
just uninstall        # Stop service and uninstall formula
just check-formula    # Validate Formula/gdocsmith-mcp.rb Ruby syntax
```

## macOS Build Prerequisites (vcpkg setup)

Install [Homebrew](https://brew.sh) and current Xcode Command Line Tools
(`xcode-select --install`; finish installation before continuing), or full Xcode.
Apple Clang 21 is the verified C++26/libc++ toolchain; older installations need
upgrading. Run this from the repository root. Set `SLIMR_CPP` to your existing
workspace checkout if you have one; otherwise this creates a sibling checkout.
These instructions assume Apple Silicon Homebrew at `/opt/homebrew`.

Add this environment snippet to `~/.zshrc`, then open a new terminal (or run
`source ~/.zshrc`). Use `~/.zshrc`, not `~/.zshenv`, to avoid changing
non-interactive zsh scripts:

`local-ports` is the dedicated vcpkg overlay directory for local development
linking: `just publish --local --all` registers symlinks to your slimr-cpp checkout,
overriding published packages without making a registry release. See the
[publishing guide on slimr-cpp](https://github.com/bdombro/slimr-cpp/blob/main/docs/publishing.md#local-environment).

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
export VCPKG_OVERLAY_PORTS="$VCPKG_ROOT/local-ports"
export SLIMR_CPP="${SLIMR_CPP:-$HOME/dev/bdombro/slimr-cpp}"
export CXX="/usr/bin/clang++"
export CLANG_TIDY="/opt/homebrew/opt/llvm/bin/clang-tidy"
export PATH="$VCPKG_ROOT:/opt/homebrew/opt/ruby/bin:/opt/homebrew/bin:$PATH"
```

Then install and build from the repository root:

```bash
brew install cmake ninja just llvm node ruby autoconf automake libtool pkgconf
mkdir -p "$(dirname "$VCPKG_ROOT")"
if [ ! -d "$VCPKG_ROOT/.git" ]; then
  git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
fi
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
mkdir -p "$(dirname "$SLIMR_CPP")"
if [ ! -d "$SLIMR_CPP/.git" ]; then
  git clone https://github.com/bdombro/slimr-cpp.git "$SLIMR_CPP"
fi
(cd "$SLIMR_CPP" && just publish --local --all)
ln -s "$SLIMR_CPP/ports/libhv" "$VCPKG_OVERLAY_PORTS/libhv"

"$CXX" --version
vcpkg version
BUGSINK_DSN="" just test
just build
```

The libhv link is a one-time step: if it exists, verify that it points to the
chosen checkout's `ports/libhv` before skipping the command. Never overwrite
a conflicting port directory or modify installed headers/caches. Current HTTP
builds need both local slimr overlays and the separately pinned libhv port;
the older published registry baseline alone is not an HTTP build substitute.
`just deps` installs manifest dependencies into `build/deps/`; vcpkg supplies
curl and OpenSSL, so separate Homebrew library installs are unnecessary.

Bootstrapping puts the executable at `$VCPKG_ROOT/vcpkg`, with no `bin/`
subdirectory. Keg-only Homebrew Ruby supplies `/opt/homebrew/opt/ruby/bin/ruby`.
Homebrew LLVM supplies lint tooling; `CXX` explicitly selects the Apple compiler.
Set a different compatible `CXX` before first configure; CMake caches it.
For reproducible vcpkg tooling, check out the tested full `VCPKG_REF` commit
before bootstrapping; package registry baselines pin resolution separately.
For live Google access, separately install the
[Google Workspace CLI](https://github.com/googleworkspace/cli) using
`npm install -g @googleworkspace/cli` and configure its OAuth profile.
Build/test commands neither authenticate Google nor start services.

## Building & Testing

Use Clang with C++26 and libc++ FAST hardening, CMake 3.31+, Ninja, [just](https://just.systems), a bootstrapped [vcpkg](https://vcpkg.io), Node, Ruby, and a compatible `clang-tidy` (set `CLANG_TIDY` if needed). Set `VCPKG_ROOT` and put vcpkg and the tools on `PATH`. Linux additionally needs `libc++` and `libc++abi`; select libc++ with `CXXFLAGS=-stdlib=libc++`.

Dependencies are declared in `vcpkg.json` (`slimr-mcp` with the `http` feature). During local development, `(cd /absolute/path/to/slimr-cpp && just publish --local --all)` publishes ports into vcpkg's local overlay; export `VCPKG_OVERLAY_PORTS="$VCPKG_ROOT/local-ports"` in your shell. `just deps` installs into `build/deps/` for the detected native triplet. `configure` always runs it, with CMake manifest installation disabled. CMake links installed `slimr::mcp_http`, `slimr::yaml`, `slimr::test` and `OpenSSL::Crypto` targets, never source-header overrides.

```bash
just test      # Debug ASan/UBSan AST, diff, isolated-storage, mock-API and HTTP tests
just build     # Optimized IPO Release app and tests; build/bin/gdocsmith-mcp
just lint      # App and test translation units using the Debug compilation database (honors $CLANG_TIDY)
just test-mcp  # MCP 2026-07-28 conformance: stdio (mcp) + ephemeral HTTP (server/discover & tools/list)
just run status
just clean     # Remove build/ only; leaves user data untouched
```

Debug and Release graphs live in `build/cmake/Debug/` and `build/cmake/Release/`; only Release apps are emitted to `build/bin/`. Set `CXX` before the first configure to choose a compiler; CMake retains its absolute path even when lint uses a different `PATH`. Both configurations enable libc++ hardening and zero automatic-variable initialization; Debug uses fatal UBSan reports. `just test-mcp` isolates HOME/XDG directories under `build/`, clears credentials, disables error reporting and external CLI access, and cleans up afterward; it never invokes a tool or accesses live Google APIs.

CI runs the same commands on native Linux/macOS. Configure `VCPKG_REF` to the tested full vcpkg commit SHA; slimr packages resolve from the public registry pinned in `vcpkg-configuration.json`, without a dependency token. Local macOS validation is not Linux execution evidence.
