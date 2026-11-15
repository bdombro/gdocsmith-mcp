import assert from "node:assert/strict";

const keptWarning = "export.md already had local edits for this revision, so it was kept; delete it and export again for a clean copy.";
const remoteText = (tab) => tab.documentTab.body.content.flatMap((block) =>
  (block.paragraph?.elements ?? []).map((element) => element.textRun?.content ?? "")).join("");

export function safetyAssertions(observation, caseId, parse) {
  const checks = [];
  const expect = (id, action) => {
    try { action(); checks.push({ id, pass: true }); }
    catch (error) { checks.push({ id, pass: false, message: error.message }); }
  };
  const freshBody = (tabs, revision) => {
    assert.equal(revision, observation.remote.revisionId);
    assert.equal(tabs.length, observation.remote.tabs.length);
    for (let index = 0; index < tabs.length; index++) {
      const remote = observation.remote.tabs[index];
      assert.equal(tabs[index].id, remote.tabProperties.tabId);
      assert.equal(tabs[index].title, remote.tabProperties.title);
      assert.equal(tabs[index].markdown, "\n\n" + remoteText(remote) + (index + 1 < tabs.length ? "\n" : ""));
    }
  };
  if (caseId === "V003-stale" || caseId === "V004-landed-lost-response-replay") {
    expect("recovery:authoritative-body", () => {
      freshBody(observation.exported.tabs, observation.exported.revision);
      const actual = parse(observation.exported.markdown);
      freshBody(actual.tabs, actual.revision);
    });
  }
  if (caseId === "V009-image-upload-lifecycle" && observation.fault === "known-upload-error") {
    expect("known-id:canonical-existing-path", () => {
      assert.equal(observation.exported.pathRole, "same");
      assert.equal(observation.result.exportPath, observation.working.path);
      assert.equal(parse(observation.initial.markdown).revision, observation.remote.revisionId);
    });
    expect("known-id:kept-local-warning", () => assert.ok(observation.result.warnings.includes(keptWarning)));
    expect("known-id:edited-bytes-preserved", () => {
      assert.equal(observation.working.exists, true);
      assert.equal(observation.working.unchanged, true);
      assert.equal(observation.working.beforeDigest, observation.working.afterDigest);
    });
    expect("known-id:authoritative-fresh-original", () => {
      freshBody(observation.exported.origTabs, observation.exported.origRevision);
      const actual = parse(observation.exported.origMarkdown);
      freshBody(actual.tabs, actual.revision);
    });
  }
  if (caseId === "V007-cell-style-edit") {
    expect("cell-style:only-intended-write", () => {
      assert.ok(observation.mutations.length > 0);
      for (const mutation of observation.mutations) {
        assert.equal(mutation.kind, "batchUpdate");
        assert.ok(mutation.requests.length > 0);
        for (const request of mutation.requests) {
          assert.deepEqual(Object.keys(request), ["updateTableCellStyle"]);
          const value = request.updateTableCellStyle;
          assert.equal(value.fields, "backgroundColor");
          assert.deepEqual(value.tableCellStyle, { backgroundColor: { color: { rgbColor: { red: 0, green: 1, blue: 0 } } } });
          assert.deepEqual(value.tableRange, {
            columnSpan: 1, rowSpan: 1,
            tableCellLocation: { columnIndex: 0, rowIndex: 0, tableStartLocation: { index: 1, tabId: "t.0" } },
          });
        }
      }
    });
  }
  if (caseId === "V008-reject-hidden-insertion") {
    for (const stage of ["initial", "exported"]) {
      expect(`review:${stage}:full-comment-payload`, () => {
        const markdown = observation[stage].markdown;
        const parsed = parse(markdown);
        assert.equal(parsed.comments.length, 1);
        const { line, replies, ...comment } = parsed.comments[0];
        assert.deepEqual(comment, { id: "c1", text: "Synthetic review\n::literal", quote: "Synthetic quote", orphaned: true });
        assert.equal(replies.length, 1);
        const { line: replyLine, ...reply } = replies[0];
        assert.deepEqual(reply, { id: "r1", text: "Synthetic reply\n::literal reply" });
        assert.ok(markdown.split("\n")[line - 1].startsWith("::comment{id=c1"));
        assert.ok(markdown.split("\n")[replyLine - 1].startsWith("::reply{id=r1"));
      });
    }
    expect("review:full-source-suggestion-grammar", () => {
      const markdown = observation.initial.markdown;
      const parsed = parse(markdown);
      const bodyLine = parsed.tabs[0].line;
      assert.ok(bodyLine > 0);
      assert.deepEqual(parsed.suggestions.rejects, []);
      assert.equal(parsed.suggestions.text,
        `::suggestion{id=s1 kind=text tab=Overview exportLine=${bodyLine}}\n::before\n:change[]\n::after\n:change[Hello world]\n`);
      assert.equal(parse(observation.exported.markdown).suggestions, undefined);
    });
  }
  if (caseId === "W2-rename-only") {
    expect("w2:rename-only-no-content-batch", () => {
      assert.equal(observation.mutations.filter((m) => m.kind === "batchUpdate").length, 0);
      assert.equal(observation.mutations.length, 1);
      assert.equal(observation.mutations[0].kind, "rename");
    });
    expect("w2:no-duplicate-writes", () => {
      assert.ok(observation.mutations.filter((m) => m.kind === "rename").length <= 1);
      assert.ok((observation.callLog ?? []).filter((c) => c.method === "updateFile" || c.kind === "rename").length <= 1);
    });
    expect("w2:protected-tab-unchanged", () => {
      const t1 = observation.remote.tabs.find((t) => t.tabProperties.tabId === "t.1");
      assert.ok(t1);
      assert.equal(remoteText(t1), "Protected by omission\n");
    });
    expect("w2:rename-only-remote-title", () => {
      assert.equal(observation.result.status, "applied");
      assert.equal(observation.remoteStored.title, "Renamed");
      assert.equal(parse(observation.exported.markdown).title, "Renamed");
    });
  }
  if (caseId === "W2-delayed-docs-title") {
    expect("w2:delayed-docs-title-succeeds", () => {
      assert.ok(observation.faultCalls > 0);
      assert.equal(observation.remote.title, "Example");
      assert.equal(observation.remoteStored.title, "Renamed");
      assert.equal(observation.result.status, "applied");
      assert.equal(observation.result.verification, "match");
      assert.ok(observation.mutations.filter((m) => m.kind === "rename").length <= 1);
      assert.equal(observation.mutations.filter((m) => m.kind === "batchUpdate").length, 0);
    });
  }
  if (caseId === "W2-rename-fail-after-content") {
    expect("w2:partial-rename-failure-state", () => {
      assert.ok(observation.faultCalls > 0);
      assert.equal(observation.result.status, "failed");
      assert.equal(observation.result.errors?.[0]?.phase, "rename");
      assert.notEqual(observation.result.writeState, "none");
      const t0 = observation.remote.tabs.find((t) => t.tabProperties.tabId === "t.0");
      const t1 = observation.remote.tabs.find((t) => t.tabProperties.tabId === "t.1");
      assert.ok(t0 && remoteText(t0).includes("Hello revised world"));
      assert.ok(t1 && remoteText(t1) === "Protected by omission\n");
    });
    expect("w2:partial-rename-old-export-title", () => {
      assert.equal(observation.exported.title, "Example");
      assert.equal(parse(observation.exported.markdown).title, "Example");
      if (observation.exported.pathRole !== "same") assert.equal(observation.working.unchanged, true);
    });
  }
  if (caseId === "W1-create-success-replay") {
    expect("w1:initial-tab-reused", () => {
      assert.equal(observation.remote?.tabs?.[0]?.tabProperties?.tabId, "t.0");
    });
    expect("w1:provider-id-returned", () => {
      assert.ok(typeof observation.result.doc === "string" && observation.result.doc.length > 0);
      assert.equal(observation.result.doc, observation.w1?.createdIds?.[0]);
      assert.equal(parse(observation.exported.markdown).doc, observation.result.doc);
    });
    expect("w1:single-create-on-replay", () => {
      assert.equal(observation.w1?.createdIds?.length, 1);
      assert.equal(observation.replay?.status, "unchanged");
    });
  }
  if (caseId === "W1-create-rollback-retry") {
    expect("w1:rollback-confirmed-before-retry", () => {
      assert.equal(observation.result.status, "failed");
      assert.equal(observation.result.writeState, "reverted");
      assert.equal(observation.result.doc, undefined);
      assert.equal(observation.w1?.temporaryDocuments?.[0]?.actualDeleteMethodCompleted, true);
    });
    if (observation.fault !== "ack-delete-without-mutation") {
      expect("w1:explicit-retry-succeeds", () => {
        assert.equal(observation.replay?.status, "applied");
        assert.equal(observation.w1?.createdIds?.length, 2);
      });
    }
  }
  if (caseId === "W1-create-rollback-rejected") {
    expect("w1:rollback-rejected-retains-id", () => {
      assert.equal(observation.result.status, "failed");
      assert.equal(observation.result.writeState, "unknown");
      assert.equal(observation.result.doc, observation.w1?.createdIds?.[0]);
      assert.equal(parse(observation.exported.markdown).doc, observation.result.doc);
    });
    expect("w1:rollback-rejected-warning", () => {
      assert.ok((observation.result.warnings ?? []).some((w) => /could not be deleted/.test(w)));
    });
  }
  if (caseId === "W1-create-mismatch-readback") {
    expect("w1:mismatch-keeps-landed-content", () => {
      assert.equal(observation.result.status, "failed");
      assert.equal(observation.result.verification, "mismatch");
      assert.equal(observation.result.writeState, "complete");
      assert.equal(observation.result.doc, observation.w1?.createdIds?.[0]);
      assert.equal(parse(observation.exported.markdown).doc, observation.result.doc);
      assert.equal(observation.w1?.deletions?.length ?? 0, 0);
    });
  }
  if (caseId === "W1-create-unknown-outcome") {
    expect("w1:unknown-outcome-no-blind-retry", () => {
      assert.equal(observation.result.status, "failed");
      assert.equal(observation.result.writeState, "unknown");
      assert.equal(observation.result.doc, undefined);
      assert.equal(observation.w1?.deletions?.length ?? 0, 0);
      assert.equal(observation.w1?.createdIds?.length, 1);
      assert.ok(observation.result.summary.includes("New Created Doc"));
    });
  }
  if (caseId === "W1-create-invalid-syntax") {
    expect("w1:invalid-syntax-zero-creates", () => {
      assert.equal(observation.result.status, "refused");
      assert.equal(observation.result.writeState, "none");
      assert.equal(observation.w1?.createdIds?.length ?? 0, 0);
    });
  }
  if (caseId === "W3-reparent-order-noop") {
    const flattenTabs = (tabs) => (tabs ?? []).flatMap((t) => [t, ...flattenTabs(t.childTabs)]);
    const flattenRemote = (tabs) => flattenTabs(tabs).map((t) => `${t.tabProperties?.title ?? ""}<${t.tabProperties?.parentTabId ?? ""}`);
    expect("w3:reparent-order-hierarchy", () => {
      if (observation.result.status !== "applied") throw new Error("reparent/reorder import not applied");
      if (observation.result.verification !== "match") throw new Error("reparent/reorder verification not match");
      const tree = flattenRemote(observation.remote?.tabs);
      if (JSON.stringify(tree) !== JSON.stringify(["A<", "B<t.a", "A1<", "C<", "A2<t.c"])) {
        throw new Error(`unexpected remote hierarchy: ${JSON.stringify(tree)}`);
      }
      const reqs = observation.mutations.filter((m) => m.kind === "batchUpdate").flatMap((m) => m.requests ?? []);
      if (!reqs.length || reqs.some((r) => Object.keys(r).length !== 1 || !("updateDocumentTabProperties" in r))) {
        throw new Error("expected only updateDocumentTabProperties requests");
      }
    });
    expect("w3:bodies-and-ids-preserved", () => {
      const ids = flattenTabs(observation.remote?.tabs).map((t) => t.tabProperties?.tabId);
      if (JSON.stringify(ids) !== JSON.stringify(["t.a", "t.b", "t.a1", "t.c", "t.a2"])) {
        throw new Error(`unexpected tab IDs: ${JSON.stringify(ids)}`);
      }
      const bodies = (observation.exported?.tabs ?? []).map((t) => (t.markdown ?? "").trim());
      if (JSON.stringify(bodies) !== JSON.stringify(["A body", "B body", "A1 body", "C body", "A2 body"])) {
        throw new Error(`unexpected bodies: ${JSON.stringify(bodies)}`);
      }
    });
    expect("w3:noop-replay-zero-writes", () => {
      if (observation.replay?.status !== "unchanged") throw new Error(`replay status was ${observation.replay?.status}`);
      if (observation.w3?.batchesAfterReplay !== observation.w3?.batchesBeforeReplay) {
        throw new Error("expected zero batchUpdate writes on no-op replay");
      }
    });
  }
  if (caseId === "W3-orphan-refusal") {
    expect("w3:orphan-refused-zero-writes", () => {
      if (observation.result.status !== "refused") throw new Error("expected refused status");
      if (observation.result.writeState !== "none") throw new Error("expected writeState none");
      if (observation.result.errors?.[0]?.code !== "invalidPlacement") throw new Error("expected invalidPlacement error");
      if (!(observation.result.errors?.[0]?.message ?? "").includes('tab "A2" is under "A", which is marked delete')) {
        throw new Error("missing orphan placement detail");
      }
      if (observation.mutations.some((m) => m.kind === "batchUpdate")) throw new Error("expected zero batchUpdate calls");
      if (observation.working?.unchanged !== true) throw new Error("working file was modified");
    });
  }
  if (caseId === "W3-delete-subtree-and-all") {
    expect("w3:subtree-deleted", () => {
      const sub = observation.w3?.subtree;
      if (sub?.status !== "applied") throw new Error("subtree delete not applied");
      const deletedIds = (sub?.deletedTabs ?? []).map((d) => (typeof d === "string" ? d : d?.id));
      if (JSON.stringify(deletedIds) !== JSON.stringify(["t.a", "t.a1", "t.a2"])) {
        throw new Error(`unexpected deletedTabs: ${JSON.stringify(deletedIds)}`);
      }
      const subTree = (sub?.tabs ?? []).map((t) => `${t.title ?? ""}<${t.parentId ?? t.parent ?? ""}`);
      if (JSON.stringify(subTree) !== JSON.stringify(["B<", "C<"])) {
        throw new Error(`unexpected subtree remaining tabs: ${JSON.stringify(subTree)}`);
      }
    });
    expect("w3:delete-all-clears-tab1", () => {
      if (observation.replay?.status !== "applied") throw new Error("delete-all not applied");
      const warnings = (observation.replay?.warnings ?? []).map((w) => (typeof w === "string" ? w : w?.message ?? ""));
      if (!warnings.some((w) => /cleared and renamed "Tab 1"/.test(w))) throw new Error("missing Tab 1 clear warning");
      const tree = (observation.remote?.tabs ?? []).map((t) => `${t.tabProperties?.title ?? ""}<${t.tabProperties?.parentTabId ?? ""}`);
      if (JSON.stringify(tree) !== JSON.stringify(["Tab 1<"])) throw new Error(`unexpected remaining tree: ${JSON.stringify(tree)}`);
    });
  }
  if (caseId === "W4-comment-allow-and-actions") {
    expect("w4:allow-and-actions", () => {
      assert.equal(observation.refusedResult?.status, "refused", "expected refusal without allow: [commentChanges]");
      assert.equal(observation.result.status, "applied", "expected applied with allow: [commentChanges]");
      const own = observation.comments?.find((c) => c.id === "c-own");
      const other = observation.comments?.find((c) => c.id === "c-other");
      const created = observation.comments?.find((c) => c.content === "New comment");
      assert.equal(own?.content, "My updated comment", "expected updated own comment");
      assert.equal(other?.resolved, true, "expected resolved c-other");
      assert.equal(other?.replies?.find((r) => r.id === "r-own")?.content, "My updated reply", "expected updated own reply");
      assert.ok(created, "expected newly created comment");
    });
  }
  if (caseId === "W4-comment-ownership-and-readonly") {
    expect("w4:ownership-and-readonly", () => {
      assert.equal(observation.ownershipRefusals?.length, 3, "expected 3 ownership/readonly refusals");
      for (const entry of observation.ownershipRefusals) {
        assert.equal(entry.result?.status, "refused", `expected refusal for ${entry.kind}`);
      }
      assert.equal(observation.commentSends, 0, "expected zero commentSends on ownership/readonly refusals");
    });
  }
  if (caseId === "W4-comment-resolved-reopen") {
    expect("w4:resolved-reopen", () => {
      assert.equal(observation.result.status, "applied", "expected applied on resolved reopen");
      const resolved = observation.comments?.find((c) => c.id === "c-resolved");
      assert.equal(resolved?.resolved, false, "expected c-resolved reopened (resolved: false)");
    });
  }
  if (caseId === "W4-comment-partial-unknown-outcome") {
    expect("w4:partial-unknown-outcome", () => {
      if (observation.side === "node") {
        assert.equal(observation.result.writeState, "unknown",
          `expected unknown writeState on node, got ${observation.result.writeState}`);
      } else {
        assert.ok(observation.result.writeState === "partial" || observation.result.writeState === "unknown",
          `expected partial or unknown writeState, got ${observation.result.writeState}`);
      }
      assert.equal(observation.commentSends, 2, "expected zero blind comment replays (commentSends === 2)");
      const own = observation.comments?.find((c) => c.id === "c-own");
      assert.equal(own?.content, "First updated", "expected first comment action confirmed");
    });
  }
  return checks;
}

