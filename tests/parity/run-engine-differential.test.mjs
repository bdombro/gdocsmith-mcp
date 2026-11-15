import assert from "node:assert/strict";
import test from "node:test";
import { EXPECTED_CASE_IDS, EXPECTED_CONTROLS, EXPECTED_VARIANTS, assertFreshDestinations, qualify } from "./run-engine-differential.mjs";

const run = () => ({ execution: "completed", exitStatus: 0, signal: null, observation: {}, assertions: [{ id: "a", pass: true }], cleanup: { status: "removed" } });
const validReport = () => ({
  schema: "engine-parity-results-v1",
  cases: EXPECTED_CASE_IDS.map((caseId) => ({ caseId, node: run(), cpp: run(), differential: [] })),
  variants: EXPECTED_VARIANTS.map((v) => ({ ...v, node: run(), cpp: run(), differential: [] })),
  controls: EXPECTED_CONTROLS.map((c) => ({
    ...c, variant: c.variant ?? undefined, baselineWasPassing: true, rejectedBy: [c.requiredAssertion], detected: true,
    ...(c.type === "backend-behavior" ? { reached: true, run: run() } : {}),
  })),
  summary: { ownedRunDirectoriesRemaining: 0 },
  frozenInputs: true,
  compiledInputsCurrent: true,
  normalization: { createdIdBijections: [{ caseId: "V009-image-upload-lifecycle", kind: "inline-image", valid: true }] },
});

test("inventory matches the verified 70 controls", () => {
  assert.equal(EXPECTED_CONTROLS.length, 70);
  assert.equal(EXPECTED_CONTROLS.filter((c) => c.type === "backend-behavior").length, 6);
});

test("complete valid report passes", () => {
  assert.deepEqual(qualify(validReport()), { pass: true, reasons: [] });
});

const backend = (r) => r.controls.find((c) => c.type === "backend-behavior");
const failing = {
  "missing case": (r) => r.cases.pop(),
  "extra case": (r) => r.cases.push({ ...r.cases[0], caseId: "V999-extra" }),
  "duplicate case": (r) => { r.cases[1] = { ...r.cases[0] }; },
  "missing variant": (r) => r.variants.pop(),
  "empty controls": (r) => { r.controls = []; },
  "one named control removed": (r) => r.controls.splice(3, 1),
  "unexpected control added": (r) => r.controls.push({ ...r.controls[0], name: "unexpected" }),
  "backend not reached": (r) => { backend(r).reached = false; },
  "baseline not passing": (r) => { r.controls[0].baselineWasPassing = false; },
  "required assertion missing from rejectedBy": (r) => { r.controls[0].rejectedBy = ["other"]; },
  "remainder 1": (r) => { r.summary.ownedRunDirectoriesRemaining = 1; },
  "case cleanup failure": (r) => { r.cases[0].cpp.cleanup = { status: "failed", reason: "x" }; },
  "backend run cleanup failure": (r) => { backend(r).run.cleanup = { status: "failed", reason: "x" }; },
  "controls missing": (r) => { delete r.controls; },
  "differential null": (r) => { r.cases[0].differential = null; },
  "missing side observation": (r) => { delete r.cases[0].node.observation; },
  "launch failure": (r) => { r.cases[0].cpp = { execution: "failed", exitStatus: 1, signal: null, cleanup: { status: "removed" } }; },
  "assertion failure": (r) => { r.variants[0].node.assertions[0].pass = false; },
  "frozenInputs false": (r) => { r.frozenInputs = false; },
  "compiledInputsCurrent false": (r) => { r.compiledInputsCurrent = false; },
  "one raw difference with passing assertions": (r) => { r.cases[4].differential = [{ path: "$.result.status", original: "a", target: "b" }]; },
  "invalid bijection": (r) => { r.normalization.createdIdBijections[0].valid = false; },
};
for (const [name, mutate] of Object.entries(failing)) {
  test(`fails: ${name}`, () => {
    const report = validReport();
    mutate(report);
    const result = qualify(report);
    assert.equal(result.pass, false);
    assert.ok(result.reasons.length > 0);
  });
}

const enoent = () => { throw Object.assign(new Error("missing"), { code: "ENOENT" }); };
test("fresh destinations: all ENOENT accepted", () => {
  assert.doesNotThrow(() => assertFreshDestinations(["runs", "results.json", "results-raw-diffs.json"], enoent));
});
for (const existing of ["runs", "results.json", "results-raw-diffs.json"]) {
  test(`fresh destinations: rejects existing ${existing}`, () => {
    const lstat = (path) => (path === existing ? { isSymbolicLink: () => false } : enoent());
    assert.throws(() => assertFreshDestinations(["runs", "results.json", "results-raw-diffs.json"], lstat), /already exists/);
  });
}
test("fresh destinations: rejects symlink stat result", () => {
  assert.throws(() => assertFreshDestinations(["results.json"], () => ({ isSymbolicLink: () => true })), /already exists/);
});
test("fresh destinations: rethrows non-ENOENT errors", () => {
  const lstat = () => { throw Object.assign(new Error("denied"), { code: "EACCES" }); };
  assert.throws(() => assertFreshDestinations(["results.json"], lstat), /denied/);
});
