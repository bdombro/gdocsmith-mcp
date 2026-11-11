# gdocsmith-mcp

```
 ▄▄▄  ▄▄█  ▄▄   ▄▄▄  ▄▄▄ ▄▄▄▄  ▀ ▄█▄  █▄▄  
█  █ █  █ █  █ █    ▀▀▄▄ █ █ █ █  █   █  █ 
 ▀▀█  ▀▀▀  ▀▀   ▀▀▀ ▀▀▀  ▀ ▀ ▀ ▀   ▀▀ ▀  ▀ 
 ▀▀                                        
```

An MCP server and CLI for Google Docs editing through local, extended Markdown + YAML files. Agents edit content; gdocsmith calculates minimal paragraph- and character-level changes, guards protected content, revision-locks writes, and verifies the result.

Compared with generic Google connectors and raw API wrappers, the file-based workflow gives agents room to do complex edits while keeping changes focused: less context bloat, fewer round trips, and fewer accidental overwrites.

> **Client compatibility:** MCP **2026-07-28 only**; legacy clients cannot connect.
> The workspace has been exercised with OpenCode v2.2.6 using explicit July
> configuration. Cursor's July support is deferred; Claude Code July runtime
> acceptance is not yet verified. Shipping a plugin manifest is not proof of
> client compatibility.

## Try It

After setup, ask your agent:

> Export this Google Doc, tighten the introduction, then import the focused edit after my approval.

## Features

- Export documents into readable, editable Markdown, including tabs, comments, and suggestions.
- Import focused edits with revision checks, explicit safety guards, and post-write verification.
- Copy complex documents losslessly when unsupported objects must be preserved.
- Small footprint: a standalone ARM64/x64 binary exposing CLI, stdio MCP, and Streamable HTTP (`127.0.0.1:8087`); no Node/V8 runtime.

## Installation

- **Prebuilt release:** the checked-in [Homebrew formula](Formula/gdocsmith-mcp.rb)
  currently has a placeholder checksum. It is not a ready-to-install release.
  Check [releases](https://github.com/bdombro/gdocsmith-mcp/releases) for published
  binaries and a finalized formula before using a release installation command.
- **Build from source:** the current path is [Development](#development).
  Local slimr overlays are for development, not a runtime requirement.
- **Already installed:** follow the setup and service steps below. Installing a
  Cursor/Claude manifest only configures an endpoint; it does not install or
  start the native service.


## Configure Google Access and Local Editing

Install the [Google Workspace CLI](https://github.com/googleworkspace/cli)
(`npm install -g @googleworkspace/cli`) and configure an OAuth profile with
Docs, Drive and any other scopes needed for your operations.
The server obtains credentials from that local profile; it does not accept
Google access tokens from the MCP client.

**This is a developer-style file workflow:** the agent must be able to read
and edit files on the server's machine (or a deliberately shared work directory).
A remote MCP connection alone does not provide access to exported files.
Choose `GDOCSMITH_WORK_DIR` in the server environment if the default workspace
location is unsuitable. See [Workflow](#workflow) and
[Authentication & Security Boundary](#authentication--security-boundary).


## Start and Manage the Service

For an already-installed Homebrew formula:

```bash
brew services start bdombro/gdocsmith-mcp/gdocsmith-mcp
brew services info gdocsmith-mcp
brew services restart bdombro/gdocsmith-mcp/gdocsmith-mcp
brew services stop bdombro/gdocsmith-mcp/gdocsmith-mcp
```

The endpoint is `http://127.0.0.1:8087/mcp`; logs are under
`/opt/homebrew/var/log/`. To run in the foreground instead, use
`gdocsmith-mcp http` (or `./build/bin/gdocsmith-mcp http` from a source checkout).
Do not run the foreground server and service on the same port.
Keep the default loopback bind; this is not a shared remote-host deployment.


## Connect Your Agent

After starting the service, add this server to OpenCode v2.2.6's
`opencode.jsonc` (merge it with any existing servers):

```json
{
  "mcp": {
    "servers": {
      "gdocsmith-mcp": {
        "type": "remote",
        "url": "http://127.0.0.1:8087/mcp",
        "protocol": "2026-07-28"
      }
    }
  }
}
```

Use either an explicit server entry or a plugin manifest, not both.
Alternative client schemas and stdio configuration are in the
[developer reference](docs/development.md). The compatibility notice above
applies to every transport.


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

## Troubleshooting

- **Client cannot connect:** confirm it supports July MCP, the service is
  running, and the client URL matches the configured port. OpenCode needs
  `"protocol": "2026-07-28"`; old `initialize` clients will fail.
- **Port occupied:** inspect the listener and stop only the service/process
  you own, or deliberately choose another port and update the client URL.
- **Works in a terminal, not as a service:** launchd does not inherit
  `~/.zshrc`. Configure the service environment separately or use the
  plugin's supported private configuration file; inspect
  `/opt/homebrew/var/log/` for errors.
- **Google authentication fails:** verify gws and its selected profile in the server environment.
- **Import reports staleBase:** reapply edits to the fresh export; do not remove revision metadata or bypass the guard.
- **Agent cannot open exportPath:** this workflow requires filesystem access to the server's work directory.
- **Source build cannot find vcpkg or slimr ports:** load the development
  environment, bootstrap vcpkg, and register both slimr links and the pinned
  libhv overlay. Never repair this by editing installed headers.
- **Compiler/hardening check fails:** upgrade the Xcode toolchain; set
  `CXX` before first configure. CMake caches the compiler, so an intentional
  compiler change needs a fresh configuration tree.


## Development

This section is for building or changing the server, not configuring an
installed binary. [Architecture, transport reference and test details](docs/development.md)
are separate from the user setup flow.

<details>
<summary>macOS source-build setup (vcpkg and local slimr overlays)</summary>

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

</details>

After the source build succeeds, stage and install the local Homebrew formula:

```bash
just install-local
```

Installing a formula and starting its service are separate actions.
For a source-only foreground run, use
`./build/bin/gdocsmith-mcp http`.
