import { createHash } from "node:crypto";
import { existsSync, lstatSync, mkdirSync, readFileSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, relative, resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";

process.umask(0o077);
const input = JSON.parse(readFileSync(0, "utf8"));
const cases = new Set([
  "V001-noop", "V002-body-and-title", "V003-stale", "V004-landed-lost-response-replay",
  "V005-font-default", "V006-font-nondefault", "V007-cell-style-edit", "V008-reject-hidden-insertion",
  "V009-image-upload-lifecycle", "V010-cross-doc-image-copy",
  "W2-rename-only", "W2-delayed-docs-title", "W2-rename-fail-after-content",
  "W1-create-success-replay", "W1-create-rollback-retry", "W1-create-rollback-rejected",
  "W1-create-mismatch-readback", "W1-create-unknown-outcome", "W1-create-invalid-syntax",
  "W3-reparent-order-noop", "W3-orphan-refusal", "W3-delete-subtree-and-all",
  "W4-comment-allow-and-actions", "W4-comment-ownership-and-readonly",
  "W4-comment-resolved-reopen", "W4-comment-partial-unknown-outcome",
]);
const faults = new Set([
  "none", "ack-without-mutation", "race-before-send", "leak-upload-document", "known-upload-error",
  "delayed-docs-title", "fail-rename-after-content", "ack-rename-without-mutation",
  "fail-content-after-create", "fail-content-and-delete", "mismatch-after-create",
  "lost-create-response", "ack-delete-without-mutation", "ack-move-without-mutation",
  "fail-second-comment-action", "ack-comment-without-mutation",
]);
if (!cases.has(input.caseId) || !faults.has(input.fault ?? "none")) throw new Error("Unknown case/fault");
const root = realpathSync(input.workRoot);
const cwd = realpathSync(process.cwd());
if (isAbsolute(input.workRoot) || root !== resolve(input.workRoot)
  || relative(cwd, root).startsWith(`..${sep}`) || root === cwd
  || !existsSync(resolve(root, ".parity-owned")) || (lstatSync(root).mode & 0o077) !== 0) {
  throw new Error("Expected private canonical project-relative owned run root");
}
const original = realpathSync(input.originalRoot);
const load = (path) => import(pathToFileURL(resolve(original, path)).href);
const [{ FakeGoogle }, { fileExport }, { fileImport }, { documentFileParse }] = await Promise.all([
  load("src/core/emulator/fakeGoogle.ts"), load("src/core/engine/export.ts"),
  load("src/core/engine/import.ts"), load("src/core/lens/document.ts"),
]);
const sha = (text) => createHash("sha256").update(text).digest("hex");
const seedBytes = readFileSync(input.seedPath, "utf8");
let seed = JSON.parse(seedBytes);
const sourceSeed = seed.source;
const reviewComments = seed.comments ?? [];
if (seed.document) seed = seed.document;
if (seed.target) seed = seed.target;
if (seed.variants) seed = seed.variants[input.caseId === "V005-font-default" ? "11" : "22"];
const isW1 = input.caseId.startsWith("W1-");
const google = new FakeGoogle();
if (!isW1) {
  google.seedDocument("synthetic-doc", seed, { revision: 1 });
  if (reviewComments.length) google.seedComments("synthetic-doc", reviewComments);
  if (sourceSeed) google.seedDocument("synthetic-source", sourceSeed, { revision: 1 });
}
const fetched = [];
const uploadReceipts = [];
const deletions = [];
const completedDeletes = new Set();
const createdIds = [];
let phase = input.fault === "known-upload-error" ? "known-upload-failure" : "positive";
const png = (width, height) => {
  const bytes = Buffer.alloc(33);
  Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]).copy(bytes);
  bytes.writeUInt32BE(13, 8);
  bytes.write("IHDR", 12, "latin1");
  bytes.writeUInt32BE(width, 16);
  bytes.writeUInt32BE(height, 20);
  return bytes;
};
const services = {
  apiUser: "parity@example.test", client: google, drive: google, root: resolve(root, "work"),
  fetch: async () => { throw new Error("Network fetch prohibited by parity adapter"); },
};
let inImagesUpload = 0;
if (input.caseId === "V009-image-upload-lifecycle" || isW1) {
  if (input.caseId === "V009-image-upload-lifecycle") {
    services.fetch = async (url) => {
      fetched.push({ phase, url });
      return new Response(png(10, 10), { headers: { "content-type": "image/png" } });
    };
    const upload = google.imagesUpload.bind(google);
    google.imagesUpload = async (name, images) => {
      inImagesUpload++;
      let receipt;
      try {
        receipt = await upload(name, images);
      } finally {
        inImagesUpload--;
      }
      uploadReceipts.push({ ...receipt, phase, images: images.map((image) => ({
        type: image.type, widthPx: image.widthPx, heightPx: image.heightPx, bytesDigest: sha(image.bytes),
      })) });
      if (phase === "known-upload-failure") throw Object.assign(new Error("synthetic upload response failure"), { documentId: receipt.documentId });
      return receipt;
    };
  }
  const remove = google.deleteFile.bind(google);
  google.deleteFile = async (id) => {
    deletions.push({ phase, id });
    if (input.fault === "fail-content-and-delete") {
      faultCalls++;
      google.callLog.push({ method: "deleteFile", args: [id] });
      throw new Error("delete rejected");
    }
    if (input.fault === "leak-upload-document" || input.fault === "ack-delete-without-mutation") {
      faultCalls++;
      google.callLog.push({ method: "deleteFile", args: [id] });
      return;
    }
    await remove(id);
    completedDeletes.add(id);
  };
}
let sourceToken;
if (sourceSeed) {
  const sourceExport = await fileExport("synthetic-source", services);
  sourceToken = /:image(?:\[[^\]]*\])?\{id=([^ }]+)\}/.exec(readFileSync(sourceExport.exportPath, "utf8"))?.[1];
  if (!sourceToken) throw new Error("Source image not exported");
}
let path;
let initialBytes;
if (isW1) {
  mkdirSync(services.root, { recursive: true, mode: 0o700 });
  path = resolve(services.root, "authored.md");
  let body = "";
  if (input.caseId === "W1-create-success-replay") {
    body = seed.tabs.map((tab, i) => `::tab{id=tab-${i} title=${tab.title}}\n\n${tab.body}\n`).join("\n");
  } else if (input.caseId === "W1-create-invalid-syntax") {
    body = `${seed.invalidBody}\n`;
  } else {
    body = `${seed.singleBody}\n`;
  }
  initialBytes = `---\ngdocsmith: 3.0.20\ndoc: null\nrevision: null\ntitle: ${seed.title}\n---\n\n${body}`;
  writeFileSync(path, initialBytes);
} else {
  const exported = await fileExport("synthetic-doc", services, {
    commentsIncludeResolved: input.caseId === "W4-comment-resolved-reopen",
  });
  path = exported.exportPath;
  initialBytes = readFileSync(path, "utf8");
}
let initialFile;
try {
  initialFile = documentFileParse(initialBytes);
} catch {}
let edited = initialBytes;
let snippetResult;
let faultCalls = 0;
let commentSends = 0;
let injectedContentFailure = false;
let wrote = false;
const mutations = [];
const create = google.createDocument.bind(google);
google.createDocument = async (title) => {
  if (!isW1 && inImagesUpload === 0) throw new Error("Unexpected createDocument in existing-document vector");
  const created = await create(title);
  createdIds.push(created.documentId);
  if (input.fault === "lost-create-response") {
    faultCalls++;
    throw new Error("socket hang up");
  }
  return created;
};
const batch = google.batchUpdate.bind(google);
const rename = google.updateFile.bind(google);
google.updateFile = async (id, body) => {
  mutations.push({ kind: "rename", body });
  if (input.fault === "fail-rename-after-content") {
    faultCalls++;
    google.callLog.push({ method: "updateFile", args: [id, body] });
    throw new Error("Google API error (403): insufficient permissions");
  }
  if (input.fault === "ack-rename-without-mutation") {
    faultCalls++;
    google.callLog.push({ method: "updateFile", args: [id, body] });
    return { id, name: typeof body.name === "string" ? body.name : "" };
  }
  return rename(id, body);
};
for (const method of ["commentCreate", "commentReply", "commentUpdate", "commentDelete"]) {
  if (typeof google[method] !== "function") continue;
  const orig = google[method].bind(google);
  google[method] = async (...args) => {
    commentSends++;
    if (input.fault === "fail-second-comment-action" && commentSends === 2) {
      faultCalls++;
      throw new Error("network down");
    }
    if (input.fault === "ack-comment-without-mutation") {
      faultCalls++;
      if (method === "commentCreate") return { id: "c-new-1", content: args[1] ?? "" };
      if (method === "commentReply") return { id: "r-new-1", content: args[2] ?? "" };
      if (method === "commentUpdate") return { id: args[1], ...(args[2] ?? {}) };
      return { id: args[1], deleted: true };
    }
    return orig(...args);
  };
}
google.batchUpdate = async (id, requests, options = {}) => {
  const entry = { kind: "batchUpdate", requests: structuredClone(requests), requiredRevisionId: options.requiredRevisionId };
  mutations.push(entry);
  if (input.fault === "race-before-send") {
    faultCalls++;
    google.externalEdit(id, []);
  }
  if (input.fault === "ack-without-mutation" || input.fault === "ack-move-without-mutation") {
    faultCalls++;
    return JSON.stringify({ documentId: id, replies: [], writeControl: { requiredRevisionId: "rev-1" } });
  }
  if ((input.fault === "fail-content-after-create" || input.fault === "ack-delete-without-mutation") && !injectedContentFailure) {
    injectedContentFailure = true;
    faultCalls++;
    throw new Error("network down");
  }
  if (input.fault === "fail-content-and-delete") {
    faultCalls++;
    throw new Error("network down");
  }
  const response = await batch(id, requests, options);
  wrote = true;
  const parsedResponse = JSON.parse(response);
  entry.receiptRevision = parsedResponse.writeControl?.requiredRevisionId;
  entry.replies = parsedResponse.replies;
  if (input.caseId === "V004-landed-lost-response-replay") throw new Error("connection lost after sending");
  return response;
};
const get = google.getDocument.bind(google);
google.getDocument = async (id) => {
  const json = await get(id);
  if (mutations.some((m) => m.kind === "batchUpdate") && /^V00[56]-/.test(input.caseId)) {
    for (const tab of json.tabs ?? []) {
      for (const element of tab.documentTab?.body?.content ?? []) {
        for (const inline of element.paragraph?.elements ?? []) {
          if (inline.textRun?.textStyle) {
            delete inline.textRun.textStyle.fontSize;
            delete inline.textRun.textStyle.weightedFontFamily;
          }
        }
      }
    }
  }
  if (input.fault === "delayed-docs-title" && id === "synthetic-doc" && mutations.some((m) => m.kind === "rename")) {
    faultCalls++;
    return { ...json, title: "Example" };
  }
  if (input.fault === "mismatch-after-create" && wrote) {
    faultCalls++;
    return {
      ...json,
      tabs: (json.tabs ?? []).map((tab) => ({
        ...tab,
        documentTab: tab.documentTab ? {
          ...tab.documentTab,
          body: tab.documentTab.body ? {
            ...tab.documentTab.body,
            content: (tab.documentTab.body.content ?? []).map((el) => el.paragraph ? {
              ...el,
              paragraph: {
                ...el.paragraph,
                elements: (el.paragraph.elements ?? []).map((inl) => inl.textRun?.content ? {
                  ...inl,
                  textRun: { ...inl.textRun, content: inl.textRun.content.replace("Body", "Bodx") },
                } : inl),
              },
            } : el),
          } : tab.documentTab.body,
        } : tab.documentTab,
      })),
    };
  }
  return json;
};
if (["V002-body-and-title", "V003-stale", "V004-landed-lost-response-replay",
  "V005-font-default", "V006-font-nondefault", "W2-rename-fail-after-content"].includes(input.caseId)) {
  if (!edited.includes("Hello world")) throw new Error("Seed text not found");
  edited = edited.replace("Hello world", "Hello revised world");
}
if (["V002-body-and-title", "W2-rename-only", "W2-delayed-docs-title", "W2-rename-fail-after-content"].includes(input.caseId)) {
  edited = edited.replace("title: Example", "title: Renamed");
}
if (input.caseId === "W3-reparent-order-noop") {
  const fmEnd = edited.indexOf("\n---\n");
  if (fmEnd !== -1) {
    const header = edited.slice(0, fmEnd + 5);
    const body = edited.slice(fmEnd + 5);
    const parts = body.split(/(?=^::tab\{)/m).filter((s) => s.trim().length > 0);
    const byId = new Map();
    for (const part of parts) {
      const m = part.match(/^::tab\{id=([^ }\n]+)/);
      if (m) byId.set(m[1], part.trimEnd());
    }
    const setParent = (block, parent) => {
      const nl = block.indexOf("\n");
      const first = nl === -1 ? block : block.slice(0, nl);
      const rest = nl === -1 ? "" : block.slice(nl);
      const cleaned = first.replace(/ parent=[^ }\n]+/, "");
      const updated = parent ? cleaned.replace(/\}$/, ` parent=${parent}}`) : cleaned;
      return `${updated}${rest}`;
    };
    const reordered = [
      setParent(byId.get("t.a"), null),
      setParent(byId.get("t.b"), "t.a"),
      setParent(byId.get("t.a1"), null),
      setParent(byId.get("t.c"), null),
      setParent(byId.get("t.a2"), "t.c"),
    ];
    edited = `${header}\n${reordered.join("\n\n")}\n`;
  }
}
if (input.caseId === "W3-orphan-refusal") {
  edited = edited
    .replace(/^(::tab\{id=t\.a\b[^}\n]*)\}/m, "$1 delete}")
    .replace(/^(::tab\{id=t\.a1\b[^}\n]*)\}/m, "$1 delete}");
}
if (input.caseId === "W3-delete-subtree-and-all") {
  edited = edited
    .replace(/^(::tab\{id=t\.a\b[^}\n]*)\}/m, "$1 delete}")
    .replace(/^(::tab\{id=t\.a1\b[^}\n]*)\}/m, "$1 delete}")
    .replace(/^(::tab\{id=t\.a2\b[^}\n]*)\}/m, "$1 delete}");
}
if (input.caseId === "V007-cell-style-edit") {
  if (!edited.includes('backgroundColor: "#FF0000"')) throw new Error("Cell declaration not exported");
  edited = edited.replace('backgroundColor: "#FF0000"', 'backgroundColor: "#00FF00"');
}
if (input.caseId === "V008-reject-hidden-insertion") {
  writeFileSync(path, edited.replace(":change[Hello world]", ":change[Hello there]"));
  snippetResult = await fileImport({ path }, services);
  edited = edited.replace(/(::suggestion\{id=s1[^\n]*)\}/, "$1 reject}");
  if (edited === initialBytes) throw new Error("Suggestion rejection edit did not reach block");
}
let preflight;
if (input.caseId === "V009-image-upload-lifecycle") {
  const directory = dirname(path);
  writeFileSync(resolve(directory, "pic.png"), png(200, 100));
  writeFileSync(resolve(directory, "not.png"), "plain text");
  writeFileSync(path, edited.replace("Hello world", "Hello world ![](not.png) ![](missing.png)"));
  const refused = await fileImport({ path }, services);
  preflight = { result: refused, uploads: uploadReceipts.length,
    batches: mutations.filter((m) => m.kind === "batchUpdate").length };
  edited = edited.replace("Hello world", `Hello world ![](pic.png) ![](${pathToFileURL(resolve(directory, "pic.png")).href}) ![](http://synthetic.invalid/image.png)`);
}
let sourceReadsBeforeImport;
if (input.caseId === "V010-cross-doc-image-copy") {
  edited = edited.replace("Hello world", `Hello world :image{src="gdoc:synthetic-source/${sourceToken}"}`);
  sourceReadsBeforeImport = google.callLog.filter((c) => c.method === "getDocument" && c.args[0] === "synthetic-source").length;
}
let refusedResult;
let ownershipRefusals;
let w4Allow;
if (input.caseId === "W4-comment-allow-and-actions") {
  edited = edited
    .replace("My open comment", "My updated comment")
    .replace(/^(::comment\{id=c-other\b[^}\n]*)\}/m, "$1 resolved}")
    .replace("My reply", "My updated reply")
    + "\n::comment\nNew comment\n";
  writeFileSync(path, edited);
  refusedResult = await fileImport({ path }, services);
  w4Allow = ["commentChanges"];
}
if (input.caseId === "W4-comment-ownership-and-readonly") {
  ownershipRefusals = [];
  for (const [kind, apply] of [
    ["edit-other", (s) => s.replace("Ada comment", "Tampered Ada")],
    ["delete-other", (s) => s.replace(/^(::comment\{id=c-other\b[^}\n]*)\}/m, "$1 delete}")],
    ["mutate-quote", (s) => s.replace("quote=Hello", "quote=Tampered")],
  ]) {
    writeFileSync(path, apply(edited));
    ownershipRefusals.push({ kind, result: await fileImport({ path, allow: ["commentChanges"] }, services) });
  }
  w4Allow = ["commentChanges"];
}
if (input.caseId === "W4-comment-resolved-reopen") {
  edited = edited.replace(/^(::comment\{id=c-resolved\b[^}\n]*) resolved\b([^}\n]*)\}/m, "$1$2}");
  w4Allow = ["commentChanges"];
}
if (input.caseId === "W4-comment-partial-unknown-outcome") {
  edited = edited.replace("My open comment", "First updated") + "\n::comment\nSecond new comment\n";
  w4Allow = ["commentChanges"];
}
writeFileSync(path, edited);
if (input.caseId === "V003-stale") google.externalEdit("synthetic-doc", []);
const importInput = isW1 && input.caseId !== "W1-create-invalid-syntax"
  ? { path, newDocStyleBaseline: "generic" }
  : { path };
