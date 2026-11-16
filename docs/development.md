# gdocsmith-mcp: Developer Reference

User installation, configuration and compatibility status are in the
[README](../README.md). This reference preserves implementation details,
alternative transport schemas and validation commands. A client configuration
example is not a claim of verified July/OAuth interoperability.

Dependencies resolve directly from the pinned `slimr-down-now/*` vcpkg git
registries in `vcpkg-configuration.json` (no local overlays). CI configuration
is not evidence that a runner/platform/client has passed.

## App Architecture

The standalone MCP server (stdio + Streamable HTTP) and JSON CLI are implemented in native C++26 on the `slimr-*` libraries; Google API access uses `libcurl`.

```text
plugins/gdocsmith-mcp/
├── VERSION                         # Compiled into binary via CMakeLists.txt
├── CHANGELOG.md                    # Release notes (## Unreleased)
├── vcpkg.json                      # Independently resolved dependencies (slimr-mcp[http])
├── vcpkg-configuration.json        # Pinned vcpkg builtin and slimr-* git registries
├── CMakeLists.txt                  # App/test targets and transitive dependency linking (slimr::mcp_http)
├── justfile                        # CMake/Ninja, doctor, fmt, lint, test-mcp, release, and brew service lifecycle
├── Formula/gdocsmith-mcp.rb        # Homebrew formula (macOS/Linux) + service definition + env file loader
├── .github/
│   ├── actions/setup/action.yml    # Toolchain & vcpkg bootstrap + binary cache
│   └── workflows/{test,release}.yml # PR/manual test workflow and tag/formula release workflow
├── scripts/
│   ├── doctor.sh                   # Tooling, C++26, LLVM_BIN, VCPKG_ROOT, and Linux linger checks
│   ├── test-mcp.ts                 # Bun stdio + ephemeral HTTP MCP 2026-07-28 conformance check
│   ├── release.ts                  # Release version bump, changelog promotion, commit/tag/gh-release
│   └── formula-shared.ts           # Formula/changelog parsing and SHA-256 helpers
├── .clang-format / .clang-tidy     # Formatting and static-analysis config (llvm@23)
├── .clangd / .vscode/settings.json # Editor compilation database (build/cmake/Debug)
├── .cursor-plugin/plugin.json      # Cursor plugin manifest (HTTP 127.0.0.1:10007/mcp)
├── .claude-plugin/                 # Claude plugin & marketplace manifests (HTTP 127.0.0.1:10007/mcp)
├── src/
│   ├── main.cpp                    # Standalone CLI, stdio (`mcp`), Streamable HTTP (`http`), and `--log-file`
│   ├── log_file.hpp                # Stdout/stderr redirect and 10 MiB rotation (`<path>` -> `<path>.1`)
│   ├── app.hpp                     # `gdocsmith::make_gdocsmith_app()`, tool adapters, `gdocsmith://guide` resource
│   ├── api.hpp                     # Docs v1 & Drive v3 REST clients over `cpp-httplib`, in-memory OAuth token manager
│   ├── model/                      # Google Docs JSON <-> DocModel AST
│   ├── diff/                       # Myers diff and unified output
│   ├── lens/                       # Extended Markdown envelope
│   ├── reconcile/                  # Model/lens intent -> back-to-front batchUpdate requests
│   ├── emulator/                   # In-memory batchUpdate simulator and conformance runner
│   └── engine/                     # Orchestration, `$(brew --prefix)/var/gdocsmith-mcp` fallback, SHA-256 snapshots
├── tests/
│   ├── test_main.cpp               # Combined doctest runner (the CMake test target)
│   ├── test_log_file.hpp           # Log rotation tests (tiny threshold)
│   ├── model/, diff/, lens/, reconcile/, emulator/, engine/, api/
│   └── fixtures/conformance/       # JSON conformance fixtures (C01a … M8, N1 … X7)
└── build/bin/
    └── gdocsmith-mcp               # Standalone release binary
```

### Libraries Used

- **`<mcp.hpp>`, `<mcp_http.hpp>` & `<reflect.hpp>` (`slimr-mcp`, `slimr-reflect`):** Typed MCP `2026-07-28` tool definitions, compile-time JSON Schema generation, Streamable HTTP transport (default `0.0.0.0:10007`), `gdocsmith://guide` resource registration, `--version`, `--skip-checks`, and narrow JSON CLI.
- **`<yaml.hpp>` (`slimr-yaml`):** Frontmatter parsing (`parse_frontmatter`) and `comments.yaml` / `suggestions.yaml` serialization (`to_yaml`, `parse_yaml`).
- **`cpp-httplib` (`httplib::Client`):** Direct HTTPS calls to Google Docs API v1, Drive API v3, and Google OAuth2 token endpoint.
- **`<json.hpp>` (`slimr-json`):** Google Docs JSON AST parsing and `batchUpdate` payload serialization.
- **`<bugsink.hpp>` (`slimr-bugsink`):** Optional error reporting configured with `BUGSINK_DSN` and `release` (`gdocsmith-mcp@<VERSION>`).
- **`<test.hpp>` (`slimr-test`):** Unit, CLI, and `McpHttpServer` test suite compiled with `-fsanitize=address,undefined`.
- **OpenSSL / C++26 Standard Library:** Portable SHA-256 file integrity checks (`<openssl/evp.h>`), `<filesystem>`, `<mutex>`.

