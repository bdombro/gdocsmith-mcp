# D5 corrected for task 5, contract rev 3

This replaces the rev 2 graph wherever they differ. It's a read-only return for the lead to record as-is; I sent the lead a bus pointer saying it's here. It changes no audit findings and authorizes no implementation. Execution stays parked until the user reviews it, so wave 1 should not launch automatically.

## Rev 3 questions answered

1. **6-A was a false barrier.** 8-A, 12-A, 7-A, 11-A and 10-A each write their own existing or feature test files. None of them consumes a scaffold.
   - The only real prerequisite is a build resource for their **check step**, meaning a build directory nobody else is using at the same time.
   - Editing never waits on it. Each executor edits, then queues its check for a build-dir token, recorded as a `blocked_on` queue reason rather than a dependency.
   - Default token: one existing build directory assigned by the lead, used by one task at a time. That's `build/cmake/GapChecks`, unless the lead names another or needs its current binaries kept as history.
   - 6-A is now optional. It checks whether the existing `build/cmake/Swarm*` directories can serve as extra tokens. Only tasks assigned to those directories depend on it.
2. **Dropped:** empty scaffold files and new build machinery. Shared import tests are handed off in the smallest way that works:
   - **7-A** owns `tests/engine/test_import.hpp`. This matches D4-R1, which puts the R1 cases at the existing `RemoteImportFixture` seam.
   - **11-A** creates a new header, `tests/engine/test_import_structure.hpp`, for the hierarchy, page and section transfer.
   - **10-A** creates a new header, `tests/engine/test_import_errors.hpp`, for image-refusal context.
   - **`tests/test_main.cpp`** is edited one include line at a time, holding the file only for that edit, in the order 11-A → 10-A → 10-C. The lead names the current holder in each contract. Each holder records the one-line edit in its Notes, then releases the file.
3. **Review prerequisites** are all spelled out in the table below, including 10-BR and 12-R. The "no phase barriers" claim is corrected in §3.
4. **Edit capability is a precondition for every edit-requiring executor.**
   - Facts: this decider ran as claude-opus-5.5/high in this host and had no file-edit tool. Tasks 1–4 ran on unspecified host defaults and recorded that file-edit tools were available.
   - Changing the agent type or reasoning effort is not known to restore edit tools.
   - Rule: an executor confirms it has file-edit tools as its first logged step. If it has none, it returns blocked immediately with no edits and no shell writers.
   - Who resolves that: the lead or the user picks an edit-capable launch configuration. That's a user choice where the preferred model conflicts with needed capability.
   - Read-only roles can always hand off through their native response, as in rev 2: the deciders (10-D*), triage (10-T*), the brief author (12-B), all reviewers (\*-R) and 13-R.
   - Runtime-only tasks (6-A, 6-B, 13-A) produce outputs from the commands they run and return notes natively. An edit block never stops unrelated read-only work.
5. **13-A is a convergence snapshot, not a closure claim.** It covers the cycle-1 repairs that were reviewed and selected, on a frozen tree. The lead must still accept it.
   - Still pending: 10-D2, 10-T1, 10-T2, 10-D3, P1 if not selected, P3, W1/W3/W4, the SA-04 corpus check, and 9-G.
   - The whole-source assertion denominator stays unknown.
   - The 206 raw groups / 250 leaves remain unresolved apart from the six traced, and the raw differential remains RED.

## 1. Corrected task table

How to read the table:
- **Status:** **F** = final graph decision. **C** = conditional branch, which is not authorized implementation until its condition is met and the lead releases it.
- **Levels:** L0 executor or author (low effort), L1 refiner (medium), L2 decider or reviewer (high).
- **G0** = lead accepts tasks 2 and 4, selects the W rows, and grants runtime and build tokens.
- **BUILD** = queues for a build-dir token at the check step only.
- **Design gate** = the existing brief, refinement or decision that clears the task to start.

