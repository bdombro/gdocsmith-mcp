#pragma once

#include "lens/put.hpp"
#include "lens/parse.hpp"
#include "lens/json_helper.hpp"
#include "model/test_docs.hpp"
#include "reconcile/reconcile.hpp"
#include "engine/guard.hpp"
#include "engine/suggestion_rule.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("markdownPut updates tab model blocks from parsed markdown") {
    slimr::Json tab = slimr::json_obj();
    tab["blocks"] = slimr::json_arr();

    std::string edited_md =
        "# Updated Title\n\n"
        "New body paragraph.\n";

    auto parsed = markdownParse(edited_md);
    auto report = markdownPut(tab, parsed);

    CHECK(report.changed);
    const auto& blocks = tab["blocks"];
    CHECK(blocks.get<slimr::Json::array_t>().size() == 2u);

    auto report2 = markdownPut(tab, parsed);
    CHECK(!report2.changed);
}

inline model::TabModel richPutFixture() {
    model::TabModel tab;
    tab.tabId = "t.rich";
    model::ParagraphBlock paragraph;
    paragraph.key = "original-paragraph";
    paragraph.headingId = "heading-identity";
    paragraph.style = slimr::Json::object_t{{"alignment", "CENTER"}, {"spaceAbove", model::dimensionFromPt(12)}};
    paragraph.newline.style = slimr::Json::object_t{{"italic", true}};
    auto text = model::Inline::from_text("the quick fox");
    text.style = slimr::Json::object_t{
        {"bold", true}, {"fontSize", model::dimensionFromPt(17)},
        {"weightedFontFamily", slimr::Json::object_t{{"fontFamily", "Georgia"}, {"weight", 600}}},
        {"foregroundColor", model::colorOptionalFromHex("#123456")}
    };
    paragraph.inlines.push_back(text);
    model::Atom atom;
    atom.key = "original-image";
    atom.type = "image";
    atom.raw = slimr::Json::object_t{{"inlineObjectId", "image-remote"}};
    paragraph.inlines.emplace_back(atom);
    tab.blocks.emplace_back(paragraph);
    model::ParagraphBlock cellParagraph;
    cellParagraph.key = "original-cell-paragraph";
    cellParagraph.inlines.push_back(model::Inline::from_text("cell", text.style));
    model::CellModel cell;
    cell.key = "original-cell";
    cell.style = slimr::Json::object_t{{"paddingTop", model::dimensionFromPt(9)}};
    cell.blocks.emplace_back(cellParagraph);
    model::RowModel row;
    row.key = "original-row";
    row.style = slimr::Json::object_t{{"minRowHeight", model::dimensionFromPt(24)}};
    row.cells.push_back(cell);
    model::TableBlock table;
    table.key = "original-table";
    table.rows.push_back(row);
    table.columns.push_back({.key = "original-column", .props = slimr::Json::object_t{{"widthType", "EVENLY_DISTRIBUTED"}}});
    tab.blocks.emplace_back(table);
    model::ParagraphBlock tail;
    tail.key = "original-tail";
    tab.blocks.emplace_back(tail);
    model::SegmentModel segment;
    segment.id = "header-id";
    segment.kind = "header";
    segment.tab = std::make_shared<model::TabModel>();
    segment.tab->blocks.emplace_back(cellParagraph);
    tab.segments.push_back(segment);
    return tab;
}

TEST_CASE("typed putback rich no-op preserves complete model and edit metadata") {
    auto tab = richPutFixture();
    const auto original = tab;
    model::EditContext context;
    const auto markdown = markdownRender(projectionBuild(tab));
    CHECK(markdown.find("fontFamily=Georgia") != std::string::npos);
    CHECK(markdown.find("foregroundColor=#123456") != std::string::npos);
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    CHECK(tab == original);
    CHECK(tab.blocks == original.blocks);
    CHECK(tab.inlineObjects == original.inlineObjects);
    CHECK(context.tombstones.empty());
}

TEST_CASE("typed putback word edit preserves rich styles paragraph identity and adjacent atom") {
    auto tab = richPutFixture();
    const auto original = tab;
    model::EditContext context;
    auto markdown = markdownRender(projectionBuild(tab));
    markdown.replace(markdown.find("quick"), 5, "slow");
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    const auto& paragraph = tab.blocks[0].as_paragraph();
    CHECK(paragraph.key == original.blocks[0].key);
    CHECK(paragraph.style == original.blocks[0].as_paragraph().style);
    CHECK(paragraph.headingId == original.blocks[0].as_paragraph().headingId);
    CHECK(paragraph.newline == original.blocks[0].as_paragraph().newline);
    REQUIRE(paragraph.inlines.size() == 2);
    CHECK(paragraph.inlines[0].text == "the slow fox");
    CHECK(model::styleEqual(paragraph.inlines[0].style, original.blocks[0].as_paragraph().inlines[0].style));
    CHECK(paragraph.inlines[1] == original.blocks[0].as_paragraph().inlines[1]);
    CHECK(tab.blocks[1] == original.blocks[1]);
    CHECK(tab.segments == original.segments);
}

TEST_CASE("typed putback cell edit preserves table layout and identities") {
    auto tab = richPutFixture();
    const auto original = tab;
    model::EditContext context;
    auto markdown = markdownRender(projectionBuild(tab));
    const auto pos = markdown.find(":style[cell]");
    REQUIRE(pos != std::string::npos);
    markdown.replace(pos, 12, ":style[edited]");
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    const auto& table = tab.blocks[1].as_table();
    const auto& old = original.blocks[1].as_table();
    CHECK(table.key == old.key);
    CHECK(table.columns == old.columns);
    CHECK(table.rows[0].key == old.rows[0].key);
    CHECK(table.rows[0].style == old.rows[0].style);
    const auto& cell = table.rows[0].cells[0];
    CHECK(cell.key == old.rows[0].cells[0].key);
    CHECK(cell.style == old.rows[0].cells[0].style);
    CHECK(cell.blocks[0].key == old.rows[0].cells[0].blocks[0].key);
    CHECK(cell.blocks[0].as_paragraph().inlines[0].text == "edited");
    CHECK(model::styleEqual(cell.blocks[0].as_paragraph().inlines[0].style,
                           old.rows[0].cells[0].blocks[0].as_paragraph().inlines[0].style));
}

TEST_CASE("typed putback applies heading list and text formatting edits") {
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab}, "# Title\n\n- **item**\n\nend\n").changed);
    CHECK(tab.blocks[0].as_paragraph().style["namedStyleType"].get<std::string>() == "HEADING_1");
    const auto& item = tab.blocks[1].as_paragraph();
    REQUIRE(item.bullet.has_value());
    CHECK(model::listKind(tab.lists.at(item.bullet->listId), 0) == "bullet");
    CHECK(item.inlines[0].text == "item");
    CHECK(item.inlines[0].style["bold"].get<bool>());
    auto markdown = markdownRender(projectionBuild(tab));
    markdown.replace(markdown.find("- **item**"), 10, "  1. *item*");
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    const auto& changed = tab.blocks[1].as_paragraph();
    CHECK(changed.bullet->nestingLevel == 1);
    CHECK(model::listKind(tab.lists.at(changed.bullet->listId), 1) == "number");
    CHECK(changed.inlines[0].style["italic"].get<bool>());
    CHECK_FALSE(changed.inlines[0].style["bold"].get<bool>());
}

TEST_CASE("typed putback refuses protected deletion and review edits atomically") {
    auto tab = richPutFixture();
    tab.blocks[0].as_paragraph().is_protected = true;
    auto original = tab;
    model::EditContext context;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, "replace everything"), model::CoreError);
    CHECK(tab == original);
    CHECK(context.tombstones.empty());
    tab.blocks[0].as_paragraph().is_protected = false;
    tab.blocks[0].as_paragraph().inlines[0].suggestedInsertionIds = std::vector<std::string>{"suggestion"};
    original = tab;
    auto markdown = markdownRender(projectionBuild(tab));
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    markdown.insert(0, "edited ");
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, markdown), model::CoreError);
    CHECK(tab == original);
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab},
        ":comment{id=c start}body:comment{id=c end}"), model::CoreError);
    CHECK(tab == original);
}

