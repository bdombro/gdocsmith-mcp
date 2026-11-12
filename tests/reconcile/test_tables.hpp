#pragma once

#include "reconcile/tables.hpp"
#include "reconcile/test_harness.hpp"
#include "test.hpp"

namespace gdocsmith::reconcile::test {

inline TableBlock& tableOf(EditTarget& t) {
    for (auto& b : t.tab.blocks) {
        if (b.is_table()) return b.as_table();
    }
    throw CoreError("internal", "no table in tab");
}

inline void tableRestyle(EditTarget& t, std::string_view key, std::function<void(TableBlock&)> edit) {
    auto& table = tableRequire(t, key);
    edit(table);
    table.stamp = t.ctx.stamp;
}

inline JsonObject docWithBlocks(const std::vector<BlockSpec>& blocks) {
    DocSpec spec;
    TabSpec tspec;
    tspec.blocks = blocks;
    spec.tabs = {std::move(tspec)};
    return docJsonBuild(spec);
}

inline BlockSpec twoByTwoTable() {
    return tableSpec({
        {{cellBlock("a1")}, {cellBlock("b1")}},
        {{cellBlock("a2")}, {cellBlock("b2")}}
    });
}

TEST_CASE("new tables - a new 2x3 table with content after a kept paragraph") {
    auto json = docWithBlocks({paraSpec({"before"}), paraSpec({"after"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditTableSpec spec;
        spec.rows = {
            {{SymSpec{.ch = "a"}}, {SymSpec{.ch = "b"}}, {SymSpec{.ch = "c"}}},
            {{SymSpec{.ch = "d"}}, {}, {SymSpec{.ch = "f"}}}
        };
        tableCreate(t, 1, spec);
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "insertTable") != kinds.end());
    CHECK(std::find(kinds.begin(), kinds.end(), "updateTableColumnProperties") != kinds.end());
}

TEST_CASE("new tables - a new paragraph right before a new table fills paragraph") {
    auto json = docWithBlocks({paraSpec({"before"}), paraSpec({"after"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditParagraphSpec eps;
        eps.syms = {SymSpec{.ch = "intro"}};
        paragraphsInsert(t, ContainerRef{.kind = "body"}, 1, {eps});

        EditTableSpec spec;
        spec.rows = {{{SymSpec{.ch = "x"}}}};
        tableCreate(t, 2, spec);
    });
    CHECK(r.compare.diffs.empty());
    CHECK(r.final.tabs[0].blocks[0].kind == "paragraph");
    CHECK(r.final.tabs[0].blocks[1].kind == "paragraph");
    CHECK(r.final.tabs[0].blocks[2].kind == "table");
    CHECK(r.final.tabs[0].blocks[3].kind == "paragraph");
}

TEST_CASE("new tables - a new table at end of body") {
    auto json = docWithBlocks({paraSpec({"only"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditTableSpec spec;
        spec.rows = {{{SymSpec{.ch = "x"}}, {SymSpec{.ch = "y"}}}};
        tableCreate(t, 1, spec);
    });
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("new tables - merges, widths, styles on new table") {
    auto json = docWithBlocks({paraSpec({"before"}), paraSpec({"after"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditTableSpec spec;
        spec.rows = {
            {{SymSpec{.ch = "h1"}}, {SymSpec{.ch = "h2"}}},
            {{SymSpec{.ch = "x"}}, {}}
        };
        std::string key = tableCreate(t, 1, spec);
        cellsMerge(t, key, CellRange{.column = 0, .columnSpan = 2, .row = 1});

        tableRestyle(t, key, [](TableBlock& table) {
            table.columns[0].props = stylePatchApply(table.columns[0].props, slimr::Json::object_t{
                {"width", dimensionFromPt(120)}
            });
            JsonObject red = slimr::Json::object_t{{"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{{"red", 1}}}}}};
            table.rows[0].cells[1].style = stylePatchApply(table.rows[0].cells[1].style, slimr::Json::object_t{
                {"backgroundColor", red}
            });
            table.rows[0].style = stylePatchApply(table.rows[0].style, slimr::Json::object_t{
                {"tableHeader", true}
            });
        });
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    for (const auto& kind : {"mergeTableCells", "updateTableColumnProperties", "updateTableCellStyle", "pinTableHeaderRows"}) {
        CHECK(std::find(kinds.begin(), kinds.end(), kind) != kinds.end());
    }
}

TEST_CASE("kept tables - editing cells") {
    auto json = docWithBlocks({paraSpec({"a"}), twoByTwoTable(), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        auto& table = tableOf(t);
        paragraphSplice(t, table.rows[0].cells[1].blocks[0].key, 2, 0, {SymSpec{.ch = "!"}});
        paragraphSplice(t, table.rows[1].cells[0].blocks[0].key, 0, 2, {SymSpec{.ch = "A2"}});
    });
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("kept tables - row and column inserts and deletes") {
    auto json = docWithBlocks({paraSpec({"a"}), twoByTwoTable(), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        auto& table = tableOf(t);
        JsonObject red = slimr::Json::object_t{{"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{{"red", 1}}}}}};
        tableRestyle(t, table.key, [&](TableBlock& tbl) {
            tbl.rows[0].cells[0].style = stylePatchApply(tbl.rows[0].cells[0].style, slimr::Json::object_t{
                {"backgroundColor", red}
            });
        });
        rowsDelete(t, table.key, {1});
        rowsInsert(t, table.key, 1, 2);
        columnsInsert(t, table.key, 0, 1);
        columnsDelete(t, table.key, {2});
        paragraphSplice(t, table.rows[2].cells[0].blocks[0].key, 0, 0, {SymSpec{.ch = "new"}});
        paragraphSplice(t, table.rows[0].cells[0].blocks[0].key, 0, 0, {SymSpec{.ch = "left"}});
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    for (const auto& kind : {"deleteTableRow", "insertTableRow", "insertTableColumn", "deleteTableColumn"}) {
        CHECK(std::find(kinds.begin(), kinds.end(), kind) != kinds.end());
    }
}

TEST_CASE("kept tables - merging empty cells") {
    auto empty = tableSpec({
        {{cellBlock("head")}, {cellBlock("")}},
        {{cellBlock("a2")}, {cellBlock("b2")}}
    });
    auto json = docWithBlocks({paraSpec({"a"}), empty, paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        cellsMerge(t, tableOf(t).key, CellRange{.column = 0, .columnSpan = 2, .row = 0});
    });
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("kept tables - widths, cell styles, and row styles in place") {
    auto json = docWithBlocks({paraSpec({"a"}), twoByTwoTable(), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        std::string key = tableOf(t).key;
        tableRestyle(t, key, [](TableBlock& tbl) {
            tbl.columns[1].props = stylePatchApply(tbl.columns[1].props, slimr::Json::object_t{
                {"width", dimensionFromPt(80)},
                {"widthType", "FIXED_WIDTH"}
            });
            JsonObject red = slimr::Json::object_t{{"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{{"red", 1}}}}}};
            for (auto& cell : tbl.rows[1].cells) {
                cell.style = stylePatchApply(cell.style, slimr::Json::object_t{{"backgroundColor", red}});
            }
            tbl.rows[0].style = stylePatchApply(tbl.rows[0].style, slimr::Json::object_t{
                {"minRowHeight", dimensionFromPt(30)}
            });
        });
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{
        "updateTableRowStyle",
        "updateTableCellStyle",
        "updateTableCellStyle",
        "updateTableColumnProperties"
    });
}

TEST_CASE("kept tables - deleting a table") {
    auto json = docWithBlocks({paraSpec({"a"}), twoByTwoTable(), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        blocksDelete(t, {k[1]});
    });
    CHECK(r.compare.diffs.empty());
    CHECK(requestKinds(r.requests) == std::vector<std::string>{"deleteContentRange"});
}

TEST_CASE("section breaks - inserting a continuous section break") {
    auto json = docWithBlocks({paraSpec({"a"}), paraSpec({"b"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        sectionBreakInsert(t, 1, "CONTINUOUS");
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "insertSectionBreak") != kinds.end());
}

TEST_CASE("section breaks - changing a kept section break's columns") {
    BlockSpec sbSpec = BlockSpec::make_section_break(slimr::Json::object_t{
        {"columnSeparatorStyle", "NONE"},
        {"contentDirection", "LEFT_TO_RIGHT"},
        {"sectionType", "CONTINUOUS"}
    });
    auto json = docWithBlocks({paraSpec({"a"}), sbSpec, paraSpec({"b"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        auto& sb = t.tab.blocks[1].as_section_break();
        sb.sectionStyle["columnSeparatorStyle"] = "BETWEEN_EACH_COLUMN";
    });
    CHECK(r.compare.diffs.empty());
    CHECK(r.requests.size() == 1);
    CHECK(r.requests[0].contains("updateSectionStyle"));
    CHECK(r.requests[0]["updateSectionStyle"]["fields"].get<std::string>() == "columnSeparatorStyle");
}

} // namespace gdocsmith::reconcile::test
