# Node replacement confidence

## Overview

Close evidence-backed parity gaps against the read-only Node reference. Reuse existing adapters and receipts rather than treating passing target tests as replacement equivalence.

Workers follow [rules.md](rules.md).

## Goal

Give each consequential source obligation and raw difference a supported disposition, repair demonstrated gaps, and expose repeatable parity checks.

- Before: bounded historical checks pass, but source-assertion transfer and 206 raw difference groups are incomplete
- After: assessed coverage and exact remaining limits, independently reviewed repairs, and fresh reproducible validation

## Roles

The lead owns contracts, dispatch, permissions, the map and acceptance; the lead chat model is unchanged. Earlier confirmation launches used user-confirmed claude-opus-5.5 (low/medium/high); subsequent launches use updated user-confirmed defaults: gemini-4-hydrogen low for level-0 (workers, brief authors, coordinator), gemini-4-hydrogen medium for level-1 (refiners, reviewers), and gemini-4-hydrogen high for level-2 (deciders). Generated role rows include historical workers whose actual model/effort remains unknown; do not relabel them

<!-- roles:begin -->
- **brief-author** (`unavailable.unavailable`, `claude-opus-5.5.low`): researches a task and writes its brief or spike answer
- **refiner** (`unavailable.unavailable`, `claude-opus-5.5.medium`): independently checks and corrects a brief
- **decider** (`unavailable.unavailable`, `claude-opus-5.5.high`): makes a bounded technical decision, or refers it to the user
- **executor** (`claude-opus-5.5.low`): implements a contract and runs its checks
- **reviewer** (`claude-opus-5.5.high`): independently reviews results at the acceptance gate
- **executor2** (`gemini-4-hydrogen.low`, `gemini-4-hydrogen.medium`)
- **reviewer2** (`gemini-4-hydrogen.medium`, `claude-opus-5.5.medium`)
- **brief-author2** (`gemini-4-hydrogen.low`)
- **decider2** (`gemini-4-hydrogen.high`)
- **refiner2** (`gemini-4-hydrogen.medium`)
- **coordinator** (`unavailable.unavailable`, `claude-opus-5.5.low`): triages returned work and proposes next steps to the lead
<!-- roles:end -->

## Status

Concurrency replanning accepted; user authorized resumption after host model switch restored general-purpose edit/create tools. Old workers remain retired/preserved with saved handoffs. Lead-written scopes are not validated briefs

- Right now: **Cycle 1 complete and accepted** (`6-B`, `7`, `8`, `9-A5`/`9-R1`, `10-A/AR`, `10-B/BR`, `10-C/CR`, `10-D1/D2/D3`, `11`, `12`, `13`, `14` all accepted). No agents or background listeners active. Frozen `HEAD` `20ed42e6…` (`just test` / `just build`: 436/436 cases, 6973/6976 assertions with 3 `may_fail()`, `just lint` & `just test-mcp` pass; `confirmation-03` differential: 0 assertion failures, 70/70 controls, 0 leftover dirs, 238 raw leaf diffs across 194 groups preserved as raw RED)
- Next: Cycle-2 / optional follow-up selection by user (`9-B..9-F`, `11-A` 3 target REDs, `render.hpp:126`, `12-B` corpus execution, `Q-L1..Q-L3` product choices, `10-D3` packets)
- Estimates and actuals: initial 90-minute/16-launch guesses are superseded by D5's roughly 2.5-4 hour cycle-1 planning estimate and roughly two-hour critical-path lower bound. Elapsed time and launches accumulate; no automatic stop or parity closure implied
- Hard limits: no overall time or launch cap; maximum four concurrent executing agents/listeners remains a resource limit

### Subagents

<!-- agents:begin -->
Agents: 🔄 running · ✅ returned · 🛑 blocked · ⏸️ idle or released · 🚫 dropped · ❓ undecided