inline model::JsonObject parityTableJson() {
    model::DocSpec spec;
    model::TabSpec tab;
    model::TableBlockSpec table;
    for (const auto& texts : {std::vector<std::string>{"a", "b"}, {"1", "2"}}) {
        std::vector<std::vector<model::BlockSpec>> row;
        for (const auto& text : texts) {
            model::ParagraphSpec paragraph;
            paragraph.content.emplace_back(text);
            row.push_back({model::BlockSpec::make_paragraph(paragraph)});
        }
        table.cells.push_back(std::move(row));
    }
    model::ParagraphSpec intro;
    intro.content = {std::string("intro")};
    tab.blocks.push_back(model::BlockSpec::make_paragraph(intro));
    tab.blocks.push_back(model::BlockSpec::make_table(table));
    intro.content = {std::string("after")};
    tab.blocks.push_back(model::BlockSpec::make_paragraph(intro));
    spec.tabs.push_back(tab);
    return model::docJsonBuild(spec);
}

inline const model::TableBlock& parityTable(const model::TabModel& tab) {
    const auto found = std::find_if(tab.blocks.begin(), tab.blocks.end(), [](const auto& block) { return block.is_table(); });
    if (found == tab.blocks.end()) throw model::CoreError("internal", "Expected a table in parity fixture.");
    return found->as_table();
}

TEST_CASE("Node image copies retain loss provenance without move permission and repeated ids reject replacement attributes") {
    model::TabModel tab;
    tab.tabId = "t.image";
    model::ParagraphBlock paragraph;
    paragraph.key = "paragraph";
    paragraph.inlines.push_back(model::Inline::from_text("prefix"));
    model::Atom atom{.key = "original-image", .type = "image",
        .raw = slimr::Json::object_t{{"inlineObjectElement",
            slimr::Json::object_t{{"inlineObjectId", "remote-image"}}}}};
    paragraph.inlines.emplace_back(atom);
    tab.blocks.emplace_back(paragraph);
    tab.inlineObjects["remote-image"]["inlineObjectProperties"]["embeddedObject"]["imageProperties"] =
        slimr::Json::object_t{{"contentUri", "https://example.invalid/synthetic"},
            {"cropProperties", slimr::Json::object_t{{"offsetLeft", 0.1}}}};
    const auto original = tab;
    model::EditContext context;
    const auto body = markdownRender(projectionBuild(tab));
    const auto begin = body.find(":image");
    REQUIRE(begin != std::string::npos);
    const auto token = body.substr(begin, body.find('}', begin) - begin + 1);
    const auto copy = markdownPut(model::EditTarget{context, tab}, body + "\n" + token + "\n");
    REQUIRE(copy.recreatedImages.size() == 1);
    CHECK_FALSE(copy.recreatedImages[0].lost.empty());
    CHECK(tab.blocks[0].as_paragraph().inlines.back().key == atom.key);
    tab = original;
    auto replacement = token;
    replacement.insert(replacement.size() - 1, " src=https://example.invalid/replacement");
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, body + "\n" + replacement + "\n"), model::CoreError);
    CHECK(tab == original);
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, "prefix\n\n" + token + "\n"), model::CoreError);
    CHECK(tab == original);
    const auto move = markdownPut(model::EditTarget{context, tab}, "prefix\n\n" + token + "\n",
        {.allowLossyImageMove = true});
    CHECK(move.changed);
    REQUIRE(move.recreatedImages.size() == 1);
    CHECK(tab.blocks[1].as_paragraph().inlines[0].key != atom.key);
    model::TabModel foreignCopy;
    int resolutions = 0;
    AtomPutOptions options;
    options.images = [&](const std::string&) -> std::optional<ImageSource> {
        ++resolutions;
        ImageSource source;
        source.create = {.type = "image", .uri = "https://example.invalid/foreign"};
        source.lost = {"crop"};
        source.source = {.docId = "foreign", .key = "foreign-key", .tabId = "foreign-tab"};
        return source;
    };
    const auto foreign = markdownPut(model::EditTarget{context, foreignCopy}, ":image{id=foreign}", options,
        {.allowTokenOmission = true});
    CHECK(foreign.changed);
    CHECK(foreign.notes.empty());
    REQUIRE(foreign.recreatedImages.size() == 1);
    CHECK(foreign.recreatedImages[0].lost == std::vector<std::string>{"crop"});
    CHECK(resolutions == 1);
}

TEST_CASE("Node opening default s1 carries review markers and opening-only settings edits remain effective") {
    model::TabModel tab;
    model::ParagraphBlock paragraph;
    paragraph.key = "paragraph";
    auto text = model::Inline::from_text("reviewed");
    text.suggestedDeletionIds = std::vector<std::string>{"synthetic-review"};
    paragraph.inlines.push_back(text);
    tab.blocks.emplace_back(paragraph);
    model::EditContext context;
    const auto original = tab;
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab},
        "::section{id=s1}\n\n" + markdownRender(projectionBuild(tab))).changed);
    CHECK(tab == original);
    tab.blocks[0].as_paragraph().inlines[0].suggestedDeletionIds.reset();
    CHECK(markdownPut(model::EditTarget{context, tab},
        "::section{id=s1 columns=2}\n\n" + markdownRender(projectionBuild(tab))).changed);
    CHECK(sectionSettingsOf(tab.leadingSectionStyle, tab.documentStyle).columns == 2);
}

TEST_CASE("Node creation-only opaque token omission is explicit and never omits foreign image sources or creatable chips") {
    model::TabModel tab;
    model::EditContext context;
    try {
        markdownPut(model::EditTarget{context, tab}, "x :equation[Formula]{id=foreign}");
        FAIL("unknown opaque identity must refuse by default");
    } catch (const model::CoreError& error) {
        CHECK(error.code == "anchorNotFound");
    }
    CHECK(tab.blocks.empty());
    const auto report = markdownPut(model::EditTarget{context, tab}, "x :equation[Formula]{id=foreign}", {},
        {.allowTokenOmission = true});
    CHECK(report.notes == std::vector<std::string>{
        "Omitted equation \"Formula\" (id=foreign): this object cannot transfer; use file copy to preserve it."});
    CHECK(markdownRender(projectionBuild(tab)).find(":equation") == std::string::npos);
    const auto image = markdownPut(model::EditTarget{context, tab}, "x :image{id=foreign}", {},
        {.allowTokenOmission = true});
    CHECK(image.notes == std::vector<std::string>{
        "Omitted image (id=foreign): this object cannot transfer; use file copy to preserve it."});
    const auto saved = tab;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, "x :image{src=gdoc:foreign/id}", {},
        {.allowTokenOmission = true}), model::CoreError);
    CHECK(tab == saved);
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, "x :equation[Formula]{}", {},
        {.allowTokenOmission = true}), model::CoreError);
    CHECK(tab == saved);
    const auto person = markdownPut(model::EditTarget{context, tab},
        "x :person{id=foreign email=person@example.invalid}", {}, {.allowTokenOmission = true});
    CHECK(person.notes.empty());
    REQUIRE(tab.blocks[0].as_paragraph().inlines.back().create);
    CHECK(tab.blocks[0].as_paragraph().inlines.back().create->email == "person@example.invalid");
    model::TabModel known;
    model::ParagraphBlock paragraph;
    paragraph.key = "known-paragraph";
    model::Atom equation{.key = "known-equation", .type = "equation",
        .raw = slimr::Json::object_t{{"equation", slimr::Json::object_t{}}}};
    paragraph.inlines.emplace_back(equation);
    known.blocks.emplace_back(paragraph);
    const auto knownToken = markdownRender(projectionBuild(known));
    const auto carried = markdownPut(model::EditTarget{context, known}, "prefix " + knownToken, {},
        {.allowTokenOmission = true});
    CHECK(carried.notes.empty());
    CHECK(known.blocks[0].as_paragraph().inlines.back().key == equation.key);
    const auto beforeCopy = known;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, known},
        markdownRender(projectionBuild(known)) + "\n" + knownToken, {}, {.allowTokenOmission = true}), model::CoreError);
    CHECK(known == beforeCopy);
}

