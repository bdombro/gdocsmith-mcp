# Raw group accounting

Static assessment of the existing resumed-final-recheck receipt, not a new execution

Root-relative receipt directory: .agents/threads/2026.11.13 20.00 - Node parity gap closure/tasks/parity-evidence/resumed-final-recheck

Authority: differential-results-raw-diffs.json, SHA-256 8291aac1269b94683c6ca0cea88ccf9cbdbbdf422e5072209ee48b42ae4162b6

G001 means groupedExactPathAndValues[0]; every numbered group below resolves to its exact path, ordered original/target values, sourceConstraints and cases in that immutable receipt. No raw observations are reproduced here. V001–V010 abbreviate the corresponding receipt caseId; V009K means V009-image-upload-lifecycle/known-upload-error.

## Units and validation

| Class | Existing classification, not an accepted disposition | Groups | Leaf occurrences |
| --- | --- | ---: | ---: |
| C1 | External call schedule/payload, unresolved | 101 | 124 |
| C2 | Native file format, unresolved | 27 | 31 |
| C3 | Observable contract, unresolved | 35 | 38 |
| C4 | Narrow previously approved recovery-directory layout | 3 | 3 |
| C5 | Write request payload/order, potential defect | 9 | 10 |
| C6 | Provider state, potential defect | 26 | 39 |
| C7 | Wording proposal, unaccepted | 5 | 5 |
| Total | No new waivers | 206 | 250 |

The brief's seven category numbers are group counts, not leaf counts. Summing the cases attached to each group yields the leaf-occurrence column. A read-only structural comparison rebuilt the case/path/original/target multiset from differential-results.json: exact equality, zero missing and zero extra entries. All 206 raw array entries are covered exactly once below.

The grouping key also includes variant, not just path and values: tests/parity/run-engine-differential.mjs:497–505. There are 187 distinct path/value triples and 206 variant-sensitive groups. The 19 repeated triples across the known-error variant are not unexplained duplication and must not be collapsed.

Per-execution leaf totals: V001 9; V002 13; V003 8; V004 13; V005 27; V006 23; V007 10; V008 28; V009 52; V010 37; V009K 30. Sum 250, matching the eleven differential arrays.

## Complete group-to-case index

Ranges are inclusive; group numbers are one-based

| Class | Cases | Groups |
| --- | --- | --- |
| C1 | V001 | 3-6 |
| C1 | V001,V002,V003,V004,V005,V006,V007,V008,V009,V010 | 1 |
| C1 | V001,V002,V003,V004,V005,V006,V007,V009 | 2 |
| C1 | V002 | 10-17 |
| C1 | V003 | 19-20 |
| C1 | V004 | 24-26, 29 |
| C1 | V004,V005,V007 | 27-28 |
| C1 | V005 | 34 |
| C1 | V005,V006 | 35 |
| C1 | V006,V008 | 57 |
| C1 | V008 | 70-77 |
| C1 | V008,V009 | 69 |
| C1 | V009 | 95-121 |
| C1 | V009K | 177-192 |
| C1 | V010 | 144-167 |
| C2 | V001 | 7 |
| C2 | V001,V002,V003,V004 | 9 |
| C2 | V002 | 18 |
| C2 | V003 | 21 |
| C2 | V004 | 30 |
| C2 | V005 | 36, 38-40 |
| C2 | V006 | 58-59 |
| C2 | V007 | 63, 65-68 |
| C2 | V008 | 78, 80-83 |
| C2 | V009 | 122 |
| C2 | V009,V010 | 134 |
| C2 | V009K | 193, 195, 206 |
| C2 | V010 | 168 |
| C3 | V001,V002 | 8 |
| C3 | V003 | 22 |
| C3 | V004 | 31 |
| C3 | V005 | 37 |
| C3 | V007 | 64 |
| C3 | V008 | 79, 92-94 |
| C3 | V009 | 125, 127-133 |
| C3 | V009,V010 | 123-124 |
| C3 | V009K | 194, 196-197, 199-205 |
| C3 | V010 | 169, 171-173, 175-176 |
| C4 | V003 | 23 |
| C4 | V004 | 32-33 |
| C5 | V005 | 41 |
| C5 | V005,V006 | 42 |
| C5 | V008 | 84-87 |
| C5 | V009 | 135-137 |
| C6 | V005 | 55 |
| C6 | V005,V006 | 43-54, 56 |
| C6 | V006 | 60-61 |
| C6 | V008 | 88-91 |
| C6 | V009 | 138-143 |
| C7 | V006 | 62 |
| C7 | V009 | 126 |
| C7 | V009K | 198 |
| C7 | V010 | 170, 174 |
