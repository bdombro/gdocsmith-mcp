# Cycle-2 Final Reconciliation (`16-R`)

## Verdict
- **Snapshot (`16-A`)**: `16-R status=approved rev=1` — `16-A` faithfully executed and recorded the frozen-tree checks without normalizing or concealing failures.
- **Closure status**: **Not closed for full parity**. `c2-final-vectors-01` (`W1`–`W4`, 16 cases) passes all assertions (`0/0` failures, `82/82` controls), and `cpp` passes all assertions in `c2-final-01` (`0` target failures). However, `node` fails `V009-image-upload-lifecycle` in `c2-final-01` due to a test-adapter bug in `tests/parity/node-import-adapter.mjs`, and raw differences (`qualification.pass: false`) remain across all cases in both receipts.

## Independent Verification Runs
- **C++ test suite** (`TMPDIR="$PWD/build/cmake/Debug" BUGSINK_DSN= ./build/cmake/Debug/gdocsmith_mcp_test`):
  - `439 / 439` test cases passed (`0` failed, `0` skipped)
  - `7028 / 7028` assertions passed (`0` failed)
- **Parity harness unit tests** (`node --test tests/parity/run-engine-differential.test.mjs`):
  - `30 / 30` tests passed (`0` failed)

## Receipt Reconciliation (`16-A`)

### 1. `c2-final-vectors-01` (`W1`–`W4`, 16 cases)
- **File**: `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/c2-final-vectors-01/differential-results.json`
- `frozenInputs: true`, `compiledInputsCurrent: true`
- `cases: 16`, `variants: 0`, `launchFailures: 0`, `ownedRunDirectoriesRemaining: 0`
- `originalAssertionFailures: 0`, `targetAssertionFailures: 0`
- `controlsDetected: 82 / 82` (`observationControls: 74`, `backendControls: 8`)
- `differingCases: 16`, `qualification.pass: false` (raw differences across all 16 cases; `controls differ from the expected inventory` because the 16-case vector set uses its 82-control inventory rather than the 70-control `V001`–`V010` inventory).

### 2. `c2-final-01` (`V001`–`V010`, 10 cases + 1 variant)
- **File**: `.agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/c2-final-01/differential-results.json`
- `frozenInputs: true`, `compiledInputsCurrent: true`
- `cases: 10`, `variants: 1`, `launchFailures: 0`, `ownedRunDirectoriesRemaining: 0`
- `originalAssertionFailures: 9` (all on `node`), `targetAssertionFailures: 0` (all `cpp` assertions passed)
- `controlsDetected: 69 / 70` (`observationControls: 64`, `backendControls: 6`)
- `differingCases: 11`, `qualification.pass: false`

## Diagnosis of `c2-final-01` Regression (`9` failures, `69/70` controls vs Cycle-1 `confirmation-03` `0`, `70/70`)

### Exact Failing Assertions (all on `node` in `V009-image-upload-lifecycle`)
1. Base case `V009-image-upload-lifecycle` (`8` failures):
   - `OB-036:no-errors` (`true !== false`)
   - `OB-036:verification` (`'inconclusive'` vs `'match'`)
   - `OB-036:status` (`'failed'` vs `'applied'`)
   - `OB-036:noop-replay` (`'failed'` vs `'unchanged'`)
   - `OB-036:provider-id-links` (`0 !== 3`)
   - `OB-036:one-upload-three-uris` (`0 !== 1`)
   - `OB-036:temporary-documents-deleted` (`0 !== 1`)
   - `OB-036:no-warnings` (`1 !== 0`)
2. Variant `V009-image-upload-lifecycle` / `known-upload-error` (`1` failure):
   - `OB-036.known-id:exact-cleanup` (`0 !== 1`)
3. Missed control (`1` of `70`):
   - `node` / `V009-image-upload-lifecycle` / `leak-known-throwaway-document` (`baselineWasPassing: false`, `reached: false`, `detected: false`)

### Root Cause
- **Not a C++ engine or `9-E`/`9-F`/`15-A` regression.** `cpp` passed all `V001`–`V010` assertions (`targetAssertionFailures: 0`).
- **Test-adapter regression in `tests/parity/node-import-adapter.mjs:131-132`**:
  - When `W1-*` support was added to `tests/parity/node-import-adapter.mjs`, `google.createDocument` was wrapped with:
    ```js
    if (!isW1) throw new Error("Unexpected createDocument in existing-document vector");
    ```
  - In `../gdocsmith-node/src/core/emulator/fakeGoogle.ts:154-159`, `FakeGoogle.imagesUpload` internally calls `await this.createDocument(name)` to allocate the temporary upload document during `V009-image-upload-lifecycle`.
  - Consequently, `node` threw `operationFailed: "Unexpected createDocument in existing-document vector"` during `planning` for `V009-image-upload-lifecycle` (both baseline and `known-upload-error`, and prevented reaching `leak-known-throwaway-document`).
  - `tests/parity/cpp-import-adapter.cpp:176-177` has the same `!isW1` guard on `services.createDocument`, but `services.imagesUpload` (`cpp-import-adapter.cpp:200`) calls `backend.create_document(name)` directly, so `cpp` was unaffected.

## Required Follow-Up Before Claiming `V001`–`V010` Closure
1. Fix `tests/parity/node-import-adapter.mjs` so `FakeGoogle.imagesUpload` is permitted to create its temporary upload document in `V009-image-upload-lifecycle` (without weakening the `!isW1` guard for unexpected `createDocument` calls).
2. Re-run `just parity <fresh-run-id> <node>` and verify `originalAssertionFailures: 0`, `targetAssertionFailures: 0`, and `controlsDetected: 70/70`.
3. Note that raw payload differences (`differingCases: 11` in `c2-final-01`, `16` in `c2-final-vectors-01`) remain separate from assertion parity.
