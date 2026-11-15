---
id: 9-FR
title: Review authorized comment journey and partial outcomes
parent: 9
kind: task
status: accepted
contract_rev: 1
worked_rev: 2
depends_on: [9-F]
blocked_on:
role: reviewer2
model: gemini-4-hydrogen
effort: medium
agent: f1c45e82-113d-4c78-aaff-3dbe3583ecfd
chat: unavailable
started: 2026-11-15T00:15
ended: 2026-11-15T00:20
evidence: [../../tasks/parity-evidence/9f-w4-verify-2/differential-results.json]
summary: Accepted (approved rev 2); Node W4 0 failures & corrupt-partial-write-state detected; diagnosed C++ baseValidate/documentFileExport newline staleBase bug
---
# 9-FR. W4 review

## Contract rev 1

- Read-only native review against [W4](../assessments/workflows-gate.md) and shared [planning contracts](../rules.md#planned-task-contracts)
- Require actual original partial outcomes, ownership/refusal/no-replay protection and meaningful backend faults; approve/changes-needed only
- Set estimate from selected evidence before dispatch