| ID (parent) | Status | Level | Depends on | Unique writes | Design gate | Acceptance |
|---|---|---|---|---|---|---|
| 6-A (6) | C (only if parallel builds are wanted) | L0 runtime | G0 | none (notes returned natively) | rules (one owner per build output) | `CMakeCache.txt` flags per `Swarm*` dir. One test build with no configure or install. `--test-case=` selector works and an empty selection fails. Stop at any configure step. |
| 6-B (6) | F | L0 runtime | G0 | fresh `tasks/parity-evidence/confirmation-orig-01/` | workspace.md runners | Pass/fail for each selected original row (R1, R2, R3, W2, P2, font, myers rows). `../gdocsmith-node` status and identity unchanged. Missing dependencies reported, never installed. |
| 7-A (7) | F | L0 | 4; BUILD | `src/lens/put.hpp`, `src/engine/suggestion_rule.hpp`, `src/engine/session.hpp`, `tests/lens/test_put.hpp`, `tests/engine/test_import.hpp`, `docs/suggestions.md` only if documented capability changes | D4-R1 (tasks 1→4) | D4-R1 cases, controls and boundary cases. The failing boundary is recorded before any repair. |
| 7-R (7) | F | L2 | 7-A, 6-B | none | D4-R1 | Independent review against the original's evidence |
| 8-A (8) | F | L0 | 4; BUILD | `src/api.hpp`, `tests/api/test_api.hpp` | D4-R3 | D4-R3 checks and controls. Retry policy unchanged. |
| 8-R (8) | F | L2 | 8-A, 6-B | none | D4-R3 | Review |
| 9-A (9) | F | L0 (node only) | 4 | `justfile` parity recipe, `tests/parity/run-engine-differential.mjs`, new `tests/parity/run-engine-differential.test.mjs`, `AGENTS.md` Commands row, README parity text | D4-G | D4-G fail-closed tooling controls. Must load `~/.agents/docs/justfiles.md`. No gate execution. |
| 9-R1 (9) | F | L2 | 9-A | none | D4-G | Review |
| 9-B (9) | C (on W2 selection) | L0 | 3, 4, G0 W2; BUILD (adapter, not in a canonical dir) | both import adapters, new W2 seed, new `tasks/parity-evidence/confirmation-vectors.json` (historical vectors stay immutable) | task 3 W2 plus D4-G | W2 rows and reached controls on the adapter side |
| 9-C (9) | C | L0 | 9-B, 9-R1 (write conflict: driver) | `run-engine-differential.mjs`, `safety-assertions.mjs` | same as 9-B | Per-case W2 assertions and reached controls |
| 9-R2 (9) | C | L2 | 9-C, 6-B | none | same as 9-B | Review |
| 9-D / 9-E / 9-F / 9-G (9) | C, cycle 2 | L0, each plus an L2 `-R` review | chain: 9-R2 → 9-G (optional; also needs 7-R) → 9-D → 9-E → 9-F (write conflict: adapter and driver files) | adapters, driver and assertions, one at a time | task 3 W1/W3/W4; D4-R1 step 5 | 9-F starts by observing the original's partial outcomes |
| 10-D1 (10) | F | L2 | 2 | `decisions/raw-repair-recipes.md`, or native return | brief B and task 2 | P1/P2 recipes. Must state whether P1 touches `session.hpp`. |
| 10-D2 (10) | F | L2 | 2 | `decisions/raw-dispositions.md`, or native return | brief B and task 2 | P3 recipe and T2 envelope disposition, or a human-required question |
| 10-T1 / 10-T2 (10) | F (preparation) | L0 read-only | 2 | `assessments/raw-c3-c7-triage.md`; `assessments/raw-c1-c2-clusters.md`, or native return | task 2 index | Every untraced group in scope accounted for. Nothing waived. |
| 10-D3 (10) | C | L2 | 10-T1, 10-T2 | `decisions/raw-followups.md` | refines the triage outputs | T1 call-schedule/adapter disposition and any new packets |
| 10-A (10) | C (on 10-D1) | L0 | 10-D1; holds `tests/test_main.cpp` after 11-A's edit; BUILD | `src/engine/import.hpp`, new `tests/engine/test_import_errors.hpp`, one include line, `docs/images.md` only if documented error shape changes | 10-D1 | Tab and line context; status and no-send behavior unchanged |
| 10-AR (10) | C | L2 | 10-A, 6-B | none | 10-D1 | Review |
| 10-B (10) | C (on 10-D1) | L0 | 10-D1; plus 7-R (write conflict) only if 10-D1 names `session.hpp`; BUILD | `src/reconcile/paragraph.hpp`, `src/reconcile/container.hpp`, `tests/reconcile/test_paragraph.hpp` | 10-D1 | Characterize on both engines before repairing. Unrelated-restyle control fails. |
| 10-BR (10) | C | L2 | 10-B, 10-D1 | none | 10-D1 | Review |
| 10-C (10) | C (on 10-D2) | L0 | 10-D2; holds `tests/test_main.cpp` after 10-A's edit; BUILD | new `tests/engine/test_import_styles.hpp`, one include line | 10-D2 | Stored-style characterization only, no production edits |
| 11-A (11) | F | L0 | 1; holds `tests/test_main.cpp` first; BUILD | new `tests/engine/test_import_structure.hpp`, one include line | task 1 R2 plus SA-06 | Six SA-05 rows and SA-07 sequences, characterized before any change. Diagnostic differences become packets, never weakened expectations. |
| 11-R (11) | F | L2 | 11-A, 6-B | none | task 1 | Assertion-fidelity review |
| 12-A (12) | F | L0 | 1; BUILD | `tests/diff/test_myers.hpp` | task 1 SA-03 | Input-identity assertion; reordered-draw control fails |
| 12-R (12) | F | L2 | 12-A, 6-B | none | task 1 | Review |
| 12-B (12) | C, filler | L0 author, then L1 `12-B-R` | 1 | `assessments/corpus-brief.md`, or native return | brief A residuals | Refined brief only. No implementation authorized. |
| 13-A (13) | F (convergence snapshot) | L0 runtime | 6-B plus the accepted reviews of selected items: 7-R, 8-R, 9-R1, 11-R, 12-R; plus 9-R2 / 10-AR / 10-BR if those items are selected; plus a lead-declared freeze on source and harness writers | canonical build dirs, fresh `tasks/parity-evidence/confirmation-01/` | D4-G and workspace.md commands | Snapshot of the frozen tree's identities. Adapter rebuild, Debug and Release unit tests, MCP discovery, original unit tests, `just parity`. A RED raw result is kept. |
| 13-R (13) | F | L2 | 13-A | `assessments/final-reconciliation.md`, or native return | all of the above | Snapshot reconciliation. Pending and cycle-2 items stay listed as unaccepted. |

