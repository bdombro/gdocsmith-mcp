import assert from "node:assert/strict";
import { createHash, randomUUID } from "node:crypto";
import { existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safetyAssertions, safetyControls } from "./safety-assertions.mjs";

export const EXPECTED_CASE_IDS = [
  "V001-noop", "V002-body-and-title", "V003-stale", "V004-landed-lost-response-replay", "V005-font-default",
  "V006-font-nondefault", "V007-cell-style-edit", "V008-reject-hidden-insertion", "V009-image-upload-lifecycle",
  "V010-cross-doc-image-copy",
];
export const EXPECTED_VARIANTS = [{ caseId: "V009-image-upload-lifecycle", variant: "known-upload-error" }];
// [side, caseId, variant, name, requiredAssertion, O = observation-corruption | B = backend-behavior]
export const EXPECTED_CONTROLS = [
  ["node", "V001-noop", null, "extra-write", "OB-001:no-mutations", "O"],
  ["node", "V001-noop", null, "missing-export", "OB-001:export-exists", "O"],
  ["node", "V002-body-and-title", null, "wrong-order", "OB-002:content-before-rename", "O"],
  ["node", "V002-body-and-title", null, "ack-without-mutation", "OB-002:remote-body", "B"],
  ["node", "V002-body-and-title", null, "race-before-send", "OB-002:status", "B"],
  ["node", "V003-stale", null, "wrong-fresh-recovery-body", "recovery:authoritative-body", "O"],
  ["node", "V003-stale", null, "flatten-refusal", "OB-006:status", "O"],
  ["node", "V004-landed-lost-response-replay", null, "wrong-fresh-recovery-body", "recovery:authoritative-body", "O"],
  ["node", "V004-landed-lost-response-replay", null, "unknown-as-complete", "OB-008:write-state", "O"],
  ["node", "V004-landed-lost-response-replay", null, "replayed-batch", "OB-008:single-batch", "O"],
  ["node", "V006-font-nondefault", null, "all-font-omissions-equivalent", "OB-009.22:verification", "O"],
  ["node", "V007-cell-style-edit", null, "unrelated-text-write", "cell-style:only-intended-write", "O"],
  ["node", "V007-cell-style-edit", null, "unrelated-cell-field", "cell-style:only-intended-write", "O"],
  ["node", "V007-cell-style-edit", null, "refusal-is-not-success", "OB-027:no-errors", "O"],
  ["node", "V008-reject-hidden-insertion", null, "corrupt-comment", "review:initial:full-comment-payload", "O"],
  ["node", "V008-reject-hidden-insertion", null, "corrupt-reply", "review:exported:full-comment-payload", "O"],
  ["node", "V008-reject-hidden-insertion", null, "corrupt-suggestion-grammar", "review:full-source-suggestion-grammar", "O"],
  ["node", "V008-reject-hidden-insertion", null, "refusal-is-not-success", "OB-029:no-errors", "O"],
  ["node", "V009-image-upload-lifecycle", null, "provider-id-not-retained", "OB-036:provider-id-retention", "O"],
  ["node", "V009-image-upload-lifecycle", null, "image-reinserted-on-noop", "OB-036:no-reinsert", "O"],
  ["node", "V009-image-upload-lifecycle", null, "bad-input-sent", "OB-036:preflight-no-send", "O"],
  ["node", "V009-image-upload-lifecycle", null, "leak-known-throwaway-document", "OB-036:temporary-documents-deleted", "B"],
  ["node", "V010-cross-doc-image-copy", null, "provider-id-not-retained", "OB-034:provider-id-retention", "O"],
  ["node", "V010-cross-doc-image-copy", null, "image-reinserted-on-noop", "OB-034:no-reinsert", "O"],
  ["node", "V010-cross-doc-image-copy", null, "duplicate-source-fetch", "OB-034:source-read-once", "O"],
  ["node", "V010-cross-doc-image-copy", null, "generic-foreign-refusal", "OB-034:unknown-source-errors", "O"],
  ["cpp", "V001-noop", null, "extra-write", "OB-001:no-mutations", "O"],
  ["cpp", "V001-noop", null, "missing-export", "OB-001:export-exists", "O"],
  ["cpp", "V001-noop", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V002-body-and-title", null, "wrong-order", "OB-002:content-before-rename", "O"],
  ["cpp", "V002-body-and-title", null, "ack-without-mutation", "OB-002:remote-body", "B"],
  ["cpp", "V002-body-and-title", null, "race-before-send", "OB-002:status", "B"],
  ["cpp", "V002-body-and-title", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V003-stale", null, "wrong-fresh-recovery-body", "recovery:authoritative-body", "O"],
  ["cpp", "V003-stale", null, "flatten-refusal", "OB-006:status", "O"],
  ["cpp", "V003-stale", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V004-landed-lost-response-replay", null, "wrong-fresh-recovery-body", "recovery:authoritative-body", "O"],
  ["cpp", "V004-landed-lost-response-replay", null, "unknown-as-complete", "OB-008:write-state", "O"],
  ["cpp", "V004-landed-lost-response-replay", null, "replayed-batch", "OB-008:single-batch", "O"],
  ["cpp", "V004-landed-lost-response-replay", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V005-font-default", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V006-font-nondefault", null, "all-font-omissions-equivalent", "OB-009.22:verification", "O"],
  ["cpp", "V006-font-nondefault", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V007-cell-style-edit", null, "unrelated-text-write", "cell-style:only-intended-write", "O"],
  ["cpp", "V007-cell-style-edit", null, "unrelated-cell-field", "cell-style:only-intended-write", "O"],
  ["cpp", "V007-cell-style-edit", null, "refusal-is-not-success", "OB-027:no-errors", "O"],
  ["cpp", "V007-cell-style-edit", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V008-reject-hidden-insertion", null, "corrupt-comment", "review:initial:full-comment-payload", "O"],
  ["cpp", "V008-reject-hidden-insertion", null, "corrupt-reply", "review:exported:full-comment-payload", "O"],
  ["cpp", "V008-reject-hidden-insertion", null, "corrupt-suggestion-grammar", "review:full-source-suggestion-grammar", "O"],
  ["cpp", "V008-reject-hidden-insertion", null, "refusal-is-not-success", "OB-029:no-errors", "O"],
  ["cpp", "V008-reject-hidden-insertion", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V009-image-upload-lifecycle", null, "provider-id-not-retained", "OB-036:provider-id-retention", "O"],
  ["cpp", "V009-image-upload-lifecycle", null, "image-reinserted-on-noop", "OB-036:no-reinsert", "O"],
  ["cpp", "V009-image-upload-lifecycle", null, "bad-input-sent", "OB-036:preflight-no-send", "O"],
  ["cpp", "V009-image-upload-lifecycle", null, "leak-known-throwaway-document", "OB-036:temporary-documents-deleted", "B"],
  ["cpp", "V009-image-upload-lifecycle", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "V010-cross-doc-image-copy", null, "provider-id-not-retained", "OB-034:provider-id-retention", "O"],
  ["cpp", "V010-cross-doc-image-copy", null, "image-reinserted-on-noop", "OB-034:no-reinsert", "O"],
  ["cpp", "V010-cross-doc-image-copy", null, "duplicate-source-fetch", "OB-034:source-read-once", "O"],
  ["cpp", "V010-cross-doc-image-copy", null, "generic-foreign-refusal", "OB-034:unknown-source-errors", "O"],
  ["cpp", "V010-cross-doc-image-copy", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["node", "V009-image-upload-lifecycle", "known-upload-error", "wrong-known-id-path", "known-id:canonical-existing-path", "O"],
  ["node", "V009-image-upload-lifecycle", "known-upload-error", "missing-kept-warning", "known-id:kept-local-warning", "O"],
  ["node", "V009-image-upload-lifecycle", "known-upload-error", "clobbered-edited-bytes", "known-id:edited-bytes-preserved", "O"],
  ["node", "V009-image-upload-lifecycle", "known-upload-error", "wrong-known-id-fresh-body", "known-id:authoritative-fresh-original", "O"],
  ["cpp", "V009-image-upload-lifecycle", "known-upload-error", "wrong-known-id-path", "known-id:canonical-existing-path", "O"],
  ["cpp", "V009-image-upload-lifecycle", "known-upload-error", "missing-kept-warning", "known-id:kept-local-warning", "O"],
  ["cpp", "V009-image-upload-lifecycle", "known-upload-error", "clobbered-edited-bytes", "known-id:edited-bytes-preserved", "O"],
  ["cpp", "V009-image-upload-lifecycle", "known-upload-error", "wrong-known-id-fresh-body", "known-id:authoritative-fresh-original", "O"],
].map(([side, caseId, variant, name, requiredAssertion, type]) => ({
  side, caseId, variant, name, requiredAssertion, type: type === "B" ? "backend-behavior" : "observation-corruption",
}));

export const CONFIRMATION_CASE_IDS = [
  "W2-rename-only", "W2-delayed-docs-title", "W2-rename-fail-after-content",
  "W1-create-success-replay", "W1-create-rollback-retry", "W1-create-rollback-rejected",
  "W1-create-mismatch-readback", "W1-create-unknown-outcome", "W1-create-invalid-syntax",
  "W3-reparent-order-noop", "W3-orphan-refusal", "W3-delete-subtree-and-all",
  "W4-comment-allow-and-actions", "W4-comment-ownership-and-readonly",
  "W4-comment-resolved-reopen", "W4-comment-partial-unknown-outcome",
];
export const CONFIRMATION_VARIANTS = [];
export const CONFIRMATION_CONTROLS = [
  ["node", "W2-rename-only", null, "extra-content-batch", "w2:rename-only-no-content-batch", "O"],
  ["node", "W2-rename-only", null, "duplicate-rename-write", "w2:no-duplicate-writes", "O"],
  ["node", "W2-rename-only", null, "mutated-protected-tab", "w2:protected-tab-unchanged", "O"],
  ["node", "W2-rename-only", null, "wrong-remote-title", "w2:rename-only-remote-title", "O"],
  ["node", "W2-rename-only", null, "ack-rename-without-mutation", "w2:rename-only-remote-title", "B"],
  ["node", "W2-delayed-docs-title", null, "equate-delayed-docs-with-failure", "w2:delayed-docs-title-succeeds", "O"],
  ["node", "W2-rename-fail-after-content", null, "erase-partial-write-state", "w2:partial-rename-failure-state", "O"],
  ["node", "W2-rename-fail-after-content", null, "wrong-failure-phase", "w2:partial-rename-failure-state", "O"],
  ["node", "W2-rename-fail-after-content", null, "wrong-old-export-title", "w2:partial-rename-old-export-title", "O"],
  ["cpp", "W2-rename-only", null, "extra-content-batch", "w2:rename-only-no-content-batch", "O"],
  ["cpp", "W2-rename-only", null, "duplicate-rename-write", "w2:no-duplicate-writes", "O"],
  ["cpp", "W2-rename-only", null, "mutated-protected-tab", "w2:protected-tab-unchanged", "O"],
  ["cpp", "W2-rename-only", null, "wrong-remote-title", "w2:rename-only-remote-title", "O"],
  ["cpp", "W2-rename-only", null, "ack-rename-without-mutation", "w2:rename-only-remote-title", "B"],
  ["cpp", "W2-rename-only", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W2-delayed-docs-title", null, "equate-delayed-docs-with-failure", "w2:delayed-docs-title-succeeds", "O"],
  ["cpp", "W2-delayed-docs-title", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W2-rename-fail-after-content", null, "erase-partial-write-state", "w2:partial-rename-failure-state", "O"],
  ["cpp", "W2-rename-fail-after-content", null, "wrong-failure-phase", "w2:partial-rename-failure-state", "O"],
  ["cpp", "W2-rename-fail-after-content", null, "wrong-old-export-title", "w2:partial-rename-old-export-title", "O"],
  ["cpp", "W2-rename-fail-after-content", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["node", "W1-create-success-replay", null, "corrupt-provider-id-result", "w1:provider-id-returned", "O"],
  ["node", "W1-create-success-replay", null, "corrupt-provider-id-export", "w1:provider-id-returned", "O"],
  ["node", "W1-create-success-replay", null, "duplicate-create-on-replay", "w1:single-create-on-replay", "O"],
  ["node", "W1-create-rollback-retry", null, "false-deletion-state", "w1:rollback-confirmed-before-retry", "O"],
  ["node", "W1-create-rollback-retry", null, "wrong-retry-status", "w1:explicit-retry-succeeds", "O"],
  ["node", "W1-create-rollback-retry", null, "ack-delete-without-mutation", "w1:rollback-confirmed-before-retry", "B"],
  ["node", "W1-create-rollback-rejected", null, "drop-retained-id", "w1:rollback-rejected-retains-id", "O"],
  ["node", "W1-create-rollback-rejected", null, "drop-delete-warning", "w1:rollback-rejected-warning", "O"],
  ["node", "W1-create-mismatch-readback", null, "wrong-mismatch-verification", "w1:mismatch-keeps-landed-content", "O"],
  ["node", "W1-create-unknown-outcome", null, "invented-doc-id", "w1:unknown-outcome-no-blind-retry", "O"],
  ["node", "W1-create-invalid-syntax", null, "unexpected-create-on-invalid", "w1:invalid-syntax-zero-creates", "O"],
  ["cpp", "W1-create-success-replay", null, "corrupt-provider-id-result", "w1:provider-id-returned", "O"],
  ["cpp", "W1-create-success-replay", null, "corrupt-provider-id-export", "w1:provider-id-returned", "O"],
  ["cpp", "W1-create-success-replay", null, "duplicate-create-on-replay", "w1:single-create-on-replay", "O"],
  ["cpp", "W1-create-success-replay", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W1-create-rollback-retry", null, "false-deletion-state", "w1:rollback-confirmed-before-retry", "O"],
  ["cpp", "W1-create-rollback-retry", null, "wrong-retry-status", "w1:explicit-retry-succeeds", "O"],
  ["cpp", "W1-create-rollback-retry", null, "ack-delete-without-mutation", "w1:rollback-confirmed-before-retry", "B"],
  ["cpp", "W1-create-rollback-retry", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W1-create-rollback-rejected", null, "drop-retained-id", "w1:rollback-rejected-retains-id", "O"],
  ["cpp", "W1-create-rollback-rejected", null, "drop-delete-warning", "w1:rollback-rejected-warning", "O"],
  ["cpp", "W1-create-rollback-rejected", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W1-create-mismatch-readback", null, "wrong-mismatch-verification", "w1:mismatch-keeps-landed-content", "O"],
  ["cpp", "W1-create-mismatch-readback", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W1-create-unknown-outcome", null, "invented-doc-id", "w1:unknown-outcome-no-blind-retry", "O"],
  ["cpp", "W1-create-unknown-outcome", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W1-create-invalid-syntax", null, "unexpected-create-on-invalid", "w1:invalid-syntax-zero-creates", "O"],
  ["node", "W3-reparent-order-noop", null, "corrupt-moved-hierarchy", "w3:reparent-order-hierarchy", "O"],
  ["node", "W3-reparent-order-noop", null, "corrupt-moved-body", "w3:bodies-and-ids-preserved", "O"],
  ["node", "W3-reparent-order-noop", null, "extra-write-on-noop-replay", "w3:noop-replay-zero-writes", "O"],
  ["node", "W3-reparent-order-noop", null, "ack-move-without-mutation", "w3:reparent-order-hierarchy", "B"],
  ["node", "W3-orphan-refusal", null, "wrong-orphan-error-code", "w3:orphan-refused-zero-writes", "O"],
  ["node", "W3-delete-subtree-and-all", null, "wrong-subtree-deleted-ids", "w3:subtree-deleted", "O"],
  ["node", "W3-delete-subtree-and-all", null, "missing-tab1-warning", "w3:delete-all-clears-tab1", "O"],
  ["node", "W4-comment-allow-and-actions", null, "corrupt-comment-refusal", "w4:allow-and-actions", "O"],
  ["node", "W4-comment-allow-and-actions", null, "corrupt-comment-postread", "w4:allow-and-actions", "O"],
  ["node", "W4-comment-allow-and-actions", null, "ack-comment-without-mutation", "w4:allow-and-actions", "B"],
  ["node", "W4-comment-ownership-and-readonly", null, "corrupt-ownership-refusal", "w4:ownership-and-readonly", "O"],
  ["node", "W4-comment-resolved-reopen", null, "corrupt-reopen-state", "w4:resolved-reopen", "O"],
  ["node", "W4-comment-partial-unknown-outcome", "fail-second-comment-action", "corrupt-partial-write-state", "w4:partial-unknown-outcome", "O"],
  ["cpp", "W3-reparent-order-noop", null, "corrupt-moved-hierarchy", "w3:reparent-order-hierarchy", "O"],
  ["cpp", "W3-reparent-order-noop", null, "corrupt-moved-body", "w3:bodies-and-ids-preserved", "O"],
  ["cpp", "W3-reparent-order-noop", null, "extra-write-on-noop-replay", "w3:noop-replay-zero-writes", "O"],
  ["cpp", "W3-reparent-order-noop", null, "ack-move-without-mutation", "w3:reparent-order-hierarchy", "B"],
  ["cpp", "W3-reparent-order-noop", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W3-orphan-refusal", null, "wrong-orphan-error-code", "w3:orphan-refused-zero-writes", "O"],
  ["cpp", "W3-orphan-refusal", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W3-delete-subtree-and-all", null, "wrong-subtree-deleted-ids", "w3:subtree-deleted", "O"],
  ["cpp", "W3-delete-subtree-and-all", null, "missing-tab1-warning", "w3:delete-all-clears-tab1", "O"],
  ["cpp", "W3-delete-subtree-and-all", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W4-comment-allow-and-actions", null, "corrupt-comment-refusal", "w4:allow-and-actions", "O"],
  ["cpp", "W4-comment-allow-and-actions", null, "corrupt-comment-postread", "w4:allow-and-actions", "O"],
  ["cpp", "W4-comment-allow-and-actions", null, "ack-comment-without-mutation", "w4:allow-and-actions", "B"],
  ["cpp", "W4-comment-allow-and-actions", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W4-comment-ownership-and-readonly", null, "corrupt-ownership-refusal", "w4:ownership-and-readonly", "O"],
  ["cpp", "W4-comment-ownership-and-readonly", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W4-comment-resolved-reopen", null, "corrupt-reopen-state", "w4:resolved-reopen", "O"],
  ["cpp", "W4-comment-resolved-reopen", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W4-comment-partial-unknown-outcome", "fail-second-comment-action", "corrupt-partial-write-state", "w4:partial-unknown-outcome", "O"],
  ["cpp", "W4-comment-partial-unknown-outcome", "fail-second-comment-action", "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
  ["cpp", "W1-create-invalid-syntax", null, "wrong-version-strict-node-parser", "OB-043:strict-node-initial-export", "O"],
].map(([side, caseId, variant, name, requiredAssertion, type]) => ({
  side, caseId, variant, name, requiredAssertion, type: type === "B" ? "backend-behavior" : "observation-corruption",
}));

// Any existing entry (file, directory or dangling symlink) is a reused destination
export function assertFreshDestinations(paths, lstat = lstatSync) {
  for (const path of paths) {
    try {
      lstat(path);
    } catch (error) {
      if (error?.code === "ENOENT") continue;
      throw error;
    }
    throw new Error(`Destination already exists: ${path}`);
  }
}

export function qualify(report, expected = { caseIds: EXPECTED_CASE_IDS, variants: EXPECTED_VARIANTS, controls: EXPECTED_CONTROLS }) {
  const reasons = [];
  try {
    const sorted = (values) => JSON.stringify([...values].sort());
    if (report.schema !== "engine-parity-results-v1") reasons.push("unexpected report schema");
    if (sorted(report.cases.map((c) => c.caseId)) !== sorted(expected.caseIds)) reasons.push("case IDs differ from the expected set");
    const variantKey = (v) => JSON.stringify([v.caseId, v.variant]);
    if (sorted(report.variants.map(variantKey)) !== sorted(expected.variants.map(variantKey))) reasons.push("variants differ from the expected set");
    const runs = [];
    for (const item of [...report.cases, ...report.variants]) {
      const label = item.variant ? `${item.caseId}/${item.variant}` : item.caseId;
      for (const side of ["node", "cpp"]) {
        const run = item[side];
        runs.push(run);
        if (run?.execution !== "completed" || !run.observation || !Array.isArray(run.assertions) || run.assertions.length === 0) {
          reasons.push(`${label} ${side} did not complete with an observation and assertions`);
        } else if (run.assertions.some((a) => a.pass !== true)) reasons.push(`${label} ${side} assertion failure`);
      }
      if (!Array.isArray(item.differential) || item.differential.length > 0) reasons.push(`${label} has raw differences or none were computed`);
    }
    const controlKey = (c) => JSON.stringify([c.side, c.caseId, c.variant ?? null, c.name, c.requiredAssertion, c.type]);
    if (sorted(report.controls.map(controlKey)) !== sorted(expected.controls.map(controlKey))) reasons.push("controls differ from the expected inventory");
    for (const control of report.controls) {
      const label = `${control.side} ${control.caseId} ${control.name}`;
      if (control.baselineWasPassing !== true) reasons.push(`${label} baseline was not passing`);
      if (!control.rejectedBy?.includes(control.requiredAssertion)) reasons.push(`${label} not rejected by its required assertion`);
      if (control.detected !== true) reasons.push(`${label} not detected`);
      if (control.type === "backend-behavior") {
        runs.push(control.run);
        if (control.reached !== true || control.run?.execution !== "completed") reasons.push(`${label} backend fault not reached or run incomplete`);
      }
    }
    if (runs.some((run) => run?.cleanup?.status !== "removed")) reasons.push("a run directory was not removed");
    if (report.summary.ownedRunDirectoriesRemaining !== 0) reasons.push("owned run directories remain");
    if (report.frozenInputs !== true) reasons.push("inputs changed during the run");
    if (report.compiledInputsCurrent !== true) reasons.push("compiled inputs are newer than the adapter");
    if (report.normalization.createdIdBijections.some((b) => b.valid !== true)) reasons.push("invalid created-ID bijection");
  } catch {
    reasons.push("malformed report");
  }
  return { pass: reasons.length === 0, reasons };
}

const isMain = Boolean(process.argv[1]) && realpathSync(process.argv[1]) === fileURLToPath(import.meta.url);
if (isMain) {
process.umask(0o077);
const args = Object.create(null);
for (let i = 2; i < process.argv.length; i += 2) {
  const key = process.argv[i];
  if (!["--original-root", "--target-adapter", "--vectors", "--case-ids", "--owned-run-root", "--results"].includes(key)
    || !process.argv[i + 1] || args[key]) throw new Error("Unknown, repeated or missing driver argument");
  args[key] = process.argv[i + 1];
}
for (const key of ["--original-root", "--target-adapter", "--vectors", "--case-ids", "--owned-run-root", "--results"]) {
  if (!args[key]) throw new Error(`Missing ${key}`);
}
for (const key of ["--owned-run-root", "--results"]) {
  if (isAbsolute(args[key]) || args[key].split(sep).includes("..")) throw new Error("Owned paths must be project-relative");
}
assertFreshDestinations([resolve(args["--owned-run-root"]), resolve(args["--results"]),
  resolve(args["--results"]).replace(/\.json$/, "") + "-raw-diffs.json"]);
const project = realpathSync(process.cwd());
const here = dirname(fileURLToPath(import.meta.url));
function ownedPath(value, createDirectory = false) {
  if (isAbsolute(value) || value.split(sep).includes("..")) throw new Error("Owned paths must be project-relative");
  const path = resolve(value);
  if (!path.startsWith(`${project}${sep}`) || !value.includes("tasks/parity-evidence/")) {
    throw new Error("Output must be in explicit task evidence scope");
  }
  const parts = relative(project, createDirectory ? path : dirname(path)).split(sep);
  let current = project;
  for (const part of parts) {
    current = resolve(current, part);
    if (existsSync(current)) {
      if (lstatSync(current).isSymbolicLink() || !lstatSync(current).isDirectory()) throw new Error("Unsafe output ancestor");
    } else mkdirSync(current, { mode: 0o700 });
  }
  return path;
}
const runs = ownedPath(args["--owned-run-root"], true);
if ((lstatSync(runs).mode & 0o077) !== 0) throw new Error("Run scope must be private");
const resultsPath = ownedPath(args["--results"]);
if (existsSync(resultsPath) && lstatSync(resultsPath).isSymbolicLink()) throw new Error("Results path is a symlink");
const original = realpathSync(args["--original-root"]);
const { documentFileParse: strictNodeParse } = await import(pathToFileURL(resolve(original, "src/core/lens/document.ts")).href);
const target = realpathSync(args["--target-adapter"]);
if (!target.startsWith(`${project}/build/cmake/ParityChecks/`)) throw new Error("Expected isolated ParityChecks executable");
const compileCommandsPath = resolve(dirname(target), "compile_commands.json");
const compileCommand = JSON.parse(readFileSync(compileCommandsPath, "utf8"))
  .find((entry) => entry.file === resolve(here, "cpp-import-adapter.cpp"))?.command;
for (const flag of ["-std=c++26", "-ftrivial-auto-var-init=zero", "-fsanitize=address,undefined",
  "-fno-sanitize-recover=undefined", "_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST"]) {
  if (!compileCommand?.includes(flag)) throw new Error(`Missing adapter hardening flag ${flag}`);
}
const vectors = JSON.parse(readFileSync(args["--vectors"], "utf8"));
const selected = args["--case-ids"].split(",");
if (new Set(selected).size !== selected.length) throw new Error("Repeated case ID");
for (const id of selected) {
  const vector = vectors.vectors.find((v) => v.id === id);
  if (!vector) throw new Error(`Unknown vector ${id}`);
  if (vector.readyForExecution === false) throw new Error(`Vector implementation remains pending: ${id}`);
}
const sha = (bytes) => createHash("sha256").update(bytes).digest("hex");
const seeds = {
  "V001-noop": "two-tab-text-v1.json", "V002-body-and-title": "two-tab-text-v1.json",
  "V003-stale": "two-tab-text-v1.json", "V004-landed-lost-response-replay": "two-tab-text-v1.json",
  "V005-font-default": "font-readback-v1.json", "V006-font-nondefault": "font-readback-v1.json",
  "V007-cell-style-edit": "cell-style-v1.json", "V008-reject-hidden-insertion": "suggestion-v1.json",
  "V009-image-upload-lifecycle": "image-upload-v1.json", "V010-cross-doc-image-copy": "cross-doc-image-v1.json",
  "W2-rename-only": "w2-rename-v1.json", "W2-delayed-docs-title": "w2-rename-v1.json",
  "W2-rename-fail-after-content": "w2-rename-v1.json",
  "W1-create-success-replay": "w1-create-v1.json", "W1-create-rollback-retry": "w1-create-v1.json",
  "W1-create-rollback-rejected": "w1-create-v1.json", "W1-create-mismatch-readback": "w1-create-v1.json",
  "W1-create-unknown-outcome": "w1-create-v1.json", "W1-create-invalid-syntax": "w1-create-v1.json",
  "W3-reparent-order-noop": "w3-nested-tabs-v1.json", "W3-orphan-refusal": "w3-nested-tabs-v1.json",
  "W3-delete-subtree-and-all": "w3-nested-tabs-v1.json",
  "W4-comment-allow-and-actions": "w4-comments-v1.json",
  "W4-comment-ownership-and-readonly": "w4-comments-v1.json",
  "W4-comment-resolved-reopen": "w4-comments-v1.json",
  "W4-comment-partial-unknown-outcome": "w4-comments-v1.json",
};
const CASE_FAULTS = {
  "W4-comment-partial-unknown-outcome": "fail-second-comment-action",
  "W2-delayed-docs-title": "delayed-docs-title",
  "W2-rename-fail-after-content": "fail-rename-after-content",
  "W1-create-rollback-retry": "fail-content-after-create",
  "W1-create-rollback-rejected": "fail-content-and-delete",
  "W1-create-mismatch-readback": "mismatch-after-create",
  "W1-create-unknown-outcome": "lost-create-response",
};
const sourceConstraints = {
  "V001-noop": "OB-001; original engine/import.test.ts:451–463 asserts status/match/none/zero content+rename writes/same existing export path, not exact body whitespace or warning prose.",
  "V002-body-and-title": "OB-002; original engine/import.test.ts:512–533 asserts no errors/applied/match/content before final rename/export title and included body text, not exact body whitespace or summary prose.",
  "V003-stale": "OB-006; original engine/import.test.ts:569–583 asserts refused/staleBase/none/preserved input/distinct export/current revision/zero batches, not exact stale message/summary or recovery-directory layout.",
  "V004-landed-lost-response-replay": "OB-008; original engine/import.test.ts:410–429 asserts failed/unknown/distinct fresh export/staleBase replay/one batch. Original production import.ts:546 + actual executions require inconclusive verification after unknown write. Other summary/recovery layout exact bytes not asserted by that test.",
  "V005-font-default": "OB-009.11; original engine/import.test.ts:115–149 requires no errors/applied/match when omitted explicit font is inherited-equivalent 11pt.",
  "V006-font-nondefault": "OB-009.22; original engine/import.test.ts:115–149 requires failed/mismatch and landed-but-mismatched summary for omitted nondefault 22pt.",
  "V007-cell-style-edit": "OB-027; original engine/import.test.ts:1323–1351 and importClean:1205–1212 require declaration+marker export, absent errors/match and actual green remote cell.",
  "V008-reject-hidden-insertion": "OB-029; original engine/import.test.ts:330–350 requires hidden body text, exact review content, immutable-snippet refusal, then reject/no errors/match and removed content/review block.",
  "V009-image-upload-lifecycle": "OB-036; original engine/import.test.ts:1917–1947 asserts local/file/http inputs, one upload, three returned URIs, exact throwaway deletion/no warnings, aggregate invalid inputs and zero upload/batches; original import.ts:562–619 retains known failure.documentId for finally cleanup.",
  "V010-cross-doc-image-copy": "OB-034; original engine/import.test.ts:1811–1844 asserts actual exported source ID, one source read during import, owning contentUri recreation, unknown-image and unreadable-document errors; provider-ID/no-reinsert checks additionally protect integrated lifecycle.",
  "W2-rename-only": "OB-002; original engine/import.test.ts:476–554 asserts rename-only mutation with zero batchUpdate writes, unchanged protected tab, and updated remote/export title.",
  "W2-delayed-docs-title": "OB-002; confirmed Drive rename wins over lagged Docs getDocument title readback without duplicate writes.",
  "W2-rename-fail-after-content": "OB-002; partial failure after landed content returns failed/rename phase, non-none writeState, landed content, and preserves old title in export.",
};
function classifyDifference(path, caseId) {
  if (["V003-stale", "V004-landed-lost-response-replay"].includes(caseId) &&
      /^\$\.(result|replay)\.exportPath$/.test(path)) {
    return "intentional-equivalent-recovery-directory-layout-parent-approved; raw path retained; fresh/distinct/contained/no-clobber checks remain required";
  }
  if (/\.warnings(?:\.|$)/.test(path)) return "warning-payload-difference-unresolved; preserve cleanup/loss/local-edit instructions; no blanket variance approved";
  if (/\.summary$|\.errors\.\d+\.message$/.test(path)) return "wording-variance-proposal-unaccepted; retain exact instructions and semantics";
  if (/\.exportPath$/.test(path)) return "native-recovery-layout-variance-proposal-unaccepted; owned prefix only normalized";
  if (/\.markdown$|\.tabs\.\d+\.markdown$/.test(path)) return "native-file-format-difference-unresolved; no trim/drop/paragraph normalization; lens owner must prove source-linked equivalence or fix";
  if (/^\$\.callLog/.test(path)) return "external-call-schedule-or-payload-difference-unresolved; ordered calls retained";
  if (/^\$\.mutations/.test(path)) return "write-request-payload-or-order-difference-potential-defect";
  if (/^\$\.remote/.test(path)) return "provider-state-difference-potential-defect";
  return "observable-contract-difference-unresolved";
}
function fingerprint(root, directories, extra = []) {
  const files = [];
  function walk(dir) {
    for (const entry of readdirSync(resolve(root, dir), { withFileTypes: true })) {
      const path = `${dir}/${entry.name}`;
      if (entry.isDirectory()) walk(path);
      else if (/\.(ts|mjs|cpp|hpp|json)$/.test(path)) files.push(path);
    }
  }
  for (const dir of directories) walk(dir);
  files.push(...extra);
  return Object.fromEntries([...new Set(files)].sort().map((p) => [p, sha(readFileSync(resolve(root, p)))]));
}
const identity = () => ({
  original: fingerprint(original, ["src", "scripts"], ["package.json", "package-lock.json", "justfile"]),
  target: fingerprint(project, ["src", "tests/parity"], ["CMakeLists.txt", "justfile"]),
  vectors: sha(readFileSync(args["--vectors"])), executable: sha(readFileSync(target)),
});
const before = identity();
const newerThanBinary = Object.keys(before.target)
  .filter((path) => path.startsWith("src/") || path === "tests/parity/cpp-import-adapter.cpp" || path === "CMakeLists.txt")
  .filter((path) => lstatSync(resolve(project, path)).mtimeMs > lstatSync(target).mtimeMs);
const report = {
  schema: "engine-parity-results-v1", startedAt: new Date().toISOString(),
  runtime: { node: process.version, executableDigest: sha(readFileSync(process.execPath)) },
  dependencies: Object.fromEntries(["argsbarg", "yaml", "marked", "zod"].map((name) =>
    [name, JSON.parse(readFileSync(resolve(original, "node_modules", name, "package.json"), "utf8")).version])),
  build: {
    compileCommand: compileCommand.replaceAll(project, "<PROJECT>"),
    compileCommandsDigest: sha(readFileSync(compileCommandsPath)),
    cacheDigest: sha(readFileSync(resolve(dirname(target), "CMakeCache.txt"))),
    inputsNewerThanBinary: newerThanBinary,
  },
  commands: {
    target: relative(project, target), originalRoot: "../gdocsmith-node",
    arguments: { ...args, "--target-adapter": relative(project, target) },
  },
  identityBefore: before, cases: [], variants: [], controls: [],
  normalization: { ownedRunRootPrefix: "<RUN>", createdIdBijections: [], seededIdsNormalized: false,
    revisionSpellingApplied: false, versionNormalizationApplied: false,
    rawFileInterchange: "strict original Node parser receives unmodified actual C++ export before and after import; no version translation" },
};
const rawText = (doc) => (doc.tabs ?? []).map((tab) =>
  (tab.documentTab?.body?.content ?? []).flatMap((element) =>
    (element.paragraph?.elements ?? []).map((inline) => inline.textRun?.content ?? "")).join(""));
function strictRead(markdown) {
  try {
    if (typeof markdown !== "string") throw new Error("Missing raw exported markdown");
    const parsed = strictNodeParse(markdown);
    return { valid: true, version: parsed.gdocsmith, doc: parsed.doc, revision: parsed.revision };
  } catch (error) {
    return { valid: false, code: error.code ?? null, message: error.message };
  }
}
function check(observation, id) {
  const checks = [];
  const r = observation.result;
  const batches = observation.mutations.filter((m) => m.kind === "batchUpdate");
  const expect = (suffix, fn) => {
    try { fn(); checks.push({ id: suffix, pass: true }); }
    catch (error) { checks.push({ id: suffix, pass: false, message: error.message }); }
  };
  const clean = (prefix) => expect(`${prefix}:no-errors`, () => assert.equal(Object.hasOwn(r, "errors"), false));
  const match = (prefix) => expect(`${prefix}:verification`, () => assert.equal(r.verification, "match"));
  const textChanged = (prefix) => expect(`${prefix}:remote-body`, () => assert.ok(rawText(observation.remote)[0].includes("Hello revised world")));
  expect("shared:working-file-exists", () => assert.equal(observation.working.exists, true));
  if (!id.startsWith("W1-")) {
    expect("shared:seeded-document-id", () => assert.equal(observation.remote.documentId, "synthetic-doc"));
    if (!id.startsWith("W3-")) {
      expect("shared:seeded-tab-id", () => assert.equal(observation.remote.tabs[0].tabProperties.tabId, "t.0"));
    }
  }
  if (r.exportPath) expect("shared:export-contained", () => assert.ok(r.exportPath.startsWith("<RUN>/")));
  if (observation.side === "cpp") {
    expect("OB-043:strict-node-initial-export", () => assert.equal(strictRead(observation.initial.markdown).valid, true));
    if (observation.exported.exists) {
      expect("OB-043:strict-node-postimport-export", () => assert.equal(strictRead(observation.exported.markdown).valid, true));
    }
  }
  expect("shared:revision-lock-chain", () => {
    let revision = "rev-1";
    for (const batch of batches) {
      assert.equal(batch.requiredRevisionId, revision);
      if (batch.receiptRevision) revision = batch.receiptRevision;
    }
  });
  if (id === "V001-noop") {
    expect("OB-001:status", () => assert.equal(r.status, "unchanged"));
    expect("OB-001:write-state", () => assert.equal(r.writeState, "none"));
    match("OB-001");
    expect("OB-001:no-mutations", () => assert.equal(observation.mutations.length, 0));
    expect("OB-001:export-path", () => assert.equal(observation.exported.pathRole, "same"));
    expect("OB-001:export-exists", () => assert.equal(observation.exported.exists, true));
    expect("OB-001:remote-body", () => assert.equal(rawText(observation.remote)[0], "Hello world\n"));
  } else if (id === "V002-body-and-title") {
    clean("OB-002"); match("OB-002"); textChanged("OB-002");
    expect("OB-002:status", () => assert.equal(r.status, "applied"));
    expect("OB-002:content-before-rename", () => {
      assert.ok(batches.length > 0);
      assert.equal(observation.mutations.at(-1)?.kind, "rename");
      assert.ok(observation.mutations.findIndex((m) => m.kind === "batchUpdate") < observation.mutations.findIndex((m) => m.kind === "rename"));
    });
    expect("OB-002:remote-title", () => assert.equal(observation.remote.title, "Renamed"));
    expect("OB-002:export-title", () => assert.equal(observation.exported.title, "Renamed"));
    expect("OB-002:export-body", () => assert.ok(observation.exported.tabs[0].markdown.includes("Hello revised world")));
  } else if (id === "V003-stale") {
    expect("OB-006:status", () => assert.equal(r.status, "refused"));
    expect("OB-006:error-code", () => assert.equal(r.errors?.[0]?.code, "staleBase"));
    expect("OB-006:write-state", () => assert.equal(r.writeState, "none"));
    expect("OB-006:preserve-input", () => assert.equal(observation.working.unchanged, true));
    expect("OB-006:distinct-export", () => assert.equal(observation.exported.pathRole, "distinct"));
    expect("OB-006:fresh-revision", () => assert.equal(observation.exported.revision, observation.remote.revisionId));
    expect("OB-006:no-batches", () => assert.equal(batches.length, 0));
  } else if (id === "V004-landed-lost-response-replay") {
    expect("OB-008:status", () => assert.equal(r.status, "failed"));
    expect("OB-008:write-state", () => assert.equal(r.writeState, "unknown"));
    expect("OB-008:observed-original-verification", () => assert.equal(r.verification, "inconclusive"));
    expect("OB-008:distinct-export", () => assert.equal(observation.exported.pathRole, "distinct"));
    expect("OB-008:preserve-input", () => assert.equal(observation.working.unchanged, true));
    expect("OB-008:fresh-revision", () => assert.equal(observation.exported.revision, observation.remote.revisionId));
    expect("OB-008:replay-code", () => assert.equal(observation.replay?.errors?.[0]?.code, "staleBase"));
    expect("OB-008:single-batch", () => assert.equal(batches.length, 1));
    textChanged("OB-008");
  } else if (id === "V005-font-default" || id === "V006-font-nondefault") {
    const normal = id === "V005-font-default";
    expect(`OB-009.${normal ? 11 : 22}:status`, () => assert.equal(r.status, normal ? "applied" : "failed"));
    expect(`OB-009.${normal ? 11 : 22}:verification`, () => assert.equal(r.verification, normal ? "match" : "mismatch"));
    if (normal) clean("OB-009.11");
    else {
      expect("OB-009.22:summary", () => assert.match(r.summary, /^Every write landed, but the readback did not match the file/));
      expect("OB-009.22:observed-original-write-state", () => assert.equal(r.writeState, "complete"));
      expect("OB-009.22:observed-original-no-recovery", () => assert.equal(Object.hasOwn(r, "exportPath"), false));
    }
    textChanged("OB-009");
  } else if (id === "V007-cell-style-edit") {
    clean("OB-027"); match("OB-027");
    expect("OB-027:initial-declaration", () => assert.equal(observation.initial.hasCellStyleDeclaration, true));
    expect("OB-027:remote-green", () => {
      const table = observation.remote.tabs[0].documentTab.body.content.find((e) => e.table).table;
      assert.equal(table.tableRows[0].tableCells[0].tableCellStyle.backgroundColor.color.rgbColor.green, 1);
    });
  } else if (id === "V008-reject-hidden-insertion") {
    expect("OB-029:hidden-in-tab", () => assert.ok(!observation.initial.tabs[0].markdown.includes("Hello world")));
    expect("OB-029:review-block", () => assert.equal(observation.initial.hasSuggestionBlock, true));
    expect("OB-029:review-snippet", () => assert.equal(observation.initial.hasInsertedSnippet, true));
    expect("OB-043:synthetic-comment-present", () => assert.equal(observation.initial.hasCommentBlock, true));
    expect("OB-043:synthetic-reply-present", () => assert.equal(observation.initial.hasSyntheticReply, true));
    expect("OB-029:snippet-refused", () => assert.equal(observation.snippetResult?.status, "refused"));
    expect("OB-029:snippet-error", () => assert.match(observation.snippetResult?.errors?.[0]?.message ?? "", /edit the tab body, not a suggestion block/));
    clean("OB-029"); match("OB-029");
    expect("OB-029:remote-insertion-removed", () => assert.ok(!JSON.stringify(observation.remote).includes("Hello world")));
    expect("OB-029:review-removed", () => assert.equal(observation.exported.hasSuggestionBlock, false));
  } else if (id === "V009-image-upload-lifecycle" || id === "V010-cross-doc-image-copy") {
    const upload = id === "V009-image-upload-lifecycle";
    const prefix = upload ? "OB-036" : "OB-034";
    const images = observation.images;
    const cleanup = () => {
      assert.equal(images.temporaryDocuments.length, 1);
      assert.equal(images.uploadReceipts.length, 1);
      assert.ok(images.temporaryDocuments.every((entry) => entry.actualDeleteMethodCompleted));
      if (observation.side === "cpp") assert.ok(images.temporaryDocuments.every((entry) => !entry.backendExists));
      assert.deepEqual(images.deletions.map((entry) => entry.id).sort(),
        images.uploadReceipts.map((entry) => entry.documentId).sort());
    };
    if (upload && observation.fault === "known-upload-error") {
      expect("OB-036.known-id:error", () => assert.ok(r.errors.length > 0));
      expect("OB-036.known-id:status", () => assert.equal(r.status, "failed"));
      expect("OB-036.known-id:observed-original-write-state", () => assert.equal(r.writeState, "unknown"));
      expect("OB-036.known-id:observed-original-verification", () => assert.equal(r.verification, "inconclusive"));
      expect("OB-036.known-id:no-document-write", () => assert.equal(batches.length, 0));
      expect("OB-036.known-id:exact-cleanup", cleanup);
      return [...checks, ...safetyAssertions(observation, id, strictNodeParse)];
    }
    clean(prefix); match(prefix);
    expect(`${prefix}:status`, () => assert.equal(r.status, "applied"));
    expect(`${prefix}:noop-replay`, () => assert.equal(observation.replay?.status, "unchanged"));
    expect(`${prefix}:no-reinsert`, () => assert.equal(images.insertionsAfterReplay, images.insertionsBeforeReplay));
    expect(`${prefix}:provider-id-retention`, () => assert.deepEqual(images.providerIdsAfterReplay, images.providerIdsBeforeReplay));
    expect(`${prefix}:provider-id-links`, () => {
      assert.equal(images.providerIdsAfterReplay.length, upload ? 3 : 1);
      assert.equal(new Set(images.providerIdsAfterReplay).size, images.providerIdsAfterReplay.length);
      const objects = observation.remote.tabs[0].documentTab.inlineObjects;
      for (const objectId of images.providerIdsAfterReplay) assert.ok(objects[objectId]);
    });
    const inserted = batches.flatMap((batch) => batch.requests).flatMap((request) =>
      request.insertInlineImage ? [request.insertInlineImage.uri] : []);
    if (upload) {
      expect("OB-036:preflight-no-send", () => {
        assert.equal(images.preflight.result.status, "refused");
        assert.equal(images.preflight.uploads, 0);
        assert.equal(images.preflight.batches, 0);
        const messages = images.preflight.result.errors.map((error) => error.message).join("\n");
        assert.match(messages, /not\.png isn't a PNG, JPEG or GIF/);
        assert.match(messages, /missing\.png can't be read/);
      });
      expect("OB-036:one-upload-three-uris", () => {
        const receipts = images.uploadReceipts.filter((receipt) => receipt.phase === "positive");
        assert.equal(receipts.length, 1);
        assert.equal(inserted.length, 3);
        for (const uri of inserted) assert.ok(receipts[0].contentUris.includes(uri));
        assert.equal(images.fetched.filter((entry) => entry.phase === "positive").length, 1);
      });
      expect("OB-036:temporary-documents-deleted", cleanup);
      expect("OB-036:no-warnings", () => assert.equal(r.warnings.length, 0));
    } else {
      expect("OB-034:source-read-once", () => assert.equal(images.sourceReadsAfterImport - images.sourceReadsBeforeImport, 1));
      expect("OB-034:owning-uri", () => assert.deepEqual(inserted, ["https://synthetic.googleusercontent.invalid/source-image"]));
      expect("OB-034:unknown-source-errors", () => {
        assert.equal(images.imageErrors.length, 2);
        for (const entry of images.imageErrors) {
          assert.equal(entry.result.status, "refused");
          assert.equal(entry.mutations, 0);
        }
        assert.match(images.imageErrors[0].result.errors[0].message, /no single image with that id in that document/);
        assert.match(images.imageErrors[1].result.errors[0].message, /document missing-source couldn't be read/);
      });
    }
  }
  if (!id.startsWith("W1-") && observation.remote?.tabs?.length === 2) {
    expect("shared:second-tab-preserved", () => assert.equal(rawText(observation.remote)[1], "Protected by omission\n"));
  }
  return [...checks, ...safetyAssertions(observation, id, strictNodeParse)];
}
function execute(side, caseId, fault = "none") {
  const directory = resolve(runs, `run-${randomUUID()}`);
  mkdirSync(directory, { mode: 0o700 });
  writeFileSync(resolve(directory, ".parity-owned"), "synthetic-parity-run-v1", { mode: 0o600 });
  mkdirSync(resolve(directory, "tmp"), { mode: 0o700 });
  const payload = {
    caseId, fault, workRoot: relative(project, directory),
    originalRoot: original, seedPath: resolve(here, "seeds", seeds[caseId]),
  };
  let outcome;
  try {
    const env = {
      ...process.env, BUGSINK_DSN: "", GDOCSMITH_WORK_DIR: resolve(directory, "work"),
      TMPDIR: resolve(directory, "tmp"), TMP: resolve(directory, "tmp"), TEMP: resolve(directory, "tmp"),
      ASAN_OPTIONS: "abort_on_error=1:halt_on_error=1", UBSAN_OPTIONS: "halt_on_error=1:print_stacktrace=1",
    };
    const child = spawnSync(side === "node" ? process.execPath : target,
      side === "node" ? [resolve(here, "node-import-adapter.mjs")] : [],
      { input: JSON.stringify(payload), env, encoding: "utf8", timeout: 120000, maxBuffer: 8 * 1024 * 1024, cwd: project });
    if (child.error || child.status !== 0) {
      outcome = { execution: "failed", exitStatus: child.status, signal: child.signal,
        message: String(child.error?.message ?? child.stderr).replaceAll(directory, "<RUN>").slice(0, 2500) };
    } else {
      const observation = JSON.parse(child.stdout.replaceAll(directory, "<RUN>"));
      if (side === "cpp") observation.strictNodeInterchange = {
        initial: strictRead(observation.initial.markdown), postimport: strictRead(observation.exported.markdown),
      };
      outcome = { execution: "completed", exitStatus: child.status, signal: child.signal,
        observation, assertions: check(observation, caseId) };
    }
  } finally {
    // Record cleanup instead of throwing so the child outcome and report survive; keep the directory on a marker mismatch
    let cleanup;
    try {
      if (readFileSync(resolve(directory, ".parity-owned"), "utf8") !== "synthetic-parity-run-v1") {
        cleanup = { status: "failed", reason: "ownership marker changed; directory kept" };
      } else {
        rmSync(directory, { recursive: true, force: false });
        cleanup = existsSync(directory) ? { status: "failed", reason: "directory still exists after removal" } : { status: "removed" };
      }
    } catch (error) {
      cleanup = { status: "failed", reason: String(error?.message).replaceAll(directory, "<RUN>").slice(0, 500) };
    }
    if (outcome) outcome.cleanup = cleanup;
  }
  return outcome;
}
function differences(a, b, path = "$", out = []) {
  if (Object.is(a, b)) return out;
  if (a === null || b === null || typeof a !== "object" || typeof b !== "object" || Array.isArray(a) !== Array.isArray(b)) {
    out.push({ path, original: a === undefined ? { absent: true } : a, target: b === undefined ? { absent: true } : b });
  } else {
    for (const key of [...new Set([...Object.keys(a), ...Object.keys(b)])].sort()) differences(a[key], b[key], `${path}.${key}`, out);
  }
  return out;
}
function observationControl(side, caseId, original, name, mutate, requiredAssertion) {
  const changed = structuredClone(original);
  mutate(changed);
  const baselineWasPassing = check(original, caseId).find((a) => a.id === requiredAssertion)?.pass === true;
  const failed = check(changed, caseId).filter((a) => !a.pass).map((a) => a.id);
  report.controls.push({ side, caseId, name, type: "observation-corruption",
    requiredAssertion, baselineWasPassing, rejectedBy: failed, detected: baselineWasPassing && failed.includes(requiredAssertion) });
}
for (const caseId of selected) {
  const vector = vectors.vectors.find((v) => v.id === caseId);
  const fault = vector?.fault ?? CASE_FAULTS[caseId] ?? "none";
  const node = execute("node", caseId, fault);
  const cpp = execute("cpp", caseId, fault);
  const differential = node.observation && cpp.observation ? differences(
    { result: node.observation.result, replay: node.observation.replay, snippetResult: node.observation.snippetResult,
      remote: node.observation.remote, remoteStored: node.observation.remoteStored,
      mutations: node.observation.mutations, exported: node.observation.exported,
      initial: node.observation.initial, callLog: node.observation.callLog, images: node.observation.images },
    { result: cpp.observation.result, replay: cpp.observation.replay, snippetResult: cpp.observation.snippetResult,
      remote: cpp.observation.remote, remoteStored: cpp.observation.remoteStored,
      mutations: cpp.observation.mutations, exported: cpp.observation.exported,
      initial: cpp.observation.initial, callLog: cpp.observation.callLog, images: cpp.observation.images }) : null;
  report.cases.push({ caseId, node, cpp, differential });
  if (caseId === "V009-image-upload-lifecycle") {
    const nodeVariant = execute("node", caseId, "known-upload-error");
    const cppVariant = execute("cpp", caseId, "known-upload-error");
    report.variants.push({ caseId, variant: "known-upload-error", node: nodeVariant, cpp: cppVariant,
      differential: nodeVariant.observation && cppVariant.observation ? differences(
        { result: nodeVariant.observation.result, images: nodeVariant.observation.images, mutations: nodeVariant.observation.mutations,
          initial: nodeVariant.observation.initial, exported: nodeVariant.observation.exported, remote: nodeVariant.observation.remote, callLog: nodeVariant.observation.callLog },
        { result: cppVariant.observation.result, images: cppVariant.observation.images, mutations: cppVariant.observation.mutations,
          initial: cppVariant.observation.initial, exported: cppVariant.observation.exported, remote: cppVariant.observation.remote, callLog: cppVariant.observation.callLog }) : null });
    for (const [side, entry] of [["node", nodeVariant], ["cpp", cppVariant]]) {
      if (!entry.observation) continue;
      for (const [name, assertion, mutate] of safetyControls(caseId, entry.observation.fault)) {
        observationControl(side, caseId, entry.observation, name, mutate, assertion);
        report.controls.at(-1).variant = "known-upload-error";
      }
    }
  }
  if (node.observation?.w1 && cpp.observation?.w1) {
    const originalIds = node.observation.w1.createdIds ?? [];
    const targetIds = cpp.observation.w1.createdIds ?? [];
    report.normalization.createdIdBijections.push({ caseId, kind: "w1-created-document", appliedToComparison: false,
      valid: originalIds.length === targetIds.length && new Set(originalIds).size === originalIds.length
        && new Set(targetIds).size === targetIds.length,
      entries: originalIds.map((original, index) => ({ original, target: targetIds[index] ?? null })) });
  }
  if (node.observation?.images && cpp.observation?.images) {
    const imageIds = (observation) => observation.mutations.flatMap((entry) => entry.replies ?? [])
      .flatMap((reply) => reply.insertInlineImage?.objectId ? [reply.insertInlineImage.objectId] : []);
    for (const [kind, originalIds, targetIds] of [
      ["upload-document", node.observation.images.uploadReceipts.map((r) => r.documentId), cpp.observation.images.uploadReceipts.map((r) => r.documentId)],
      ["inline-image", imageIds(node.observation), imageIds(cpp.observation)],
    ]) {
      report.normalization.createdIdBijections.push({ caseId, kind, appliedToComparison: false,
        valid: originalIds.length === targetIds.length && new Set(originalIds).size === originalIds.length
          && new Set(targetIds).size === targetIds.length,
        entries: originalIds.map((original, index) => ({ original, target: targetIds[index] ?? null })) });
    }
  }
  console.log(`${caseId}: Node ${node.execution}, C++ ${cpp.execution}, differences ${differential?.length ?? "unavailable"}`);
  for (const [side, entry] of [["node", node], ["cpp", cpp]]) {
    if (!entry.observation) continue;
    for (const [name, assertion, mutate] of safetyControls(caseId, entry.observation.fault)) {
      observationControl(side, caseId, entry.observation, name, mutate, assertion);
    }
    if (side === "cpp") {
      observationControl(side, caseId, entry.observation, "wrong-version-strict-node-parser", (o) => {
        o.initial.markdown = o.initial.markdown.replace(/^gdocsmith:[^\n]*$/m, "gdocsmith: invalid-parity-version");
      }, "OB-043:strict-node-initial-export");
      report.controls.at(-1).actualOriginalParserInvoked = true;
    }
    if (caseId === "V001-noop") {
      observationControl(side, caseId, entry.observation, "extra-write", (o) => o.mutations.push({ kind: "batchUpdate", requiredRevisionId: "rev-1" }), "OB-001:no-mutations");
      observationControl(side, caseId, entry.observation, "missing-export", (o) => { o.exported.exists = false; }, "OB-001:export-exists");
    }
    if (caseId === "V002-body-and-title") {
      observationControl(side, caseId, entry.observation, "wrong-order", (o) => o.mutations.reverse(), "OB-002:content-before-rename");
      for (const fault of ["ack-without-mutation", "race-before-send"]) {
        const control = execute(side, caseId, fault);
        const failed = control.assertions?.filter((a) => !a.pass).map((a) => a.id) ?? [];
        const requiredAssertion = fault === "ack-without-mutation" ? "OB-002:remote-body" : "OB-002:status";
        const baselineWasPassing = entry.assertions.find((a) => a.id === requiredAssertion)?.pass === true;
        report.controls.push({ side, caseId, name: fault, type: "backend-behavior", requiredAssertion,
          baselineWasPassing,
          reached: control.observation?.faultCalls > 0, rejectedBy: failed,
          detected: baselineWasPassing && control.observation?.faultCalls > 0 && failed.includes(requiredAssertion), run: control });
      }
    }
    if (caseId === "W1-create-rollback-retry") {
      const control = execute(side, caseId, "ack-delete-without-mutation");
      const failed = control.assertions?.filter((a) => !a.pass).map((a) => a.id) ?? [];
      const requiredAssertion = "w1:rollback-confirmed-before-retry";
      const baselineWasPassing = entry.assertions.find((a) => a.id === requiredAssertion)?.pass === true;
      report.controls.push({ side, caseId, name: "ack-delete-without-mutation", type: "backend-behavior", requiredAssertion,
        baselineWasPassing,
        reached: control.observation?.faultCalls > 0, rejectedBy: failed,
        detected: baselineWasPassing && control.observation?.faultCalls > 0 && failed.includes(requiredAssertion), run: control });
    }
    if (caseId === "V003-stale") observationControl(side, caseId, entry.observation, "flatten-refusal", (o) => { o.result.status = "failed"; }, "OB-006:status");
    if (caseId === "V004-landed-lost-response-replay") {
      observationControl(side, caseId, entry.observation, "unknown-as-complete", (o) => { o.result.writeState = "complete"; }, "OB-008:write-state");
      observationControl(side, caseId, entry.observation, "replayed-batch", (o) => o.mutations.push({ kind: "batchUpdate", requiredRevisionId: "rev-1" }), "OB-008:single-batch");
    }
    if (caseId === "V006-font-nondefault") observationControl(side, caseId, entry.observation, "all-font-omissions-equivalent", (o) => { o.result.verification = "match"; }, "OB-009.22:verification");
    if (caseId === "V007-cell-style-edit" || caseId === "V008-reject-hidden-insertion") {
      const prefix = caseId === "V007-cell-style-edit" ? "OB-027" : "OB-029";
      observationControl(side, caseId, entry.observation, "refusal-is-not-success", (o) => {
        o.result = { status: "refused", verification: "notApplicable", errors: [{ code: "unsupportedSyntax", message: "unsupported" }] };
      }, `${prefix}:no-errors`);
    }
    if (caseId === "V009-image-upload-lifecycle" || caseId === "V010-cross-doc-image-copy") {
      const prefix = caseId.startsWith("V009") ? "OB-036" : "OB-034";
      observationControl(side, caseId, entry.observation, "provider-id-not-retained", (o) => {
        o.images.providerIdsAfterReplay.push("synthetic-wrong-created-id");
      }, `${prefix}:provider-id-retention`);
      observationControl(side, caseId, entry.observation, "image-reinserted-on-noop", (o) => {
        o.images.insertionsAfterReplay++;
      }, `${prefix}:no-reinsert`);
      if (caseId.startsWith("V009")) {
        observationControl(side, caseId, entry.observation, "bad-input-sent", (o) => {
          o.images.preflight.batches++;
        }, "OB-036:preflight-no-send");
        const control = execute(side, caseId, "leak-upload-document");
        const requiredAssertion = "OB-036:temporary-documents-deleted";
        const baselineWasPassing = entry.assertions.find((assertion) => assertion.id === requiredAssertion)?.pass === true;
        const rejectedBy = control.assertions?.filter((assertion) => !assertion.pass).map((assertion) => assertion.id) ?? [];
        report.controls.push({ side, caseId, name: "leak-known-throwaway-document", type: "backend-behavior",
          requiredAssertion, baselineWasPassing, reached: control.observation?.faultCalls > 0, rejectedBy,
          detected: baselineWasPassing && control.observation?.faultCalls > 0 && rejectedBy.includes(requiredAssertion), run: control });
      } else {
        observationControl(side, caseId, entry.observation, "duplicate-source-fetch", (o) => {
          o.images.sourceReadsAfterImport++;
        }, "OB-034:source-read-once");
        observationControl(side, caseId, entry.observation, "generic-foreign-refusal", (o) => {
          if (o.images.imageErrors[0]?.result.errors?.[0]) o.images.imageErrors[0].result.errors[0].message = "unsupported";
        }, "OB-034:unknown-source-errors");
      }
    }
    if (caseId === "W2-rename-only") {
      const fault = "ack-rename-without-mutation";
      const control = execute(side, caseId, fault);
      const failed = control.assertions?.filter((a) => !a.pass).map((a) => a.id) ?? [];
      const requiredAssertion = "w2:rename-only-remote-title";
      const baselineWasPassing = entry.assertions.find((a) => a.id === requiredAssertion)?.pass === true;
      report.controls.push({
        side, caseId, name: fault, type: "backend-behavior", requiredAssertion,
        baselineWasPassing,
        reached: control.observation?.faultCalls > 0, rejectedBy: failed,
        detected: baselineWasPassing && control.observation?.faultCalls > 0 && failed.includes(requiredAssertion), run: control,
      });
    }
    if (caseId === "W3-reparent-order-noop") {
      const fault = "ack-move-without-mutation";
      const control = execute(side, caseId, fault);
      const failed = control.assertions?.filter((a) => !a.pass).map((a) => a.id) ?? [];
      const requiredAssertion = "w3:reparent-order-hierarchy";
      const baselineWasPassing = entry.assertions.find((a) => a.id === requiredAssertion)?.pass === true;
      report.controls.push({
        side, caseId, name: fault, type: "backend-behavior", requiredAssertion,
        baselineWasPassing,
        reached: control.observation?.faultCalls > 0, rejectedBy: failed,
        detected: baselineWasPassing && control.observation?.faultCalls > 0 && failed.includes(requiredAssertion), run: control,
      });
    }
    if (caseId === "W4-comment-allow-and-actions") {
      const fault = "ack-comment-without-mutation";
      const control = execute(side, caseId, fault);
      const failed = control.assertions?.filter((a) => !a.pass).map((a) => a.id) ?? [];
      const requiredAssertion = "w4:allow-and-actions";
      const baselineWasPassing = entry.assertions.find((a) => a.id === requiredAssertion)?.pass === true;
      report.controls.push({
        side, caseId, name: fault, type: "backend-behavior", requiredAssertion,
        baselineWasPassing,
        reached: control.observation?.faultCalls > 0, rejectedBy: failed,
        detected: baselineWasPassing && control.observation?.faultCalls > 0 && failed.includes(requiredAssertion), run: control,
      });
    }
  }
}
report.identityAfter = identity();
report.frozenInputs = JSON.stringify(report.identityBefore) === JSON.stringify(report.identityAfter);
report.compiledInputsCurrent = newerThanBinary.length === 0;
report.completedAt = new Date().toISOString();
const executed = [...report.cases, ...report.variants];
report.summary = {
  cases: report.cases.length,
  variants: report.variants.length,
  originalAssertionFailures: executed.flatMap((c) => c.node.assertions ?? []).filter((a) => !a.pass).length,
  targetAssertionFailures: executed.flatMap((c) => c.cpp.assertions ?? []).filter((a) => !a.pass).length,
  launchFailures: executed.flatMap((c) => [c.node, c.cpp]).filter((r) => r.execution !== "completed").length,
  differingCases: executed.filter((c) => c.differential === null || c.differential.length > 0).length,
  controlsDetected: report.controls.filter((c) => c.detected).length,
  controlsTotal: report.controls.length,
  observationControls: report.controls.filter((c) => c.type === "observation-corruption").length,
  backendControls: report.controls.filter((c) => c.type === "backend-behavior").length,
  ownedRunDirectoriesRemaining: readdirSync(runs).filter((name) => name.startsWith("run-")).length,
};
const expectedQualification = vectors.schema === "confirmation-engine-vectors-v1"
  ? { caseIds: CONFIRMATION_CASE_IDS, variants: CONFIRMATION_VARIANTS, controls: CONFIRMATION_CONTROLS }
  : { caseIds: EXPECTED_CASE_IDS, variants: EXPECTED_VARIANTS, controls: EXPECTED_CONTROLS };
report.qualification = qualify(report, expectedQualification);
writeFileSync(resultsPath, JSON.stringify(report, null, 2), { mode: 0o600, flag: "wx" });
const grouped = new Map();
for (const item of executed) {
  for (const difference of item.differential ?? []) {
    const key = JSON.stringify([difference, item.variant ?? null]);
    if (!grouped.has(key)) grouped.set(key, { ...difference, classification: classifyDifference(difference.path, item.caseId), cases: [], sourceConstraints: [] });
    const group = grouped.get(key);
    group.cases.push(item.variant ? `${item.caseId}/${item.variant}` : item.caseId);
    group.sourceConstraints.push(sourceConstraints[item.caseId]);
  }
}
const rawDiffPath = ownedPath(relative(project, resultsPath.replace(/\.json$/, "") + "-raw-diffs.json"));
if (existsSync(rawDiffPath) && lstatSync(rawDiffPath).isSymbolicLink()) throw new Error("Raw-diff path is a symlink");
writeFileSync(rawDiffPath, JSON.stringify({
  schema: "engine-parity-all-raw-differences-v1", sourceResult: relative(project, resultsPath),
  completedAt: report.completedAt, frozenInputs: report.frozenInputs, compiledInputsCurrent: report.compiledInputsCurrent,
  allValuesAreSynthetic: true, onlyOwnedRunPrefixSanitized: true,
  acceptedVariances: [{
    authority: "parent-after-historical-22-differences",
    cases: ["V003-stale", "V004-landed-lost-response-replay"],
    paths: ["$.result.exportPath", "$.replay.exportPath"],
    disposition: "intentional equivalent recovery directory layout only; raw paths retained, no nonce normalization",
    required: "distinct fresh contained export/current revision/content/preserved input and retained edits",
  }],
  totalLeafDifferences: executed.reduce((count, item) => count + (item.differential?.length ?? 0), 0),
  groupedExactPathAndValues: [...grouped.values()],
}, null, 2), { mode: 0o600, flag: "wx" });
console.log(JSON.stringify({ summary: report.summary, qualification: report.qualification }));
process.exitCode = report.qualification.pass ? 0 : 1;
}
