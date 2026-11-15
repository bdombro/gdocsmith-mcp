# Resumed parity-only confidence probe

Completed **22:39:40–22:39:46Z, 5.797 seconds**, exit **1**, NOT a timeout.
One execution, dedicated 120-second allowance, existing cached Node24.11.0
and existing ParityChecks adapter. No builds or source edits.

- Ten vectors plus isolated known-ID variant completed, zero launch failures.
- Original **151/151** assertions pass; target **171/172** pass.
- Sole target named failure: V008 `review:full-source-suggestion-grammar`.
  Actual header now contains `tab=Overview`, but omits original
  `exportLine=13`. The formerly observed extra trailing newline is absent.
- Controls **69/70**: 63/64 observation controls and 6/6 backend controls.
  The remaining observation control has an already-failing baseline and is
  correctly not credited as detected.
- Raw differences remain **250 leaves / 206 exact groups**, across eleven
  executions. No normalization, dropping or acceptance of raw differences.
- `frozenInputs=true`, `compiledInputsCurrent=true`, no newer compiled inputs,
  no original/target identity changes during this probe. This does not freeze
  the independently active export-fix worker after probe completion.
- Actual adapter SHA-256:
  `f09126d4f09050fde8e4f308229722d22328f88f73df38a62e4d751376e3cda5`.
  Complete input/runtime/build identities and raw synthetic observations remain
  in `results.json`; `confidence-summary.json` gives bounded execution results.
- Zero surviving driver run directories; owned scratch removed; no owned
  commands running.

The harness completes promptly when given its own budget. This establishes
runtime feasibility, not parity or full qualification. Earlier stopped runs and
RED receipts remain unchanged. Current V008 failure and all raw distinctions
need disposition before acceptance. No source was read to reconstruct the
formerly excluded test; thread messages were not accessed.
