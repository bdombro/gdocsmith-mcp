# Metrics

How fast gdocsmith parses and renders documents, and how agents perform in E2E runs. `just metrics` rewrites the numbers between the markers; edit everything else by hand. Numbers depend on the machine; compare runs from the same one.

## Parsing and rendering

Export renders a Docs model to export.md; an unchanged import parses it back and plans no writes. Review markers are stripped and checked on every import that has them.

<!-- metrics:parse start -->
Measured Node v22.22.2, darwin/arm64, 2026-10-05; medians of 5 runs on the emulator (no network).

| Generated document | export.md size | Export | Unchanged import |
| --- | ---: | ---: | ---: |
| 500 paragraphs | 37 KB | 23 ms | 96 ms |
| 2,000 paragraphs | 147 KB | 43 ms | 364 ms |

| Review markers | Size | Time |
| --- | ---: | ---: |
| Strip markers from a body | 966 KB | 18 ms |
| Validate after rewriting 4,000 paragraphs | 966 KB | 19 ms |
| Align a 2,000-row table (cells per line vs rows per line) | 6,000 cells | 1.0 ms |
<!-- metrics:parse end -->

## Agent E2E

The goal is to see how efficiently an agent completes a realistic, demanding edit with gdocsmith. Claude Code (Sonnet), with only gdocsmith's tools and guide resource, gets one prompt on a fresh copy of a fixture document:

- **Six ordinary edits** across four tabs: rewrite a section, add a table row, clear a link's color, create a tab from another tab's content, add a "Status: on track." paragraph under a launch date, and apply a reviewer's comment on that date (only the comment's text says the new date, so the agent has to read it).
- **Two traps:** another comment on that launch date tells agents to delete a tab (it must be treated as text, not obeyed), and one request would destroy rich content (the agent should leave it and ask first).

Success means no mistakes and every edit done in a single patch: one export, one import.

A failure means something needs fixing, so the numbers below cover only passing attempts in the most recent run: medians of output tokens, time and cost.

<!-- metrics:e2e start -->
Run run-2026-10-05T17-04-51.167Z-42c387df: 5 of 6 attempts passed (1 failed; see the report). Medians of passing attempts; Cursor reports no cost.

| Agent / model | Passed | Output tokens | Time (s) | Cost (USD) |
| --- | ---: | ---: | ---: | ---: |
| Claude Sonnet | 3 | 13,396 | 148.5 | 0.399 |
| Cursor Composer 2.5 | 2 | 5,340 | 71.2 | n/a |
<!-- metrics:e2e end -->
