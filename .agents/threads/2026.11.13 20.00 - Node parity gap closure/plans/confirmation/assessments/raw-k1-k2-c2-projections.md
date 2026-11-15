# Raw K1/K2 & C2 Projections (`confirmation-03/differential-results.json`)

Source: `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/confirmation-03/differential-results.json`
Scope: All 11 items (`10` cases + `1` variant: `V009-image-upload-lifecycle [known-upload-error]`).

## D-K1/K2: `callLog` Method Subsequence Alignment

Stripped methods: `getFileMetadata`, `commentsList`.

| Case / Variant | Node Raw | C++ Raw | Node Stripped | C++ Stripped | Exact Match | Residual Mismatch |
| --- | --- | --- | --- | --- | --- | --- |
| `V001-noop` | 7 | 8 | 4 | 4 | `true` | None |
| `V002-body-and-title` | 11 | 12 | 8 | 7 | `false` | Node has 1 extra trailing `getDocument` |
| `V003-stale` | 7 | 7 | 5 | 4 | `false` | Node has 1 extra trailing `getDocument` |
| `V004-landed-lost-response-replay` | 12 | 14 | 8 | 7 | `false` | Node has 1 extra trailing `getDocument` |
| `V005-font-default` | 9 | 9 | 6 | 5 | `false` | Node has 1 extra trailing `getDocument` |
| `V006-font-nondefault` | 8 | 8 | 6 | 5 | `false` | Node has 1 extra trailing `getDocument` |
| `V007-cell-style-edit` | 9 | 9 | 6 | 5 | `false` | Node has 1 extra trailing `getDocument` |
| `V008-reject-hidden-insertion` | 15 | 14 | 11 | 8 | `false` | Node has 4 `textExport` vs C++ 2 `textExport`, plus 1 extra trailing `getDocument` |
| `V009-image-upload-lifecycle` | 19 | 23 | 12 | 12 | `true` | None |
| `V010-cross-doc-image-copy` | 23 | 27 | 14 | 14 | `true` | None |
| `V009-image-upload-lifecycle [known-upload-error]` | 15 | 16 | 11 | 10 | `false` | C++ has `createDocument -> getDocument -> deleteFile` vs Node `createDocument -> deleteFile`; Node has 1 extra trailing `getDocument` |

## D-C2: Exported Frontmatter Key Sets & Style Declarations

| Case / Variant | Side | Top-Level Keys | `namedStyleSets` | `cellStyles` | FM Bytes | Total Bytes |
| --- | --- | --- | --- | --- | --- | --- |
| `V001-noop` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 533 |
| `V001-noop` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `2` | `0` | 269 | 440 |
| `V002-body-and-title` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 541 |
| `V002-body-and-title` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `2` | `0` | 269 | 448 |
| `V003-stale` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 533 |
| `V003-stale` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `2` | `0` | 269 | 440 |
| `V004-landed-lost-response-replay` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 541 |
| `V004-landed-lost-response-replay` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `2` | `0` | 269 | 448 |
| `V005-font-default` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 451 |
| `V005-font-default` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `1` | `0` | 180 | 267 |
| `V006-font-nondefault` | Node | `null` (no exported markdown) | `n/a` | `n/a` | `n/a` | `n/a` |
| `V006-font-nondefault` | C++ | `null` (no exported markdown) | `n/a` | `n/a` | `n/a` | `n/a` |
| `V007-cell-style-edit` | Node | `gdocsmith, doc, revision, title, apiUser, cellStyles, namedStyleSets` | `0` | `0` | 419 | 529 |
| `V007-cell-style-edit` | C++ | `gdocsmith, doc, revision, title, cellStyles, namedStyleSets` | `1` | `1` | 240 | 359 |
| `V008-reject-hidden-insertion` | Node | `gdocsmith, doc, revision, title, apiUser, commentsLine, namedStyleSets` | `0` | `0` | 383 | 576 |
| `V008-reject-hidden-insertion` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `1` | `0` | 180 | 386 |
| `V009-image-upload-lifecycle` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 491 |
| `V009-image-upload-lifecycle` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `1` | `0` | 180 | 307 |
| `V010-cross-doc-image-copy` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 459 |
| `V010-cross-doc-image-copy` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `1` | `0` | 180 | 275 |
| `V009-image-upload-lifecycle [known-upload-error]` | Node | `gdocsmith, doc, revision, title, apiUser, namedStyleSets` | `0` | `0` | 366 | 753 |
| `V009-image-upload-lifecycle [known-upload-error]` | C++ | `gdocsmith, doc, revision, title, namedStyleSets` | `1` | `0` | 180 | 569 |

### `V005-font-default` 52-Byte Shrink on Re-Export

- **Node:** `initial` = 443 bytes, `exported` = 451 bytes (`+8` bytes).
- **C++:** `initial` = 319 bytes (`fmLen` = 180 bytes / 12 lines, `bodyLen` = 139 bytes); `exported` = 267 bytes (`fmLen` = 180 bytes / 12 lines, `bodyLen` = 87 bytes).
- **Explanation:** Frontmatter size is unchanged (180 bytes). The entire `-52` byte delta (`139 -> 87`) is in the document body, where C++ omits the redundant default-font attribute/span on re-export.
