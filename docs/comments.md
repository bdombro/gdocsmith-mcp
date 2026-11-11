# Comments

Comments are blocks at the end of export.md, after the tabs and before the suggestion blocks. Import manages them; there is no separate review tool.

## Blocks

- Export writes unresolved comments in document order (quote-less ones last); `commentsIncludeResolved: true` adds resolved ones.
- Header fields are read-only: `id`, `tab`, `quote`, `author` (Drive's display name), `mine` (Drive's `me`), `ambiguous`, `orphaned`.
- Tab bodies mark each located quote with `:comment{id=… start}` / `end` markers; overlapping matches of one comment merge into one pair. Markers are read-only: import checks each stays beside the same surviving text (text typed or deleted next to it doesn't move it), and refuses moved, missing or added ones, except a pair deleted with all its text. Anchors in headers, footers and footnotes aren't marked (quote matching covers the body).
- Locating reads the Drive plain-text export, which marks where Google keeps each anchor's end (`[a]`) and lists the comment texts; the start is where the longest opening of the quote last occurs before the end. This finds comments on edited text, which Drive's frozen quote no longer matches. A quoted comment the export's list doesn't have is orphaned: Google lists every anchored comment. A listed comment the export can't pin (two comments with the same text and no quote to tell them apart), or any comment when the export has no list, falls back to quote matching: chips match by their text, a quote of 990+ characters (Drive truncates them) matches as a prefix, and an ambiguous quote protects every occurrence. Quote-less comments (API-created, or from a Word upload) can't be located, so nothing protects them.
- Comment and reply text is literal plain text; a line starting with `::` is escaped `\::`.

## Actions

| Edit in the block | Drive request |
| --- | --- |
| `::reply` (bare) and text | `replies.create` |
| change a reply's text (`mine` only) | `replies.update` |
| `delete` on a reply line (`mine` only) | `replies.delete` |
| add / remove `resolved` | `replies.create` with `action: resolve` / `reopen` |
| change the text (`mine` only) | `comments.update` |
| `delete` (`mine` only) | `comments.delete` |
| `::comment` without attributes | `comments.create` (unanchored) |

Replies export as `::reply{id=… author=… mine}`; `id`, `author` and `mine` are read-only. Import validates the blocks against the same revision rendered again: changed read-only fields, edits to others' replies, removed reply lines and removed unresolved blocks refuse. Every action needs `allow: ["commentChanges"]`.

Actions run after content, one Drive request each. A failure is listed in `errors` (failed or unknown outcome) and the rest continue, so the agent retries only those. Comment actions bump the Docs revision, so import's final export comes after them.

## Anchors During Content Edits

Edits strictly inside a comment's quoted text use the carry move (insert the new text inside the anchor, then delete the old characters), which keeps the comment on the new text. Edits crossing an anchor's edge leave part of the new text outside, as Google does. Editing an open comment's quoted text, or removing any comment's anchor text, needs `allow: ["commentChanges"]`. Verification is inconclusive only when an import touched anchored characters: the stable Drive API can't confirm where Google kept the anchor. Restoring a revision did not restore a comment on removed content; don't offer version history as comment recovery.
