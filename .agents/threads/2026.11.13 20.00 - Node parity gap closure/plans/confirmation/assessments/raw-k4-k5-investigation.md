# Raw K4 & K5 Investigation (`10-T3`)

## D-K4: `V008-reject-hidden-insertion` `textExport` Count & Classification

### Observed `callLog` (`differential-results.json`)

- **Node (`V008-reject-hidden-insertion`)**: 4 `textExport` calls:
  - `[2] textExport("synthetic-doc")`
  - `[5] textExport("synthetic-doc")`
  - `[8] textExport("synthetic-doc")`
  - `[9] batchUpdate("synthetic-doc")`
  - `[10] getDocument("synthetic-doc")`, `[11] commentsList("synthetic-doc")`, `[12] textExport("synthetic-doc")`
- **C++ (`V008-reject-hidden-insertion`)**: 2 `textExport` calls:
  - `[4] textExport("synthetic-doc")`
  - `[8] textExport("synthetic-doc")`
  - `[9] batchUpdate("synthetic-doc")`
  - `[10] getDocument("synthetic-doc")`, `[11] getFileMetadata("synthetic-doc")`, `[12] commentsList("synthetic-doc")` (no `textExport`)

### Root Cause

1. **Node (`../gdocsmith-node/src/core/engine/export.ts:245-250`)**: `documentTextFetch` fetches `drive.textExport(model.docId)` whenever:
   - `hiddenChipTabs(model).length > 0`, **or**
   - `comments.some((comment) => !comment.deleted && comment.quotedFileContent?.value)`.
   This labels hidden chips (`chipKindsLabel`) in addition to locating quoted comments.
2. **C++ (`src/engine/import.hpp:1204-1206`)**: In `refresh()`, C++ only calls `drive.textExport(id)` when:
   ```cpp
   if (std::any_of(comments.begin(), comments.end(), [](const auto& c) { return !c.quote().empty(); }))
   ```
   It does **not** check whether the model contains hidden chips.

### Classification

- **Not an adapter boundary difference**: Both `tests/parity/cpp-import-adapter.cpp` and `tests/parity/node-import-adapter.mjs` wire `textExport` directly to the underlying engine.
- **Not a `batchUpdate` verification gap**: `batchUpdate` verification uses `getDocument` (`[13]` in C++, `[13..14]` in Node).
- **Engine-level re-export gap**: Skipping the final `textExport` in `src/engine/import.hpp:1204` means C++'s post-write re-export omits hidden-chip kind labeling when no quoted comments are present.

---

## D-K5: `V010-cross-doc-image-copy` Early Read Order & Source-Read-Fault Behavior

### Observed `callLog` (`differential-results.json`)

- **Node (`V010-cross-doc-image-copy`)**:
  - `[0] getDocument("synthetic-source")`, `[1] commentsList("synthetic-source")` (setup)
  - `[2] getDocument("synthetic-doc")`, `[3] commentsList("synthetic-doc")` (setup)
  - `[4] getDocument("synthetic-source")` (pre-fetch of cross-doc `gdoc:` source)
  - `[5] getDocument("synthetic-doc")`, `[6] commentsList("synthetic-doc")` (`session.docOpen` of destination)
  - `[7] batchUpdate("synthetic-doc")`
  - `[18] getDocument("missing-source")` (negative-case source-read fault)
- **C++ (`V010-cross-doc-image-copy`)**:
  - `[0] getDocument("synthetic-source")`, `[1] getDocument("synthetic-doc")` (setup)
  - `[2] getDocument("synthetic-doc")`, `[3] getFileMetadata("synthetic-doc")`, `[4] commentsList("synthetic-doc")` (`session.docOpen` of destination)
  - `[5] getDocument("synthetic-source")` (lazy fetch inside `session.atomOptions.images`)
  - `[6] batchUpdate("synthetic-doc")`
  - `[25] getDocument("missing-source")` (negative-case source-read fault)

### Copy-Source vs Destination-Comment Read Order

- **Node (`../gdocsmith-node/src/core/engine/import.ts:122-149`)**: Scans `snapshot.text` for `gdoc:<id>/` image sources and `gdoc:<id>` links and pre-fetches each non-self `id` via `services.client.getDocument(id)` **before** entering `transactionRun` and calling `session.docOpen(file.doc)` (which reads the destination document and its comments).
- **C++ (`src/engine/import.hpp:1236-1259`)**: Calls `session.docOpen(doc)` **first** (reading destination `getDocument`, `getFileMetadata`, and `commentsList`), and lazily fetches `sourceId` inside `session.atomOptions.images` when resolving image atoms.

### Source-Read-Fault Behavior

- **Node (`../gdocsmith-node/src/core/engine/import.ts:132-138, 217, 303`)**: Catches `getDocument(id)` errors during pre-fetch and records `unreadable.set(id, error)`, deferring failure to `imageResolverBuild` / `documentLinksResolve` so only tokens/links that actually reference the unreadable source fail (e.g., a mention inside a code block only costs a fetch).
- **C++ (`src/engine/import.hpp:1246-1250`)**: Lazily fetches `sourceId` only when an image atom actually references it, and immediately throws `model::CoreError("anchorNotFound", "document " + sourceId + " couldn't be read: " + error.what())` if `services.getDocument(sourceId)` fails.