export function safetyControls(caseId, fault = "none") {
  const controls = [];
  if (["V003-stale", "V004-landed-lost-response-replay"].includes(caseId)) {
    controls.push(["wrong-fresh-recovery-body", "recovery:authoritative-body", (o) => { o.exported.markdown = o.exported.markdown.replace("Hello", "WRONG"); }]);
  }
  if (caseId === "V009-image-upload-lifecycle" && fault === "known-upload-error") {
    controls.push(
      ["wrong-known-id-path", "known-id:canonical-existing-path", (o) => { o.exported.pathRole = "distinct"; }],
      ["missing-kept-warning", "known-id:kept-local-warning", (o) => { o.result.warnings = []; }],
      ["clobbered-edited-bytes", "known-id:edited-bytes-preserved", (o) => { o.working.afterDigest = "wrong"; }],
      ["wrong-known-id-fresh-body", "known-id:authoritative-fresh-original", (o) => { o.exported.origMarkdown = (o.exported.origMarkdown ?? o.initial.markdown).replace("Hello", "WRONG"); }],
    );
  }
  if (caseId === "V007-cell-style-edit") {
    controls.push(["unrelated-text-write", "cell-style:only-intended-write", (o) => { o.mutations[0].requests.push({ insertText: { text: "wrong" } }); }]);
    controls.push(["unrelated-cell-field", "cell-style:only-intended-write", (o) => { o.mutations[0].requests[0].updateTableCellStyle.fields += ",paddingTop"; }]);
  }
  if (caseId === "V008-reject-hidden-insertion") {
    controls.push(
      ["corrupt-comment", "review:initial:full-comment-payload", (o) => { o.initial.markdown = o.initial.markdown.replace("Synthetic review", "Wrong review"); }],
      ["corrupt-reply", "review:exported:full-comment-payload", (o) => { o.exported.markdown = o.exported.markdown.replace("Synthetic reply", "Wrong reply"); }],
      ["corrupt-suggestion-grammar", "review:full-source-suggestion-grammar", (o) => { o.initial.markdown = o.initial.markdown.replace("::before", "::WRONG"); }],
    );
  }
  if (caseId === "W2-rename-only") {
    controls.push(
      ["extra-content-batch", "w2:rename-only-no-content-batch", (o) => { o.mutations.push({ kind: "batchUpdate", requiredRevisionId: "rev-1" }); }],
      ["duplicate-rename-write", "w2:no-duplicate-writes", (o) => { o.mutations.push({ kind: "rename", title: "Renamed" }); }],
      ["mutated-protected-tab", "w2:protected-tab-unchanged", (o) => {
        const t1 = o.remote.tabs.find((t) => t.tabProperties.tabId === "t.1");
        const block = t1?.documentTab?.body?.content?.find((b) => b.paragraph);
        if (block) block.paragraph.elements[0].textRun.content = "CORRUPTED\n";
      }],
      ["wrong-remote-title", "w2:rename-only-remote-title", (o) => { o.remoteStored.title = "Example"; }],
    );
  }
  if (caseId === "W2-delayed-docs-title") {
    controls.push(
      ["equate-delayed-docs-with-failure", "w2:delayed-docs-title-succeeds", (o) => { o.result.status = "failed"; }],
    );
  }
  if (caseId === "W2-rename-fail-after-content") {
    controls.push(
      ["erase-partial-write-state", "w2:partial-rename-failure-state", (o) => { o.result.writeState = "none"; }],
      ["wrong-failure-phase", "w2:partial-rename-failure-state", (o) => { if (o.result.errors?.[0]) o.result.errors[0].phase = "content"; }],
      ["wrong-old-export-title", "w2:partial-rename-old-export-title", (o) => { o.exported.title = "Renamed"; }],
    );
  }
  if (caseId === "W1-create-success-replay") {
    controls.push(
      ["corrupt-provider-id-result", "w1:provider-id-returned", (o) => { o.result.doc = "wrong-id"; }],
      ["corrupt-provider-id-export", "w1:provider-id-returned", (o) => { o.exported.markdown = (o.exported.markdown ?? "").replace(/doc: [^\n]+/, "doc: wrong-id"); }],
      ["duplicate-create-on-replay", "w1:single-create-on-replay", (o) => { o.w1.createdIds.push("fake-doc-extra"); }],
    );
  }
  if (caseId === "W1-create-rollback-retry") {
    controls.push(
      ["false-deletion-state", "w1:rollback-confirmed-before-retry", (o) => { if (o.w1?.temporaryDocuments?.[0]) o.w1.temporaryDocuments[0].actualDeleteMethodCompleted = false; }],
      ["wrong-retry-status", "w1:explicit-retry-succeeds", (o) => { if (o.replay) o.replay.status = "failed"; }],
    );
  }
  if (caseId === "W1-create-rollback-rejected") {
    controls.push(
      ["drop-retained-id", "w1:rollback-rejected-retains-id", (o) => { delete o.result.doc; }],
      ["drop-delete-warning", "w1:rollback-rejected-warning", (o) => { o.result.warnings = []; }],
    );
  }
  if (caseId === "W1-create-mismatch-readback") {
    controls.push(
      ["wrong-mismatch-verification", "w1:mismatch-keeps-landed-content", (o) => { o.result.verification = "match"; }],
    );
  }
  if (caseId === "W1-create-unknown-outcome") {
    controls.push(
      ["invented-doc-id", "w1:unknown-outcome-no-blind-retry", (o) => { o.result.doc = "invented-id"; }],
    );
  }
  if (caseId === "W1-create-invalid-syntax") {
    controls.push(
      ["unexpected-create-on-invalid", "w1:invalid-syntax-zero-creates", (o) => { o.w1.createdIds.push("fake-doc-1"); }],
    );
  }
  if (caseId === "W3-reparent-order-noop") {
    controls.push(
      ["corrupt-moved-hierarchy", "w3:reparent-order-hierarchy", (o) => { if (o.remote?.tabs?.[1]?.tabProperties) o.remote.tabs[1].tabProperties.parentTabId = "t.c"; }],
      ["corrupt-moved-body", "w3:bodies-and-ids-preserved", (o) => { if (o.exported?.tabs?.[1]) o.exported.tabs[1].markdown = "Corrupted body"; }],
      ["extra-write-on-noop-replay", "w3:noop-replay-zero-writes", (o) => { if (o.w3) o.w3.batchesAfterReplay = (o.w3.batchesBeforeReplay ?? 0) + 1; }],
    );
  }
  if (caseId === "W3-orphan-refusal") {
    controls.push(
      ["wrong-orphan-error-code", "w3:orphan-refused-zero-writes", (o) => { if (o.result.errors?.[0]) o.result.errors[0].code = "unknown"; }],
    );
  }
  if (caseId === "W3-delete-subtree-and-all") {
    controls.push(
      ["wrong-subtree-deleted-ids", "w3:subtree-deleted", (o) => { if (o.w3?.subtree) o.w3.subtree.deletedTabs = ["t.a"]; }],
      ["missing-tab1-warning", "w3:delete-all-clears-tab1", (o) => { if (o.replay) o.replay.warnings = []; }],
    );
  }
  if (caseId === "W4-comment-allow-and-actions") {
    controls.push(
      ["corrupt-comment-refusal", "w4:allow-and-actions", (o) => { if (o.refusedResult) o.refusedResult.status = "applied"; }],
      ["corrupt-comment-postread", "w4:allow-and-actions", (o) => { if (o.comments?.[0]) o.comments[0].content = "Corrupted"; }],
    );
  }
  if (caseId === "W4-comment-ownership-and-readonly") {
    controls.push(
      ["corrupt-ownership-refusal", "w4:ownership-and-readonly", (o) => { if (o.ownershipRefusals?.[0]?.result) o.ownershipRefusals[0].result.status = "applied"; }],
    );
  }
  if (caseId === "W4-comment-resolved-reopen") {
    controls.push(
      ["corrupt-reopen-state", "w4:resolved-reopen", (o) => { const c = o.comments?.find((x) => x.id === "c-resolved"); if (c) c.resolved = true; }],
    );
  }
  if (caseId === "W4-comment-partial-unknown-outcome") {
    controls.push(
      ["corrupt-partial-write-state", "w4:partial-unknown-outcome", (o) => { o.result.writeState = "none"; }],
    );
  }
  return controls;
}
