# Rules - Node replacement confidence

This phase reuses the existing source reference, adapters and historical receipts. Earlier plans and Markdown logs remain historical; do not migrate them or access the excluded old inbox.

## Scope and evidence

- Workspace: gdocsmith-mcp; sibling ../gdocsmith-node is read-only reference, not a write scope
- Preserve all existing dirty changes; no blanket staging, reset or cleanup
- Explicitly load AGENTS.md; load ~/.agents/docs/justfiles.md before any justfile edit
- Use app-rewrite assertion-transfer and verification rules, not matching test counts
- Historical 398/6521 suites and 151/172/70 shared checks are not fresh executions
- Do not normalize status, payload, ordering, errors, IDs or state to manufacture parity
- Never write secrets, personal data, confidential source, raw prompts or private reasoning into artifacts
- No access, reconstruction or migration of the previously denied messages.md

## Protocol

- Bus: ../../bus.db from this plan; workspace-relative .agents/threads/2026.11.13 20.00 - Node parity gap closure/bus.db
- Use node /Users/briandombrowski/.agents/skills/swarm-mode/scripts/bus.mjs --db "<bus>" exclusively, no handwritten SQL
- Log intent before each step and obey returned mail; send artifact pointers, never substantive messages
- Lead owns map, rules, contracts, permissions and acceptance; workers edit only their owned files
- User explicitly confirmed claude-opus-5.5 for subsequent launches: low effort for workers/brief authors, medium for refiners, high for deciders/acceptance reviewers. Pass model and reasoning_effort explicitly; leave context_tier unspecified. If rejected, stop the affected launch and report rather than substitute
- Earlier agents used unspecified host defaults; preserve their unknown model/effort attribution and completed evidence without relabeling
- Use host file-edit tools; no shell/script manual file writers
- Skill script failures stop the affected step; no workaround or generated-map hand edits
- No worker launches helpers; request a separately bounded decider/reviewer when required
- No resident worker follow-up waits unless assigned; return idle to preserve slots

## Execution and authorization

- Full mode selected by user on 2026-11-14; two or three substantive workstreams, maximum four executing agents including coordinator
- Initial phase estimate: about 90 minutes from 17:38Z, including about 20 minutes for validation/docs; roughly 16 launches, not a time or launch cap
- Preserve original/latest estimates and cumulative actual usage; elapsed time alone never stops this phase or a worker
- Coordinator investigates assignments running beyond their estimate or lacking expected evidence. Check results, repeated failures, completed steps and actual activity; productive overruns continue with revised estimates/checkpoints
- Redirect, diagnose or escalate stalled/repeating failures within authority. Optional hard worker limits need an explicit reason, owner and stop action; none are currently imposed
- Preparation and implementation estimates are checkpoints; independently refined briefs and exact ownership remain required
- User authorized local commits on current main, pushes, publishing, system changes and external changes
- These grants are limited to this parity task, not arbitrary services, releases or user data; select precise destinations and justified actions before execution
- Workers receive no commit, push, publish, installation or live-service permission unless a later contract explicitly grants it
- Do not commit unrelated pre-existing changes; no attribution language
- Stop immediately for auth failures, manual consent, missing user credentials or content-exclusion denials
- Runtime and canonical build output have one owner at a time; no live fallthrough from offline tests

## Concurrency replanning pause

User requested a graceful stop and decision-led graph restructuring at 18:04Z on 2026-11-14. All six existing agents were already native-idle, with assessments/decisions and coordinator notes saved; no application command was running. Their assignments are parked, and the coordinator watcher is stopped. Do not resume implementation or old listeners during replanning. Only the new concurrency decision task and its explicitly authorized preparation helpers may execute. The lead applies the accepted graph before releasing further work

## Resumption and brief gates - 18:23Z

User authorized continuing the swarm after adding the author/refiner gate to the skill. The replanning pause above is historical. Keep old agents parked; launch a fresh explicitly configured coordinator and independent preparation workers

- D5 is an accepted graph decision only. Lead-written task recipes below it are unvalidated requirements/proposals, not authored or refined briefs
- Task 14 supplies explicit level-0 author and separate level-1 refinement assignments. Implementation requires both task/result pointers and the checked brief identity; accepted static assessments or a decider's recipe cannot substitute
- The lead may record native worker outputs verbatim and bind metadata as a mechanical handoff, but must not author or silently correct the brief. Send corrections back to the assigned author/refiner
- Read-only preparation/refinement/coordination roles need read/search and bus tools, not file-edit capability. Their contract may explicitly request native handoffs; the lead records the supplied artifact and worker state. This is not permission for shell source writers

## Planned task contracts

These terms apply to tasks 6-14 and C2 together with each task's bounded assignment. Task files without a worker-authored Brief and separate refinement result remain planning scopes, not executable implementation authority

- Required sources: AGENTS.md, swarm roles/common.md and the assigned role playbook, current rules, task contract and named canonical context. Explicitly load sources; do not claim automatic inheritance. Read ~/.agents/docs/justfiles.md before any justfile work
- Model/effort: claude-opus-5.5 explicitly selected; low author/executor/coordinator, medium independent refiner, high decider/acceptance reviewer. Never silently substitute
- Scope: preserve all dirty work and historical receipts; never read excluded messages.md. Sanitize all artifacts/logs/native handoffs; no secrets, personal data, confidential source, raw prompts or private reasoning
- Edit-requiring implementation first checks required host edit tools. Missing capability blocks that assignment without changing model, shell source writers or unsupported workaround. Read-only work continues independently
- One owner per written file; feature test headers contain meaningful tests, not empty scaffolds. tests/test_main.cpp has a one-include-at-a-time lease: planned order 11-A, 10-A, 10-C, skipping unselected tasks. Release immediately after the edit, not after the task finishes
- Build tokens apply only at check steps, not editing. Lead leases one existing test-build directory initially; 6-A is optional extra capacity. Test-only targets must not overwrite canonical app/adapter outputs. No implicit configure/install; actual canonical ownership is granted only to 13-A
- No builds/tests/live APIs/installs/commits/pushes for preparation/refinement unless explicitly granted. Implementation exact commands/selectors must come from the independently refined brief; unknown commands need a bounded discovery gate
- Workers own only their assigned outputs/Notes; read-only native handoffs are recorded by the lead. No nested helpers without a separate explicit assignment. No one self-accepts
- Before each step: bus.mjs log with task/name and sanitized intent; obey returned mail. Artifacts first, then bus send pointer to coord-v2 (lead if coordinator unavailable). Native completion carries authorized read-only handoffs; the lead saves before acceptance or dependent dispatch
- Estimates and checkpoints are not stop limits. Max four executing agents/listeners including coordinator; no arbitrary time/retry/token caps. Continue productive work and investigate actual no-progress/failure patterns
- Stop for auth/manual-consent/missing-secret/content-exclusion blockers; do not retry or switch access routes. Unexpected technical issues preserve work and report exact evidence to coordinator/lead
- Lead owns graph, permissions and acceptance. Future conditional tasks get explicit brief/refinement assignments when selected; unresolved choices stay blocked, not silently retired. Source repairs require independent high-effort acceptance review after their own checks