if (w4Allow) importInput.allow = w4Allow;
const result = await fileImport(importInput, services);
const sourceReadsAfterImport = sourceSeed
  ? google.callLog.filter((c) => c.method === "getDocument" && c.args[0] === "synthetic-source").length : undefined;
const inserts = () => mutations.flatMap((m) => m.requests ?? []).filter((r) => r.insertInlineImage).length;
const insertionsBeforeReplay = inserts();
const providerIds = (doc) => (doc.tabs ?? []).flatMap((tab) =>
  (tab.documentTab?.body?.content ?? []).flatMap((block) =>
    (block.paragraph?.elements ?? []).flatMap((element) =>
      element.inlineObjectElement ? [element.inlineObjectElement.inlineObjectId] : []))).sort();
const providerIdsBeforeReplay = input.caseId.startsWith("V009") || sourceSeed
  ? providerIds(await google.getDocument("synthetic-doc")) : undefined;
const batchesBeforeReplay = mutations.filter((m) => m.kind === "batchUpdate").length;
let replay = ["V004-landed-lost-response-replay", "V009-image-upload-lifecycle", "V010-cross-doc-image-copy"].includes(input.caseId)
  && input.fault !== "known-upload-error" ? await fileImport({ path }, services) : undefined;
if (input.caseId === "W1-create-success-replay" && result.exportPath) {
  replay = await fileImport({ path: result.exportPath }, services);
}
if (input.caseId === "W1-create-rollback-retry" && input.fault !== "ack-delete-without-mutation") {
  writeFileSync(path, edited);
  replay = await fileImport({ path, newDocStyleBaseline: "generic" }, services);
}
let w3Subtree;
if (input.caseId === "W3-reparent-order-noop" && result.exportPath) {
  replay = await fileImport({ path: result.exportPath }, services);
}
if (input.caseId === "W3-delete-subtree-and-all" && result.exportPath) {
  const subBytes = readFileSync(result.exportPath, "utf8");
  const subFile = documentFileParse(subBytes);
  w3Subtree = {
    status: result.status,
    deletedTabs: result.deletedTabs ?? [],
    tabs: subFile.tabs.map((tab) => ({ id: tab.id, title: tab.title, parentId: tab.parentId ?? null, markdown: tab.markdown })),
  };
  const deleteAllBytes = subBytes
    .replace(/^(::tab\{id=t\.b\b[^}\n]*)\}/m, "$1 delete}")
    .replace(/^(::tab\{id=t\.c\b[^}\n]*)\}/m, "$1 delete}");
  writeFileSync(result.exportPath, deleteAllBytes);
  replay = await fileImport({ path: result.exportPath }, services);
}
const batchesAfterReplay = mutations.filter((m) => m.kind === "batchUpdate").length;
const insertionsAfterReplay = inserts();
const providerIdsAfterReplay = input.caseId.startsWith("V009") || sourceSeed
  ? providerIds(await google.getDocument("synthetic-doc")) : undefined;
