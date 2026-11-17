# gdocsmith-mcp

```
 ▄▄▄  ▄▄█  ▄▄   ▄▄▄  ▄▄▄ ▄▄▄▄  ▀ ▄█▄  █▄▄  
█  █ █  █ █  █ █    ▀▀▄▄ █ █ █ █  █   █  █ 
 ▀▀█  ▀▀▀  ▀▀   ▀▀▀ ▀▀▀  ▀ ▀ ▀ ▀   ▀▀ ▀  ▀ 
 ▀▀                                        
```

An MCP server for Google Docs workflows through local, extended Markdown + YAML files.

Compared with generic Google connectors and raw API wrappers, the file-based workflow gives agents room to do complex edits while keeping changes focused: less context bloat, fewer round trips, and fewer accidental overwrites.

> **Client compatibility:** MCP **2026-07-28 only**; legacy clients cannot connect.
> OpenCode v2.2.6 works with explicit July configuration. Cursor and Claude
> plugin manifests ship in this repository, though their July runtime/OAuth
> acceptance is not yet verified.

## Try It

After setup, ask your agent:

> Export this Google Doc, make the requested supported edits, then import the file and report verification and warnings.

## Features

- Export rich Markdown with identities and review context; edit supported rich intent with explicit guards and unsupported-operation refusals.
- Import supported changes through revision-locked, guarded transactions and authoritative readback; preserve intended edits during failure recovery.
- Copy complex documents losslessly when unsupported objects must be preserved.
- Standalone ARM64 macOS and x64 Linux binary exposing Streamable HTTP (`127.0.0.1:10007`); no Node/V8 runtime.

## Install

```bash
brew tap bdombro/gdocsmith-mcp https://github.com/bdombro/gdocsmith-mcp
brew install bdombro/gdocsmith-mcp/gdocsmith-mcp
```

