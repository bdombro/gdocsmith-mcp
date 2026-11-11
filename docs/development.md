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