TEST_CASE("Node suggested paragraph break refuses joined edits but allows an unrelated edit") {
    model::TabModel tab;
    for (const auto& text : {"left", "right", "outside"}) {
        model::ParagraphBlock paragraph;
        paragraph.key = text;
        paragraph.inlines.push_back(model::Inline::from_text(text));
        tab.blocks.emplace_back(paragraph);
    }
    tab.blocks[0].as_paragraph().newline.suggestedInsertionIds = std::vector<std::string>{"synthetic-break"};
    model::EditContext context;
    const auto original = tab;
    auto markdown = markdownRender(projectionBuild(tab));
    auto joined = markdown;
    joined.replace(joined.find("left"), 4, "LEFT");
    try {
        markdownPut(model::EditTarget{context, tab}, joined);
        FAIL("joined suggested paragraph edits must refuse");
    } catch (const model::CoreError& error) {
        CHECK(error.code == "suggestionConflict");
        CHECK(std::string(error.what()) ==
            "this paragraph holds a pending suggested paragraph break (synthetic-break), so it can't be edited; add reject to its suggestion block first");
    }
    CHECK(tab == original);
    markdown.replace(markdown.find("outside"), 7, "OUTSIDE");
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    REQUIRE(tab.blocks.size() == 3);
    CHECK(tab.blocks[0] == original.blocks[0]);
    CHECK(tab.blocks[1] == original.blocks[1]);
    CHECK(tab.blocks[2].as_paragraph().inlines[0].text == "OUTSIDE");
}

TEST_CASE("Node put969 table settings write request payloads reset absent fields and repeat atomically") {
    const auto json = parityTableJson();
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    auto& tab = final.tabs[0];
    auto markdown = markdownRender(projectionBuild(tab));
    CHECK(markdown.find("::table") == std::string::npos);
    const auto grid = markdown;
    const std::string settings = "::table{widths=120,auto headerRows=1 heights=30,0 preventOverflow=2}\n";
    markdown.insert(markdown.find('|'), settings);
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    const auto& table = parityTable(tab);
    CHECK(table.key == parityTable(original.tabs[0]).key);
    CHECK(table.columns[0].props["width"] == model::dimensionFromPt(120));
    CHECK(table.columns[1].props["widthType"] == slimr::Json("EVENLY_DISTRIBUTED"));
    CHECK(table.rows[0].style["tableHeader"] == slimr::Json(true));
    CHECK(table.rows[1].style["preventOverflow"] == slimr::Json(true));
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    std::set<std::string> kinds;
    for (const auto& request : plan.contentRequests) kinds.insert(request.get<slimr::Json::object_t>().begin()->first);
    CHECK(kinds.contains("updateTableColumnProperties"));
    CHECK(kinds.contains("updateTableRowStyle"));
    CHECK(kinds.contains("pinTableHeaderRows"));
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, markdownRender(projectionBuild(tab))).changed);
    auto reset = grid;
    reset.insert(reset.find('|'), "::table{widths=120,auto}\n");
    CHECK(markdownPut(model::EditTarget{context, tab}, reset).changed);
    CHECK_FALSE(parityTable(tab).rows[0].style.contains("tableHeader"));
    CHECK_FALSE(parityTable(tab).rows[1].style.contains("preventOverflow"));
    CHECK(model::dimensionPt(parityTable(tab).rows[0].style["minRowHeight"]) == 0);
    for (const auto& line : {"widths=100", "heights=10,10,10", "headerRows=3", "preventOverflow=5", "border=1", "widths=x,1"}) {
        const auto saved = tab;
        const auto tombstones = context.tombstones;
        auto invalid = grid;
        invalid.insert(invalid.find('|'), "::table{" + std::string(line) + "}\n");
        CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, invalid), model::CoreError);
        CHECK(tab == saved);
        CHECK(context.tombstones == tombstones);
    }
}

TEST_CASE("Node new table mixed fixed and automatic widths clears provider default fixed width") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::ParagraphSpec paragraph;
    paragraph.content = {std::string("intro")};
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(paragraph));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]},
        "intro\n\n::table{widths=120,auto}\n| a | b |\n| --- | --- |\n| 1 | 2 |\n").changed);
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
}

TEST_CASE("Node table-only cell-style edits do not invent paragraph anchors") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::TableBlockSpec table;
    std::vector<std::vector<model::BlockSpec>> row;
    for (const auto& text : {"a", "b"}) {
        model::ParagraphSpec paragraph;
        paragraph.content = {std::string(text)};
        row.push_back({model::BlockSpec::make_paragraph(paragraph)});
    }
    table.cells.push_back(row);
    tabSpec.blocks.push_back(model::BlockSpec::make_table(table));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    auto markdown = markdownRender(projectionBuild(final.tabs[0]));
    markdown.insert(markdown.find('|', markdown.find('|') + 1), " :cell{.green} ");
    markdown = "---\ncellStyles:\n  green:\n    backgroundColor: '#00FF00'\n---\n" + markdown;
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
    CHECK(final.tabs[0].blocks.size() == original.tabs[0].blocks.size());
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
}

TEST_CASE("Node put908 downward merge preserves actual content through request planning") {
    const auto json = parityTableJson();
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId="synthetic", .keys=keys});
    auto final = original;
    model::EditContext context{.keys=keys};
    CHECK(markdownPut(model::EditTarget{context,final.tabs[0]},
        "intro\n\n| a | :merge{down} |\n| --- | --- |\n| 1 | 2 |\n\nafter\n").changed);
    const auto& table = parityTable(final.tabs[0]);
    CHECK(table.rows[0].cells[1].blocks[0].as_paragraph().inlines[0].text == "2");
    CHECK(table.rows[0].cells[1].style["rowSpan"] == slimr::Json(2));
    reconcile::DocReconcileInput input{.final=final,.original=original,.originalJson=json};
    const auto plan = reconcile::docReconcile(input);
    const auto check = reconcile::docPlanSelfCheck(plan,input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
}

TEST_CASE("Node put1152 raw hidden chips cannot be authored or moved but weighted retention and deletion work") {
    model::TabModel tab;
    model::ParagraphBlock paragraph;
    paragraph.key = "paragraph";
    paragraph.inlines.push_back(model::Inline::from_text("abc"));
    tab.blocks.emplace_back(paragraph);
    model::EditContext context;
    const auto plain = tab;
    try {
        markdownPut(model::EditTarget{context,tab}, "abc\uE907");
        FAIL("new hidden chip must refuse");
    } catch (const model::CoreError& error) { CHECK(error.code == "unrecreatableMove"); }
    CHECK(tab == plain);
    tab.blocks[0].as_paragraph().inlines[0].text = "\uE907abc";
    CHECK(markdownPut(model::EditTarget{context,tab}, "abc\uE907").changed);
    CHECK(markdownRender(projectionBuild(tab)).find("abc\uE907") != std::string::npos);
    CHECK(markdownPut(model::EditTarget{context,tab}, "abc").changed);
    tab.blocks[0].as_paragraph().inlines[0].text = "\uE907abc";
    const auto saved = tab;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context,tab}, "abc\n\n\uE907"),model::CoreError);
    CHECK(tab == saved);
}

