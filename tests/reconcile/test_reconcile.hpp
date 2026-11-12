#pragma once

#include "reconcile/reconcile.hpp"
#include "reconcile/test_harness.hpp"
#include "test.hpp"

#include <string>
#include <vector>

namespace gdocsmith::reconcile::test {

inline JsonObject reconcileDoc(const std::vector<BlockSpec>& blocks) {
    DocSpec spec;
    TabSpec tspec;
    tspec.blocks = blocks;
    spec.tabs = {std::move(tspec)};
    return docJsonBuild(spec);
}

struct ReconcileRoundTrip {
    DocPlan plan;
    CompareResult check;
};

inline ReconcileRoundTrip reconcileRun(
    const JsonObject& json,
    std::function<void(EditTarget& target, const std::vector<std::string>& keys)> mutate
) {
    KeyAllocator keys;
    DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    DocModel final = original;
    TabModel& tab = final.tabs[0];

    EditContext editCtx{
        .keys = keys,
        .stamp = EditStamp{.force = false, .tabIndex = 0},
        .tombstones = {}
    };
    EditTarget target{.ctx = editCtx, .tab = tab};

    std::vector<std::string> origKeys;
    for (const auto& b : tab.blocks) origKeys.push_back(b.key);
    mutate(target, origKeys);

    DocReconcileInput input{
        .final = std::move(final),
        .original = std::move(original),
        .originalJson = json,
        .carryRanges = {}
    };
    auto plan = docReconcile(input);
    auto check = docPlanSelfCheck(plan, input);
    return ReconcileRoundTrip{.plan = std::move(plan), .check = std::move(check)};
}

TEST_CASE("docReconcile - no change is an empty plan") {
    auto r = reconcileRun(reconcileDoc({paraSpec({"a"})}), [](EditTarget&, const std::vector<std::string>&) {});
    CHECK(r.check.equal);
    CHECK(r.plan.contentRequests.empty());
    CHECK(r.plan.tabOps.empty());
    CHECK(r.plan.tabsToAdd.empty());
    CHECK(r.plan.deletedRanges.empty());
    CHECK(r.plan.summary.contentRequests == 0);
}

TEST_CASE("docReconcile - a text edit round-trips through the self-check") {
    auto r = reconcileRun(reconcileDoc({paraSpec({"the cat"})}), [](EditTarget& t, const std::vector<std::string>& k) {
        paragraphSplice(t, k[0], 4, 3, {SymSpec{.ch = "dog"}});
    });
    CHECK(r.check.equal);
    CHECK(!r.plan.contentRequests.empty());
    CHECK(r.plan.summary.contentRequests == static_cast<int>(r.plan.contentRequests.size()));
}

TEST_CASE("docReconcile - deleting a paragraph records a deleted range") {
    auto r = reconcileRun(reconcileDoc({paraSpec({"a"}), paraSpec({"b"}), paraSpec({"c"})}),
                          [](EditTarget& t, const std::vector<std::string>& k) {
        blocksDelete(t, {k[1]});
    });
    CHECK(r.check.equal);
    CHECK(!r.plan.deletedRanges.empty());
}

TEST_CASE("docReconcile - renaming a tab emits a single tab op") {
    KeyAllocator keys;
    auto json = reconcileDoc({paraSpec({"a"})});
    DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    DocModel final = original;
    final.tabs[0].title = "Renamed";

    DocReconcileInput input{
        .final = std::move(final),
        .original = std::move(original),
        .originalJson = json,
        .carryRanges = {}
    };
    auto plan = docReconcile(input);

    CHECK(plan.tabOps.size() == 1);
    CHECK(plan.tabOps[0].contains("updateDocumentTabProperties"));
    CHECK(plan.summary.tabOps == 1);
}

TEST_CASE("docReconcile - a new tab is recorded in tabsToAdd") {
    KeyAllocator keys;
    auto json = reconcileDoc({paraSpec({"a"})});
    DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    DocModel final = original;

    TabModel second;
    second.tabId = "new_tab_1";
    second.title = "Second";
    second.isNew = true;
    second.documentStyle = final.tabs[0].documentStyle;
    second.leadingSectionStyle = final.tabs[0].leadingSectionStyle;
    second.namedStyles = final.tabs[0].namedStyles;
    second.blocks.push_back(Block(paragraphEmptyCreate(keys, make_json_object(), EditStamp{.force = false, .tabIndex = 0})));
    final.tabs.push_back(std::move(second));

    DocReconcileInput input{
        .final = std::move(final),
        .original = std::move(original),
        .originalJson = json,
        .carryRanges = {}
    };
    auto plan = docReconcile(input);

    CHECK(plan.tabsToAdd.size() == 1);
    CHECK(plan.tabsToAdd[0].tabId == "new_tab_1");
    CHECK(plan.tabsToAdd[0].title == "Second");
    CHECK(plan.summary.tabsToAdd == 1);
}

} // namespace gdocsmith::reconcile::test