The graph has no cycles, and every file has a single owner at any one time.

## 2. Proposed waves (only after the user releases execution)

Three worker slots run alongside coordinator C. If C is released, the next ready item takes the fourth slot.

- **Wave 1:** 7-A, 9-A, 11-A. All are edit-capable executors with disjoint files. The first builder to reach its check step gets the build token.
- **Ready queue, in order:**
  1. Reviews as soon as their inputs exist: 7-R, 8-R, 9-R1, 11-R, 12-R, then 9-R2, 10-AR, 10-BR.
  2. 6-B. It is needed before the first review.
  3. 8-A
  4. 10-D1
  5. 12-A
  6. 10-D2
  7. 9-B, then 9-C
  8. 10-A
  9. 10-T1 and 10-T2
  10. 10-B
  11. 10-D3
  12. 10-C
  13. 6-A, only if build-token waits become material
  14. 12-B
- **Freeze, then 13-A, then 13-R.** Read-only 10-D/10-T work and 12-B may keep running during the freeze.

## 3. Barriers and estimates

**Corrected barrier statement.** No task-level barrier is left except the 13-A writer freeze. Three resource and write-conflict serializations remain:
- the build-dir token or tokens
- one-line `tests/test_main.cpp` handoffs
- the adapter/driver sequence in lane 9

6-A blocks only work assigned to the directories it would verify.

**Estimates:**
- **Critical-path lower bound:** about 2 hours of wall time. That's 7-A (50 min) → 7-R (12) → 13-A (30) → 13-R (15), plus the 6-B prerequisite. It ignores slot contention, build-token waits, repair loops and acceptance latency.
- **Schedule estimate for cycle 1:** about 3 hours, range 2.5–4.
  - About 320 agent-minutes before 13-A, spread across three slots, plus about 45 minutes for the snapshot.
  - This covers R1, R3, G, R2, SA-03, P2 and W2. It excludes P1, P3, the triage follow-ups and cycle 2.
  - It's an estimate, not a deadline.

## 4. Skill recommendation (proposal only)

1. Preflight edit-tool availability for each model and agent-type configuration before writing edit-required contracts. Keep read-only roles on native-return handoffs by default.
2. Document a "check-step build token" pattern for codebases where all tests compile as one unit. Edits proceed in parallel and only builds serialize; no scaffolds or new machinery.

5 status=review rev=3 file=.agents/threads/2026.11.13 20.00 - Node parity gap closure/plans/confirmation/tasks/5.md (decision: this response, for decisions/concurrency-plan.md)