TEST_CASE("Node put827 edits beside retained deletion and style metadata preserve the reviewed symbols") {
    for (bool deletion : {false,true}) {
        model::DocSpec spec;
        model::TabSpec tabSpec;
        model::ParagraphSpec paragraph;
        model::ParagraphTextSpec reviewed{.text="reviewed"};
        if (deletion) reviewed.sugDel = std::vector<std::string>{"synthetic"};
        else reviewed.sugStyle = slimr::Json::object_t{{"synthetic",slimr::Json::object_t{{"bold",true}}}};
        paragraph.content = {std::string("prefix "),reviewed};
        tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(paragraph));
        spec.tabs.push_back(tabSpec);
        const auto json=model::docJsonBuild(spec);
        model::KeyAllocator keys;
        const auto original=model::docModelParse(json,{.docId="synthetic",.keys=keys});
        auto final=original;
        auto& tab=final.tabs[0];
        model::EditContext context{.keys=keys};
        auto markdown = markdownRender(projectionBuild(tab));
        markdown.replace(markdown.find("prefix"),6,"PREFIX");
        CHECK(markdownPut(model::EditTarget{context,tab},markdown).changed);
        const auto symbols = model::paragraphSymbols(tab.blocks[0].as_paragraph());
        const auto old = model::paragraphSymbols(original.tabs[0].blocks[0].as_paragraph());
        REQUIRE(symbols.size() == old.size());
        for (size_t i=7;i<old.size();++i) CHECK(symbols[i] == old[i]);
        reconcile::DocReconcileInput input{.final=final,.original=original,.originalJson=json};
        const auto plan=reconcile::docReconcile(input);
        const auto check=reconcile::docPlanSelfCheck(plan,input);
        INFO((check.diffs.empty() ? "" : check.diffs.front()));
        CHECK(check.equal);
    }
}
TEST_CASE("Node table geometry retains matched row column cell identity and emits structural requests") {
    const auto json = parityTableJson();
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    auto& tab = final.tabs[0];
    CHECK(markdownPut(model::EditTarget{context, tab},
        "intro\n\n| a | added | b |\n| --- | --- | --- |\n| 1 | new | 2 |\n| 3 | row | 4 |\n\nafter\n").changed);
    const auto& table = parityTable(tab);
    const auto& old = parityTable(original.tabs[0]);
    CHECK(table.key == old.key);
    REQUIRE(table.columns.size() == 3);
    REQUIRE(table.rows.size() == 3);
    CHECK(table.columns[0].key == old.columns[0].key);
    CHECK(table.columns[2].key == old.columns[1].key);
    CHECK(table.rows[1].key == old.rows[1].key);
    CHECK(table.rows[1].cells[2].key == old.rows[1].cells[1].key);
    CHECK(table.rows[1].cells[1].blocks[0].as_paragraph().inlines[0].text == "new");
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    bool row = false, column = false, deleteWhole = false;
    for (const auto& request : plan.contentRequests) {
        row = row || request.contains("insertTableRow");
        column = column || request.contains("insertTableColumn");
        deleteWhole = deleteWhole || request.contains("deleteContentRange");
    }
    CHECK(row);
    CHECK(column);
    CHECK_FALSE(deleteWhole);
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
}

TEST_CASE("Node unlinked heading deletion plans successfully while real inbound links retain their guard") {
    for (bool linked : {false,true}) {
        model::DocSpec spec;
        model::TabSpec tab;
        model::ParagraphSpec heading;
        heading.content = {std::string("Goals")};
        heading.headingId = "provider-heading";
        heading.style = slimr::Json::object_t{{"namedStyleType","HEADING_1"}};
        tab.blocks.push_back(model::BlockSpec::make_paragraph(heading));
        model::ParagraphSpec body;
        model::ParagraphTextSpec text{.text="outside"};
        if (linked) text.style = slimr::Json::object_t{{"link",slimr::Json::object_t{{"headingId","provider-heading"},{"tabId","t.0"}}}};
        body.content.push_back(text);
        tab.blocks.push_back(model::BlockSpec::make_paragraph(body));
        spec.tabs.push_back(tab);
        const auto json = model::docJsonBuild(spec);
        model::KeyAllocator keys;
        const auto original = model::docModelParse(json,{.docId="synthetic",.keys=keys});
        auto final = original;
        model::EditContext context{.keys=keys};
        auto markdown = markdownRender(projectionBuild(final.tabs[0],&final));
        markdown = markdown.substr(markdown.find("\n\n")+2);
        CHECK(markdownPut(model::EditTarget{context,final.tabs[0]},markdown).changed);
        CHECK_FALSE(context.tombstones.empty());
        reconcile::DocReconcileInput input{.final=final,.original=original,.originalJson=json};
        const auto plan = reconcile::docReconcile(input);
        CHECK_FALSE(plan.deletedRanges.empty());
        CHECK(reconcile::docPlanSelfCheck(plan,input).equal);
        engine::GuardDoc guarded;
        guarded.original=original; guarded.final=final; guarded.plan=plan; guarded.tombstones=context.tombstones;
        const auto findings = engine::guardEvaluate({.docs={guarded}});
        CHECK(std::any_of(findings.begin(),findings.end(),[](const auto& finding) {
            return finding.kind=="headingLink" && finding.severity=="block";
        }) == linked);
    }
}
TEST_CASE("Node table merge intent emits real merge requests and preserves surviving identities") {
    const auto json = parityTableJson();
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]},
        "intro\n\n| a | :merge{left} |\n| --- | --- |\n| 1 | 2 |\n\nafter\n").changed);
    const auto& table = parityTable(final.tabs[0]);
    CHECK(table.rows[0].cells[0].key == parityTable(original.tabs[0]).rows[0].cells[0].key);
    CHECK(table.rows[0].cells[0].style["columnSpan"] == slimr::Json(2));
    CHECK(table.rows[0].cells[1].blocks[0].as_paragraph().inlines.empty());
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    CHECK(std::any_of(plan.contentRequests.begin(), plan.contentRequests.end(), [](const auto& request) { return request.contains("mergeTableCells"); }));
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
    CHECK_FALSE(markdownPut(model::EditTarget{context, final.tabs[0]}, markdownRender(projectionBuild(final.tabs[0]))).changed);
}

TEST_CASE("Node put1060 section settings write reset Google-clearable fields and refuse impossible clears") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::ParagraphSpec text;
    text.content.emplace_back(std::string("intro"));
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(text));
    tabSpec.blocks.push_back(model::BlockSpec::make_section_break(
        slimr::Json::object_t{{"sectionType", "CONTINUOUS"}, {"columnSeparatorStyle", "NONE"}}));
    text.content = {std::string("after")};
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(text));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    auto markdown = markdownRender(projectionBuild(final.tabs[0]));
    const std::string line = "::section{id=s2 continuous}";
    REQUIRE(markdown.find(line) != std::string::npos);
    const std::string written = "::section{id=s2 columns=2 columnGap=18 margins=36,36,36,36 pageNumberStart=5 continuous columnSeparator firstPage landscape rtl}";
    markdown.replace(markdown.find(line), line.size(), written);
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
    CHECK(final.tabs[0].blocks[1].key == original.tabs[0].blocks[1].key);
    CHECK(final.tabs[0].blocks[1].as_section_break().sectionStyle["columnProperties"].get<slimr::Json::array_t>().size() == 2);
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    CHECK(std::any_of(plan.contentRequests.begin(), plan.contentRequests.end(), [](const auto& request) { return request.contains("updateSectionStyle"); }));
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
    markdown.replace(markdown.find(written), written.size(), "::section{id=s2 margins=36,36,36,36 pageNumberStart=5 continuous}");
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
    CHECK(final.tabs[0].blocks[1].as_section_break().sectionStyle["useFirstPageHeaderFooter"] == slimr::Json(false));
    const auto saved = final.tabs[0];
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, final.tabs[0]}, "intro\n\n::section{id=s2 continuous}\n\nafter\n"), model::CoreError);
    CHECK(final.tabs[0] == saved);
}

