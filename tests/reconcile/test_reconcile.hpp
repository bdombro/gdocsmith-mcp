#pragma once

#include "reconcile/reconcile.hpp"
#include "reconcile/test_harness.hpp"
#include "test.hpp"

#include <string>
#include <vector>

namespace gdocsmith::reconcile::test {

inline JsonObject reconcileDoc(const std::vector<BlockSpec> &blocks) {
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
    const JsonObject &json,
    std::function<void(EditTarget &target, const std::vector<std::string> &keys)> mutate) {
    KeyAllocator keys;
    DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    DocModel final = original;
    TabModel &tab = final.tabs[0];

    EditContext editCtx{
        .keys = keys,
        .stamp = EditStamp{.force = false, .tabIndex = 0},
        .tombstones = {}};
    EditTarget target{.ctx = editCtx, .tab = tab};

    std::vector<std::string> origKeys;
    for (const auto &b : tab.blocks)
        origKeys.push_back(b.key);
    mutate(target, origKeys);

    DocReconcileInput input{
        .final = std::move(final),
        .original = std::move(original),
        .originalJson = json,
        .carryRanges = {}};
    auto plan = docReconcile(input);
    auto check = docPlanSelfCheck(plan, input);
    return ReconcileRoundTrip{.plan = std::move(plan), .check = std::move(check)};
}

TEST_CASE("docReconcile - no change is an empty plan") {
    auto r = reconcileRun(reconcileDoc({paraSpec({"a"})}), [](EditTarget &, const std::vector<std::string> &) {});
    CHECK(r.check.equal);
    CHECK(r.plan.contentRequests.empty());
    CHECK(r.plan.tabOps.empty());
    CHECK(r.plan.tabsToAdd.empty());
    CHECK(r.plan.deletedRanges.empty());
    CHECK(r.plan.summary.contentRequests == 0);
}

TEST_CASE("named range edges in nested cells borrow the live model and validate offsets") {
    ParagraphBlock paragraph;
    paragraph.key = "nested";
    paragraph.inlines.push_back(Inline::from_text("A\xF0\x9F\x98\x80"));
    CellModel cell;
    cell.blocks.emplace_back(paragraph);
    RowModel row;
    row.cells.push_back(cell);
    TableBlock table;
    table.key = "table";
    table.rows.push_back(row);
    TabModel tab;
    tab.blocks.emplace_back(table);
    const auto *found = blockFindConst(tab, "nested");
    REQUIRE(found != nullptr);
    CHECK(found == &tab.blocks[0].as_table().rows[0].cells[0].blocks[0]);
    const auto layout = layoutCompute(tab);
    const int start = layout.blockRanges.at("nested").start;
    CHECK(rangeEdgeIndex(tab, layout, "nested", 2) == start + 3);
    CHECK_THROWS_AS(rangeEdgeIndex(tab, layout, "nested", -1), CoreError);
    CHECK_THROWS_AS(rangeEdgeIndex(tab, layout, "nested", 3), CoreError);
    auto hidden = tab;
    hidden.blocks[0].as_table().rows[0].cells[0].blocks[0].as_paragraph().inlines[0].suggestedInsertionIds =
        std::vector<std::string>{"suggestion"};
    CHECK_THROWS_AS(rangeEdgeIndex(hidden, layout, "nested", 1), CoreError);
    auto &nested = tab.blocks[0].as_table().rows[0].cells[0].blocks[0].as_paragraph();
    nested.origin = Range{.end = start + 4, .start = start};
    NamedRangeDef nr;
    nr.name = "unicode";
    nr.namedRangeId = "id";
    nr.ranges.emplace_back();
    nr.ranges[0].start = start;
    nr.ranges[0].end = start + 3;
    tab.namedRanges.push_back(nr);
    CHECK(lens::namedRangesShown(tab).size() == 1u);
    tab.namedRanges[0].create = NamedRangeDef::EdgePair{
        .end = {"nested", 2}, .start = {"nested", 1}};
    ReconcileContext ctx;
    ctx.tabId = "t";
    namedRangesReconcile(TabModel{}, tab, ctx);
    REQUIRE(ctx.requests.size() == 1u);
    CHECK(ctx.requests[0]["createNamedRange"]["range"]["startIndex"].get<int64_t>() == start + 1);
    CHECK(ctx.requests[0]["createNamedRange"]["range"]["endIndex"].get<int64_t>() == start + 3);
    tab.namedRanges[0].create->end.offset = 1;
    CHECK_THROWS_AS(namedRangesReconcile(TabModel{}, tab, ctx), CoreError);
}

TEST_CASE("named range marker offsets count Unicode scalars and atoms, not bytes or markers") {
    lens::ProjSpan text;
    text.text = "A\xF0\x9F\x98\x80";
    lens::ProjSpan marker;
    marker.kind = "token";
    marker.token = lens::TokenRef{.kind = "namedrange"};
    marker.token->attrs.values.emplace_back("name", "scope");
    marker.token->attrs.flags.push_back("start");
    lens::ProjSpan atom;
    atom.kind = "token";
    atom.token = lens::TokenRef{.kind = "image"};
    auto end = marker;
    end.token->attrs.flags = {"end"};
    const auto marks = lens::rangeMarks({text, marker, atom, end});
    REQUIRE(marks.size() == 2u);
    CHECK(marks[0].offset == 2);
    CHECK(marks[1].offset == 3);
    marker.token->attrs.flags.push_back("end");
    CHECK_THROWS_AS(lens::rangeMarks({marker}), CoreError);
}

TEST_CASE("named range marker application preserves attached identities and recreates moved edges") {
    TabModel tab;
    ParagraphBlock p;
    p.key = "p";
    p.origin = Range{.end = 12, .start = 1};
    p.inlines.push_back(Inline::from_text("abcdefghij"));
    tab.blocks.emplace_back(p);
    NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "original";
    NamedRangeDef::RangePart part;
    part.start = 3;
    part.end = 7;
    nr.ranges.push_back(part);
    tab.namedRanges.push_back(nr);
    auto hidden = nr;
    hidden.namedRangeId = "hidden";
    hidden.ranges[0].segmentId = "header";
    tab.namedRanges.push_back(hidden);
    const auto shown = lens::namedRangesShown(tab);
    REQUIRE(shown.size() == 1u);
    std::vector<lens::RangeMark> before{
        {.edge = "start", .key = "p", .name = "scope", .offset = 2},
        {.edge = "end", .key = "p", .name = "scope", .offset = 6}};
    auto after = before;
    after[0].same = true;
    after[1].same = true;
    after[1].offset = 7; // internal edit follows kept edge symbols
    const auto managed = lens::namedRangesApply(tab, before, after, shown, [] { return "new"; });
    CHECK(managed == std::set<std::string>{"original"});
    REQUIRE(tab.namedRanges.size() == 2u);
    CHECK(tab.namedRanges[0].namedRangeId == "original");
    REQUIRE(tab.namedRanges[0].expect.has_value());
    CHECK(lens::namedRangeExpected(tab, tab.namedRanges[0])->end == 8);

    auto readback = tab;
    readback.namedRanges[0].ranges[0].end = 8;
    CHECK(lens::namedRangeVerify(tab, tab.namedRanges[0], readback) == std::optional<bool>(true));
    readback.namedRanges[0].namedRangeId = "wrong";
    CHECK(lens::namedRangeVerify(tab, tab.namedRanges[0], readback) == std::optional<bool>(false));

    after[1].same = false;
    lens::namedRangesApply(tab, before, after, shown, [] { return "recreated"; });
    REQUIRE(tab.namedRanges.size() == 2u);
    CHECK(tab.namedRanges[0].namedRangeId == "hidden");
    CHECK(tab.namedRanges[1].namedRangeId == "recreated");
    REQUIRE(tab.namedRanges[1].create.has_value());
    ReconcileContext ctx;
    ctx.tabId = "t";
    TabModel original;
    original.namedRanges.push_back(nr);
    namedRangesReconcile(original, tab, ctx);
    REQUIRE(ctx.requests.size() == 2u);
    CHECK(ctx.requests[0].contains("deleteNamedRange"));
    CHECK(ctx.requests[1]["createNamedRange"]["range"]["startIndex"].get<int64_t>() == 3);
    CHECK(ctx.requests[1]["createNamedRange"]["range"]["endIndex"].get<int64_t>() == 8);
}

TEST_CASE("named range duplicate names pair FIFO in start order and hidden ranges survive removal") {
    TabModel tab;
    for (int i = 0; i < 2; ++i) {
        NamedRangeDef nr;
        nr.name = "same";
        nr.namedRangeId = std::to_string(i);
        nr.ranges.emplace_back();
        tab.namedRanges.push_back(nr);
    }
    const std::vector<lens::ShownRange> shown{{tab.namedRanges[0], 0}, {tab.namedRanges[1], 0}};
    const std::vector<lens::RangeMark> before{
        {.edge = "start", .key = "p", .name = "same", .offset = 0},
        {.edge = "start", .key = "p", .name = "same", .offset = 1},
        {.edge = "end", .key = "p", .name = "same", .offset = 3},
        {.edge = "end", .key = "p", .name = "same", .offset = 4}};
    auto after = before;
    for (auto &m : after)
        m.same = true;
    lens::namedRangesApply(tab, before, after, shown, [] { return "new"; });
    REQUIRE(tab.namedRanges.size() == 2u);
    CHECK(tab.namedRanges[0].expect->end.offset == 3);
    CHECK(tab.namedRanges[1].expect->start.offset == 1);
    lens::namedRangesApply(tab, before, {}, shown, [] { return "new"; });
    CHECK(tab.namedRanges.empty());
}

TEST_CASE("named range malformed empty and multispan changes refuse without mutating ranges") {
    TabModel tab;
    NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "original";
    nr.ranges.resize(2);
    tab.namedRanges.push_back(nr);
    std::vector<lens::RangeMark> marks{
        {.edge = "start", .key = "p", .name = "scope", .offset = 0, .same = true},
        {.edge = "end", .key = "p", .name = "scope", .offset = 3, .same = true}};
    std::vector<lens::ShownRange> shown{{nr, 0}};
    lens::namedRangesApply(tab, marks, marks, shown, [] { return "new"; });
    CHECK(tab.namedRanges[0].namedRangeId == "original");
    CHECK_FALSE(tab.namedRanges[0].expect.has_value());
    auto moved = marks;
    moved[1].same = false;
    CHECK_THROWS_AS(lens::namedRangesApply(tab, marks, moved, shown, [] { return "new"; }), CoreError);
    CHECK_THROWS_AS(lens::namedRangesApply(tab, marks, {}, shown, [] { return "new"; }), CoreError);
    moved[1].offset = 0;
    CHECK_THROWS_AS(lens::namedRangesApply(tab, marks, moved, shown, [] { return "new"; }), CoreError);
    moved.erase(moved.begin());
    CHECK_THROWS_AS(lens::namedRangesApply(tab, marks, moved, shown, [] { return "new"; }), CoreError);
    CHECK(tab.namedRanges[0].namedRangeId == "original");
}

TEST_CASE("named range predictions use UTF16 and outside-edge insertion semantics") {
    auto insert = [](int at, std::string text) -> JsonObject {
        return slimr::Json::object_t{{"insertText", slimr::Json::object_t{
                                                        {"location", slimr::Json::object_t{{"index", int64_t{at}}, {"tabId", "t"}}},
                                                        {"text", std::move(text)}}}};
    };
    const Range range{.end = 10, .start = 5};
    auto predict = [&](int at) { return lens::namedRangePredict(range, "t", {insert(at, "\xF0\x9F\x98\x80")}); };
    REQUIRE(predict(5).has_value());
    CHECK(predict(5)->start == 7);
    CHECK(predict(5)->end == 12);
    CHECK(predict(7)->start == 5);
    CHECK(predict(7)->end == 12);
    CHECK(predict(10)->end == 10);
    CHECK(predict(11)->end == 10);
    JsonObject erase = slimr::Json::object_t{{"deleteContentRange", slimr::Json::object_t{
                                                                        {"range", slimr::Json::object_t{{"startIndex", int64_t{6}}, {"endIndex", int64_t{8}}, {"tabId", "t"}}}}}};
    CHECK(lens::namedRangePredict(range, "t", {erase})->end == 8);
    erase["deleteContentRange"]["range"]["startIndex"] = int64_t{5};
    erase["deleteContentRange"]["range"]["endIndex"] = int64_t{10};
    CHECK_FALSE(lens::namedRangePredict(range, "t", {erase}).has_value());
    CHECK_FALSE(lens::namedRangePredict(range, "t", {slimr::Json::object_t{{"insertTable", slimr::Json::object_t{}}}}).has_value());
}

TEST_CASE("docReconcile - a text edit round-trips through the self-check") {
    auto r = reconcileRun(reconcileDoc({paraSpec({"the cat"})}), [](EditTarget &t, const std::vector<std::string> &k) {
        paragraphSplice(t, k[0], 4, 3, {SymSpec{.ch = "dog"}});
    });
    CHECK(r.check.equal);
    CHECK(!r.plan.contentRequests.empty());
    CHECK(r.plan.summary.contentRequests == static_cast<int>(r.plan.contentRequests.size()));
}

TEST_CASE("docReconcile - deleting a paragraph records a deleted range") {
    auto r = reconcileRun(reconcileDoc({paraSpec({"a"}), paraSpec({"b"}), paraSpec({"c"})}),
                          [](EditTarget &t, const std::vector<std::string> &k) {
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
        .carryRanges = {}};
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
        .carryRanges = {}};
    auto plan = docReconcile(input);

    CHECK(plan.tabsToAdd.size() == 1);
    CHECK(plan.tabsToAdd[0].tabId == "new_tab_1");
    CHECK(plan.tabsToAdd[0].title == "Second");
    CHECK(plan.summary.tabsToAdd == 1);
}

} // namespace gdocsmith::reconcile::test
