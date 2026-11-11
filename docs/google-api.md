# Verified Google Behavior

Behavior gdocsmith relies on, verified live against the stable APIs.

## Suggestions (2026-10-01/02)

- The Docs API can't create, accept or reject suggestions. A .docx upload with tracked changes (Drive multipart upload converted to a Google Doc) creates real ones: insertions, deletions, `w:rPrChange` text-style and `w:pPrChange` paragraph-style changes (each holding the full proposed style), rows, cell styles, paragraph breaks, whole paragraphs, lists, inline images and page setup (`w:sectPrChange`). Column insertions and named-style changes don't convert.
- Drive `files.copy` drops pending suggestions and comments.
- batchUpdate never errors on suggested content. Deleting a whole suggested run, image, row or newline removes that suggestion; deleting half of a replacement leaves the other half pending.
- Text inserted strictly inside a suggested run, or into a cell or paragraph left with only suggested characters, joins that suggestion; at a run's edge or between plain neighbors it is plain. New rows and paragraphs beside suggested ones are plain.
- Restyling suggested formatting changes the current style; the suggestion survives, but Google recomputes the proposal's stored style, so only suggestion ids are compared.
- Re-break (delete a paragraph's newline, insert a new one) clears its paragraph-style or list suggestion; Google applies the suggested style to that paragraph and the next, so both are restored explicitly. A suggestion on the next paragraph survives.
- Splitting a paragraph with a paragraph-style suggestion keeps the suggestion on the later part (the original newline); the new earlier part takes the suggested style.
- Insert a new row, fill it, delete the old one: a plain row.
- `updateDocumentStyle` keeps a page-setup suggestion; only deleting the tab clears it.
- Developer Preview (`acceptSuggestion`, `rejectSuggestion`, `insertComment`) needs enrollment; this account gets 400 "No request set."

## Comments (2026-10-02)

- UI comments have `quotedFileContent` and a kix anchor; Drive comments carry no tab; .docx comments are anchored but have no quote; API-created ones are unanchored.
- A full rewrite of anchored words with the carry move kept the comment on the new words.
- Every comment action changes the Docs revision id.
- Authors come back without email; only `me` identifies the caller.
- Drive's HTML export marks where comment anchors end and embeds images as base64 (0.5–1.2 MB for 2–3 images); gdocsmith uses it in test code only.

## Comment anchors and exports (2026-10-04)

- `quotedFileContent` stays as it was when the comment was made: edits to the anchored text move the comment but not its quote (real docs: "mailing executions" anchored on "mailing list subscriptions executions").
- Drive cuts quotes off near 1,000 characters (a 1,270-character anchor had a 999-character quote).
- Quotes spell chips out by their text (person names, dates).
- The Drive plain-text export (`files.export` text/plain) marks each anchored comment's end as `[a]`, `[b]`… and lists the comment texts after the body (`[a]Comment text`), resolved and quote-less comments included. It covers every tab's body in one call (no headers or footers), writes chips as their text, and is small (~10 KB for 12 tabs).
- The Markdown export has no comment markers.
- The per-tab `docs.google.com/document/d/…/export?format=txt&tab=` endpoint throttles quickly ("Too Many Requests").
- The .docx export has exact comment ranges (start and end) but no Drive comment ids.

## Images (2026-10-02)

- `insertInlineImage` from an image's `contentUri` (with its size) recreates a Drive-hosted image within and across documents: a plain copy with a new object id.
- The copy loses crop (`contentUri` serves the uncropped original), rotation, border and alt text; `insertInlineImage` keeps aspect ratio. `replaceImage` keeps the object but substitutes its own center crop and drops the alt description.

- An image's `contentUri` serves its bytes to a plain GET with no auth (a bearer URL, about 30 minutes).
- A converted .docx with embedded PNG, JPEG and GIF images gives one inline image per picture, in order, at natural size (0.75 pt/px); bytes are re-encoded and sides over 2048 px downscaled. Their `contentUri`s insert into another document and still serve after the throwaway is deleted.
- Drive's `name contains` search matches words, and search results lag deletes.

## Headers, Footers And Named Styles (2026-10-02)

- `createHeader`/`createFooter` take only DEFAULT; first-page and even-page ones can't be created. Segment-scoped edits work on any existing one; `deleteHeader` works on first-page ones.
- Header and footer content starts at index 0, which the JSON omits; their paragraphs can't take `pageBreakBefore` ("Cannot update page-break-before in a header"), and `updateDocumentStyle` refuses `defaultHeaderId`-style fields ("Unallowed field").
- `updateNamedStyle` (public API) restyles every paragraph using the style in that tab, except local overrides.

## Tables

- Setting a cell's bottom border sets the top border of the cell below; a right border sets the left border of the cell beside it.

## Tabs (2026-10-02)

- After deleting tab `t.0`, later tab renames and reorders fail with 500. Import moves `t.0` last and deletes it after other tab updates.
