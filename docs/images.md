# Images

`imagesDownload: true` downloads all available inline images; an array selects
full Google inline-object IDs as emitted in exported tokens. `images` maps IDs
to local PNG/JPEG/GIF files; `imagesDir` is present when files were downloaded.
Unknown IDs, unsupported bytes and download failures are errors, not successful
empty results. An empty selection returns an empty map.

Directories are private and files are `0600`; filenames use a SHA-256 ID digest.
Downloads use Google-hosted HTTPS content URLs without forwarding OAuth headers.
Bearer content URLs are not written into Markdown, filenames or error messages.
No live Google download smoke test is claimed.

## Editing boundary

Public import is enabled by default. Independently reviewed atom helpers support
safe image recreation intent, preserve identity distinctions and refuse
uncreatable sources. Public upload, replacement, cross-document copy/move,
provider-created IDs, cleanup and authoritative readback remain engine/integration
acceptance gates. The historical synthetic live smoke did not exercise images;
current bounded qualification passed, not broader parity/release acceptance.
Editing a downloaded file alone
changes nothing remotely.

The internal image resolver/guard classifies crop, adjustments, rotation,
rendered border and alt metadata recreation losses. Classified moves need
`allow: ["lossyImageMove"]`; unknown/ambiguous moves refuse without a waiver.
That guard is not certification of every public move/provider workflow.
An authoritative source-only HTTPS fallback is local to recreation, not a
general copy resolver. A present invalid fresh URI must not be bypassed using
another source. Unknown/missing safe sources remain unwaivable.

Drawings, charts, equations and positioned objects cannot be recreated through
this workflow. Use Drive `file` action `copy` when existing opaque content must
be preserved; do not recreate it from Markdown. Reviewed layout intent does not
imply editable crop/rotation/border/alt metadata. Export carries document/review context but does not
support recreating it as a standalone restorable backup.

See the [agent guide](agent-guide.md) for guards, outcome fields and unknown-write
recovery. Offline tests do not establish live Google behavior.
