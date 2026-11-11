# Features And Limits

| Area | V3 behavior |
| --- | --- |
| Content | Full-document export; complete tab bodies with minimal diffs |
| Creation | doc:null creates a doc; empty tab ID creates a tab where its line is |
| Documents | Copy, trash/delete and explicit sharing; frontmatter title renames on import |
| Tabs | Tab lines create, rename, re-parent, reorder, lay out pages, set emoji, page color, header/footer margins and page number start, and delete (never implicitly deleting children) |
| Formatting | Markdown plus `::list` glyph presets, text, paragraph (including borders, direction and keep/widow/page-break flags), cell and named-style declarations and null resets; `- [ ]` checklists (checked state is preserve-only) |
| Tables | Content, rows, columns, alignment, merged cells, `:cell{ref=…}` block/nested-table cells, and `::table` column widths, row heights, header rows and `preventOverflow` |
| Breaks | `::pagebreak`; `::section` breaks with editable columns, separator, margins, orientation, page numbering and first-page flag |
| Chips | Person, date (with format, time and zone) and rich-link (Drive file or Calendar event) chips created; existing chips move or are deleted; dropdowns insert, reselect, move and delete, with editable option lists |
| Links | Same-tab and tab-qualified textual fragments; `gdoc:` links to another document's headings and tabs resolve on import and export as Google's native URL; native Google targets internally |
| Footnotes | GFM `[^n]` references and definitions: create, edit, move and delete |
| Named ranges | `:namedrange{name=… start}` / `end` markers: create, move, delete; edits inside keep them |
| Headers and footers | Editable under header/footer lines, including a later section's (`section=N`); default ones created and deleted |
| Comments | Comment blocks: reply, resolve, reopen, edit or delete your own comments and replies, create unanchored; read-only `:comment` markers on body anchors |
| Suggestions | Current-only tab bodies with read-only `:suggestion` markers; suggestion blocks; edits carry suggestions; `reject` (and accept by edit) |
| Images | Inserted at natural or given size; replaced in place (`src=`); Drive-hosted images move and copy within and across tabs and documents; lossy copies warn, lossy moves need `allow` |
| Recovery | Revision locks, automatic readback, fresh exports after conflicts, and explicit partial/unknown results |

Markdown creation is not lossless cloning; unsupported transferred objects are omitted with warnings. Stable API data cannot prove every comment-anchor attachment.

The goal is an export that is a complete copy of the document. Everything below falls short of that, either because the API can't read or write it, or because gdocsmith doesn't yet. Planned work is tracked in `feature-parity-plan.md`.

## Support Status

Status: **Preserved** (kept untouched, no markdown syntax) · **Partial** · **Unsupported**. Reason: **API limit** (no Docs API request can do it), **gdocsmith gap** (the API can; gdocsmith doesn't yet), **Developer Preview** (only in Google's gated preview) or **strategic** (deliberately not supported). Verified against the Docs API discovery document (revision 20260928) on 2026-10-03.

### Gaps (the API can; planned)

| Feature | Status | Notes |
| --- | --- | --- |
| Named ranges in headers, footers and footnotes, or starting or ending at a table edge | Preserved | Unshown and untouched; `namedRangeDamage` guards deleting all their text |

### API limits

| Feature | Status | Notes |
| --- | --- | --- |
| First-page and even-page header/footer creation | Unsupported | `createHeader`/`createFooter` take DEFAULT only; existing ones are editable |
| Section header link-to-previous | Preserved | No request sets it |
| Section column widths | Unsupported | "Column widths cannot be updated"; count and gap only |
| Section break type after creation | Unsupported | `sectionType` is creation-only |
| Clearing a section field | Unsupported | Every `updateSectionStyle` field needs a value; set margins and page numbering can't return to inherited |
| Image crop, rotation, border, alt text, adjustments on recreation | Unsupported | `insertInlineImage` can't set them; alt text (`title`/`description`) has no update request |
| Custom list glyphs | Preserved | Lists matching none of the 15 presets keep their glyphs untouched; no request edits glyphs |
| Tab stops | Preserved | `tabStops` is read-only ("Unallowed field") |
| Bookmarks | Preserved | No create request |
| Checkbox checked state | Preserved | No request sets it |
| Equations, drawings, charts, tables of contents | Preserved | No create request |
| Horizontal rules, column breaks, auto text (page numbers) | Preserved | No insert request; kept as object directives |
| Positioned (floating) object creation and moves | Unsupported | No create request; existing ones export as `:positioned[…]{id=…}` and can be deleted |
| Rich links other than Drive files and Calendar events | Unsupported | `insertRichLink` rejects Maps and YouTube URLs ("The URL is invalid", every form tried); existing ones are preserved |
| Hidden chips: stopwatches, timers, votes, placeholder chips, "Listen to this tab", watermarks | Partial | Exported as fixed `:chip{kind=…}` directives (kind from the tab's plain-text export; `unknown` in tables and headers). The API shows each only as a U+E907 character and Google drops an inserted one, so they can't be created or moved to another paragraph; deleting works |
| Variables | Partial | Exported as `:variable{id=…}` with values in the frontmatter; a value edit rewrites every instance. Names exist only in .docx/ODT exports; variables can't be created; two variables with one value share an id |
| Task assignees and due dates | Preserved | Checklist items export; assignee and due date are absent from the API |
| Linked Sheets tables | Partial | Identical to plain tables in the API. Edits in the doc never reach the sheet, and a refresh in the UI overwrites them |
| Code blocks | Partial | A Google Docs code block exports as a fence (markers hidden) and stays one when edited. A new fence can't create one (the U+E907 markers can't be inserted), so it becomes Roboto Mono Light lines. Text added through the API isn't syntax-highlighted; toggling the block's language in the UI re-highlights it |
| Citations | Preserved | Plain text in the API and the UI; nothing to support |
| Rejecting page-layout, named-style, list, row-style, column or footnote suggestions | Unsupported | No stable request clears them until Developer Preview |

