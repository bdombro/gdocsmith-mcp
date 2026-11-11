# Guards

Guards evaluate the complete planned model before sending, not one low-level request at a time. Fresh cloud review state is authoritative.

A refusal lists one error per `allow` kind, with the affected ids and the next step (ask the user, then retry with that kind). A kind waives only its own findings in that import.

| allow kind | Planned effect |
| --- | --- |
| `commentChanges` | Any comment-block action; editing an open comment's quoted text; removing any comment's anchor text |
| `brokenLink` | A surviving link to a removed heading or tab |
| `namedRangeDamage` | Deleting all the text of a named range export can't show (in a header, or starting at a table edge) |
| `lossyImageMove` | Moving (or rejecting the deletion of) an image a recreation can't reproduce |
| `lossyListRebuild` | Rebuilding a custom list whose look changes |

Refused with no `allow` override:

- An edit that removes a pending suggestion not marked `reject` (see [suggestions](suggestions.md)).
- Stale revision, ambiguity, invalid syntax, view mode, permissions and a failed emulator self-check.
- Duplicate tab titles, a kept tab under a deleted parent.

## File operations

The `file` tool has its own `allow` kinds, checked before any Drive call. A refusal (`guardRefused`) says to ask the user, then retry with the kind.

| allow kind | Needed for |
| --- | --- |
| `delete` | `delete` (permanent) |
| `share` | `grant` |

Refused even with `allow`: grants to `anyone` or `domain`, and ownership transfers (role `owner` or `transferOwnership`). The message says to ask the document owner to do it in Drive. Trash, revoke, copy and permissions need no `allow`.

Deletions need no approval: removing content from export.md is the intent, and version history can restore it. Deleting every tab clears the original tab and renames it "Tab 1" instead. Unsupported objects copied through markdown into a new tab or document are omitted with warnings.

Comment coverage is the union of original-character deletions. Link checks use surviving references in the final model, including untouched tabs; removing a heading and repairing its references in one import is fine.