TEST_CASE("Node declaration edits apply reused names null reset and removed fields through typed put") {
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab},
        "---\ntextStyles:\n  accent:\n    bold: true\n    fontSize: 18\nparagraphStyles:\n  layout:\n    alignment: CENTER\n    keepWithNext: true\n---\n"
        ":style[body]{.accent} :p{.layout}\n").changed);
    const auto key = tab.blocks[0].key;
    auto& paragraph = tab.blocks[0].as_paragraph();
    CHECK(paragraph.inlines[0].style["bold"] == slimr::Json(true));
    CHECK(paragraph.inlines[0].style["fontSize"] == model::dimensionFromPt(18));
    CHECK(paragraph.style["alignment"] == slimr::Json("CENTER"));
    CHECK(paragraph.style["keepWithNext"] == slimr::Json(true));
    CHECK(markdownPut(model::EditTarget{context, tab},
        "---\ntextStyles:\n  accent:\n    bold: null\n    fontSize: null\nparagraphStyles:\n  layout:\n    alignment: null\n    keepWithNext: false\n---\n"
        ":style[body]{.accent} :p{.layout}\n").changed);
    CHECK(tab.blocks[0].key == key);
    const auto& reset = tab.blocks[0].as_paragraph();
    CHECK_FALSE(reset.inlines[0].style.contains("bold"));
    CHECK_FALSE(reset.inlines[0].style.contains("fontSize"));
    CHECK_FALSE(reset.style.contains("alignment"));
    CHECK(reset.style["keepWithNext"] == slimr::Json(false));
}

TEST_CASE("Node cell declaration resets restore fresh padding and mirror shared borders atomically") {
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab},
        "---\ncellStyles:\n  shaded:\n    paddingTop: 12\n    backgroundColor: '#112233'\n"
        "    borderRight: {color: '#445566', width: 2, dashStyle: SOLID}\n---\n"
        "| a :cell{.shaded} | b |\n| --- | --- |\n").changed);
    const auto& table = parityTable(tab);
    CHECK(table.rows[0].cells[0].style["paddingTop"] == model::dimensionFromPt(12));
    CHECK(table.rows[0].cells[0].style["borderRight"] == table.rows[0].cells[1].style["borderLeft"]);
    const auto saved = tab;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab},
        "---\ncellStyles:\n  a:\n    borderRight: {color: '#445566', width: 2, dashStyle: SOLID}\n"
        "  b:\n    borderLeft: {color: '#000000', width: 2, dashStyle: SOLID}\n---\n"
        "| a :cell{.a} | b :cell{.b} |\n| --- | --- |\n"), model::CoreError);
    CHECK(tab == saved);
    CHECK(markdownPut(model::EditTarget{context, tab},
        "---\ncellStyles:\n  cleared:\n    paddingTop: null\n    backgroundColor: null\n    borderRight: null\n---\n"
        "| a :cell{.cleared} | b |\n| --- | --- |\n").changed);
    const auto& reset = parityTable(tab);
    CHECK(reset.rows[0].cells[0].style["paddingTop"] == model::dimensionFromPt(5));
    CHECK_FALSE(reset.rows[0].cells[0].style.contains("backgroundColor"));
    CHECK_FALSE(reset.rows[0].cells[0].style.contains("borderRight"));
    CHECK_FALSE(reset.rows[0].cells[1].style.contains("borderLeft"));
}

TEST_CASE("Node put1093 person creation and email replacement reach real requests with fresh atom identity") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::ParagraphSpec paragraph;
    paragraph.content = {std::string("a")};
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(paragraph));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, "a :person{id=zzzz email=first@example.com}\n").changed);
    const auto& created = final.tabs[0].blocks[0].as_paragraph();
    REQUIRE(created.inlines.back().create);
    CHECK(created.inlines.back().create->email == std::optional<std::string>{"first@example.com"});
    const auto firstKey = created.inlines.back().key;
    auto markdown = markdownRender(projectionBuild(final.tabs[0]));
    CHECK(markdown.find("id=zzzz") == std::string::npos);
    CHECK(markdown.find("email=first@example.com") != std::string::npos);
    const auto email = markdown.find("email=first@example.com");
    markdown.replace(email, 23, "email=second@example.com");
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
    const auto& changed = final.tabs[0].blocks[0].as_paragraph();
    CHECK(changed.key == original.tabs[0].blocks[0].key);
    CHECK(changed.inlines.back().key != firstKey);
    CHECK(changed.inlines.back().create->email == std::optional<std::string>{"second@example.com"});
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    CHECK(std::any_of(plan.contentRequests.begin(), plan.contentRequests.end(), [](const auto& request) { return request.contains("insertPerson"); }));
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
}

TEST_CASE("Node declarations drive paragraph text and shared-cell-style API payloads") {
    const auto json = parityTableJson();
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]},
        "---\ntextStyles:\n  accent: {bold: true, fontSize: 18}\n"
        "paragraphStyles:\n  layout: {alignment: CENTER, keepWithNext: true}\n"
        "cellStyles:\n  shaded:\n    paddingTop: 12\n    backgroundColor: '#112233'\n"
        "    borderRight: {color: '#445566', width: 2, dashStyle: SOLID}\n---\n"
        "intro :p{.layout}\n\n| :style[a]{.accent} :cell{.shaded} | b |\n| --- | --- |\n| 1 | 2 |\n\nafter\n").changed);
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    const auto plan = reconcile::docReconcile(input);
    std::set<std::string> kinds;
    for (const auto& request : plan.contentRequests) kinds.insert(request.get<slimr::Json::object_t>().begin()->first);
    CHECK(kinds.contains("updateTextStyle"));
    CHECK(kinds.contains("updateParagraphStyle"));
    CHECK(kinds.contains("updateTableCellStyle"));
    const auto check = reconcile::docPlanSelfCheck(plan, input);
    INFO((check.diffs.empty() ? "" : check.diffs.front()));
    CHECK(check.equal);
}

TEST_CASE("Node put1003 list presets apply reset and reject incompatible kinds") {
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab},
        "::list{preset=NUMBERED_UPPERROMAN_UPPERALPHA_DECIMAL}\n1. one\n1. two\n").changed);
    const auto id = tab.blocks[0].as_paragraph().bullet->listId;
    CHECK(tab.lists.at(id).preset == std::optional<std::string>{"NUMBERED_UPPERROMAN_UPPERALPHA_DECIMAL"});
    CHECK(markdownRender(projectionBuild(tab)).starts_with("::list{preset=NUMBERED_UPPERROMAN_UPPERALPHA_DECIMAL}"));
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, markdownRender(projectionBuild(tab))).changed);
    CHECK(markdownPut(model::EditTarget{context, tab}, "1. one\n1. two\n").changed);
    CHECK(tab.lists.at(tab.blocks[0].as_paragraph().bullet->listId).preset == std::optional<std::string>{"NUMBERED_DECIMAL_ALPHA_ROMAN"});
    const auto saved = tab;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, "::list{preset=BOGUS}\n- one\n"), model::CoreError);
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, "::list{preset=NUMBERED_DECIMAL_NESTED}\n- one\n"), model::CoreError);
    CHECK(tab == saved);
}

TEST_CASE("Node put1093 copying a section creates a fresh break while type changes refuse atomically") {
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab}, "intro\n\n::section{continuous}\n\nafter\n").changed);
    const auto first = std::find_if(tab.blocks.begin(), tab.blocks.end(), [](const auto& block) { return block.is_section_break(); });
    REQUIRE(first != tab.blocks.end());
    const auto key = first->key;
    auto markdown = markdownRender(projectionBuild(tab));
    CHECK(markdown.find("::section{id=s2 continuous}") != std::string::npos);
    markdown += "\n::section{id=s2 continuous}\n\nlast\n";
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    const auto written = markdownRender(projectionBuild(tab));
    CHECK(written.find("::section{id=s3 continuous}") != std::string::npos);
    CHECK(std::any_of(tab.blocks.begin(), tab.blocks.end(), [&](const auto& block) { return block.key == key; }));
    const auto saved = tab;
    auto invalid = written;
    const auto type = invalid.find("continuous");
    invalid.replace(type, 10, "nextPage");
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, invalid), model::CoreError);
    CHECK(tab == saved);
}

