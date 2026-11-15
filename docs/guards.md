# Guards

> **Current C++ boundary:** Public remote import is default-enabled and scoped
> earlier bounded implementation accepted; current bounded qualification passed,
> with broader parity/raw-difference obligations still open.
> A historical synthetic live stale-revision refusal and guarded deletion passed,
> not every loss/review guard. `allow` waives classified findings only, never
> unsupported operations or protected review conflicts. Reviewed lens/atom intent
> and pending backend workflows are distinguished in [features](features.md). Unknown/touching/
> destructive named-range structural effects are unwaivable; proven-safe
> later-table operations are distinguished. The [agent guide](agent-guide.md)
> overrides the inherited Node design examples below. Deletion is not a blanket
> permission, version history is not guaranteed comment recovery, and unsupported
> objects must not silently disappear during recreation.

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

Removing supported content from export.md expresses deletion intent, subject to
guards; version history is not guaranteed comment recovery. Deleting every tab
clears the original tab and renames it "Tab 1" instead. Do not silently omit
unsupported objects during recreation; use lossless Drive copy when needed.

Comment coverage is the union of original-character deletions. Link checks use surviving references in the final model, including untouched tabs; removing a heading and repairing its references in one import is fine.