1. ✅ [0](tasks/0.md) · 17:38 · 2m · brief-author unavailable.unavailable — About to accept preserved bootstrap and release independent assessment contracts after symlink bus send/read validation
2. ⏸️ [C](tasks/C.md) · 17:45 · 12m · coordinator unavailable.unavailable — about to record actual registry checkpoint and resume assigned listener
3. ✅ [1](tasks/1.md) · 17:46 · 3m · refiner unavailable.unavailable — about to send completed task review pointer and return idle without follow-up commands
4. ✅ [2](tasks/2.md) · 17:46 · 7m · refiner unavailable.unavailable — about to verify persisted review contract and hand off assessment pointer, then return idle
5. ✅ [3](tasks/3.md) · 17:46 · 3m · refiner unavailable.unavailable — about to verify owned assessment references and contract revision, finalize task review fields and send artifact pointer
6. ✅ [4](tasks/4.md) · 17:51 · 2m · decider unavailable.unavailable — about to verify persisted owned artifact and unchanged contract revision before handoff
7. ✅ [5](tasks/5.md) · 18:05 · 12m · decider claude-opus-5.5.high — about to recheck contract revision and send rev 3 review pointer; corrected graph returned natively
8. ✅ [14-A](tasks/14-A.md) · 18:27 · 1m · brief-author claude-opus-5.5.low — brief drafted; sending native-handoff pointer
9. ✅ [14-B](tasks/14-B.md) · 18:27 · 1m · brief-author claude-opus-5.5.low — send native-handoff pointer
10. ✅ [14-C](tasks/14-C.md) · 18:27 · 2m · brief-author claude-opus-5.5.low — handoff: native Brief for 9-A ready for review
11. ⏸️ [C2](tasks/C2.md) · 18:27 · 17m · coordinator claude-opus-5.5.low — about to send repair triage pointer and return
12. ✅ [14-AR](tasks/14-AR.md) · 18:32 · 24m · refiner claude-opus-5.5.medium — re-checked modelMap; returning clarified complete brief and pointer
13. ✅ [14-BR](tasks/14-BR.md) · 18:32 · 23m · refiner claude-opus-5.5.medium — clarification complete: AGENTS.md and R3 explicitly loaded, lint commands corrected, G2 assigned to 6-B; sending pointer
14. ✅ [14-CR](tasks/14-CR.md) · 18:32 · 6m · refiner claude-opus-5.5.medium — compose corrected brief; then send native-handoff pointer to coord-v2
15. ✅ [14-D](tasks/14-D.md) · 18:34 · 3m · brief-author claude-opus-5.5.low — composing native 11-A brief handoff; then sending pointer
16. ✅ [14-E](tasks/14-E.md) · 18:39 · 1m · brief-author claude-opus-5.5.low — send native-handoff pointer to lead
17. ✅ [14-G](tasks/14-G.md) · 18:39 · 1m · refiner claude-opus-5.5.medium — about to send review pointer to lead; triage returned as native handoff for verbatim save
18. 🚫 [9-A](tasks/9-A.md) · 18:42 · 0m · executor — Actual preferred-model grunter has no host file-edit tools; source execution blocked, no edits/tests or silent substitution. Accepted Brief…
19. 🚫 [9-A2](tasks/9-A2.md) · 18:52 · 0m · executor — BLOCKED: host file-edit tools absent (no edit/create/apply_patch in inventory); stopped before any source writes per missing-tool rule
20. 🚫 [9-A3](tasks/9-A3.md) · 18:54 · 0m · executor — Updated Copilot missing-tool recovery; retired old idle worker and tried two fresh same-profile Opus5.5/low replacements. All three actual …
21. ✅ [14-ER](tasks/14-ER.md) · 18:56 · 4m · refiner claude-opus-5.5.medium — AGENTS.md loaded; composing native read-only refined brief and result for lead verbatim persistence
22. 🚫 [9-A4](tasks/9-A4.md) · 19:07 · 0m · executor — capability check: no host file-edit tool; returning native blocker
23. ✅ [10-T1](tasks/10-T1.md) · 19:07 · 2m · brief-author claude-opus-5.5.low — compose native triage handoff and send pointer
24. ✅ [10-T2](tasks/10-T2.md) · 19:07 · 2m · brief-author claude-opus-5.5.low — compose native handoff for raw-c1-c2-clusters and return
25. ✅ [9-A5](tasks/9-A5.md) · 19:23 · 3m · executor — ready for review: node --check ok, node --test 30/30, just list/dry-run ok; no parity run/build/network
26. ✅ [10-D1](tasks/10-D1.md) · 19:24 · 2h 3m · decider claude-opus-5.5.high — about to send pointer for P1 rev-3 addendum (native return); seam keeps rejection-emptied paragraph in unpaired deletion branch
27. ✅ [10-D2](tasks/10-D2.md) · 19:24 · 3m · decider claude-opus-5.5.high — Decision composed (read-only native return); rechecked contract_rev 1; about to send review pointer to coord-v2
28. ✅ [8-A](tasks/8-A.md) · 19:28 · 3m · executor — about to clang-tidy main and tests
29. ✅ [9-R1](tasks/9-R1.md) · 19:28 · 3m · reviewer — review complete: approve; native handoff returned to lead (read-only, no task-file write)
30. ✅ [14-DR](tasks/14-DR.md) · 19:30 · 4m · refiner claude-opus-5.5.medium — about to send escalation pointer: known-RED landing policy for 11-A (native return carries detail)
31. 🚫 [7-A](tasks/7-A.md) · 19:32 · 3m · executor — about to build GapChecks test target for pre-repair characterization
32. ✅ [10-D3](tasks/10-D3.md) · 19:32 · 5m · decider claude-opus-5.5.high — about to hash-check receipt R and project group paths/cases only (no values)
33. ✅ [14-F](tasks/14-F.md) · 19:36 · 2m · brief-author claude-opus-5.5.low — composing rev2 brief handoff (read-only, no runs of original tests)
34. ✅ [7-A2](tasks/7-A2.md) · 20:09 · 11m · executor — controls reverted; about to run clang-tidy
35. ✅ [14-FR](tasks/14-FR.md) · 20:09 · 4m · refiner claude-opus-5.5.medium — Verified env -C cwd mechanism; composing corrected brief
36. ✅ [6-B](tasks/6-B.md) · 20:13 · 4m · executor — no log rows yet
37. ✅ [8-R](tasks/8-R.md) · 20:17 · 6m · reviewer — verdict approve returned as native handoff to lead; no issues routed
38. ✅ [7-R](tasks/7-R.md) · 20:21 · 8m · reviewer — about to return native read-only handoff (approve with recorded limits) and notify coord-v2
39. ✅ [11-A](tasks/11-A.md) · 20:24 · 11h 58m so far · executor — no log rows yet
40. ✅ [12-A](tasks/12-A.md) · 20:24 · 4m · executor — about to run clang-tidy on main.cpp and test_main.cpp
41. ✅ [12-R](tasks/12-R.md) · 20:29 · 3m · reviewer — review complete: approve via native handoff; focused doctest 4/4 3036 assertions, clang-tidy main+test_main exit 0, no src/diff changes
42. ✅ [14-J](tasks/14-J.md) · 20:38 · 2m · brief-author claude-opus-5.5.low — about to return native 10-C brief handoff; flagged E1 vs 10-D2 s1.6 conflict
43. ✅ [11-R](tasks/11-R.md) · 20:41 · 11h 41m so far · reviewer — focused run reproduced 5/5, 87/90, exit 0 with absolute TMPDIR; relative TMPDIR per brief yields path refusals; section RED root cause trac…
44. ✅ [14-JR](tasks/14-JR.md) · 20:41 · 3m · refiner claude-opus-5.5.medium — about to return native refined brief for 10-C (sha 64b6bf6d...2def verified); result=changes made; corrections: adapter line cites, seed sh…
45. ✅ [10-C](tasks/10-C.md) · 20:44 · 7m · executor — phase 2 done; releasing test_main.cpp lease and GapChecks token
46. ✅ [10-CR](tasks/10-CR.md) · 20:52 · 3m · reviewer — about to return native handoff: verdict approve; focused 2/2 cases 31/31 assertions, clang-tidy test_main.cpp exit 0 no warnings; Q-L1 open
47. ✅ [14-HR](tasks/14-HR.md) · 20:59 · 6m · refiner claude-opus-5.5.medium — about to return native handoff: changes made on 10-A.author.md rev2 (sha c4d41bdc verified)
48. ✅ [10-A](tasks/10-A.md) · 21:07 · 11m · executor — ready for review: focused 7/7 (104 asserts), regressions 2/2, full 430/432 (2 known 10-B RED), tidy clean, docs grep none; GapChecks token …
49. ✅ [10-AR](tasks/10-AR.md) · 21:19 · 6m · reviewer — done: native handoff returned (approve code, evidence conditions)
50. ✅ [14-I](tasks/14-I.md) · 21:29 · 2m · brief-author claude-opus-5.5.low — rev3: native brief returned to lead (ready for 14-IR rev3); open Q: C-P1-g helper placement vs writeSegment-only session diff
51. ✅ [14-IR](tasks/14-IR.md) · 21:32 · 4m · refiner claude-opus-5.5.medium — refinement complete: changes made (line cites, Q1 resolved per lead, C-P1-d/e/f/g test shapes, commands aligned to 10-A); returning brief n…
52. ✅ [12-B](tasks/12-B.md) · 22:06 · 1m · brief-author2 — Completed read-only 12-B corpus-execution brief authoring; returning native Brief for 12-B-R.
53. ✅ [12-B-R](tasks/12-B-R.md) · 22:08 · 1m · refiner2 — completed independent verification of assessments/corpus-brief.md (SHA-256 e7c7c716...bbb0); returning changes-made native handoff
54. ✅ [10-B](tasks/10-B.md) · 22:09 · 2m · executor2 gemini-4-hydrogen.low — done: all green (436 cases), tidy exit 0 x2; GapChecks token released
55. ✅ [10-BR](tasks/10-BR.md) · 22:12 · 3m · reviewer2 claude-opus-5.5.medium — checks done: focused 6/6 105/105, full 436/436 (3 may_fail asserts in import_structure), tidy both exit 0; verdict approve
56. ✅ [13-A](tasks/13-A.md) · 22:16 · 6m · executor2 gemini-4-hydrogen.low — 13-A contract rev 2 complete. just test/build/lint/test-mcp passed; just parity confirmation-03 exited 1 (10 cases, 11 differingCases, 70/7…
57. ✅ [13-R](tasks/13-R.md) · 22:23 · 5m · reviewer2 gemini-4-hydrogen.medium — Verification complete; preparing final-reconciliation.md native Markdown output and 13-A/13-R verdict
58. ✅ [10-T3](tasks/10-T3.md) · 22:33 · 1m · brief-author2 — completed D-K4 and D-K5 investigation; returning native markdown report
59. ✅ [10-T4](tasks/10-T4.md) · 22:33 · 1m · brief-author2 — no log rows yet
60. ✅ [14-K](tasks/14-K.md) · 22:33 · 0m · brief-author2 — about to return complete 9-B Brief (14-K rev 1)
61. ✅ [14-KR](tasks/14-KR.md) · 22:35 · 3m · refiner2 — verified 9-B.author.md against cpp-import-adapter.cpp, node-import-adapter.mjs, two-tab-text-v1.json, src/engine/import.hpp:1418-1434, work…
62. ✅ [14-L](tasks/14-L.md) · 22:35 · 1m · brief-author2 — completed inspection of 5 export/lens parity items and C++ tests; returning native Brief
63. ✅ [14-M](tasks/14-M.md) · 22:35 · 1m · brief-author2 — about to return completed 9-C Brief for 14-M rev 1
64. ✅ [11-D1](tasks/11-D1.md) · 22:40 · 10m · decider2 — completed verification of all 5 items and affected tests; returning native decision for export-lens-repairs.md
65. ✅ [14-MR](tasks/14-MR.md) · 22:40 · 2m · refiner2 — verified 9-C.author.md against run-engine-differential.mjs, run-engine-differential.test.mjs, safety-assertions.mjs, and 9-B.md; preparing …
66. ✅ [9-B](tasks/9-B.md) · 22:42 · 2m · executor2 gemini-4-hydrogen.low — no log rows yet
67. ✅ [9-C](tasks/9-C.md) · 22:46 · 13m · executor2 gemini-4-hydrogen.low — no log rows yet
68. ✅ [9-R2](tasks/9-R2.md) · 22:48 · 14m · reviewer2 gemini-4-hydrogen.medium — completed 9-R2 rev 1 verification: approve (30/30 unit tests, 21/21 controls, 0 assertion failures, 0 remaining run dirs, honest raw diffs)
69. ✅ [14-LR](tasks/14-LR.md) · 22:51 · 2m · refiner2 — verified 11-D1 and 11-B.author.md SHA-256s, inspected C++ sources/tests/CMakeLists/justfile, prepared corrected 11-B brief for native hando…
70. ✅ [11-B](tasks/11-B.md) · 22:55 · 11m · executor2 gemini-4-hydrogen.low — no log rows yet
71. ✅ [14-N](tasks/14-N.md) · 23:04 · 1m · brief-author2 — no log rows yet
72. ✅ [11-R2](tasks/11-R2.md) · 23:08 · 2m · reviewer2 gemini-4-hydrogen.medium — Completed 11-R2 rev 1 review of 11-B: approve (437/437 full, 5/5 structure with 2 remaining may_fail, 3/3 markdownRender/markdownTextEscape…
73. ✅ [14-NR](tasks/14-NR.md) · 23:08 · 2m · refiner2 — verified 9-D.author.md (sha256 c9d1da1b1e293bbdb2f9d12330a3ab41e69a0f2da0a92244056198982163bbea) against Node/C++ sources, parity adapters,…
74. ✅ [9-D](tasks/9-D.md) · 23:12 · 17m · executor2 gemini-4-hydrogen.low — Fixed 9-DR findings in run-engine-differential.mjs:532 and safety-assertions.mjs:163,262; run-engine-differential.test.mjs 30/30 passed; fu…
75. ✅ [14-O](tasks/14-O.md) · 23:12 · 1m · brief-author2 — no log rows yet
76. ✅ [14-OR](tasks/14-OR.md) · 23:14 · 2m · refiner2 — verified 9-E.author.md (sha256 33be06df3746c50ec1a46e7f6c47f546c5e8dac2f1feaca90632551ef237a572) against node/cpp sources and parity harnes…
77. ✅ [14-P](tasks/14-P.md) · 23:14 · 1m · brief-author2 — Drafted 9-F W4 brief; returning native handoff to lead/coord-v2
78. ✅ [14-PR](tasks/14-PR.md) · 23:20 · 5m · refiner2 — completed independent verification of 9-F.author.md; preparing corrected Brief (changes made) for native handoff
79. ✅ [9-DR](tasks/9-DR.md) · 23:29 · 3m · reviewer2 gemini-4-hydrogen.medium — completed 9-DR rev 2 verification: 30/30 unit tests, fresh 9dr-w1-w2-verify-2 matches 9d-w1-verify-4 (0 orig fails, 9 cpp fails from doc:nu…
80. ✅ [9-E](tasks/9-E.md) · 23:33 · 22m · executor2 gemini-4-hydrogen.low — starting 9-E W3 nested-tab lifecycle & refusal journey
81. ✅ [9-ER](tasks/9-ER.md) · 23:55 · 4m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
82. ✅ [9-F](tasks/9-F.md) · 00:00 · 15m · executor2 gemini-4-hydrogen.low — about to inspect owned parity files and existing seeds
83. ✅ [9-FR](tasks/9-FR.md) · 00:15 · 5m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
84. ✅ [15-D](tasks/15-D.md) · 00:25 · 1m · brief-author2 — no log rows yet
85. ✅ [15-DR](tasks/15-DR.md) · 00:26 · 4m · refiner2 — no log rows yet
86. ✅ [15-A](tasks/15-A.md) · 00:30 · 14m · executor2 gemini-4-hydrogen.medium — about to run clang-tidy and ParityChecks
87. ✅ [15-R](tasks/15-R.md) · 00:45 · 4m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
88. ✅ [16-A](tasks/16-A.md) · 00:50 · 4m · executor2 gemini-4-hydrogen.medium — no log rows yet
89. ✅ [16-R](tasks/16-R.md) · 00:54 · 4m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
90. ✅ [17-A](tasks/17-A.md) · 01:05 · 1m · executor2 gemini-4-hydrogen.medium — about to run confirmation-vectors differential run c2-final-vectors-02
91. ✅ [18-A](tasks/18-A.md) · 01:05 · 5m · executor2 gemini-4-hydrogen.medium — no log rows yet
92. ✅ [17-R](tasks/17-R.md) · 01:06 · 1m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
93. ✅ [18-R](tasks/18-R.md) · 01:10 · 2m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
94. ✅ [19-DA](tasks/19-DA.md) · 08:00 · 2m · brief-author2 — no log rows yet
95. ✅ [19-DB](tasks/19-DB.md) · 08:00 · 2m · brief-author2 — no log rows yet
96. ✅ [19-DC](tasks/19-DC.md) · 08:00 · 2m · brief-author2 — no log rows yet
97. ✅ [19-DRA](tasks/19-DRA.md) · 08:03 · 2m · refiner2 — no log rows yet
98. ✅ [19-DRB](tasks/19-DRB.md) · 08:03 · 2m · refiner2 — no log rows yet
99. ✅ [19-DRC](tasks/19-DRC.md) · 08:03 · 2m · refiner2 — no log rows yet
100. ✅ [19-A](tasks/19-A.md) · 08:06 · 6m · executor2 gemini-4-hydrogen.medium — no log rows yet
101. ✅ [19-B](tasks/19-B.md) · 08:06 · 15m · executor2 gemini-4-hydrogen.medium — no log rows yet
102. ✅ [19-RA](tasks/19-RA.md) · 08:12 · 2m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
103. ✅ [19-RB](tasks/19-RB.md) · 08:21 · 2m · reviewer2 gemini-4-hydrogen.medium — no log rows yet
104. 🔄 [19-C](tasks/19-C.md) · 08:23 · 0m so far · executor2 gemini-4-hydrogen.medium — about to build and run GapChecks gdocsmith_mcp_test
<!-- agents:end -->

## Tasks

Preparation precedes independently refined repair contracts. Parallel assessment has disjoint outputs; canonical build validation is serialized.

The [accepted graph decision](decisions/concurrency-plan.md) splits eight deliverables into owned subtasks. Only check-step build tokens, narrow shared-file handoffs and the final writer freeze serialize work. Task 14 makes author/refiner gates explicit. Historical model/effort stays unknown; planned task rows show selected launch settings, not evidence that an agent has run

<!-- tasks:begin -->
Progress: ⬜ ready/queued · 🛑 blocked · 🔄 running/review/changes · ✅ accepted · ⏸️ paused · 🚫 dropped · ❓ undecided

- ✅ [0. Refresh current parity evidence and prepare assessment briefs](tasks/0.md): Prepared three independent bounded assessment briefs; selected identities match historical receipt, full freshness and parity remain unverified
- ✅ [1. Refine source-obligation transfer assessment](tasks/1.md) `[0]`: Eight-family static supplement and three follow-up briefs; collection reconciled, runtime unverified
- ✅ [2. Refine complete raw-difference accounting](tasks/2.md) `[0]`: Accepted static accounting: 206 groups and 250 occurrences verified; six traces and three proposals, no semantic waiver or fresh runtime proof
- ✅ [3. Refine uncovered workflows and repeatable parity gate](tasks/3.md) `[0]`: Refined four shared-workflow gaps and a no-install fail-closed parity gate specification; static assessment only
- ✅ [4. Decide bounded parity repair recipes](tasks/4.md) `[1,3]`: Accepted bounded static recipes for checked review carry, raw helpers/checked clients and existing-driver gate reuse; execution remains unverified
- ✅ [5. Restructure parity tasks for maximum useful concurrency](tasks/5.md): Accepted corrected concurrency graph only; level-0 briefs and separate level-1 refinement still required before implementation
- ✅ [6. Establish source evidence and optional build capacity](tasks/6.md): Original-side characterization (6-B) accepted; optional extra build directory (6-A) dropped as unneeded
  - 🚫 [6-A. Verify optional extra test-build capacity](tasks/6-A.md)
  - ✅ [6-B. Execute contained original-side characterization rows](tasks/6-B.md) `[14-FR]`
- ✅ [7. Preserve review spans during supported edits](tasks/7.md): R1 review-carry repair (7-A2) and independent review (7-R) accepted
  - 🚫 [7-A. Characterize and repair faithful review carry](tasks/7-A.md) `[4,14-AR]`
  - ✅ [7-A2. Characterize and repair faithful review carry](tasks/7-A2.md) `[4,14-AR]`
  - ✅ [7-R. Review review-carry fidelity and refusal controls](tasks/7-R.md) `[7-A2,6-B]`
- ✅ [8. Separate raw HTTP responses from checked clients](tasks/8.md): R3 HTTP raw/checked split (8-A) and independent review with 6-B G2 404 evidence (8-R) accepted
  - ✅ [8-A. Return raw HTTP errors and preserve checked-client failures](tasks/8-A.md) `[4,14-BR]`
  - ✅ [8-R. Review raw-helper and public-client boundaries](tasks/8-R.md) `[8-A,6-B]`
- ✅ [9. Harden the parity gate and transfer shared workflows](tasks/9.md): Parity gate (9-A5/9-R1) and Cycle-2 shared workflows W2 (9-B/9-C/9-R2), W1 (9-D/9-DR), W3 (9-E/9-ER), and W4 (9-F/9-FR) all accepted; 9-A..9-A4 & 9-G/9-GR dropped
  - 🚫 [9-A. Harden existing parity driver and expose a direct recipe](tasks/9-A.md) `[4,14-CR]`
  - 🚫 [9-A2. Retry parity-tooling execution with a fresh Copilot worker](tasks/9-A2.md) `[4,14-CR]`
  - 🚫 [9-A3. Retry intermittent Copilot tool loading for parity execution](tasks/9-A3.md) `[4,14-CR]`
  - 🚫 [9-A4. Compare a compatible full-capability executor profile](tasks/9-A4.md) `[4,14-CR]`
  - ✅ [9-A5. Harden existing parity driver and expose a direct recipe](tasks/9-A5.md) `[4,14-CR]`
  - ✅ [9-B. Add partial-rename journey at the adapter boundary](tasks/9-B.md) `[3,4,14-KR]`
  - ✅ [9-C. Add W2 driver assertions and reached controls](tasks/9-C.md) `[9-B,9-R1,14-MR]`
  - ✅ [9-D. Transfer create, rollback and explicit-retry journey](tasks/9-D.md) `[9-R2,14-NR]`
  - ✅ [9-DR. Review create, rollback and retry fidelity](tasks/9-DR.md) `[9-D]`
  - ✅ [9-E. Transfer nested-tab lifecycle and refusal journey](tasks/9-E.md) `[9-DR]`
  - ✅ [9-ER. Review nested-tab journey fidelity](tasks/9-ER.md) `[9-E]`
  - ✅ [9-F. Characterize and transfer authorized comment partial outcomes](tasks/9-F.md) `[9-ER]`
  - ✅ [9-FR. Review authorized comment journey and partial outcomes](tasks/9-FR.md) `[9-F]`
  - 🚫 [9-G. Transfer review-carry cases into shared adapters](tasks/9-G.md) `[7-R,9-R2]`
  - 🚫 [9-GR. Review the shared review-carry journey](tasks/9-GR.md) `[9-G]`
  - ✅ [9-R1. Review fail-closed gate behavior and direct recipe](tasks/9-R1.md) `[9-A5]`
  - ✅ [9-R2. Review the partial-rename shared journey](tasks/9-R2.md) `[9-C,6-B]`
- ✅ [10. Disposition raw differences and repair demonstrated defects](tasks/10.md): Independent decision/triage branches feed narrowly owned diagnostics, restyle and style-characterization tasks
  - ✅ [10-A. Preserve image-refusal line and tab context](tasks/10-A.md) `[10-D1,7-R,14-HR]`
  - ✅ [10-AR. Review image diagnostic fidelity](tasks/10-AR.md) `[10-A,6-B]`
  - ✅ [10-B. Characterize and prevent unrelated review-rejection restyles](tasks/10-B.md) `[10-D1,7-R,14-IR,10-A]`
  - ✅ [10-BR. Review review-rejection write and state fidelity](tasks/10-BR.md) `[10-B,10-D1]`
  - ✅ [10-C. Characterize stored font semantics before readback transforms](tasks/10-C.md) `[10-D2,14-JR]`
  - ✅ [10-CR. Review stored-font characterization fidelity](tasks/10-CR.md) `[10-C]`
  - ✅ [10-D1. Decide review-restyle and image-diagnostic repair recipes](tasks/10-D1.md) `[2]`
  - ✅ [10-D2. Decide stored-font characterization and native-envelope questions](tasks/10-D2.md) `[2]`
  - ✅ [10-D3. Refine raw clusters and decide follow-up packets](tasks/10-D3.md) `[10-T1,10-T2]`
  - ✅ [10-T1. Account for untraced observable-contract and wording groups](tasks/10-T1.md) `[2]`
  - ✅ [10-T2. Cluster untraced call-boundary and native-file groups](tasks/10-T2.md) `[2]`
  - ✅ [10-T3. Investigate V008 textExport count and V010 cross-doc read order](tasks/10-T3.md) `[10-D3]`
  - ✅ [10-T4. Project call-schedule alignment and native-file key sets on confirmation-03](tasks/10-T4.md) `[10-D3]`
- ✅ [11. Transfer hierarchy, page, section and deletion-state assertions](tasks/11.md): Cycle-1 (11-A/11-R) and Cycle-2 export/lens parity repairs (11-D1, 11-B, 11-R2) accepted
  - ✅ [11-A. Transfer hierarchy, page, section and deletion-state checks](tasks/11-A.md) `[1,14-DR]`
  - ✅ [11-B. Close 11-A target REDs, TAB_SETTINGS_DEFAULT, and colon escaping](tasks/11-B.md) `[11-D1,14-LR]`
  - ✅ [11-D1. Decide styleBaseline, empty-paragraph rendering, and 11-A RED repairs](tasks/11-D1.md) `[14-L]`
  - ✅ [11-R. Review import assertion-transfer fidelity](tasks/11-R.md) `[11-A,6-B]`
  - ✅ [11-R2. Review 11-B export/lens parity repairs](tasks/11-R2.md) `[11-B]`
- ✅ [12. Preserve generated samples and plan corpus verification](tasks/12.md): Myers sample transfer (12-A/12-R) and SA-04 corpus brief/refinement (12-B/12-B-R) accepted
  - ✅ [12-A. Match original generated Myers sample draw order](tasks/12-A.md) `[1,14-ER]`
  - ✅ [12-B. Author a fail-closed corpus-execution brief](tasks/12-B.md) `[1]`
  - ✅ [12-B-R. Refine corpus evidence and execution recipe](tasks/12-B-R.md) `[12-B]`
  - ✅ [12-R. Review generated sample identity and retained assertions](tasks/12-R.md) `[12-A,6-B]`
- ✅ [13. Qualify and reconcile one frozen convergence snapshot](tasks/13.md): Cycle-1 frozen convergence snapshot (13-A) and final reconciliation (13-R) accepted; 436/436 cases pass, confirmation-03 0 assertion failures / 70/70 controls / 238 raw diffs preserved
  - ✅ [13-A. Run one frozen-tree convergence snapshot](tasks/13-A.md) `[6-B,7-R,8-R,9-R1,10-AR,10-BR,10-CR,11-R,12-R]`
  - ✅ [13-R. Reconcile fresh evidence without overstating closure](tasks/13-R.md) `[13-A]`
- ✅ [14. Draft and independently refine executable briefs](tasks/14.md) `[5]`: All Cycle-1 and Cycle-2 brief gates (14-A..14-PR) authored, independently refined, and accepted
  - ✅ [14-A. Draft the review-carry implementation brief](tasks/14-A.md) `[4,5]`
  - ✅ [14-AR. Independently refine the review-carry brief](tasks/14-AR.md) `[14-A]`
  - ✅ [14-B. Draft the raw-helper and checked-client implementation brief](tasks/14-B.md) `[4,5]`
  - ✅ [14-BR. Independently refine the HTTP boundary brief](tasks/14-BR.md) `[14-B]`
  - ✅ [14-C. Draft the direct parity-gate tooling brief](tasks/14-C.md) `[4,5]`
  - ✅ [14-CR. Independently refine the parity-gate tooling brief](tasks/14-CR.md) `[14-C]`
  - ✅ [14-D. Draft the import assertion-transfer brief](tasks/14-D.md) `[1,5]`
  - ✅ [14-DR. Independently refine the import assertion-transfer brief](tasks/14-DR.md) `[14-D]`
  - ✅ [14-E. Draft the generated-sample transfer brief](tasks/14-E.md) `[1,5]`
  - ✅ [14-ER. Independently refine the generated-sample brief](tasks/14-ER.md) `[14-E]`
  - ✅ [14-F. Draft the contained original-runtime evidence brief](tasks/14-F.md) `[5]`
  - ✅ [14-FR. Independently refine the original-runtime evidence brief](tasks/14-FR.md) `[14-F]`
  - ✅ [14-G. Resolve incomplete coordinator setup and stale readiness triage](tasks/14-G.md) `[1]`
  - ✅ [14-H. Draft the P2 image-diagnostic repair brief](tasks/14-H.md) `[10-D1]`
  - ✅ [14-HR. Independently refine the P2 image-diagnostic brief](tasks/14-HR.md) `[14-H]`
  - ✅ [14-I. Draft the P1 review-restyle repair brief](tasks/14-I.md) `[10-D1]`
  - ✅ [14-IR. Independently refine the P1 review-restyle brief](tasks/14-IR.md) `[14-I]`
  - ✅ [14-J. Draft the P3 stored-font characterization brief](tasks/14-J.md) `[10-D2]`
  - ✅ [14-JR. Independently refine the P3 stored-font brief](tasks/14-JR.md) `[14-J]`
  - ✅ [14-K. Draft the W2 partial-rename adapter brief](tasks/14-K.md) `[3,4]`
  - ✅ [14-KR. Independently refine the W2 partial-rename adapter brief](tasks/14-KR.md) `[14-K]`
  - ✅ [14-L. Draft brief for 11-A target RED gaps and render.hpp diffs](tasks/14-L.md) `[11-R,7-R]`
  - ✅ [14-LR. Independently refine 11-B export/lens parity brief](tasks/14-LR.md) `[11-D1]`
  - ✅ [14-M. Draft the W2 driver assertions brief](tasks/14-M.md) `[9-R1,14-K]`
  - ✅ [14-MR. Independently refine the W2 driver assertions brief](tasks/14-MR.md) `[14-M]`
  - ✅ [14-N. Draft the W1 create, rollback, and retry adapter/driver brief](tasks/14-N.md) `[9-R2]`
  - ✅ [14-NR. Independently refine the W1 create, rollback, and retry brief](tasks/14-NR.md) `[14-N]`
  - ✅ [14-O. Draft the W3 nested-tab lifecycle and refusal brief](tasks/14-O.md) `[9-R2]`
  - ✅ [14-OR. Independently refine the W3 nested-tab lifecycle brief](tasks/14-OR.md) `[14-O]`
  - ✅ [14-P. Draft the W4 authorized comment partial-outcomes brief](tasks/14-P.md) `[9-R2]`
  - ✅ [14-PR. Independently refine the W4 authorized comment brief](tasks/14-PR.md) `[14-P]`
- ✅ [15. Repair C++ engine parity bugs uncovered in Cycle 2](tasks/15.md) `[9,11]`: Accepted; W1 doc:null ::section, W4 baseValidate newline, both 11-A may_fails (namedStyleSet/pageSize & no section M), and 10-T3 hiddenChipTabs all fixed and verified (439/439, 82/82 controls)
  - ✅ [15-A. Fix doc:null ::section, baseValidate newline, 11-A REDs, and 10-T3](tasks/15-A.md) `[15-DR]`
  - ✅ [15-D. Draft brief for Cycle-2 C++ engine parity repairs](tasks/15-D.md) `[9-FR]`
  - ✅ [15-DR. Independently refine Cycle-2 C++ repair brief](tasks/15-DR.md) `[15-D]`
  - ✅ [15-R. Review Cycle-2 C++ engine parity repairs](tasks/15-R.md) `[15-A]`
- ✅ [16. Qualify and reconcile Cycle-2 frozen convergence snapshot](tasks/16.md) `[15-R]`: Accepted; Cycle-2 frozen snapshot (439/439, clang-tidy 0, test-mcp pass, 16-case W1-W4 0/0 & 82/82; c2-final-01 cpp 0 failures, node V009 adapter bug diagnosed)
  - ✅ [16-A. Run Cycle-2 frozen-tree convergence snapshot](tasks/16-A.md) `[15-R]`
  - ✅ [16-R. Reconcile Cycle-2 frozen snapshot without overstating closure](tasks/16-R.md) `[16-A]`
- ✅ [17. Fix V009 node-import-adapter regression and re-verify parity suites](tasks/17.md) `[16]`: Accepted; fixed V009 node-import-adapter.mjs createDocument/imagesUpload guard (c2-final-02 0/0 & 70/70, c2-final-vectors-02 0/0 & 82/82)
  - ✅ [17-A. Fix node-import-adapter createDocument guard for imagesUpload](tasks/17-A.md) `[16-R]`
  - ✅ [17-R. Review V009 node-import-adapter fix and parity receipts](tasks/17-R.md) `[17-A]`
- ✅ [18. Implement fail-closed synthetic corpus test (12-B)](tasks/18.md) `[12]`: Accepted; fail-closed synthetic corpus test in tests/model/test_corpus.hpp (293 docs, 337 tabs, 444/444 gdocsmith_mcp_test, clang-tidy 0)
  - ✅ [18-A. Add fail-closed synthetic corpus test in tests/model/](tasks/18-A.md) `[12-B-R]`
  - ✅ [18-R. Review fail-closed synthetic corpus test](tasks/18-R.md) `[18-A]`
- 🔄 [19. Close remaining Node behavioral parity gaps (Q-L1, Q-L2, Q-L3, 10-T4, render.hpp:126)](tasks/19.md) `[17,18]`: Brief, refine, implement, and review remaining Node parity gaps: Q-L1 stored-font inheritance, Q-L2 apiUser + 10-T4 V010 pre-fetch, Q-L3 style declarations/native format + render.hpp:126
  - ✅ [19-A. Implement Q-L1 stored-font inheritance parity in lens/project.hpp](tasks/19-A.md) `[19-DRA]`
  - ✅ [19-B. Implement Q-L2 apiUser export and 10-T4 V010 pre-fetch order](tasks/19-B.md) `[19-DRB]`
  - 🔄 [19-C. Implement Q-L3 style declarations/keys and remove render.hpp:126 :style[]{}](tasks/19-C.md) `[19-DRC,19-RA,19-RB]`
  - ✅ [19-DA. Draft brief for Q-L1 stored-font inheritance parity](tasks/19-DA.md) `[10-C]`
  - ✅ [19-DB. Draft brief for Q-L2 apiUser and 10-T4 V010 pre-fetch order](tasks/19-DB.md) `[10-T4]`
  - ✅ [19-DC. Draft brief for Q-L3 style declarations, native format, and render.hpp:126](tasks/19-DC.md) `[11-D1]`
  - ✅ [19-DRA. Independently refine Q-L1 stored-font inheritance brief](tasks/19-DRA.md) `[19-DA]`
  - ✅ [19-DRB. Independently refine Q-L2 apiUser and 10-T4 V010 brief](tasks/19-DRB.md) `[19-DB]`
  - ✅ [19-DRC. Independently refine Q-L3 style declarations and render.hpp:126 brief](tasks/19-DRC.md) `[19-DC]`
  - ✅ [19-RA. Review Q-L1 stored-font inheritance parity](tasks/19-RA.md) `[19-A]`
  - ✅ [19-RB. Review Q-L2 apiUser and 10-T4 V010 pre-fetch order](tasks/19-RB.md) `[19-B]`
  - 🛑 [19-RC. Review Q-L3 and render.hpp:126 parity changes](tasks/19-RC.md) `[19-C]`
<!-- tasks:end -->