Note: check [releases](https://github.com/bdombro/gdocsmith-mcp/releases) for published binaries before running `brew install`; if no release is published yet, build from source per [contributing](docs/contributing.md).

## Configure Google Access and Local Editing

Install the [Google Workspace CLI](https://github.com/googleworkspace/cli)
(`npm install -g @googleworkspace/cli`) and configure an OAuth profile with
Docs, Drive and any other scopes needed for your operations.
The server obtains credentials from that local profile; it does not accept
Google access tokens from the MCP client.

**This is a file-based workflow:** the agent must be able to read and edit
files on the server's machine (or a shared work directory). Set
`GDOCSMITH_WORK_DIR` in the server environment if the default workspace or
Homebrew data directory is unsuitable. See [Workflow](#workflow) and
[Authentication](#authentication).

## Start and Manage the Service

```bash
brew services start gdocsmith-mcp
brew services info gdocsmith-mcp
brew services restart gdocsmith-mcp
brew services stop gdocsmith-mcp
```

The HTTP server binds `127.0.0.1:10007` (`http://127.0.0.1:10007/mcp`; pass `--host 0.0.0.0` to expose)
and writes rotated logs to `$(brew --prefix)/var/log/gdocsmith-mcp.log`
(rotates to `gdocsmith-mcp.log.1` at 10 MiB). To run in the foreground instead,
use `gdocsmith-mcp http` (or `./build/bin/gdocsmith-mcp http` from a source
checkout). Do not run the foreground server and service on the same port.

## Connect Your Agent

After starting the service, add this server to OpenCode v2.2.6's
`opencode.jsonc` (merge it with any existing servers):

```json
{
  "mcp": {
    "servers": {
      "gdocsmith-mcp": {
        "type": "remote",
        "url": "http://127.0.0.1:10007/mcp",
        "protocol": "2026-07-28"
      }
    }
  }
}
```

Or in `.mcp.json`:

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

Use either an explicit server entry or a plugin manifest, not both. Alternative
client schemas are in [contributing](docs/contributing.md).

## Workflow

1. **Lossless copy (when needed):** Call `file` with `action: "copy"` when duplicating a document that contains opaque/unsupported objects that must be preserved verbatim.
2. **Export:** Call `export` with a document ID or Google Docs URL. Read or search the returned Markdown file (`exportPath`).
3. **Edit locally:** Modify the Markdown file using your standard file-editing tools.
4. **Import:** Pass the absolute edited path to `import`; inspect `status`, `writeState`, `verification` and warnings. Use the returned working file after verified success. The supported subset is bounded; see [features and limits](docs/features.md) and the [agent guide](docs/agent-guide.md).

## Tools & HTTP Endpoints

Every tool is exposed over Streamable HTTP (`POST /mcp`) and as a direct HTTP endpoint (`POST /tools/<name>`, enabled by default alongside `GET /tools/openapi.json` and `GET /tools/schema`). The MCP server also serves the complete syntax and safety specification as the `gdocsmith://guide` resource (readable via `resources/read` or the `mcp_resource_read` tool). When installed via Homebrew, `docs/agent-guide.md` is installed into `pkgshare` and resolved relative to the executable.

| Tool | Mutating | Description |
| --- | --- | --- |
| `export` | No | Export a Google Doc by ID or URL into `<workDir>/exports/<docId>/<revisionId>/` (`export.md`, `export.orig.md`); review blocks are in Markdown. Preserve unimported local edits in `export.md`. |
| `import` | Yes | Validate local/live revision and supported intent, plan/guard, apply transactions, verify readback and re-export; report partial/unknown outcomes honestly. |
| `file` | Yes | Manage Drive files: `copy`, `trash`, `delete` (requires `allow: ["delete"]`), or manage sharing permissions (`grant` requires `allow: ["share"]`; public `anyone`, `domain`, and ownership transfers are unconditionally refused). |
| `status` | No | Returns plugin version (`4.0.1` / `VERSION`) without calling Google APIs. |

### HTTP Debugging (`curl`)

```bash
# Export a Google Doc to local Markdown
curl -X POST http://127.0.0.1:10007/tools/export -H "Content-Type: application/json" -d '{"doc":"1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms"}'

# Import supported local edits with revision locking and readback
curl -X POST http://127.0.0.1:10007/tools/import -H "Content-Type: application/json" -d '{"path":"/absolute/path/to/workspace/.gdocsmith/exports/example-doc/rev1/export.md"}'

# Copy a Google Doc losslessly in Drive
curl -X POST http://127.0.0.1:10007/tools/file -H "Content-Type: application/json" -d '{"action":"copy","doc":"1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms","title":"Copy of RFC"}'

# Check version/status
curl -X POST http://127.0.0.1:10007/tools/status -H "Content-Type: application/json" -d '{}'
gdocsmith-mcp --version
```

## Extended Markdown & Multi-Tab Format

```markdown
---
gdocsmith: "4.0.1"
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

- **Document creation:** `doc: null` / `revision: null` requests a new document; pass `newDocStyleBaseline: "generic"` for recreation/copy/split intent or `"account"` for new content. Unsupported recreation still refuses.
- **Tabs (`::tab{...}`):** Existing tabs are addressed by `id` (`t.0`, `t.1`, …), not title. Editing `title="..."` renames the tab; `id=""` creates a new tab at that position; adding `delete` to the directive deletes the tab (when `allow` permits).
- **Rich inline atoms:** Person, date, rich-link, dropdown, and safe image recreation intent are supported; opaque objects and new/moved hidden chips remain restricted. See [images](docs/images.md).
- **Review artifacts:** Comment and suggestion blocks follow tabs. See [comments](docs/comments.md) and [suggestions](docs/suggestions.md).
- **File compatibility:** Recognizes native `4.0.1`, source `3.0.20`, and legacy integer `1` envelopes.

## Safety Guards, Storage & Concurrency

- **Explicit `allow` gates:** Potentially destructive or lossy operations refuse until explicitly authorized via `allow`:
  - `commentChanges` — resolving/deleting/editing comments or removing text anchored by an open comment
  - `brokenLink`, `namedRangeDamage`, `lossyImageMove`, `lossyListRebuild`
  - `delete` (on `file` permanent deletion) and `share` (on `file` permission grants; `anyone`/`domain`/`owner` grants are always blocked)
- **Import safety:** Validates live revisions, guards, and readback through transactions, with partial/unknown outcome reporting.
- **Recovery:** Never retry creates, copies or comments blindly after an unknown outcome. Inspect authoritative cloud state and reapply only outstanding intent to a fresh export; do not overwrite a revision to bypass a conflict.
- **Work directory precedence (`src/engine/artifacts.hpp`):**
  1. `$GDOCSMITH_WORK_DIR` if set
  2. `<workspace>/.gdocsmith` (when inside a workspace, created `0700` with a `0600` `.gitignore` containing `*\n`)
  3. `$(brew --prefix)/var/gdocsmith-mcp` (`0700`, used when running outside a workspace)
  All file writes use atomic temporary-file-and-rename, snapshots are checked with SHA-256, and locally edited `export.md` files are preserved with a warning on re-export.
- **Per-document FIFO serialization:** Concurrent tool calls targeting the same `docId` are serialized in-process while different documents run in parallel.

## Authentication

- Credentials are obtained locally via `gws auth export --unmasked` (using your host `gws` CLI profile) and exchanged in memory against `https://oauth2.googleapis.com/token` over `cpp-httplib`. Access tokens stay in process memory and are never written to disk.
- `gdocsmith-mcp` does **not** run a persistent HTTP Google token broker database, accept external Google OAuth tokens over HTTP, or enable MCP OAuth (`oauth.enabled = false`).
- Because `brew services` runs under `launchd` or `systemd` without shell profiles, [`Formula/gdocsmith-mcp.rb`](Formula/gdocsmith-mcp.rb) sets `PATH` to `#{HOMEBREW_PREFIX}/bin:/usr/bin:/bin` and sources `$(brew --prefix)/etc/gdocsmith-mcp/env` (`chmod 600`, `KEY=VALUE` pairs) before starting the server.

## Environment Variables

| Name | Required | Default | Purpose |
| --- | --- | --- | --- |
| `GDOCSMITH_WORK_DIR` | Optional | `<workspace>/.gdocsmith` or `$(brew --prefix)/var/gdocsmith-mcp` | Override directory for exported Markdown revisions (`exports/<docId>/<revId>/`) |
| `GWS_PROFILE` | Optional | Active `gws` profile | Profile selected by `gws` when exporting OAuth credentials |
| `GWS_BIN` | Optional | `gws` | Path or name of the `gws` CLI executable |
| `BUGSINK_DSN` | Optional | Unset (disabled) | Error reporting DSN (`""` disables) |

## Troubleshooting

- **Client cannot connect:** confirm it supports MCP `2026-07-28`, the service is running (`brew services info gdocsmith-mcp`), and the client URL matches the configured port (`10007`).
- **Port occupied:** stop the conflicting listener or pass `--port <n>` when running `gdocsmith-mcp http` manually.
- **Works in a terminal, not as a service:** `launchd` and `systemd` do not read `~/.zshrc`. Put service variables in `$(brew --prefix)/etc/gdocsmith-mcp/env` (`chmod 600`), restart the service, and check `$(brew --prefix)/var/log/gdocsmith-mcp.log`.
- **Google authentication fails:** verify `gws` and its selected profile in the server environment.
- **Import fails / reports staleBase:** keep local edits. Import refuses stale revisions and returns a fresh recovery export.
- **Agent cannot open exportPath:** this workflow requires filesystem access to the server's work directory.

## Documentation

- [Contributing](docs/contributing.md) — source builds, tests, linting, PR/release flow, and architecture
- [Architecture](docs/architecture.md) — engine internals and module boundaries
- [Features & limits](docs/features.md) — supported Docs elements and refusals
- [Agent guide](docs/agent-guide.md) — complete file syntax (`gdocsmith://guide`)