const imageErrors = [];
if (sourceSeed && result.exportPath) {
  const fresh = readFileSync(result.exportPath, "utf8");
  for (const source of ["gdoc:synthetic-source/unknown-image", "gdoc:missing-source/unknown-image"]) {
    const before = mutations.length;
    writeFileSync(path, fresh.replace("Hello world", `Hello world :image{src="${source}"}`));
    imageErrors.push({ source, result: await fileImport({ path }, services), mutations: mutations.length - before });
  }
  writeFileSync(path, fresh);
}
const temporaryDocuments = [];
for (const receipt of uploadReceipts) {
  temporaryDocuments.push({ id: receipt.documentId, actualDeleteMethodCompleted: completedDeletes.has(receipt.documentId) });
}
const workingExists = existsSync(path);
const finalExportPath = input.caseId === "W3-delete-subtree-and-all" && replay?.exportPath
  ? replay.exportPath
  : result.exportPath;
const exportedExists = !!finalExportPath && existsSync(finalExportPath);
const outputFile = exportedExists ? documentFileParse(readFileSync(finalExportPath, "utf8")) : undefined;
const origPath = finalExportPath ? resolve(dirname(finalExportPath), "export.orig.md") : undefined;
const origMarkdown = origPath && existsSync(origPath) ? readFileSync(origPath, "utf8") : undefined;
const origFile = origMarkdown === undefined ? undefined : documentFileParse(origMarkdown);
let remote = null;
let remoteStored = null;
if (isW1) {
  const targetId = (result.doc && !completedDeletes.has(result.doc))
    ? result.doc
    : (createdIds.length && !completedDeletes.has(createdIds[createdIds.length - 1])
      ? createdIds[createdIds.length - 1]
      : null);
  if (targetId) {
    try {
      remote = await google.getDocument(targetId);
      remoteStored = await get(targetId);
    } catch {}
  }
} else {
  remote = await google.getDocument("synthetic-doc");
  remoteStored = await get("synthetic-doc");
}
const tabsOf = (file) => file?.tabs.map((tab) => ({ id: tab.id, title: tab.title, markdown: tab.markdown }));
const observation = {
  ...(input.caseId.startsWith("W4-") ? {
    commentSends,
    comments: await google.commentsList("synthetic-doc"),
    ...(refusedResult !== undefined ? { refusedResult } : {}),
    ...(ownershipRefusals !== undefined ? { ownershipRefusals } : {}),
  } : {}),
  schema: "engine-parity-observation-v1", side: "node", caseId: input.caseId,
  seedDigest: sha(seedBytes), result, replay, snippetResult, faultCalls, fault: input.fault,
  w1: isW1 ? {
    createdIds,
    deletions,
    temporaryDocuments: createdIds.map((id) => ({
      id,
      actualDeleteMethodCompleted: completedDeletes.has(id),
    })),
  } : undefined,
  w3: input.caseId.startsWith("W3-") ? {
    batchesBeforeReplay,
    batchesAfterReplay,
    subtree: w3Subtree,
  } : undefined,
  images: input.caseId.startsWith("V009") || sourceSeed ? {
    fetched, uploadReceipts, deletions, temporaryDocuments, preflight,
    deletionEvidence: "actual FakeGoogle.deleteFile completion; original getDocument ignores deleted flag",
    sourceReadsBeforeImport, sourceReadsAfterImport, insertionsBeforeReplay, insertionsAfterReplay, imageErrors,
    providerIdsBeforeReplay, providerIdsAfterReplay,
  } : undefined,
  initial: { version: initialFile?.gdocsmith ?? "3.0.20", markdown: initialBytes, tabs: tabsOf(initialFile) ?? [], hasSuggestionBlock: initialBytes.includes("::suggestion{"),
    hasCommentBlock: initialBytes.includes("::comment{id=c1"), hasSyntheticReply: initialBytes.includes("::reply{id=r1"),
    hasInsertedSnippet: initialBytes.includes(":change[Hello world]"),
    hasCellStyleDeclaration: initialBytes.includes('backgroundColor: "#FF0000"') && /\| A :cell\{/.test(initialBytes) },
  working: { path, exists: workingExists, beforeDigest: sha(edited), afterDigest: workingExists ? sha(readFileSync(path)) : null,
    unchanged: workingExists && readFileSync(path, "utf8") === edited },
  exported: { exists: exportedExists, pathRole: !finalExportPath ? "absent" : realpathSync(finalExportPath) === realpathSync(path) ? "same" : "distinct",
    origMarkdown, origTabs: tabsOf(origFile), origRevision: origFile?.revision,
    revision: outputFile?.revision, title: outputFile?.title, tabs: tabsOf(outputFile),
    markdown: exportedExists ? readFileSync(finalExportPath, "utf8") : null,
    hasSuggestionBlock: exportedExists && readFileSync(finalExportPath, "utf8").includes("::suggestion{") },
  remote, remoteStored, mutations, callLog: google.callLog,
};
process.stdout.write(JSON.stringify(observation));
