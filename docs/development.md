# gdocsmith-mcp: Developer Reference

User installation, configuration and compatibility status are in the
[README](../README.md). This reference preserves implementation details,
alternative transport schemas and validation commands. A client configuration
example is not a claim of verified July/OAuth interoperability.

Current HTTP source builds use local slimr and libhv overlays. The pinned
published registry baseline predates these HTTP changes; do not claim that
baseline builds the current HTTP app without overrides. CI configuration is
not evidence that a runner/platform/client has passed.

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
│   ├── main.cpp                    # Standalone CLI, stdio (`mcp`), and Streamable HTTP (`http`) entrypoint
│   ├── app.hpp                     # `gdocsmith::make_gdocsmith_app()`, tool adapters, `gdocsmith://guide` resource
│   ├── api.hpp                     # Docs v1 & Drive v3 REST clients over `curl.hpp`, in-memory OAuth token manager
│   ├── model/                      # Google Docs JSON <-> DocModel AST
│   │   ├── from_json.hpp           # Docs JSON -> `DocModel` (`docModelParse`)
│   │   ├── to_json.hpp             # `RequestBuilder` batchUpdate object builders
│   │   ├── document.hpp            # Top-level model re-exports
│   │   └── …                       # element, tab, table, paragraph, style, lists, comments, suggestions,
│   │                               # equivalence, invariants, layout, object IDs, keys, walk, errors
│   ├── diff/                       # Myers diff and unified output
│   │   ├── myers.hpp               # O(ND) shortest edit script (`Hunk`)
│   │   ├── patch.hpp               # Applies hunks to reproduce the target sequence
│   │   ├── tree_diff.hpp           # Structural / tree-sequence diffing
│   │   ├── unified.hpp             # Unified-diff formatting
│   │   └── match.hpp, slice.hpp, types.hpp
│   ├── lens/                       # Extended Markdown envelope
│   │   ├── parse.hpp / render.hpp  # Inline parser and renderer
│   │   ├── markdown.hpp            # Parse + render umbrella
│   │   ├── document.hpp            # `DocumentFile` tab projection (`documentFileExport`)
│   │   ├── project.hpp / put.hpp   # Projection into and out of the model
│   │   └── …                       # frontmatter, directives, tables, cell refs/styles, images, links,
│   │                               # named ranges/styles, comments & suggestions serializers, tokens
│   ├── reconcile/                  # Model/lens intent -> back-to-front batchUpdate requests
│   │   ├── reconcile.hpp           # Plan structures, segment/footnote request handling, self-check
│   │   ├── requests.hpp            # Request-object helpers
│   │   └── container.hpp, context.hpp, lists.hpp, paragraph.hpp, tables.hpp
│   ├── emulator/                   # In-memory batchUpdate simulator and conformance runner
│   │   ├── document_state.hpp / document_mutator.hpp / document_emulator.hpp
│   │   ├── google_docs_emulator.hpp / fake_google.hpp
│   │   ├── field_mask.hpp / emulate.hpp / emulate_tables.hpp / types.hpp
│   │   └── conformance.hpp         # Fixture-driven conformance checks
│   └── engine/                     # Orchestration (no direct network I/O)
│       ├── artifacts.hpp           # Work-root resolution, atomic writes, SHA-256 snapshots, `WriteTracker`
│       ├── load.hpp, guard.hpp, plan.hpp
│       ├── session.hpp, transaction.hpp, flush.hpp
│       └── export.hpp, import.hpp
├── tests/
│   ├── test_main.cpp               # Combined doctest runner (the CMake test target)
│   ├── model/, diff/, lens/, reconcile/, emulator/, engine/, api/
│   │                               # Header-only suites, one directory per `src/` module
│   ├── emulator/test_emulator_main.cpp   # Focused emulator-only runner
│   ├── reconcile/test_reconcile_main.cpp # Focused reconciler-only runner
│   └── fixtures/conformance/       # JSON conformance fixtures (C01a … M8, N1 … X7)
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
