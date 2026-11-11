# Markdown Files

The editable file is a view of a Google Doc, not a lossless backup of every object. Unchanged content stays in place; changed paragraphs receive character-level edits.

```markdown
---
gdocsmith: 3.0.12 # exact gdocsmith release; import refuses others
doc: document-id
revision: revision-id
title: Document Title
---
::tab{id=t.0 title=Overview}

## Summary

Text.
```

Keep identity/revision unchanged. Null doc creates a document. Existing documents need a tab line for every existing tab; a missing one refuses. Content omitted inside a tab is removed. Code-fenced and escaped directives remain literal text. Editing the frontmatter title renames the document after content lands (not revision-checked).

## Grammar

The file is GFM plus CommonMark generic directives for everything GFM can't express. gdocsmith parses them with its own marked extensions (`directives.ts`, `markedLens.ts`).

| Form | Use | Example |
| --- | --- | --- |
| `:name{attrs}` | Inline object, no content | `:chip{kind=vote}` |
| `:name[label]{attrs}` | Inline span; on an exported object the label is read-only | `:style[March 1]{.accent}` |
| `::name{attrs}` | A line of its own | `::section{nextPage}` |
| `::name{… start}` … `::name{… end}` | A block, as paired lines | `::cell{id=t1r2c3 start}` |

- **Attributes:** space-separated `.class` (styles only), `name=value` and bare camelCase flags. A value is quoted (JSON string) only when it contains whitespace, `=`, quotes, braces or brackets; lists use commas (`margins=54,54,54,54`). Use `id=…`, never `#…`.
- **Casing:** Google's enum values stay verbatim (`CENTER`, `HEADING_1`, `A4`); gdocsmith's keywords and flags are camelCase (`firstPage`, `nextPage`).
- **Names:** only registered names are directives; any other `:word` stays text, so prose like "10:00" or "note:this" needs no escaping. Export escapes `[` and `{` in text, so text never forms a directive.
- **Colons:** an object may directly follow a colon (`Date::date[…]{…}` is "Date:" plus a chip). The markers `:p` and `:cell` may not, because `::cell{…}` is a definition line.
- **Blocks are paired lines, not `:::` containers.** Cell definitions nest (nested tables) and named start/end lines nest without colon counting.
- **Tables:** inside a GFM table cell, `|` is written `\|`.
- **Labels:** an exported object's `[label]` is informational and read-only; changing it refuses.

## Tabs

The tab line holds the tab's structure and page layout: `::tab{id=… title=… parent=<tab id> size=<NAME>|<W>x<H> margins=<top>,<right>,<bottom>,<left> namedStyleSet=<name> background=#RRGGBB emoji=<emoji> footerMargin=<pt> headerMargin=<pt> pageNumberStart=<n> pageful landscape firstPage oddEven delete}`. Only id and title are required. Export writes values (id, title, parent, size, margins, namedStyleSet, then the settings) and then flags, and never `delete`.

Settings: `emoji` is one emoji (ZWJ sequences count as one) shown as the tab's icon; `background` is the page color; `headerMargin` and `footerMargin` are points from the page edge; `pageNumberStart` numbers the first page. Export omits defaults (no emoji, white, 36pt, 1), and an absent setting means its default.

- A changed title renames the tab. Titles must be unique ignoring case once the import's renames, creations and deletions apply, so titles can be swapped.
- `parent` names an existing tab whose line comes earlier; absent means root. A changed, added or removed parent moves the tab with its children.
- Siblings take tab line order. An empty ID (`id=""`) creates a tab where its line is; it can be a parent only after export gives it an ID.
- `delete` removes an existing tab and ignores its body. A tab whose parent stays the deleted tab must also be marked (give it another parent to keep it). Deleting every tab clears the original tab and renames it "Tab 1" instead. Results list `deletedTabs`. Guards still protect comment anchors and named ranges in the body.

Page attributes: A line without `pageful` is pageless; size, landscape and margins require `pageful`. Names are A3, A4, A5, LEGAL, LETTER and TABLOID (portrait; `landscape` swaps them). Custom sizes and margins are literal points; `landscape` with a custom size refuses. Export omits the gdocsmith defaults, LETTER portrait and 72pt margins. Importing a pageless line leaves stored size/margins untouched; a pageful line writes the explicit size and all four margins.

File order: frontmatter, tabs, comment blocks, suggestion blocks. Frontmatter keys: gdocsmith, doc, revision, title, variables, dropdowns, textStyles, paragraphStyles, cellStyles, namedStyleSets, plus read-only apiUser, commentsLine and suggestionsLine (export regenerates them; import ignores them).

## Dropdowns

Frontmatter `dropdowns` maps tab ids to option lists by title; an option is a name or `Name: {color, background}`. Import applies the lists before the body (so new chips can name them): titles keep their definition ids; options match by name, and an unmatched option at the same position is renamed in place (chips selecting it relabel, as Google does). New definitions and options get client-chosen ids, so creation needs no reply. Requests run in order: create lists and widen changed ones (keeping dropped options at the end), content (new `:dropdown{list=… option=…}` chips), reselections (`option=` on an existing chip, `updateDropdownProperties`), then the final option lists and deletions. A list must keep 2–50 distinct options; removing a list or option a chip still uses refuses. Google stores an unset color of a styled option as `{}`.