TEST_CASE("typed putback creates tables and code content through the model") {
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab},
        "intro\n\n| A | B |\n| --- | --- |\n| C | D |\n\nend\n").changed);
    const auto table = std::find_if(tab.blocks.begin(), tab.blocks.end(),
                                   [](const auto& block) { return block.is_table(); });
    REQUIRE(table != tab.blocks.end());
    CHECK(table->as_table().rows[1].cells[1].blocks[0].as_paragraph().inlines[0].text == "D");
    const auto markdown = markdownRender(projectionBuild(tab));
    const auto original = tab;
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    CHECK(tab == original);
    model::TabModel code;
    CHECK(markdownPut(model::EditTarget{context, code}, "```\na = *x*\n```\n").changed);
    CHECK(code.blocks[0].as_paragraph().inlines[0].text == "a = *x*");
    CHECK(code.blocks[0].as_paragraph().inlines[0].style["weightedFontFamily"]["fontFamily"].get<std::string>() == "Roboto Mono");
}

inline model::TabModel rangePutFixture() {
    model::TabModel tab;
    tab.tabId = "t.range";
    model::ParagraphBlock paragraph;
    paragraph.key = "range-paragraph";
    paragraph.origin = model::Range{.end = 13, .start = 1};
    paragraph.inlines.push_back(model::Inline::from_text("hello world"));
    tab.blocks.emplace_back(paragraph);
    model::NamedRangeDef range;
    range.name = "selection";
    range.namedRangeId = "original-range";
    model::NamedRangeDef::RangePart part;
    part.start = 7;
    part.end = 12;
    range.ranges.push_back(part);
    tab.namedRanges.push_back(range);
    return tab;
}

TEST_CASE("typed putback named range no-op is untouched and prefix edit tracks retained symbols") {
    auto tab = rangePutFixture();
    const auto original = tab;
    model::EditContext context;
    const auto markdown = markdownRender(projectionBuild(tab));
    CHECK(markdown.find(":namedrange{name=selection start}") != std::string::npos);
    const auto unchanged = markdownPut(model::EditTarget{context, tab}, markdown);
    CHECK_FALSE(unchanged.changed);
    CHECK(unchanged.managedNamedRangeIds.empty());
    CHECK(tab == original);
    const auto report = markdownPut(model::EditTarget{context, tab}, "X " + markdown);
    CHECK(report.changed);
    CHECK(report.managedNamedRangeIds == std::vector<std::string>{"original-range"});
    REQUIRE(tab.namedRanges.size() == 1);
    CHECK(tab.namedRanges[0].namedRangeId == "original-range");
    const auto range = namedRangeExpected(tab, tab.namedRanges[0]);
    REQUIRE(range.has_value());
    CHECK(range->start == 9);
    CHECK(range->end == 14);
}

TEST_CASE("typed putback equal-offset changed adjacent symbol recreates range and deletion is explicit") {
    auto tab = rangePutFixture();
    model::EditContext context;
    auto markdown = markdownRender(projectionBuild(tab));
    markdown.replace(markdown.find("world"), 5, "zorld");
    const auto report = markdownPut(model::EditTarget{context, tab}, markdown);
    CHECK(report.changed);
    CHECK(report.managedNamedRangeIds == std::vector<std::string>{"original-range"});
    REQUIRE(tab.namedRanges.size() == 1);
    CHECK(tab.namedRanges[0].namedRangeId != "original-range");
    REQUIRE(tab.namedRanges[0].create.has_value());
    CHECK(tab.namedRanges[0].create->start.offset == 6);
    tab = rangePutFixture();
    const auto deleted = markdownPut(model::EditTarget{context, tab}, "hello world");
    CHECK(deleted.changed);
    CHECK(deleted.managedNamedRangeIds == std::vector<std::string>{"original-range"});
    CHECK(tab.namedRanges.empty());
}

TEST_CASE("typed putback invalid named markers leave model and intent untouched") {
    auto tab = rangePutFixture();
    const auto original = tab;
    model::EditContext context;
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab},
        "hello :namedrange{name=selection start}world"), model::CoreError);
    CHECK(tab == original);
    CHECK(context.tombstones.empty());
}

TEST_CASE("typed rich putback no-op emits zero requests and edited styles survive emulator readback") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::ParagraphSpec paragraph;
    const auto style = slimr::Json::object_t{
        {"bold", true}, {"fontSize", model::dimensionFromPt(17)},
        {"weightedFontFamily", slimr::Json::object_t{{"fontFamily", "Georgia"}}}};
    paragraph.content.emplace_back(model::ParagraphTextSpec{.text = "the quick fox", .style = style});
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(paragraph));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, {.docId = "d", .keys = keys});
    auto final = original;
    model::EditContext context{.keys = keys};
    auto markdown = markdownRender(projectionBuild(final.tabs[0]));
    CHECK_FALSE(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
    reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
    auto plan = reconcile::docReconcile(input);
    CHECK(plan.contentRequests.empty());
    CHECK(plan.tabOps.empty());
    CHECK(reconcile::docPlanSelfCheck(plan, input).equal);
    markdown.replace(markdown.find("quick"), 5, "slow");
    CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
    input.final = final;
    plan = reconcile::docReconcile(input);
    CHECK_FALSE(plan.contentRequests.empty());
    CHECK(reconcile::docPlanSelfCheck(plan, input).equal);
    const auto back = emulator::requestsEmulate(json, plan.contentRequests);
    const auto reopened = model::docModelParse(back.json, {.docId = "d"});
    const auto& text = reopened.tabs[0].blocks[0].as_paragraph().inlines[0];
    CHECK(text.text == "the slow fox");
    CHECK(text.style["bold"].get<bool>());
    CHECK(model::dimensionPt(text.style["fontSize"]) == 17);
    CHECK(text.style["weightedFontFamily"]["fontFamily"].get<std::string>() == "Georgia");
}

TEST_CASE("typed body headings lists and simple table creation reconcile through existing planner") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::ParagraphSpec paragraph;
    paragraph.content.emplace_back(std::string("original"));
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(paragraph));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    for (const auto& markdown : {
            "# Heading\n\n- item\n  - child\n\nend\n",
            "intro\n\n| A | B |\n| --- | --- |\n| C | D |\n\nend\n"}) {
        model::KeyAllocator keys;
        const auto original = model::docModelParse(json, {.docId = "d", .keys = keys});
        auto final = original;
        model::EditContext context{.keys = keys};
        CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
        reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
        const auto plan = reconcile::docReconcile(input);
        CHECK_FALSE(plan.contentRequests.empty());
        const auto check = reconcile::docPlanSelfCheck(plan, input);
        INFO((check.diffs.empty() ? "" : check.diffs.front()));
        CHECK(check.equal);
    }
}

TEST_CASE("typed word edits preserve checkbox identity bullet style and opaque links") {
    model::TabModel tab;
    model::ParagraphBlock paragraph;
    paragraph.key = "checkbox-paragraph";
    paragraph.bullet = model::BulletRef{
        .listId = "checkbox-list", .nestingLevel = 0,
        .textStyle = slimr::Json::object_t{{"bold", true}}};
    paragraph.inlines.push_back(model::Inline::from_text("quick",
        slimr::Json::object_t{{"link", slimr::Json::object_t{{"headingId", "remote-heading"}}}}));
    tab.blocks.emplace_back(paragraph);
    model::ListDef definition;
    definition.preset = "BULLET_CHECKBOX";
    tab.lists["checkbox-list"] = definition;
    model::EditContext context;
    auto markdown = markdownRender(projectionBuild(tab));
    markdown.replace(markdown.find("quick"), 5, "slow");
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    const auto& changed = tab.blocks[0].as_paragraph();
    CHECK(changed.bullet == paragraph.bullet);
    CHECK(tab.lists.at("checkbox-list") == definition);
    CHECK(changed.inlines[0].text == "slow");
    CHECK(changed.inlines[0].style["link"]["headingId"].get<std::string>() == "remote-heading");
}

