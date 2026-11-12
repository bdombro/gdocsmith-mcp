#pragma once

#include "reconcile/container.hpp"
#include "reconcile/test_harness.hpp"
#include "test.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace gdocsmith::reconcile::test {

inline JsonObject containerDoc(const std::vector<BlockSpec>& blocks) {
    DocSpec spec;
    TabSpec tspec;
    tspec.blocks = blocks;
    spec.tabs = {std::move(tspec)};
    return docJsonBuild(spec);
}

TEST_CASE("containerReconcile - no change produces no requests") {
    auto r = differentialRun(containerDoc({paraSpec({"a"})}), [](EditTarget&, const std::vector<std::string>&) {});
    CHECK(r.requests.empty());
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("containerReconcile - inserting a paragraph between kept paragraphs") {
    auto json = containerDoc({paraSpec({"a"}), paraSpec({"b"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditParagraphSpec eps;
        eps.syms = {SymSpec{.ch = "x"}};
        paragraphsInsert(t, ContainerRef{.kind = "body"}, 1, {eps});
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "insertText") != kinds.end());
}

TEST_CASE("containerReconcile - deleting a middle paragraph") {
    auto json = containerDoc({paraSpec({"a"}), paraSpec({"b"}), paraSpec({"c"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        blocksDelete(t, {k[1]});
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "deleteContentRange") != kinds.end());
}

TEST_CASE("containerReconcile - inserting a new table after a kept paragraph") {
    auto json = containerDoc({paraSpec({"a"}), paraSpec({"b"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditTableSpec spec;
        spec.rows = {{{SymSpec{.ch = "x"}}, {SymSpec{.ch = "y"}}}};
        tableCreate(t, 1, spec);
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "insertTable") != kinds.end());
}

TEST_CASE("containerReconcile - inserting a section break") {
    auto json = containerDoc({paraSpec({"a"}), paraSpec({"b"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        sectionBreakInsert(t, 1, "CONTINUOUS");
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "insertSectionBreak") != kinds.end());
}

TEST_CASE("containerReconcile - inserting a page break") {
    auto json = containerDoc({paraSpec({"a"}), paraSpec({"b"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        pageBreakParagraphInsert(t, 1);
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "insertPageBreak") != kinds.end());
}

TEST_CASE("containerReconcile - a block that moved is rejected as a delete plus create") {
    auto json = containerDoc({paraSpec({"a"}), paraSpec({"b"})});
    KeyAllocator keys;
    DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});

    std::vector<Block> reordered = original.tabs[0].blocks;
    std::swap(reordered[0], reordered[1]);

    ReconcileContext ctx = reconcileContextCreate("t.0");
    bool threw = false;
    try {
        containerReconcile(original.tabs[0].blocks, reordered, ctx);
    } catch (const CoreError& err) {
        threw = true;
        CHECK(err.code == std::string("internal"));
        CHECK(std::string(err.what()).find("moved") != std::string::npos);
    }
    CHECK(threw);
}

} // namespace gdocsmith::reconcile::test
