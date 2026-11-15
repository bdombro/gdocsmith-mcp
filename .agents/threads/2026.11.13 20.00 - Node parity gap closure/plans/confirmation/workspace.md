# Confirmation workspace

## Authority and storage

- Phase: assessment/preparation, not implementation or release acceptance
- Root: current gdocsmith-mcp repository; original ../gdocsmith-node is read-only
- This plan is under .agents/threads/2026.11.13 20.00 - Node parity gap closure/plans/confirmation
- Existing thread workspace.md is historical architecture context, not current gate status
- Lead owns contracts, map, acceptance and subsequent execution permissions
- Preserve dirty inputs. No original, production, test, runner or justfile edits in task 0
- Excluded old messages.md remains inaccessible and was not consulted
- All paths below are repository-relative unless marked original; thread artifacts are linked relatively

## Architecture reused, not redesigned

AGENTS.md defines model → lens → reconcile → engine orchestration, with API transport isolated in src/api.hpp and artifact storage in src/engine/artifacts.hpp. src/main.cpp is the entrypoint; tests/test_main.cpp aggregates header suites. Reverse-index writes, revision locking, no-clobber recovery, sharing guards and honest unknown-write handling remain constraints.

The existing thread workspace is adequate for these boundaries but contains superseded capability and runtime snapshots. Its old missing-helper statements must not be copied as current defects. Assessment should trace named source obligations to current target behavior, not infer capability from helper names or accepted historical narratives.

Proposal: three independent evidence assessments, specified in briefs.md. No architectural redesign, assertion retirement or new normalizer is established here.

## Latest historical qualification

Primary receipt: [final summary](../../tasks/parity-evidence/resumed-final-recheck/final-summary.json), recorded 2026-11-13T23:00:29Z, with [handoff](../../tasks/parity-evidence/resumed-final-recheck/handoff.md).

1. Debug and Release: 398 cases / 6521 assertions each, zero failures/skips
2. Ten shared vectors plus known-upload-error variant: 151 original / 172 target assertions, 70/70 controls; six backend controls and 64 observation-corruption controls
3. Raw differential exit 1: 250 leaves / 206 exact groups remain, not blanket equivalence
4. Original 846-case baseline retained by that receipt, not all-846 assertion transfer
5. App/adapter builds, their tidy and isolated MCP were retained gates, not fresh reruns in that receipt

These are historical executions. Task 0 ran no builds, tests, network or live workflows.

## Current identity spot-check

Read-only shasum and git rev-parse checks matched:

| Input | Current identity |
|---|---|
| Target HEAD | 20ed42e6a67341d71a13f404f149aee34b4c30bc |
| Original HEAD | d00069dd188ca995bfda9828cc9eee5016603b63 |
| build/bin/gdocsmith-mcp | d5380255433a528b15457e4ebe150e1182af42af6b8ad42952e93ba63be4fa0c |
| build/cmake/ParityChecks/gdocsmith_parity_adapter | cb10ff614af627265cde78f00dc65370cb23b5b60d76e729c744e86a8ea282fc |
| src/main.cpp | f453bede48ea44707ce8b35dbc114c99c630f9d4e1f119aec050d970671ae820 |
| tests/test_main.cpp | 9f90997fa990e547a536e20044ec0919b8967c3febe0e7894cb76d5e21127e2c |
| tests/parity/run-engine-differential.mjs | 72765c0498e9e30e22fc005e4569fbe2458c1595ac19475d4dd319ab28b6b40f |
| CMakeLists.txt | 2a901a2c3045a76f3e45593d0fc33ca921a5fd2406ee54c43a9ef7235f567d5e |

The four selected text inputs match [final input manifest](../../tasks/parity-evidence/resumed-final-recheck/inputs-final-after-gates.json); binaries match final-summary.json. This is not a full 311-entry revalidation or dirty-source freeze. Receipt freshness beyond those checks is unverified.

## Actual configured runners and limits

- Original justfile test recipe: node --test "src/**/*.test.ts" "scripts/**/*.test.ts"; earlier inventory reports 61 files. Original package.json has no test script. Original test-live and test-mcp are separate integration recipes, excluded here
- Target CMakeLists.txt registers one aggregate gdocsmith_mcp_test through tests/test_main.cpp, timeout 120 seconds, source working directory and build-local TMPDIR
- Existing exact target command: ctest --test-dir build/cmake/GapChecks --output-on-failure --no-tests=error; Release substitutes GapRelease. These are future execution candidates, not task-0 executions
- tests/parity/run-engine-differential.mjs accepts original-root, target-adapter, vectors, case-ids, owned-run-root and results. Its existing hardening checks require the ParityChecks adapter. Output paths must be relative, private, under tasks/parity-evidence; no arbitrary plan-local output substitution
- The driver uses real original documentFileParse and engine adapters, plus safety-assertions.mjs. Exit 1 includes remaining raw differences; it must not be relabeled success from named assertions alone
- Exact historical differential arguments are in final-summary.json freshGates[0]. Do not reuse its output destination for a new run or execute it during assessment
- Portable integration remains outside authorized execution; no install, services, live calls or canonical build concurrency

## Coverage limits and open questions

[obligations.md](../parity/obligations.md) maps 43 initial groups with partial transferred subassertions. [baseline-transfer.md](../parity/baseline-transfer.md) separately covers foundation residuals, including BF-API-3's injected-helper 404 distinction, random trial preservation and conditional corpus execution. Neither supplies a whole-source denominator.

Raw evidence is [differential-results-raw-diffs.json](../../tasks/parity-evidence/resumed-final-recheck/differential-results-raw-diffs.json), schema engine-parity-all-raw-differences-v1. Only V003/V004 recovery directory layout has recorded parent equivalence authority; payload, state, ordering, native bytes and other diagnostics are not waived.

Unknowns: remaining meaningful assertion families; operationally consequential raw leaves; broader native interchange; workflow and non-Apple qualification gaps; a repeatable gate that preserves raw failures. Each assessment must distinguish missing test transfer from demonstrated implementation defect and send consequential decisions to lead.

## Explicit instruction loading

Task 0 explicitly loaded repository AGENTS.md; swarm roles/common.md and roles/brief-author.md; confirmation rules.md and task 0 contract rev 1; app-rewrite skill and references/prepare-transfer.md and verify.md. Host model/effort defaults retained; automatic instruction inheritance unverified. No whole swarm reference, excluded inbox or nested agents used.