TEST_CASE("typed merged-table no-op and head edits preserve geometry and identity") {
    auto tab = richPutFixture();
    tab.blocks[1].as_table().rows[0].cells[0].style["rowSpan"] = 2;
    auto covered = tab.blocks[1].as_table().rows[0];
    covered.key = "covered-row";
    covered.cells[0].key = "covered-cell";
    covered.cells[0].style = model::make_json_object();
    model::ParagraphBlock blank;
    blank.key = "covered-paragraph";
    covered.cells[0].blocks = {model::Block(blank)};
    tab.blocks[1].as_table().rows.push_back(covered);
    const auto original = tab;
    model::EditContext context;
    auto markdown = markdownRender(projectionBuild(tab));
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    CHECK(tab == original);
    markdown.replace(markdown.find(":style[cell]"), 12, ":style[edited]");
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    CHECK(tab.blocks[1].key == original.blocks[1].key);
    CHECK(tab.blocks[1].as_table().rows[0].cells[0].style == original.blocks[1].as_table().rows[0].cells[0].style);
    CHECK(tab.blocks[1].as_table().rows[0].cells[0].blocks[0].as_paragraph().inlines[0].text == "edited");
    CHECK(context.tombstones.empty());
}

TEST_CASE("typed new plain characters do not inherit expressible rich style and code conversion keeps monospace") {
    model::DocSpec spec;
    model::TabSpec tabSpec;
    model::ParagraphSpec paragraph;
    paragraph.content.emplace_back(model::ParagraphTextSpec{
        .text = "x", .style = slimr::Json::object_t{
            {"weightedFontFamily", slimr::Json::object_t{{"fontFamily", "Georgia"}}},
            {"fontSize", model::dimensionFromPt(19)},
            {"foregroundColor", model::colorOptionalFromHex("#123456")}}});
    tabSpec.blocks.push_back(model::BlockSpec::make_paragraph(paragraph));
    spec.tabs.push_back(tabSpec);
    const auto json = model::docJsonBuild(spec);
    for (bool code : {false, true}) {
        model::KeyAllocator keys;
        const auto original = model::docModelParse(json, {.docId = "d", .keys = keys});
        auto final = original;
        model::EditContext context{.keys = keys};
        auto markdown = markdownRender(projectionBuild(final.tabs[0]));
        if (code) markdown = "`x`";
        else markdown.insert(markdown.find("\n"), " plain");
        CHECK(markdownPut(model::EditTarget{context, final.tabs[0]}, markdown).changed);
        const auto projection = markdownParse(markdownRender(projectionBuild(final.tabs[0])));
        const auto requested = markdownParse(markdown);
        if (!code) CHECK(projection == requested);
        reconcile::DocReconcileInput input{.final = final, .original = original, .originalJson = json};
        const auto plan = reconcile::docReconcile(input);
        const auto check = reconcile::docPlanSelfCheck(plan, input);
        INFO((check.diffs.empty() ? "" : check.diffs.front()));
        CHECK(check.equal);
        const auto reopened = model::docModelParse(emulator::requestsEmulate(json, plan.contentRequests).json, {.docId = "d"});
        const auto& inlines = reopened.tabs[0].blocks[0].as_paragraph().inlines;
        if (code) {
            REQUIRE(inlines.size() == 1);
            CHECK(inlines[0].style["weightedFontFamily"]["fontFamily"].get<std::string>() == "Roboto Mono");
        } else {
            REQUIRE(inlines.size() == 2);
            CHECK(inlines[1].text == " plain");
            CHECK_FALSE(inlines[1].style.contains("weightedFontFamily"));
            CHECK_FALSE(inlines[1].style.contains("foregroundColor"));
            CHECK_FALSE(inlines[1].style.contains("fontSize"));
        }
    }
}

TEST_CASE("typed plain replacement of an initially blank paragraph preserves implicit readonly context") {
    model::TabModel tab;
    tab.tabId = "t.blank";
    tab.leadingSectionStyle = slimr::Json::object_t{{"sectionType", "CONTINUOUS"}};
    model::ParagraphBlock paragraph;
    paragraph.key = "blank-paragraph";
    paragraph.style = model::make_table_cell_paragraph_style_default();
    tab.blocks.emplace_back(paragraph);
    const auto original = tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab}, "Replacement body\n").changed);
    REQUIRE(tab.blocks.size() == 1);
    CHECK(tab.blocks[0].key == paragraph.key);
    CHECK(tab.blocks[0].as_paragraph().style == paragraph.style);
    CHECK(tab.leadingSectionStyle == original.leadingSectionStyle);
    CHECK(tab.blocks[0].as_paragraph().inlines[0].text == "Replacement body");
    CHECK(context.tombstones.empty());
}

TEST_CASE("typed referenced cells propagate unsupported nonbody declarations before mutation") {
    for (const auto* name : {"header", "footer", "footnote"}) {
        auto tab = richPutFixture();
        model::ParagraphBlock second;
        second.key = "referenced-second-paragraph";
        second.inlines.push_back(model::Inline::from_text("second"));
        tab.blocks[1].as_table().rows[0].cells[0].blocks.emplace_back(second);
        const auto original = tab;
        model::EditContext context;
        auto markdown = markdownRender(projectionBuild(tab));
        const auto location = markdown.find(":style[cell]");
        REQUIRE(location != std::string::npos);
        markdown.insert(location, std::string("::") + name + "{id=synthetic}\n\n");
        const auto parsed = markdownParse(markdown);
        CHECK(parsed.unsupportedReview);
        CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, parsed), model::CoreError);
        CHECK(tab == original);
        CHECK(context.tombstones.empty());
    }
}

TEST_CASE("typed unrelated edit preserves a hidden-only suggestion paragraph identity") {
    for (bool insertedNewline : {false,true}) {
    for (bool positionedObject : {false,true}) {
    model::TabModel tab;
    model::ParagraphBlock hidden;
    hidden.key = "hidden-identity";
    if (positionedObject) hidden.positionedObjectIds=std::vector<std::string>{"preserved-positioned"};
    auto pending = model::Inline::from_text("pending insertion");
    pending.suggestedInsertionIds = std::vector<std::string>{"hidden-review"};
    if (insertedNewline) hidden.newline.suggestedInsertionIds = std::vector<std::string>{"hidden-review"};
    hidden.inlines.push_back(pending);
    tab.blocks.emplace_back(hidden);
    model::ParagraphBlock visible;
    visible.key = "visible-identity";
    visible.inlines.push_back(model::Inline::from_text("normal paragraph"));
    tab.blocks.emplace_back(visible);
    auto markdown = markdownRender(projectionBuild(tab));
    markdown.replace(markdown.find("normal"), 6, "edited");
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab}, markdown).changed);
    REQUIRE(tab.blocks.size() == 2);
    CHECK(tab.blocks[0].as_paragraph() == hidden);
    CHECK(tab.blocks[1].key == visible.key);
    CHECK(tab.blocks[1].as_paragraph().inlines[0].text == "edited paragraph");
    CHECK(context.tombstones.empty());
    }
    }
}

