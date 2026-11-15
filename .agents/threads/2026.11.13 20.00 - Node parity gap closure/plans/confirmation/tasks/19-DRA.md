---
id: 19-DRA
title: Independently refine Q-L1 stored-font inheritance brief
parent: 19
kind: task
status: accepted
contract_rev: 1
worked_rev: 1
depends_on: [19-DA]
blocked_on:
role: refiner2
model: gemini-4-hydrogen
effort: medium
agent: brief-19a-refiner
chat: unavailable
started: 2026-11-15T08:03
ended: 2026-11-15T08:05
evidence: [../briefs/19-A.md]
summary: Accepted (changes made); resolved root cause in src/lens/project.hpp:334 (diff vs textStyleEffective instead of raw inlineItem.style)
---
# 19-DRA. Refine 19-A brief

## Contract rev 1

- Independently verify `briefs/19-A.author.md`, resolve whether `[1, 20)` pinning comes from `src/reconcile/paragraph.hpp`, `src/model/edit.hpp`, or `src/emulator/document_emulator.hpp`, and return complete `## Brief` natively for `briefs/19-A.md`.
