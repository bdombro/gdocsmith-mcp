<!-- Generated from swarm-mode templates/task-state.md. -->
# 3 Engine segments/footnotes/rejections — state
Status: unassigned; common contract in README applies.

## Assignment contract
- Objective: faithfully wire confirmed source import settings/segments/footnotes/rejection workflows rather than simply delete current refusals.
- Inputs: external engine import source cases supplied in assignment; target readonlyValidate and explicit rejection refusal; reconcile segment/footnote plans.
- Proposed exclusive writes: `src/engine/import.hpp`, `src/engine/session.hpp`, parent-named engine tests/helper. Parent explicitly serializes any flush/transaction/reconcile/model hook.
- Required evidence: existing/new segment and footnote lifecycle, settings/style propagation, rejection semantics/guard authority, phased replies/identity resolution, stale-base and no-write-on-failure, authoritative readback and partial/unknown outcome protection.
- Validation: exact affected engine and transaction case selectors selected by owner; cache Debug/Release affected/full checks and faithful source obligations. Offline service fakes only, no Google.
- Acceptance: parent after independent review; source feature refusal is not an equivalent replacement.
- Depends on: none for bounded engine work; final rich/style import proof depends on accepted lens/helper APIs and serialized integration.

## Gate evidence
Implementation, transfer, controls and review unrun. Existing typed session surface lacks these explicit editing methods in bootstrap inspection.
