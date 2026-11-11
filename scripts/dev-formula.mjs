#!/usr/bin/env node
/**
 * Stage or restore the local Homebrew formula for `just install-local` / `just reinstall-local`.
 * `install` — back up Formula/gdocsmith-mcp.rb, pack build/bin/gdocsmith-mcp and docs/ into a tarball, and point Formula/gdocsmith-mcp.rb at file://.
 * `reset`   — restore Formula/gdocsmith-mcp.rb from backup and remove staging artifacts.
 */

import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const KEY = "gdocsmith-mcp";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const stagingDir = path.join(root, "Formula", ".staging");
const archivePath = path.join(stagingDir, `${KEY}-local.tar.gz`);
const formulaPath = path.join(root, "Formula", `${KEY}.rb`);
const backupPath = path.join(stagingDir, `${KEY}.rb.bak`);
const binaryPath = path.join(root, "build", "bin", KEY);
const docsPath = path.join(root, "docs");

const cmd = process.argv[2];
if (cmd !== "install" && cmd !== "reset") {
  process.stderr.write("Usage: node scripts/dev-formula.mjs <install|reset>\n");
  process.exit(1);
}

if (cmd === "install") {
  if (!existsSync(binaryPath)) {
    process.stderr.write(`Missing release binary: ${binaryPath} (run 'just build' first)\n`);
    process.exit(1);
  }
  if (!existsSync(formulaPath)) {
    process.stderr.write(`Missing formula: ${formulaPath}\n`);
    process.exit(1);
  }
  if (!existsSync(docsPath)) {
    process.stderr.write(`Missing docs directory: ${docsPath}\n`);
    process.exit(1);
  }

  mkdirSync(stagingDir, { recursive: true });
  if (!existsSync(backupPath)) {
    copyFileSync(formulaPath, backupPath);
  }

  execFileSync("tar", ["-czf", archivePath, "-C", path.dirname(binaryPath), KEY, "-C", root, "docs"], { stdio: "inherit" });
  const sha256 = createHash("sha256").update(readFileSync(archivePath)).digest("hex");

  const original = readFileSync(backupPath, "utf8");
  const staged = original
    .replace(/^\s*url\s+"[^"]*"/m, `  url "file://${archivePath}"`)
    .replace(/^\s*sha256\s+"[^"]*"/m, `  sha256 "${sha256}"`);

  if (staged === original) {
    process.stderr.write(`Failed to rewrite url/sha256 in ${formulaPath}\n`);
    process.exit(1);
  }
  writeFileSync(formulaPath, staged, "utf8");
  console.log(`Staged local dev formula: ${formulaPath}`);
} else {
  if (existsSync(backupPath)) {
    copyFileSync(backupPath, formulaPath);
    rmSync(backupPath, { force: true });
    console.log(`Restored ${formulaPath}`);
  }
  rmSync(archivePath, { force: true });
  rmSync(stagingDir, { recursive: true, force: true });
}