## Transports & MCP `2026-07-28`

`gdocsmith-mcp` implements the MCP **`2026-07-28`** specification over both stdio and Streamable HTTP:

- **Breaking change (`2026-07-28`):** Session initialization (`initialize`), `notifications/initialized`, `MCP-Session-Id` headers, and SSE `GET`/`DELETE` streams are removed. Clients discover capabilities via `server/discover`, list tools via `tools/list`, read `gdocsmith://guide` via `resources/read` (or the fallback `mcp_resource_read` tool), and invoke tools via `tools/call`. On HTTP `POST /mcp`, callers must send `MCP-Protocol-Version: 2026-07-28` (and `Accept: application/json`), plus `Mcp-Method` (and `Mcp-Name` on `tools/call`). Calling `initialize` returns `-32601 Method not found`.
- **Streamable HTTP (default `0.0.0.0:10007`):**
  ```bash
  ./build/bin/gdocsmith-mcp http --port 20007
  # Dev endpoint: http://127.0.0.1:20007/mcp
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
        "url": "http://127.0.0.1:10007/mcp"
      }
    }
  }
  ```
- **Cursor / Claude plugins:** `.cursor-plugin/plugin.json`, `.claude-plugin/plugin.json`, and `.claude-plugin/marketplace.json` configure the HTTP server at `http://127.0.0.1:10007/mcp`. Host clients using these manifests must support MCP `2026-07-28` Streamable HTTP.

## Building & Testing

Prerequisites (`just doctor` checks all of them):

- **macOS:** Homebrew `llvm@23` (`clang++` with `-stdlib=libc++`, static `libc++` in Release builds), `cmake`, `ninja`, `just`, `bun`, `gh`, `jq`, and bootstrapped `vcpkg` (`VCPKG_ROOT` set).
- **Linux (`ubuntu-24.04`):** Homebrew on Linux (`llvm@23` provides `clang++` with `-stdlib=libc++`, static libc++ in Release builds), `ninja`, `just`, `bun`, `gh`, `jq`, bootstrapped `vcpkg`, and `sudo loginctl enable-linger "$USER"` for `brew services`.

Set `LLVM_BIN` to override the `$(brew --prefix llvm@23)/bin` path used by `just configure`, `just deps`, `just fmt`, and `just lint`. Set `CXX` before first configure to override the compiler.

```bash
just doctor            # Verify tools, C++26 compiler, LLVM_BIN, VCPKG_ROOT, and Linux linger
just test              # Debug ASan/UBSan test suite (BUGSINK_DSN="")
just test "<filter>"   # Build test target and run matching doctest cases
just build             # Optimized IPO Release binary at build/bin/gdocsmith-mcp (no tests)
just fmt               # Format C++ sources and justfile
just lint              # Format check + clang-tidy
just test-mcp          # Bun stdio + ephemeral HTTP MCP 2026-07-28 conformance check
just upgrade-deps      # Pull vcpkg, update registry baselines, and run tests
just release patch     # Trigger release.yml (test -> build -> tag -> GitHub release) and pull
just clean             # Remove build/ only; leaves user data untouched
```

Debug and Release graphs live in `build/cmake/Debug/` and `build/cmake/Release/`; only Release binaries are emitted to `build/bin/`. For local development without touching the installed Homebrew service (`10007`), run `./build/bin/gdocsmith-mcp http --port 20007`.

`just test-mcp` isolates HOME/XDG directories under a temporary folder, passes `--skip-checks`, clears credentials, disables error reporting, and cleans up afterward without calling live Google APIs. Note: `just parity <run_id> <node>` runs offline Node-vs-C++ engine differential checks against a prebuilt `ParityChecks` adapter.

## Qualification History

Prior qualification runs on the C++26 engine verified `447` doctest cases (now extended with log rotation tests), isolated stdio/HTTP MCP discovery, and offline parity vectors; CI workflows (`test.yml` and `release.yml`) run on `macos-15` and `ubuntu-24.04` on pull requests or manual `just release` dispatch. Local macOS validation is not Linux execution evidence.