// Paragraph text with review runs bracketed: "[d=s1:text]", "[i=s1:text]", "[s=s1:text]".
inline std::string reviewText(const model::ParagraphBlock& paragraph) {
    std::string out, key;
    bool open = false;
    for (const auto& sym : model::paragraphSymbols(paragraph)) {
        const auto marks = model::symMarks(sym);
        std::string next;
        auto add = [&](const char* kind, const std::vector<std::string>& ids) {
            for (const auto& id : ids) next += std::string(next.empty() ? "" : ",") + kind + "=" + id;
        };
        if (marks.suggestedDeletionIds) add("d", *marks.suggestedDeletionIds);
        if (marks.suggestedInsertionIds) add("i", *marks.suggestedInsertionIds);
        add("s", model::marksIds({.suggestedTextStyleChanges = marks.suggestedTextStyleChanges}));
        if (next != key) {
            if (open) out += "]";
            open = !next.empty();
            if (open) out += "[" + next + ":";
            key = next;
        }
        out += sym.ch;
    }
    return out + (open ? "]" : "");
}

// Writes `from` -> `to` over a one-paragraph review fixture with the review-carry rule enabled.
inline std::string carryPut(std::vector<model::ParagraphContentSpec> content, const std::string& from, const std::string& to,
                            bool carry = true) {
    const auto json = model::docJsonBuild({.tabs = {{.blocks = {model::BlockSpec::make_paragraph({.content = std::move(content)})}}}});
    model::KeyAllocator keys;
    auto tab = model::docModelParse(json, {.docId = "synthetic", .keys = keys}).tabs[0];
    auto markdown = markdownRender(projectionBuild(tab));
    INFO(markdown);
    const auto at = markdown.find(from);
    REQUIRE(at != std::string::npos);
    markdown.replace(at, from.size(), to);
    model::EditContext context{.keys = keys};
    PutOptions options;
    if (carry) options.carryReview = engine::suggestionRuleApply;
    markdownPut(model::EditTarget{context, tab}, markdown, {}, options);
    return reviewText(tab.blocks[0].as_paragraph());
}

inline model::ParagraphTextSpec styled(std::string text, const std::string& id) {
    return {.text = std::move(text), .sugStyle = slimr::Json::object_t{{id, slimr::Json::object_t{{"bold", true}}}}};
}

inline model::ParagraphTextSpec hiddenInsertion(std::string text, const std::string& id) {
    return {.text = std::move(text), .sugIns = std::vector<std::string>{id}};
}

inline void carryRefused(std::vector<model::ParagraphContentSpec> content, const std::string& from, const std::string& to,
                         const std::string& ids) {
    try {
        carryPut(std::move(content), from, to);
        FAIL("expected suggestionConflict");
    } catch (const model::CoreError& error) {
        CHECK(error.code == "suggestionConflict");
        CHECK(error.message.find("removes pending suggestions " + ids + "; add reject") != std::string::npos);
    }
}

// Rejects `reject`, then carry-puts `desired` with `rejectionEmptied`; returns block keys and tombstones or the refusal.
inline std::string rejectionEmptiedPut(std::vector<model::BlockSpec> blocks, const std::set<std::string>& reject,
                                       const std::string& desired, bool protect, bool keyInSet = true) {
    const auto json = model::docJsonBuild({.tabs = {{.blocks = std::move(blocks)}}});
    model::KeyAllocator keys;
    const auto document = model::docModelParse(json, {.docId = "synthetic", .keys = keys});
    auto tab = document.tabs[0];
    model::EditContext context{.keys = keys};
    AtomPutReport recreations;
    if (!reject.empty()) engine::suggestionRejectsApply(model::EditTarget{context, tab}, reject, document, recreations);
    auto& target = tab.blocks[0];
    if (protect) target.as_paragraph().is_protected = true;
    if (reject.empty()) REQUIRE_FALSE(markdownParse(markdownRender(projectionBuild({target}, tab))).blocks.empty());
    else CHECK(target.as_paragraph().inlines.empty());
    PutOptions options;
    options.carryReview = engine::suggestionRuleApply;
    if (keyInSet) options.rejectionEmptied = {target.key};
    else options.rejectionEmptied = {"absent-key"};
    std::string out;
    try {
        markdownPut(model::EditTarget{context, tab}, desired, {}, options);
    } catch (const model::CoreError& error) {
        return "refused:" + error.code;
    }
    for (const auto& block : tab.blocks) out += "key=" + block.key + ";";
    for (const auto& tombstone : context.tombstones) out += "tombstone=" + tombstone.key + ";";
    return out;
}

inline model::BlockSpec rejectionEmptiedV008() {
    return model::BlockSpec::make_paragraph({.content = {
        model::ParagraphTextSpec{.text = "Hello world", .sugIns = std::vector<std::string>{"s1"}}}});
}

TEST_CASE("review carry - rejection emptied gate keeps baseline outside the sole emptied paragraph") {
    // (i) visible, not in the set: still tombstoned and re-keyed
    CHECK(rejectionEmptiedPut({model::BlockSpec::make_paragraph({.content = {"Hello world"}})}, {}, "", false, false) ==
          "key=n1;tombstone=o1-t-0;");
    // (ii) emptied with a sibling paragraph
    CHECK(rejectionEmptiedPut({rejectionEmptiedV008(), model::BlockSpec::make_paragraph({.content = {"Other"}})}, {"s1"},
          "Other\n", false) == "key=o2-t-0;tombstone=o1-t-0;");
    // (iii) desired content where the emptied paragraph was
    CHECK(rejectionEmptiedPut({rejectionEmptiedV008()}, {"s1"}, "New text\n", false) == "key=o1-t-0;");
    // (iv) protected
    CHECK(rejectionEmptiedPut({rejectionEmptiedV008()}, {"s1"}, "", true) == "refused:unsupportedSyntax");
}

TEST_CASE("review carry - span edges, interiors and hidden gaps follow the original rule") {
    const std::vector<model::ParagraphContentSpec> go = {"Go ", styled("Fast", "s3"), " now"};
    const std::vector<model::ParagraphContentSpec> gap = {"ab", hiddenInsertion("HID", "s5"), "cd"};
    CHECK(carryPut(go, "Fast:suggestion{id=s3 end} now", "Fast:suggestion{id=s3 end}X now") == "Go [s=s3:Fast]X now");
    CHECK(carryPut(go, "start}Fast", "start}FaZst") == "Go [s=s3:FaZst] now");
    CHECK(carryPut(gap, "ab:", "abX:") == "abX[i=s5:HID]cd");
    CHECK(carryPut(gap, "ab:", "aY:") == "aY[i=s5:HID]cd");
    CHECK(carryPut(gap, "end}cd", "end}Zd") == "ab[i=s5:HID]Zd");
    carryRefused({"Go ", styled("F", "s4"), "ast"}, "start}F:", "start}G:", "s4");
    carryRefused(go, ":suggestion{id=s3 start}Fast:suggestion{id=s3 end}", "", "s3");
    carryRefused({styled("Fast", "s2"), " and safe"}, "start}Fast:suggestion{id=s2 end} and", "start}Quick:suggestion{id=s2 end} or", "s2");
    carryRefused(gap, "b:suggestion{id=s5 start}:suggestion{id=s5 end}c", "YZ", "s5");
}

TEST_CASE("review carry - standalone markdownPut stays restrictive without the opt-in") {
    const auto json = model::docJsonBuild({.tabs = {{.blocks = {model::BlockSpec::make_paragraph(
        {.content = {"Go ", styled("Fast", "s3"), " now"}})}}}});
    model::KeyAllocator keys;
    auto tab = model::docModelParse(json, {.docId = "synthetic", .keys = keys}).tabs[0];
    auto markdown = markdownRender(projectionBuild(tab));
    markdown.replace(markdown.find("start}Fast"), 10, "start}Fuel");
    const auto saved = tab;
    model::EditContext context{.keys = keys};
    CHECK_THROWS_WITH_AS(markdownPut(model::EditTarget{context, tab}, markdown), doctest::Contains("reject it first"), model::CoreError);
    CHECK(tab == saved);
}

} // namespace gdocsmith::lens::test
