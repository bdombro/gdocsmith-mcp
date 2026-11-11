# Architecture

V3 exposes file workflows over the model-first content engine. Commands validate inputs and delegate; the lens, engine and reconciler own editing decisions.

| Layer | Responsibility |
| --- | --- |
| commands | Four strict tool/CLI contracts (export, import, file, status) and bounded results |
| lens | Document envelope, tab boundaries, styles, tokens and link binding |
| engine | Fresh models, planning, guards, lifecycle and verification |
| model | Paragraph/table/list mutations and structural invariants |
| reconcile | Minimal Google requests with character-level paragraph diffs |
| emulator | Offline request execution checked against recorded Google behavior |

## Agent Guidance

The MCP server carries all agent guidance; there is no skill. It serves [docs/agent-guide.md](agent-guide.md) as the `gdocsmith://guide` resource, and its instructions tell agents to read that resource before the first export or import and name export's `doc` argument.

- One source: a separately installed skill could be skipped or drift from the server it describes. The resource always matches the running server, including `npx` installs that have no plugin.
- Parity: on the hard `full-edit` E2E case (2026-10-05, 3 runs per cell), the resource matched an installed skill on Claude Code (3/3 passed each, no failed calls) and Cursor (2/3 passed each, failing the same check; one recovered syntax error).
- The `doc` hint matters for Cursor, which defers tool schemas: without it, Cursor sometimes called export with a guessed argument before reading the guide. A pointer in the tool descriptions did not prevent that.

## Import

Read and freeze the local file, parse its comment and suggestion blocks, fetch the document and review state, and assert the exported revision. Comment blocks become Drive actions sent after content; suggestion blocks are checked against a fresh render and their `reject` flags applied before each tab's markdown is written. Every existing tab has a line describing its complete desired content, structure, or deletion. New tabs/documents are planned virtually before creation. Resolve authored links against the complete final document, preserving unchanged existing link identity where correspondence is known.

Reconcile, emulate and guard before sending. Track sends in memory. Bind created IDs through responses; every Docs batch uses requiredRevisionId and chains the resulting revision. Import never rebases a stale file or automatically replays a conflicted mutation.

Fetch final state and compare it with the planned model, accounting for documented API normalization. A newer revision makes verification inconclusive. Disagreement is an error even when writes landed. Successful verification produces the next export before retiring an unchanged app-owned input.

## Preserved Knowledge

The engine retains Unicode/code-point versus UTF-16 handling, newline ownership, heading identity on splits/merges, list inheritance/spacing, table API ordering, link chrome and opaque-object preservation. Existing model, reconciler, emulator, property and conformance tests remain the primary regression evidence.

There is no persistent document cache or public step layer. Internal model handles remain useful for deterministic planning and conformance tests, not as a second agent API.

## Verification Limits

Stable Drive comments expose quotes and threads, not complete native anchor attachment guarantees. Native suggestion and anchored-comment requests require Developer Preview; suggestion rejects use stable-API moves instead. The legacy semantic comparator does not by itself establish all named-range, preserve-only metadata or review-state guarantees. Do not infer full proof from a successful body comparison or an offline test alone.