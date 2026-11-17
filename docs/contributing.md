# gdocsmith-mcp: Contributing

User installation and configuration are in [README.md](../README.md). This reference preserves implementation details, alternative transport schemas, and validation commands. A client configuration example is not a claim of verified July/OAuth interoperability.

Dependencies resolve directly from the pinned `slimr-down-now/*` vcpkg git registries in `vcpkg-configuration.json` (no local overlays). CI configuration is not evidence that a runner/platform/client has passed.

<!-- cpp-templates:begin -->
## Contents

- [Develop and test](#develop-and-test)
- [Unreleased libraries](#unreleased-libraries)
- [Rules](#rules)
- [PR, CI, and release flow](#pr-ci-and-release-flow)
- [Maintainer setup](#maintainer-setup)
- [Dependency bumps](#dependency-bumps)

## Develop and test

```bash
just doctor
just test
just lint
```

Iterate on a single doctest case with `just test "<case>"` (Debug build with ASan + UBSan, `BUGSINK_DSN=""`). For `homebrew-mcp` repos, run `just test-mcp` as well.

## Unreleased libraries

vcpkg reads extra port dirs from `VCPKG_OVERLAY_PORTS`, and an overlay wins over the registry. Point it at a dir of symlinks, one per unreleased lib; manifests never change:

```sh
mkdir -p ~/.vcpkg/overlays/dev
ln -s ~/dev/slimr/<lib>/ports/slimr-<lib> ~/.vcpkg/overlays/dev/slimr-<lib>
export VCPKG_OVERLAY_PORTS=~/.vcpkg/overlays/dev
just test
```

Remove the symlink (or `unset VCPKG_OVERLAY_PORTS`) to build against the released lib again. Keep the export out of shell startup files.

## Rules

- Never push directly to `main`; all changes land through squash-merged pull requests.
- PRs touch no release files (`VERSION`, `CHANGELOG.md`, `versions/`, `Formula/*.rb`, `Casks/*.rb`).
- Write the PR title (plus optional body) as the changelog entry; squash-merge turns it into one commit on `main`.
- Sanitize everything (no secrets, tokens, DSNs, or personal data in commits, logs, or docs).
- Run `just test` (ASan + UBSan) and `just lint` before opening or updating a PR.

## PR, CI, and release flow

- Every PR runs `test.yml`: a `gate` job skips macOS/Linux builds on docs-only PRs, and a single `ci` summary job gates merge.
- Merging to `main` triggers `release.yml` (one release at a time). It collects squash commits since the last tag (skipping docs-only ranges), picks the bump from merged PR labels (`patch` default, or `release:minor` / `release:major`; agents must ask before adding labels), writes `VERSION`/`vcpkg.json`, `CHANGELOG.md` (and `vcpkg x-add-version` for libraries), commits as the GitHub App, tags `vX.Y.Z`, and publishes the GitHub Release (plus assets and `Formula/*.rb` / `Casks/*.rb` updates).
- Pre-releases are a known option (tag/channel), not currently built.

## Maintainer setup

Direct pushes to `main` are blocked; `release.yml` uses a dedicated GitHub App so it can commit release metadata and formula updates without giving humans or agents an admin bypass.

### 1. Create and install the release GitHub App

Create one GitHub App (Settings -> Developer settings -> GitHub Apps):

- **Homepage URL:** any URL (for example, the owner's GitHub profile)
- **Callback / Setup URLs:** blank; **Webhook Active:** unchecked
- **Repository permissions:** `Contents: Read and write`, `Metadata: Read-only`
- **Where can this GitHub App be installed?:** Any account

Record `<app-id>`, generate a private key, and save it locally if needed for manual recovery (it can always be regenerated):

```sh
chmod 600 ~/.ssh/<app-name>.private-key.pem
```

Install the App on each owner (org or user) and select the target repositories.

### 2. Store Actions secrets

Org-level (requires org owner and `admin:org` scope):

```sh
gh auth refresh -h github.com -s admin:org
gh secret set RELEASE_APP_ID --org <owner> --visibility all --body "<app-id>"
gh secret set RELEASE_APP_PRIVATE_KEY --org <owner> --visibility all < ~/.ssh/<app-name>.private-key.pem
gh secret list --org <owner>
```

Per-repo (for user-owned repos or selected repos):

```sh
gh secret set RELEASE_APP_ID -R <owner>/<repo> --body "<app-id>"
gh secret set RELEASE_APP_PRIVATE_KEY -R <owner>/<repo> < ~/.ssh/<app-name>.private-key.pem
gh secret list -R <owner>/<repo>
```

`gh secret list` prints secret names only.

### 3. Protect `main`

Create (or `PUT /repos/<owner>/<repo>/rulesets/<ruleset-id>` to update) the `main` ruleset with App-only bypass, required `ci` check, squash-only merges, and no force push or deletion, then enforce squash-only merges on the repo:

```sh
gh api -X POST /repos/<owner>/<repo>/rulesets --input - <<'JSON'
{
  "name": "main",
  "target": "branch",
  "enforcement": "active",
  "conditions": {
    "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] }
  },
  "bypass_actors": [
    { "actor_id": <app-id>, "actor_type": "Integration", "bypass_mode": "always" }
  ],
  "rules": [
    { "type": "deletion" },
    { "type": "non_fast_forward" },
    {
      "type": "pull_request",
      "parameters": {
        "required_approving_review_count": 0,
        "dismiss_stale_reviews_on_push": false,
        "require_code_owner_review": false,
        "require_last_push_approval": false,
        "required_review_thread_resolution": false,
        "allowed_merge_methods": ["squash"]
      }
    },
    {
      "type": "required_status_checks",
      "parameters": {
        "strict_required_status_checks_policy": false,
        "required_status_checks": [{ "context": "ci" }]
      }
    }
  ]
}
JSON

gh api -X PATCH /repos/<owner>/<repo> \
  -F allow_squash_merge=true \
  -F allow_merge_commit=false \
  -F allow_rebase_merge=false

gh api /repos/<owner>/<repo>/rulesets
```

### 4. Act as the App (emergency fix or manual release step)

Use [`scripts/release-bot`](https://github.com/slimr-down-now/cpp-templates/blob/main/scripts/release-bot) with `RELEASE_APP_ID` and `RELEASE_APP_KEY` set (under the hood it signs a short-lived `RS256` App JWT, exchanges it for an installation token, and revokes the token on exit):

```sh
export RELEASE_APP_ID=<app-id>
export RELEASE_APP_KEY=~/.ssh/<app-name>.private-key.pem

release-bot push-main                  # push HEAD to origin main as the App
release-bot gh release create ...      # run one gh command with GH_TOKEN set
release-bot shell                      # subshell with GH_TOKEN; revokes on exit
```

Flags before the subcommand: `--owner <owner>` (token scope; defaults to `origin` owner) and `--repo <owner>/<repo>` (`push-main` target; defaults to `origin`).

## Dependency bumps

Bump a library baseline in `vcpkg-configuration.json` only when a bug fix, new API, or security fix requires it; otherwise wait for the next `just upgrade-deps`.
<!-- cpp-templates:end -->

## Repository tips

Run the direct binary on a separate dev port:

```bash
./build/bin/gdocsmith-mcp http --port 20007
# Dev endpoint: http://127.0.0.1:20007/mcp
```

## App Architecture

The standalone MCP server (Streamable HTTP) is implemented in native C++26 on the `slimr-*` libraries.

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
│   ├── test-mcp.ts                 # Bun ephemeral HTTP MCP 2026-07-28 conformance check
│   ├── release.ts                  # Release version bump, changelog promotion, commit/tag/gh-release
│   └── formula-shared.ts           # Formula/changelog parsing and SHA-256 helpers
├── .clang-format / .clang-tidy     # Formatting and static-analysis config (llvm@23)
├── .clangd / .vscode/settings.json # Editor compilation database (build/cmake/Debug)
├── .cursor-plugin/plugin.json      # Cursor plugin manifest (HTTP 127.0.0.1:10007/mcp)
├── .claude-plugin/                 # Claude plugin & marketplace manifests (HTTP 127.0.0.1:10007/mcp)
├── src/
│   ├── main.cpp                    # Standalone server entrypoint (`http`), `--version`, and `--log-file`
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

- **`<mcp.hpp>`, `<mcp_http.hpp>` & `<reflect.hpp>` (`slimr-mcp`, `slimr-reflect`):** Typed MCP `2026-07-28` tool definitions, compile-time JSON Schema generation, Streamable HTTP transport (default `127.0.0.1:10007`, `/mcp` and `/tools/*`), `gdocsmith://guide` resource registration, `--version`, and `--skip-checks`.
- **`<yaml.hpp>` (`slimr-yaml`):** Frontmatter parsing (`parse_frontmatter`) and `comments.yaml` / `suggestions.yaml` serialization (`to_yaml`, `parse_yaml`).
- **`cpp-httplib` (`httplib::Client`):** Direct HTTPS calls to Google Docs API v1, Drive API v3, and Google OAuth2 token endpoint.
- **`<json.hpp>` (`slimr-json`):** Google Docs JSON AST parsing and `batchUpdate` payload serialization.
- **`<bugsink.hpp>` (`slimr-bugsink`):** Optional error reporting configured with `BUGSINK_DSN` and `release` (`gdocsmith-mcp@<VERSION>`).
- **`<test.hpp>` (`slimr-test`):** Unit, CLI, and `McpHttpServer` test suite compiled with `-fsanitize=address,undefined`.
- **OpenSSL / C++26 Standard Library:** Portable SHA-256 file integrity checks (`<openssl/evp.h>`), `<filesystem>`, `<mutex>`.

## Transports & MCP `2026-07-28`

`gdocsmith-mcp` implements the MCP **`2026-07-28`** specification over Streamable HTTP:

- **Breaking change (`2026-07-28`):** Session initialization (`initialize`), `notifications/initialized`, `MCP-Session-Id` headers, and SSE `GET`/`DELETE` streams are removed. Clients discover capabilities via `server/discover`, list tools via `tools/list`, read `gdocsmith://guide` via `resources/read` (or the fallback `mcp_resource_read` tool), and invoke tools via `tools/call`. On HTTP `POST /mcp`, callers must send `MCP-Protocol-Version: 2026-07-28` (and `Accept: application/json`), plus `Mcp-Method` (and `Mcp-Name` on `tools/call`). Calling `initialize` returns `-32601 Method not found`.
- **Streamable HTTP (default `127.0.0.1:10007`):**
  ```bash
  ./build/bin/gdocsmith-mcp http --port 20007
  # Dev endpoint: http://127.0.0.1:20007/mcp
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
just test-mcp          # Bun ephemeral HTTP MCP 2026-07-28 conformance check
just upgrade-deps      # Pull vcpkg, update registry baselines, and run tests
just clean             # Remove build/ only; leaves user data untouched
```

Debug and Release graphs live in `build/cmake/Debug/` and `build/cmake/Release/`; only Release binaries are emitted to `build/bin/`. For local development without touching the installed Homebrew service (`10007`), run `./build/bin/gdocsmith-mcp http --port 20007`.

`just test-mcp` isolates HOME/XDG directories under a temporary folder, passes `--skip-checks`, clears credentials, disables error reporting, and cleans up afterward without calling live Google APIs. Note: `just parity <run_id> <node>` runs offline Node-vs-C++ engine differential checks against a prebuilt `ParityChecks` adapter.

## Qualification History

Prior qualification runs on the C++26 engine verified `447` doctest cases (now extended with log rotation tests), isolated stdio/HTTP MCP discovery, and offline parity vectors; CI workflows (`test.yml` and `release.yml`) run on `macos-26` and `ubuntu-24.04` on pull requests and merges to `main`. Local macOS validation is not Linux execution evidence.