## Named Ranges

Named ranges export as zero-width marker pairs, `:namedrange{name=X start}` and `:namedrange{name=X end}`, at their start and end indices, in body paragraphs and table cells (ends before starts at one index). Ranges whose ends fall outside paragraphs (a table edge) and ranges in headers, footers and footnotes stay unshown and untouched. Import pairs written ranges with exported ones by name, in order. A range whose markers still sit beside the same kept characters is left to Google, which moves it with edits (inserts at an edge land outside); verification checks it lands where its markers say. A moved marker, or text typed just inside one, recreates the range after content (`deleteNamedRange` then `createNamedRange` at the final indices); a new pair creates one, and removed markers delete it. `replaceNamedRangeContent` is never used (it collapses formatting and merges paragraphs). Empty ranges, unpaired markers and changes to multi-span ranges refuse.

## Footnotes

Footnotes are GFM footnotes. Export writes `[^n]` at each reference, numbered in document order per tab, and `[^n]: body` definitions at the end of the tab body (after everything else, before header and footer lines; later lines indented four spaces). A blank body (Google's `" "`) is `[^n]:`. `[^n]` with an exported number names that footnote; any other label creates one (`createFootnote`, whose body starts as `" \n"`; the body is written in a later batch once the reply names the new footnote). Editing a definition rewrites the body through the footnote segment. Moving a reference recreates the footnote there, with its definition as the body. Removing a reference deletes the footnote, and its definition must go too. Every reference needs one definition and every definition a reference. Footnotes can't be referenced from headers, footers or footnotes. New footnote text takes the default size, not the 10pt Google's UI uses.

## Sections

A section break line carries its type and layout: `::section{id=sN columns=N columnGap=<pt> margins=<top>,<right>,<bottom>,<left> pageNumberStart=N nextPage|continuous columnSeparator firstPage landscape rtl}`. A new break omits id; `rtl` sets `contentDirection: RIGHT_TO_LEFT`. Export shows what's set and omits defaults (one column, 36pt gap, no separator, inherited margins and numbering). The type is fixed at creation, so changing it refuses. Google can't clear a section field once set: removing a flag sets it false, removing columns sets one column, and removing set margins or pageNumberStart refuses. Section style requests target the character after the break, because a range on a break inserted earlier in the same batch also restyles the previous section. Every tab body starts with its opening section's line, `::section{id=sN columns=… columnGap=… columnSeparator rtl}` with no type; the tab line holds the rest of its layout. Content above it, or dropping it from an existing tab, refuses; a new tab may omit it. Adding columns shrinks wider inline images and drawings to fit, permanently.

## Headers And Footers

After a tab's body, `::header{kind=default|firstPage|evenPage}` or `::footer{kind=…}` starts that segment's complete content (body syntax). `section=N` names a later section's own header or footer (section 2 starts at the tab's first `::section` break); export writes the tab's own first, then each section's. Every existing header and footer needs its line; `delete` on the line removes one; a new line creates a default one (Google can't create first-page or even-page ones). The tab line's `firstPage` and `oddEven` flags switch those pages' separate headers on or off.

## Content

Supported: headings, paragraphs, same-kind nested lists, `- [ ]` checklists, code, emphasis, links, GFM tables, text styles and paragraph declarations.

A `::list{preset=NAME}` line directly before a list names one of the 15 `createParagraphBullets` presets; export writes it only for a non-default preset. Changing or removing it rebuilds the list with the new glyphs. Lists with custom glyphs match no preset, export no line and keep their glyphs while untouched.

Code is gdocsmith's code font, Roboto Mono Light (`CODE_FONT_STYLE`): a run in it is inline code, a paragraph entirely in it is a fence line, and new code is written in it. Other monospace fonts are ordinary text styles. A Google Docs code block (a U+E907 start marker before its first line, and a paragraph that is only U+E907 after its last) exports as a fence whatever its font; the markers stay hidden and survive edits, and typed text takes the neighboring code's font.

Tables keep every column in every row. A cell whose whole content is `:merge{left}`, `:merge{up}` or `:merge{down}` joins that neighbor's merged region; markers chain. Each region must be a rectangle with exactly one content cell. Export puts content in the top-left cell, `:merge{left}` in the rest of the top row and `:merge{up}` below. A cell with trailing empty paragraphs edits like a one-paragraph cell and keeps them.

A `::table{widths=<pt|auto>,… headerRows=N heights=<pt>,… preventOverflow=<row>,…}` line directly before a table holds its layout (column widths, pinned header rows, row minimum heights with `0` for automatic, and 1-based rows kept on one page). Export writes only non-default attributes and omits the line for a plain table. An absent attribute, or no line, means the default; a changed list must match the table's size. Merged rows carry Google's 21pt minimum, which export treats as the default and which stays after unmerging.

Any other cell (several paragraphs, a heading, list, line break or nested table) is exactly `:cell{ref=name}` (name `[A-Za-z0-9_-]+`, unique per tab) and its content is ordinary markdown between block lines `::cell{id=name start}` and `::cell{id=name end}`. A table's definitions follow it directly, in any order, before other content; a nested table's definitions sit inside the enclosing definition. Export names cells by table ordinal and 1-based row/column, nested ones prefixed by their cell (`t2r1c3`, `t2r1c3-t1r2c1`); re-exports are stable and imports accept any name. Missing, unused, duplicate, twice-referenced, misplaced or misnested definitions, and references mixed with other cell text or merge markers, are refused. A GFM table written in a definition creates a nested table and removing it deletes the table; nested tables edit in place like body ones (text, rows, columns, merges). A table of contents or other object in a cell exports as its object directive and stays in place. Page and section breaks can't go in a cell (Google Docs refuses them). Unknown fields and unsupported syntax refuse rather than coerce. Existing UI-created mixed-kind lists remain preserved when untouched; authoring changes to them may refuse.

## Objects

Exported objects are `:kind[label]{id=… attributes}` inline, or a `::kind{id=…}` line for page breaks, section breaks (`::section`) and tables of contents (`::toc`). Import matches each token to the exported object of its kind with that id in its tab; a token without an id is new, and a repeated id copies. Ids come from one index of the document (`objectIdsAssign`):
- **Objects with a Google id** (chips, images, drawings, charts, positioned objects) take its tail: 4 characters, lengthened until unique across the document.
- **Others** take a kind counter across the document: `s` (sections, the opening one included), `pb`, `hr`, `toc`, `cb`, `at`, `eq`.
- **Chips** also carry their creation attributes: person `email`, date `value`/`time`/`zone`/`format`, rich-link `url`, dropdown `list`. Editing them (or an unknown id) recreates the chip from them. Kinds: person, date, richlink, image, positioned (a floating object, zero-width at the start of its anchor paragraph; deleting the token sends `deletePositionedObject`, moving it refuses), equation, drawing, chart, dropdown, hr, columnbreak, autotext, pagebreak, section, toc.

Chips the API shows only as U+E907 export as `:chip{kind=…}` (kind from each tab's plain-text export, fetched only for tabs that have them; `unknown` inside tables and headers, `watermark` at a header's start). A placeholder pair around trimmed text is a variable, `:variable{id=v1}`, its value in the frontmatter `variables` (ids by first appearance; equal values share an id). Both parse back to the same U+E907 text, so unchanged files write nothing; a value edit rewrites every instance; inserting U+E907 (a new or moved chip) refuses, since Google drops it. Preserve them exactly; never change a ref or label. New objects: `:person{email=…}`, `:date{value=YYYY-MM-DD format=iso|short|long time=HH:MM zone=<IANA> locale=<code>}` (export shows `locale` unless `en`) (a date alone is noon UTC; a time without a zone is floating wall time; a time with a zone is that moment, shown with the zone), `:richlink{url=…}`, `:image[Alt]{src=… width=<pt> height=<pt>}` (a sized image; one dimension keeps the aspect ratio), `src=` on an existing image (`replaceImage`: same object and size, CENTER_CROP), `::pagebreak`, `::section{nextPage|continuous}`, and markdown images. Foreign object references copied into a new tab or document can be omitted with explicit warnings.

## Styles

`textStyles` maps names to text properties; apply with `:style[text]{.a .b}`, whose classes compose (two setting the same property refuse). Export declares one style per property value, named for it (`size9`, `colorE01C47`, `bgFFF4CC`, `fontRobotoMono`, `weight300`, `super`, `noBold`), shared across tabs. `paragraphStyles` maps names to paragraph properties; end the paragraph with `:p{.name}`. `cellStyles` maps names to cell properties; end a cell with `:cell{.name}`. `namedStyleSets` maps set names to named-style definitions, picked by a tab line's `namedStyleSet=…`. Shared declarations affect all included uses. Null resets supported properties; absence is not a blanket formatting reset.

## Links

Use `#heading-slug` within the tab, `#tab-slug__heading-slug` across tabs, or `#tab-slug__` for a tab. Slugs lowercase normalized text and replace whitespace with hyphens; reserved characters are percent-encoded. New ambiguous or missing targets refuse. Unchanged links preserve known existing targets, including duplicate headings, rather than guessing a new destination.

## Comment And Suggestion Blocks

The first `::comment` line ends the last tab's body; comment blocks hold a `::comment{…}` header, literal text and `::reply{…}` blocks, and are editable (see [comments](comments.md)). The first `::suggestion{…}` line starts the suggestion blocks, which run to the end of the file; each has `::before` and `::after` snippets with the suggested span in `:change[…]`. They must come back exactly as exported except for `reject` flags added to headers (see [suggestions](suggestions.md)).

Table styling, equations, drawings, charts, TOCs and positioned objects are not freely editable/recreatable. Their unchanged model data is preserved.