### gdocsmith refusals and strategic choices

| Feature | Status | Notes |
| --- | --- | --- |
| Editing a paragraph joined by a hidden suggested paragraph break, or a table with a hidden suggested row | Unsupported | The edit's side of the suggestion would be a guess; reject it first |
| Comments on suggestions | Unsupported | Absent from Drive `comments.list`; visible only in .docx/ODT exports, which gdocsmith doesn't read (strategic: comment anchors are adequate without parsing exports) |
| Anchored comment creation | Unsupported | Developer Preview (`insertComment`); `:comment` markers are read-only for the same reason |
| Comment markers in headers, footers and footnotes | Unsupported | Anchors are located by quote matching over the body; strategic, since Drive gives no anchor positions on the stable API |
| Comment markers for comments without a quote | Unsupported | They export as comment blocks with no `:comment` markers, so edits to their text aren't guarded. Drive lists no `quotedFileContent` for comments converted from .docx/ODT, or for a point anchor whose text was deleted, and the plain-text export marks only the anchor's end (`[a]`), not its start. Spike 2026-10-05: a Word-style .docx comment had no quote on upload or 60 s later; 2 of 147 comments in 14 real docs had none (one point anchor, one unanchored) |
| Native accept/reject, creating suggestions | Unsupported | Developer Preview (`acceptSuggestion`, `rejectSuggestion`, `deleteSuggestion`, `writeMode: SUGGEST`); `reject` uses stable-API moves |

### Developer Preview

Google documented comment and suggestion requests for `documents.batchUpdate` on 2026-08-27, gated per Google Cloud project by human-reviewed enrollment in the Workspace Developer Preview Program. The public discovery document (revision 20260928) lists them, each marked Developer Preview: `insertComment`, `addCommentReply`, `updateCommentPost`, `deleteComment`, `deleteCommentReply`, `acceptSuggestion`, `rejectSuggestion`, `deleteSuggestion` and `WriteControl.writeMode: SUGGEST`. An unenrolled project gets 400 "No request set." for `insertComment`, `acceptSuggestion` and `rejectSuggestion`, and "Unsupported WriteControl mode." for `writeMode: SUGGEST` (checked 2026-10-01 and 2026-10-02). Request shapes: https://developers.google.com/workspace/docs/api/how-tos/suggestions. Check back when these are generally available.

## Drive Export Formats

gdocsmith reads documents through the Docs API, not Drive exports. These formats were measured on 2026-10-03 to see what they'd add:

| Format | Size and time (one small tab) | Captures |
| --- | --- | --- |
| .docx | 8 KB / 1.5 s; grows with embedded fonts and images | Variable names and values, hidden chips as text, comments with exact anchor ranges (including comments on suggestions), suggestions as tracked changes, images, fonts |
| ODT | 3.1 MB / 5.8 s (always embeds 8 font files) | The same as .docx |
| HTML | 3 KB / 0.7 s | Hidden chips as text; no tab boundaries or variable names |
| Plain text / markdown | Under 100 B / 1.4 s; markdown embeds images as base64 | Hidden chips as text (`⏱️00:00:00`, `(👍 0)`, `Person`), every tab, footnotes; no headers |
| RTF | 16 KB | Nothing beyond plain text |

- Drive refuses exports over 10 MB (`exportSizeLimitExceeded`). The per-tab URL `docs.google.com/document/d/<id>/export?format=<fmt>&tab=<tabId>` avoids it.
- About six quick exports in a row return 429.
- Neither .docx nor ODT carries Drive comment or Docs suggestion IDs.
- Uploading a .docx or ODT converts tracked changes to suggestions and annotations to anchored comments; gdocsmith uses a .docx upload to host local images.

Removed: run/steps, surgical editing/query APIs, public dry runs, snapshot-cache controls, public character offsets and backward-compatibility aliases.
