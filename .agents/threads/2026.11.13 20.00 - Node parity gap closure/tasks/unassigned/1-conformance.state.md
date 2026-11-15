<!-- Generated from swarm-mode templates/task-state.md. -->
# 1 Conformance fail-closed — state
Status: unassigned; common contract in README applies.

## Assignment contract
- Objective: eliminate silent unreadable/malformed fixture skips in `tests/emulator/test_conformance.hpp`; assert all discovered expected fixtures are processed and failures are attributed.
- Inputs: supplied surviving actual-binary malformed-B1 negative control; 149 identical JSON baseline.
- Proposed exclusive writes: runner header plus dedicated regression helper/tests explicitly assigned by parent; no fixture deletion/normalization or broad emulator changes.
- Required evidence: malformed JSON, unreadable/missing fixture as feasible, invalid/unsupported fixture shape and valid corpus; prove each consequential fault reaches an assertion and yields nonzero failure, not merely launch failure.
- Validation: workspace cached test-only build plus `--test-suite='emulator::conformance'`; full affected suite after repair; independent expected fixture accounting, not an arbitrary minimum threshold.
- Acceptance: parent after review; no source behavior retirement. Clarify exact fixture-shape contract within lane, do not broaden blindly.
- Depends on: none for dispatch; parent assigns paths, budget and execution slot.

## Gate evidence
All execution/review gates unrun in this shell. Historical surviving control is a known failure of the old harness.
