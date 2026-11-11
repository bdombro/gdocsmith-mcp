# Mechanics

Export reads fresh cloud state into a v3 markdown file. Import reads that file once, checks its document/revision, builds a final model, emulates the minimal request plan, applies guards, and sends revision-locked phases. No public dry-run or snapshot-freshness control exists.

## Identity

Existing documents require nonempty doc/revision values. Existing tabs use IDs; the readable title is not a fallback selector. Tab directives contain id, title, optional parent, page layout and delete. Bodies are complete, and every existing tab needs a line; a missing tab refuses before any write.

An empty tab ID creates a tab at its line's position. Explicit doc:null creates a document, allocating new root-tab IDs in file order and reusing the initial tab. Unknown nonempty IDs on existing documents refuse. A changed frontmatter title renames the document through Drive after content lands, without a revision check.

Tab titles, parents and sibling order come from the tab lines and flow through the model, so the emulator self-check and readback verify them. Parents must be existing tabs listed earlier (so no cycles form). Deleted tabs contribute their whole body to the comment and named-range guards; a deleted parent needs every tab still under it deleted. Title uniqueness is checked on the final state. The tabs phase moves kept tabs out of deleted parents, sends deletes, then title/parent/index updates in final order; a parent change without an index appends (C27e–C27h). The API rejects creating or renaming a tab to another tab's exact title (400 "Tab title must be unique", C27i, C27k; case-only differences are accepted, C27j, C27l), so no request creates a duplicate: a tab whose title an earlier-processed tab takes, or a new tab whose title is still held when the create phase runs, gets a temporary `(renaming)` title first.

Each tab line sets its tab's page mode: pageless changes only the mode, while pageful writes explicit size and all four margins. New tabs are created pageless, so pageful new tabs need those writes; matching layouts send nothing.

## Outcomes

| Field | Values |
| --- | --- |
| status | applied, unchanged, refused, failed |
| writeState | none, complete, partial, unknown, reverted |
| verification | match, mismatch, inconclusive, notApplicable |

These are independent: failed/complete means writes landed but later verification or export failed. Unknown means at least one request lacks a confirmed outcome. Refused means pre-send validation/guards prevented the operation, not rollback.

Write phases: create, content (revision-locked Docs batches), tabs, links, the Drive rename, then one Drive request per comment action, then the final fresh export. Deleting tab `t.0` breaks later tab renames and reorders (500), so the tabs phase moves it last and deletes it after the other tab updates.

Sends are tracked in memory for the result; nothing is persisted. Revision locks stop a repeated file from resending writes that landed; creates, copies and comments have no lock, so their results warn against blind retries.

## Files

Private files live under GDOCSMITH_WORK_DIR; else, for MCP servers, `<working directory>/.gdocsmith` (created 0700 with a `*` .gitignore), capturing CWD at invocation after Argsbarg's workspace discovery; else (CLI, or a working directory of `/` or home) a private per-user directory under the OS temp directory. Regular UTF-8 import files are limited to 16 MiB. Exports go to `exports/<doc>/<revision>/` (`export.md` and `export.orig.md`, an unedited copy; `no-revision.<time>/` for read-only access). Every export reads the document fresh; an `export.md` differing from both the new export and export.orig.md holds local edits and is kept, with a warning. The frontmatter's `apiUser` is the authenticated user's email; `commentsLine` and `suggestionsLine` are where each group of blocks starts. Import overwrites its input with the fresh export unless the input changed meanwhile. Stale revisions, concurrent edits and uncertain writes keep the input and export fresh beside it.

Cloud batches are not a transaction across phases or Drive/Docs services. There is no automatic rollback. Export fresh and reapply only remaining intended edits after an uncertain outcome; never manually replace the file revision.