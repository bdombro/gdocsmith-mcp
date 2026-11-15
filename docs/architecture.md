# Architecture

The native C++26 app exposes four file-oriented tools: `export`, `import`,
`file` and `status`. The read-only Node reference is available at
`../gdocsmith-node` (`gdocsmith` 3.0.20); availability is not parity proof.

| Layer | Responsibility |
| --- | --- |
| app / API | Strict tool contracts, per-document queue, injected Docs/Drive transport |
| lens | Markdown envelope, rich body projection/rendering and atomic typed putback |
| engine | Fresh models, sessions, planning, guards, phased transactions, readback and artifacts |
| model | Typed document state, preserved metadata, identity and structural invariants |
| reconcile | Minimal requests, ordered back-to-front within content batches |
| emulator | Offline request execution and deterministic regression evidence |

## Import

The real import path runs through `fileImport`, typed session putback and
`transactionRun`, not a direct-write shortcut. **The public app factory defaults
to `remoteImportEnabled = true`.** The earlier bounded implementation was
accepted at 19:21Z on 2026-11-13. Current parity work changes those inputs:
that acceptance is historical, not a current source freeze or final runtime gate.

Import freezes the local input, loads authoritative
Docs/Drive state, validates its revision and original export when present,
plans/emulates/guards before sends, chains required revisions through Docs
batches, resolves provider-created IDs and reads back authoritative state.
No-op imports read back without mutations. Drive title/comment actions follow
content; multi-service phases are not an atomic transaction or automatic rollback.

Existing public scope includes rich body/tab/document/comment operations.
Independently reviewed lens/helper additions cover declarations/resets,
table geometry/layout/directional merges, supported atom intent, internal URL
projection, unlinked heading deletion and nonconflicting review-adjacent edits.
Auth, dropdown, new-tab and rollback-boundary scopes now have independent
acceptance. Broader segment/review/image lifecycle remains qualified.
Both source-grounded fixture corrections now pass full fresh Debug/Release:
398/398 cases and 6,521/6,521 assertions each, zero failures/skips.
Fresh test builds/test-TU tidy and shared 151/151 original, 172/172 target,
70/70 controls passed. App/adapter builds/lint and isolated MCP passes were
retained only after exact input/binary identity verification. Qualification is
bounded: 250 raw leaves/206 groups and foreign-error/test-transfer/platform/raw
interchange residuals remain unaccepted; no broader parity/release acceptance.
See [features](features.md) for precise
platform, raw-envelope and unsupported distinctions. Refusal is never permission
to discard opaque content; `allow` cannot override an unsupported operation.

## Export and artifacts

Export combines rich content with identities and document/review context:
non-body content, complete settings/styles, merged-table topology and actual
suggestion before/after context/markers are encoded. Unsupported contexts remain
unchanged. Temporary URLs are stripped; representation is not proof of write
capability. Export is not a standalone restorable backup or full parity claim.
Private image downloads do not prove provider upload/recreation/readback.

Canonical revision artifacts are `export.md` (working copy) and
`export.orig.md` (recorded original), as verified against Node. Re-export
preserves local edits. No migration, deletion or renaming of user work is required.
If recovery's normal working file contains local edits, the enabled importer can
return a separate private `export.remote.<unique>.md` snapshot rather than
misrepresent that edited file as fresh cloud state.

## Guards and verification

Named-range single-body-span marker tracking and UTF-16 edge prediction have
scoped independent approval. Managed marker removal is explicit intent;
unmanaged loss remains guarded. Unknown/touching/destructive structural effects
refuse unwaivably; proven-safe later-table edits can proceed. Multi-span,
non-body and unresolved created-ID verification can remain inconclusive, never
fabricated matches. `allow` waives classified losses only, not unsupported edits.

Receipts distinguish confirmed requests from unknown attempts. Failure can follow
landed writes; verification mismatch/inconclusive is not success. Stale,
concurrent, partial, unknown or unverified results preserve input and seek a
fresh recovery export. Never replay creates, copies or comments blindly.
Independent core review has closed the same-revision partial-comment recovery,
comment outcome/readback and authoritative recovery-title findings. This is
historical scoped approval; current bounded qualification passed, while broader
parity/raw-difference acceptance remains open.

## Agent guidance and evidence

The app serves [agent-guide.md](agent-guide.md) as `gdocsmith://guide`, including
executable-relative Homebrew `pkgshare` lookup. Guidance describes the running
build's gates, not a separately installed skill or a Node runtime requirement.

The earlier bounded enabled-source Debug ASan/fatal-UBSan and Release suites each passed
**308 tests / 5,302 assertions, zero skipped**. App/full-test clang-tidy and
default-release MCP `2026-07-28` stdio/ephemeral HTTP passed, with unchanged
source/binary hashes across that historical validation. These are not current
parity-build counts; the current source-bound qualification above supersedes them.

The real default public CLI passed synthetic create and actual body edit
(`applied/complete/match`), authoritative export text match, exact no-op
(`unchanged/none/match`), stale original-revision refusal (`staleBase/none`),
and guarded delete followed by export not-found. Both consented synthetic
documents were deleted; cleanup was confirmed. No existing documents were used.

This live smoke does not cover representative images, rich non-body content or
review mutation. Offline tests remain regression evidence for those paths;
historical Node experiments are not new C++ live validation. No blanket Node
parity or standalone restorable-backup/recreation claim is made.
