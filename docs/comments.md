# Comments

Comments are blocks in `export.md`, after tabs and before suggestions; there
is no separate review tool or required review YAML file.

**Public import is enabled by default; scoped implementation is accepted.** The
import path validates fresh review state, plans/guards content
before writes, then runs supported Drive actions one at a time after content.
Independent core review closed the same-revision partial-comment recovery,
comment receipt/readback and authoritative recovery-title findings. Final
runtime gates passed for that earlier bounded implementation, not current
parity changes. Fresh bounded qualification now passed; broader parity remains
unaccepted (see [features](features.md)). The synthetic live smoke did not exercise comment
mutations; offline tests are not live Google verification of those actions.

## Blocks and actions

Preserve exported IDs, authorship, quotes, markers and required reply lines.
Only comments/replies marked `mine` may be edited/deleted; others' content is
read-only. Export normally includes unresolved comments;
`commentsIncludeResolved: true` requests resolved context too.
Comment/reply text is literal plain text; escape a leading `::` as `\::`.

| Block edit | Implemented Drive action |
| --- | --- |
| Bare `::comment` and text | Create unanchored comment |
| Bare `::reply` and text | Create reply |
| Edit owned text | Update comment/reply |
| `delete` on owned comment/reply | Delete |
| Add/remove `resolved` | Resolve/reopen |

All actions require user-approved `allow: ["commentChanges"]`. Read-only
fields/markers cannot be rewritten; conflicting protected review edits refuse.
Nonconflicting edits beside retained deletion/style suggestions have scoped lens
approval. The allowance does not enable unsupported operations or certify
pending segment/suggestion backend workflows.
Anchored comment creation is unsupported.

## Recovery and anchors

Read `status`, `writeState` and `verification` separately: failure may follow
landed writes, and unknown means at least one request lacks confirmation.
Do not blindly replay a comment action. Inspect authoritative cloud state and
fresh blocks before retrying only outstanding intent; an edited local working
copy is not authoritative recovery evidence.

Stable Drive quote/thread data does not prove complete native anchor attachment.
Do not promise that version history restores removed comments, or that a
matching body comparison verifies all anchors. Historical Node carry-move
behavior is not a promise of C++ protected-body editing support.
See the [agent guide](agent-guide.md) for the current subset and recovery rules.
