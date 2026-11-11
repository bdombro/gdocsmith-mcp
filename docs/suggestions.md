# Suggestions

## Export

- Tab bodies show the current document: suggested deletions as normal text, suggested insertions (text, paragraph breaks, paragraphs, rows, objects) hidden, current formatting.
- Text, textStyle and object suggestions are marked in the body: `:suggestion{id=… start}` / `end` around suggested runs, and a point `:suggestion{id=…}` where only hidden insertions are, at the end of the text before a hidden paragraph, and at a joining hidden paragraph break. Markers are read-only, as comment markers are; a `reject` header frees its id's markers.
- Suggestion blocks come last, one per Google suggestion id: header (`id`, `kind`, `tab`, `exportLine` or `exportLines` for kinds without markers (read hints import ignores), `segment` for headers and footers), then `::before` and `::after` snippets of the smallest whole context with the span inside `:change[…]`. Named-style suggestions get a one-line notice. Proposed styles are ordinary `suggestion-N` declarations (or reuse an equal one).

## The Rule (Import)

A suggestion's region is the content carrying its id. Edits outside a region never affect it; edits inside carry it. An edit that would remove a suggestion refuses, listing every id, unless its block's header has `reject`:

1. deleting its content (including hidden insertions inside deleted content);
2. a text edit crossing its edge;
3. changing a one-character region;
4. replacing text across a hidden insertion.

Replacements separated only by spaces count as one edit (`Fast and` → `Quick or` is one edit crossing `Fast`'s edge). Text inserted at a region's edge stays plain; content written at a hidden insertion's position goes before it.

## Moves

| Goal | Move |
| --- | --- |
| Carry a text edit (region of 2+ characters) | Insert the new text at an interior position, then delete the old characters |
| Carry a restyle | The normal style request; the suggestion survives |
| Reject a text insertion / deletion / style | Delete the inserted text / rewrite the text plain in current style |
| Reject a paragraph-style suggestion or a suggested paragraph-break deletion | Re-break: delete the newline, insert a new one, restore both paragraphs' styles |
| Reject a suggested paragraph break | Delete just that newline, joining the paragraphs |
| Reject a row insertion | Delete the row |
| Reject a row deletion or cell-style suggestion | Insert a new row, fill it, delete the old one |
| Reject an image deletion / insertion | Insert a copy from `contentUri`, delete the original / delete it |

Accept by edit: write the proposal into the tab body and add `reject`.

## Refused

- Page-layout, named-style, list, row-style, column and footnote suggestions have no reject move.
- Re-breaks need a plain next paragraph (no list, heading or paragraph-level suggestion); paragraph-break joins need plain paragraphs on both sides.
- Row rebuilds need unmerged cells holding one paragraph of text, and every other suggestion in the row rejected too.
- Edits to a paragraph joined by a hidden suggested paragraph break, or to a table with a hidden suggested row, refuse until that suggestion is rejected: which side the edit belongs to would be a guess.

The emulator models Google's suggestion behavior (see [google-api](google-api.md)) and the plan self-check compares every suggestion mark, so a plan that would fragment or drop a suggestion fails before sending.
