#pragma once

#include "engine/export.hpp"
#include "lens/put.hpp"
#include "test.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::engine::test {

inline void checkPartialExportDisclosure(const ExportWritten &written) {
    for (const auto &warning : written.warnings) {
        CHECK(warning.find("headers, footers and footnotes are read-only") == std::string::npos);
        CHECK(warning.find("non-body sections") == std::string::npos);
    }
}

TEST_CASE("export - readonly rich contexts roundtrip and survive an unrelated body edit") {
    model::DocModel doc;
    doc.docId = "synthetic-rich-context";
    doc.revisionId = "revision";
    doc.title = "Rich contexts";
    model::TabModel tab;
    tab.tabId = "t.rich";
    tab.title = "Rich tab";
    tab.documentStyle = slimr::Json::object_t{
        {"documentFormat", slimr::Json::object_t{{"documentMode", "PAGES"}}},
        {"documentSize", slimr::Json::object_t{{"width", model::dimensionFromPt(612)}, {"height", model::dimensionFromPt(1008)}}},
        {"marginTop", model::dimensionFromPt(36)},
        {"marginHeader", model::dimensionFromPt(24)},
        {"useEvenPageHeaderFooter", true},
        {"background", slimr::Json::object_t{{"color", model::colorOptionalFromHex("#112233")}}},
        {"opaqueSetting", "retained"}};
    tab.leadingSectionStyle = slimr::Json::object_t{{"contentDirection", "RIGHT_TO_LEFT"},
                                                    {"columnProperties", slimr::Json::array_t{
                                                                             slimr::Json::object_t{{"paddingEnd", model::dimensionFromPt(18)}}, slimr::Json::object_t{}}}};
    tab.namedStyles = slimr::Json::object_t{{"styles", slimr::Json::array_t{slimr::Json::object_t{
                                                           {"namedStyleType", "TITLE"}, {"textStyle", slimr::Json::object_t{{"bold", true}, {"fontSize", model::dimensionFromPt(20)}, {"weightedFontFamily", slimr::Json::object_t{{"fontFamily", "Georgia"}}}}}, {"paragraphStyle", slimr::Json::object_t{{"alignment", "CENTER"}}}}}}};
    model::ParagraphBlock body;
    body.key = "body";
    body.style = slimr::Json::object_t{{"alignment", "END"}, {"keepWithNext", true}, {"tabStops", slimr::Json::array_t{slimr::Json::object_t{{"alignment", "CENTER"}, {"offset", model::dimensionFromPt(21)}}}}};
    body.inlines.emplace_back(model::Inline::from_text("Body with footnote "));
    model::Atom reference;
    reference.key = "footnote-reference";
    reference.type = "footnoteRef";
    reference.raw = slimr::Json::object_t{{"footnoteReference", slimr::Json::object_t{{"footnoteId", "fn.actual"}}}};
    body.inlines.emplace_back(reference);
    tab.blocks.emplace_back(body);
    model::TableBlock table;
    table.key = "merged-table";
    table.columns = {{.key = "col0", .props = slimr::Json::object_t{{"width", model::dimensionFromPt(100)}, {"widthType", "FIXED_WIDTH"}}},
                     {.key = "col1", .props = slimr::Json::object_t{{"width", model::dimensionFromPt(200)}, {"widthType", "FIXED_WIDTH"}}}};
    for (int r = 0; r < 2; ++r) {
        model::RowModel row;
        row.key = "row" + std::to_string(r);
        row.style = slimr::Json::object_t{{"minRowHeight", model::dimensionFromPt(r ? 42 : 30)}};
        if (!r) {
            row.style["tableHeader"] = true;
            row.style["preventOverflow"] = true;
        }
        for (int c = 0; c < 2; ++c) {
            model::CellModel cell;
            cell.key = "cell" + std::to_string(r) + std::to_string(c);
            model::ParagraphBlock paragraph;
            paragraph.key = "paragraph-" + cell.key;
            if (!r && !c) {
                cell.style = slimr::Json::object_t{{"rowSpan", 2}, {"columnSpan", 2}, {"paddingTop", model::dimensionFromPt(9)}, {"backgroundColor", model::colorOptionalFromHex("#FFFF00")}};
                paragraph.inlines.push_back(model::Inline::from_text("Merged anchor"));
            }
            cell.blocks.emplace_back(paragraph);
            row.cells.push_back(cell);
        }
        table.rows.push_back(row);
    }
    tab.blocks.emplace_back(table);
    model::ParagraphBlock tail;
    tail.key = "tail";
    tail.inlines.push_back(model::Inline::from_text("Editable tail"));
    tab.blocks.emplace_back(tail);
    for (const auto &[kind, id, type, content] : {
             std::tuple{"header", "h.actual", "first", "Actual header"},
             std::tuple{"footer", "f.actual", "even", "Actual footer"},
             std::tuple{"footnote", "fn.actual", "default", "Actual footnote"}}) {
        auto segment = std::make_shared<model::TabModel>();
        segment->tabId = "t.rich::" + std::string(id);
        model::ParagraphBlock paragraph;
        paragraph.key = id;
        paragraph.inlines.push_back(model::Inline::from_text(content, slimr::Json::object_t{{"italic", true}}));
        segment->blocks.emplace_back(paragraph);
        tab.segments.push_back({.id = id, .kind = kind, .type = type, .section = 2, .tab = segment});
    }
    doc.tabs.push_back(tab);
    const auto exported = exportDocumentFile(doc, {}, ExportOptions{.sourceVersion = std::nullopt});
    const auto markdown = lens::documentFileExport(exported);
    CHECK(markdown.find("::header{kind=firstPage section=2}") != std::string::npos);
    CHECK(markdown.find("::footer{kind=evenPage section=2}") != std::string::npos);
    CHECK(markdown.find("Actual header") != std::string::npos);
    CHECK(markdown.find("Actual footer") != std::string::npos);
    CHECK(markdown.find("[^1]: *Actual footnote*") != std::string::npos);
    CHECK(markdown.find("Body with footnote [^1]") != std::string::npos);
    CHECK(markdown.find("size=LEGAL") != std::string::npos);
    CHECK(markdown.find("namedStyleSet=1") != std::string::npos);
    for (const auto *setting : {"widths=100,200", "headerRows=1", "heights=30,42", "preventOverflow=1"}) {
        INFO(std::string(setting));
        CHECK(markdown.find(setting) != std::string::npos);
    }
    CHECK(markdown.find(":merge{left}") != std::string::npos);
    CHECK(markdown.find(":merge{up}") != std::string::npos);
    const auto parsed = lens::documentFileParse(markdown);
    REQUIRE(parsed.tabs.size() == 1);
    REQUIRE(parsed.tabs[0].segments.size() == 2);
    REQUIRE(parsed.tabs[0].footnotes.size() == 1);
    CHECK(parsed.tabs[0].footnotes[0].markdown == "*Actual footnote*\n");
    CHECK(parsed.tabs[0].segments[0].markdown == "*Actual header*\n");
    CHECK(parsed.tabs[0].markdown.find("Actual header") == std::string::npos);
    CHECK(parsed.frontmatter.namedStyleSets["1"]["TITLE"]["fontFamily"] == slimr::Json("Georgia"));
    CHECK(parsed.frontmatter.namedStyleSets["1"]["TITLE"]["alignment"] == slimr::Json("CENTER"));
    REQUIRE(parsed.frontmatter.styleBaseline);
    const auto settings = slimr::parse_yaml(slimr::to_yaml(tab.documentStyle));
    const auto paragraphStyle = slimr::parse_yaml(slimr::to_yaml(body.style));
    REQUIRE(settings);
    REQUIRE(paragraphStyle);
    CHECK((*parsed.frontmatter.styleBaseline)["tabs"]["t.rich"]["documentStyle"] == *settings);
    CHECK((*parsed.frontmatter.styleBaseline)["tabs"]["t.rich"]["paragraphs"]["body"]["style"] == *paragraphStyle);
    CHECK(parsed.frontmatter.paragraphStyles.at(lens::projectionStyleName('p', body.key)).keepWithNext == true);
    CHECK(parsed.frontmatter.cellStyles.at(lens::projectionStyleName('c', "cell00")).paddingTop == 9);
    const auto projected = lens::markdownParse(parsed.tabs[0].markdown);
    REQUIRE(projected.blocks.size() == 4);
    REQUIRE(projected.blocks[2].table);
    REQUIRE(projected.blocks[2].table->merges.size() == 1);
    CHECK(projected.blocks[2].table->merges[0] == lens::TableMerge{.column = 0, .columnSpan = 2, .row = 0, .rowSpan = 2});
    model::EditContext context;
    CHECK_FALSE(lens::markdownPut(model::EditTarget{context, tab}, parsed.tabs[0].markdown).changed);
    CHECK(tab == doc.tabs[0]);
    auto edited = parsed.tabs[0].markdown;
    edited.replace(edited.find("Editable tail"), std::string("Editable tail").size(), "Changed tail");
    CHECK(lens::markdownPut(model::EditTarget{context, tab}, edited).changed);
    CHECK(tab.blocks[0] == doc.tabs[0].blocks[0]);
    CHECK(tab.blocks[1] == doc.tabs[0].blocks[1]);
    CHECK(tab.segments == doc.tabs[0].segments);
    CHECK(tab.documentStyle == doc.tabs[0].documentStyle);
    CHECK(tab.namedStyles == doc.tabs[0].namedStyles);
    CHECK(tab.leadingSectionStyle == doc.tabs[0].leadingSectionStyle);
    CHECK(tab.blocks[2].as_paragraph().inlines[0].text == "Changed tail");
    const auto roundtrip = lens::documentFileParse(lens::documentFileExport(parsed));
    CHECK(lens::frontmatterReadonlyEqual(parsed.frontmatter, roundtrip.frontmatter));
    CHECK(lens::tabReadonlyEqual(parsed.tabs[0], roundtrip.tabs[0]));
    auto changed = roundtrip;
    changed.tabs[0].segments[0].markdown = "Changed header\n";
    CHECK_FALSE(lens::tabReadonlyEqual(parsed.tabs[0], changed.tabs[0]));
    changed = roundtrip;
    (*changed.frontmatter.styleBaseline)["tabs"]["t.rich"]["documentStyle"]["opaqueSetting"] = "changed";
    CHECK_FALSE(lens::frontmatterReadonlyEqual(parsed.frontmatter, changed.frontmatter));
}

TEST_CASE("export - readonly style snapshot strips temporary image URLs without changing identity or layout") {
    model::DocModel doc;
    doc.docId = "synthetic-private-images";
    doc.title = "Private images";
    model::TabModel tab;
    tab.tabId = "t.images";
    const std::string temporary = "https://images/download?access_token=synthetic-secret";
    const auto properties = slimr::Json::object_t{{"inlineObjectProperties", slimr::Json::object_t{
                                                                                 {"embeddedObject", slimr::Json::object_t{
                                                                                                        {"size", slimr::Json::object_t{{"width", model::dimensionFromPt(42)}}},
                                                                                                        {"imageProperties", slimr::Json::object_t{{"contentUri", temporary}, {"sourceUri", temporary}, {"cropProperties", slimr::Json::object_t{{"offsetLeft", 0.125}}}}}}}}}};
    tab.inlineObjects["image-id"] = properties;
    tab.positionedObjects["positioned-id"] = properties;
    model::Atom image;
    image.key = "image-key";
    image.type = "image";
    image.raw = slimr::Json::object_t{{"inlineObjectElement", slimr::Json::object_t{{"inlineObjectId", "image-id"}}},
                                      {"cachedProperties", slimr::Json::object_t{{"contentUri", temporary}}}};
    model::ParagraphBlock paragraph;
    paragraph.key = "image-paragraph";
    paragraph.inlines.emplace_back(image);
    tab.blocks.emplace_back(paragraph);
    doc.tabs.push_back(tab);
    const auto original = doc;
    const auto snapshot = exportStyleSnapshot(tab);
    const auto &embedded = snapshot["inlineObjects"]["image-id"]["inlineObjectProperties"]["embeddedObject"];
    CHECK(lens::dimensionPt(embedded["size"]["width"]) == 42);
    CHECK(embedded["imageProperties"]["cropProperties"]["offsetLeft"].as<double>() == 0.125);
    CHECK_FALSE(embedded["imageProperties"].contains("contentUri"));
    CHECK_FALSE(embedded["imageProperties"].contains("sourceUri"));
    CHECK(snapshot["paragraphs"]["image-paragraph"]["inlines"][0]["atomRaw"]["inlineObjectElement"]["inlineObjectId"] ==
          slimr::Json("image-id"));
    const auto markdown = lens::documentFileExport(exportDocumentFile(doc));
    CHECK(markdown.find("https://images/") == std::string::npos);
    CHECK(markdown.find("synthetic-secret") == std::string::npos);
    CHECK(markdown.find(":image{id=image-id}") != std::string::npos);
    CHECK(doc == original);
}

TEST_CASE("export - documentId parsing") {
    CHECK_EQ(documentId("1234567890abcdef_-"), "1234567890abcdef_-");
    CHECK_EQ(documentId("https://docs.google.com/document/d/1234567890abcdef_-/edit"), "1234567890abcdef_-");
    CHECK_EQ(documentId("https://docs.google.com/document/d/doc_abc123"), "doc_abc123");

    CHECK_THROWS_AS(documentId("http://docs.google.com/document/d/doc"), model::CoreError);
    CHECK_THROWS_AS(documentId("https://google.com/document/d/doc"), model::CoreError);
    CHECK_THROWS_AS(documentId("https://docs.google.com/spreadsheets/d/sheet"), model::CoreError);
    CHECK_THROWS_AS(documentId("bad id with spaces"), model::CoreError);
}

TEST_CASE("export - rich file body putback is a complete model no-op") {
    const auto directory = std::filesystem::current_path() / "build/cmake/SwarmFidelity/export-roundtrip";
    model::DocModel doc;
    doc.docId = "synthetic-rich-roundtrip";
    doc.title = "Synthetic rich roundtrip";
    doc.revisionId = "revision";
    model::TabModel tab;
    tab.tabId = "t.body";
    model::ParagraphBlock paragraph;
    paragraph.key = "paragraph-id";
    paragraph.style = slimr::Json::object_t{{"alignment", "END"}};
    paragraph.inlines.push_back(model::Inline::from_text("rich * literal | text",
                                                         slimr::Json::object_t{{"bold", true}, {"fontSize", model::dimensionFromPt(19)}, {"weightedFontFamily", slimr::Json::object_t{{"fontFamily", "Georgia"}}}}));
    tab.blocks.emplace_back(paragraph);
    doc.tabs.push_back(tab);
    FileServices services;
    services.root = directory.string();
    const auto written = exportModel(doc, {}, services);
    const auto parsed = lens::documentFileParse(written.markdown);
    REQUIRE(parsed.tabs.size() == 1);
    CHECK(written.markdown.find("fontFamily=Georgia") != std::string::npos);
    model::EditContext context;
    CHECK_FALSE(lens::markdownPut(model::EditTarget{context, tab}, parsed.tabs[0].markdown).changed);
    CHECK(tab == doc.tabs[0]);
    CHECK(context.tombstones.empty());
    std::filesystem::remove_all(directory);
}

TEST_CASE("export - pure file projection exposes pending review context without mutating review payloads") {
    model::DocModel doc;
    doc.docId = "synthetic-review";
    doc.title = "Synthetic review";
    model::TabModel tab;
    tab.tabId = "t.review";
    model::ParagraphBlock paragraph;
    paragraph.key = "review-paragraph";
    auto text = model::Inline::from_text("pending text");
    text.suggestedInsertionIds = std::vector<std::string>{"pending-id"};
    paragraph.inlines.push_back(text);
    tab.blocks.emplace_back(paragraph);
    doc.tabs.push_back(tab);
    const auto original = doc;
    const auto file = exportDocumentFile(doc);
    REQUIRE(file.suggestions.size() == 1);
    CHECK(file.suggestions[0].id == "pending-id");
    CHECK_FALSE(file.suggestions[0].notice.has_value());
    CHECK(file.suggestions[0].before == std::optional<std::string>{":change[]"});
    CHECK(file.suggestions[0].after == std::optional<std::string>{":change[pending text]"});
    CHECK(file.tabs[0].markdown.find(":suggestion{id=pending-id start}") == std::string::npos);
    CHECK(file.tabs[0].markdown.find("pending text") == std::string::npos);
    CHECK(doc == original);
    const auto parsed = lens::documentFileParse(lens::documentFileExport(file));
    REQUIRE(parsed.suggestions.size() == 1);
    CHECK(parsed.suggestions[0].id == "pending-id");
    CHECK(parsed.suggestions[0].before == file.suggestions[0].before);
    CHECK(parsed.suggestions[0].after == file.suggestions[0].after);
    model::EditContext context;
    auto editable = doc.tabs[0];
    CHECK_FALSE(lens::markdownPut(model::EditTarget{context, editable},
                                  lens::documentTabMarkdown(parsed, parsed.tabs[0], doc, doc.tabs[0].tabId))
                    .changed);
    CHECK(editable == doc.tabs[0]);
}

TEST_CASE("export - commentReplies filtering") {
    DriveComment comment;
    comment.id = "c1";
    comment.content = "Top comment";

    DriveCommentReply r1;
    r1.id = "r1";
    r1.content = "First reply";
    r1.deleted = false;

    DriveCommentReply r2;
    r2.id = "r2";
    r2.content = "Deleted reply";
    r2.deleted = true;

    DriveCommentReply r3;
    r3.id = "r3";
    r3.content = "   \t\n  ";
    r3.deleted = false;

    comment.replies = {r1, r2, r3};

    auto filtered = commentReplies(comment);
    CHECK_EQ(filtered.size(), 1u);
    CHECK_EQ(filtered[0].id, "r1");
    CHECK_EQ(filtered[0].content, "First reply");
}

TEST_CASE("export - commentBlocksBuild ordering and anchors") {
    model::DocModel doc;
    doc.docId = "doc1";
    doc.revisionId = "rev1";
    doc.title = "Test Doc";

    model::TabModel tab1;
    tab1.tabId = "t.0";
    tab1.title = "Tab 1";
    model::ParagraphBlock p1;
    p1.inlines.push_back(model::Inline::from_text("First paragraph content\n"));
    tab1.blocks.push_back(model::Block(std::move(p1)));

    model::TabModel tab2;
    tab2.tabId = "t.1";
    tab2.title = "Tab 2";
    model::ParagraphBlock p2;
    p2.inlines.push_back(model::Inline::from_text("Second tab content\n"));
    tab2.blocks.push_back(model::Block(std::move(p2)));

    doc.tabs = {std::move(tab1), std::move(tab2)};

    DriveComment c_unresolved;
    c_unresolved.id = "c1";
    c_unresolved.content = "Unresolved comment";
    c_unresolved.resolved = false;
    c_unresolved.author = DriveCommentAuthor{.displayName = "Alice", .me = true};

    DriveComment c_resolved;
    c_resolved.id = "c2";
    c_resolved.content = "Resolved comment";
    c_resolved.resolved = true;
    c_resolved.author = DriveCommentAuthor{.displayName = "Bob", .me = false};

    std::vector<DriveComment> comments = {c_resolved, c_unresolved};

    // Without includeResolved
    auto blocks1 = commentBlocksBuild(doc, comments, false);
    CHECK_EQ(blocks1.size(), 1u);
    CHECK_EQ(blocks1[0].id, "c1");
    CHECK_EQ(blocks1[0].text, "Unresolved comment");
    CHECK_EQ(blocks1[0].author, std::optional<std::string>("Alice"));
    CHECK(blocks1[0].mine);

    // With includeResolved
    auto blocks2 = commentBlocksBuild(doc, comments, true);
    CHECK_EQ(blocks2.size(), 2u);
}

TEST_CASE("export - exportModel creates artifacts and writes markdown") {
    auto tmpDir = std::filesystem::temp_directory_path() / ("gdocsmith-export-test-" + detail::unique_tmp_suffix());
    std::filesystem::create_directories(tmpDir);

    model::DocModel doc;
    doc.docId = "doc_export_123";
    doc.revisionId = "r1";
    doc.title = "Export Test";

    model::TabModel tab;
    tab.tabId = "t.0";
    tab.title = "Overview";
    model::ParagraphBlock p;
    p.inlines.push_back(model::Inline::from_text("Hello export world\n"));
    tab.blocks.push_back(model::Block(std::move(p)));
    doc.tabs = {std::move(tab)};

    DriveComment c;
    c.id = "c1";
    c.content = "Looks good!";
    c.resolved = false;

    FileServices services;
    services.apiUser = "agent@example.com";
    services.root = tmpDir.string();

    auto written = exportModel(doc, {c}, services);

    checkPartialExportDisclosure(written);
    CHECK(std::filesystem::exists(written.exportPath));
    CHECK(written.markdown.find("apiUser: agent@example.com") != std::string::npos);
    CHECK(written.markdown.find("Hello export world") != std::string::npos);
    CHECK(written.markdown.find("\n::comment{id=c1") != std::string::npos);
    REQUIRE(written.commentsLine.has_value());
    CHECK(*written.commentsLine > 1);

    FileServices noUserServices;
    noUserServices.root = tmpDir.string();
    auto noUserWritten = exportModel(doc, {c}, noUserServices);
    CHECK(noUserWritten.markdown.find("apiUser:") == std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

TEST_CASE("export - rich synthetic content does not claim editable segments are read-only") {
    auto tmpDir = std::filesystem::temp_directory_path() / ("gdocsmith-export-rich-" + detail::unique_tmp_suffix());
    std::filesystem::create_directories(tmpDir);

    model::DocModel doc;
    doc.docId = "doc_export_rich";
    doc.revisionId = "r1";
    doc.title = "Synthetic rich export";
    model::TabModel tab;
    tab.tabId = "t.0";
    tab.title = "Body";
    model::ParagraphBlock paragraph;
    auto text = model::Inline::from_text("Synthetic body text");
    text.style["bold"] = true;
    paragraph.inlines.push_back(std::move(text));
    model::Atom image;
    image.key = "image_synthetic";
    image.type = "image";
    paragraph.inlines.push_back(model::Inline::from_atom(std::move(image)));
    tab.blocks.emplace_back(std::move(paragraph));

    model::ParagraphBlock cellParagraph;
    cellParagraph.inlines.push_back(model::Inline::from_text("Synthetic table cell"));
    model::CellModel cell;
    cell.blocks.emplace_back(std::move(cellParagraph));
    model::RowModel row;
    row.cells.push_back(std::move(cell));
    model::TableBlock table;
    table.rows.push_back(std::move(row));
    tab.blocks.emplace_back(std::move(table));

    model::SegmentModel header;
    header.id = "header_synthetic";
    header.kind = "header";
    header.tab = std::make_shared<model::TabModel>();
    model::ParagraphBlock headerParagraph;
    headerParagraph.inlines.push_back(model::Inline::from_text("Synthetic header"));
    header.tab->blocks.emplace_back(std::move(headerParagraph));
    tab.segments.push_back(std::move(header));
    doc.tabs.push_back(std::move(tab));

    FileServices services;
    services.root = tmpDir.string();
    auto written = exportModel(doc, {}, services);
    checkPartialExportDisclosure(written);
    CHECK(std::filesystem::exists(written.exportPath));
    CHECK(written.markdown.find("Synthetic body text") != std::string::npos);
    const auto parsed = lens::documentFileParse(written.markdown);
    REQUIRE(parsed.tabs.size() == 1u);
    const auto body = lens::markdownParse(parsed.tabs[0].markdown);
    REQUIRE(body.blocks.size() == 2u);
    REQUIRE(body.blocks[0].spans.size() == 2u);
    CHECK(body.blocks[0].spans[0].marks.bold);
    REQUIRE(body.blocks[0].spans[1].token.has_value());
    CHECK(body.blocks[0].spans[1].token->kind == "image");
    REQUIRE(body.blocks[1].table.has_value());
    REQUIRE(body.blocks[1].table->cells.size() == 1u);
    REQUIRE(body.blocks[1].table->cells[0].size() == 1u);
    CHECK(body.blocks[1].table->cells[0][0].empty());
    REQUIRE(body.blocks[1].table->rows.size() == 1u);
    REQUIRE(body.blocks[1].table->rows[0].size() == 1u);
    REQUIRE(body.blocks[1].table->rows[0][0].size() == 1u);
    CHECK(body.blocks[1].table->rows[0][0][0].text == "Synthetic table cell");
    REQUIRE(parsed.tabs[0].segments.size() == 1u);
    CHECK(parsed.tabs[0].segments[0].kind == "header");
    CHECK(parsed.tabs[0].segments[0].markdown.find("Synthetic header") != std::string::npos);
    model::EditContext context;
    auto editable = doc.tabs[0];
    CHECK_FALSE(lens::markdownPut(model::EditTarget{context, editable},
                                  lens::documentTabMarkdown(parsed, parsed.tabs[0], doc, "t.0"))
                    .changed);
    CHECK(editable == doc.tabs[0]);
    CHECK(!written.images.has_value());
    CHECK(!written.imagesDir.has_value());

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

TEST_CASE("export - multi-tab namedStyleSets deduplicates identical sets and omits empty cellStyles") {
    model::DocModel doc;
    doc.docId = "doc_named_dedup";
    doc.revisionId = "r1";
    doc.title = "Named Style Dedup";

    auto makeTab = [](const std::string &id, const std::string &title, const std::string &font) {
        model::TabModel tab;
        tab.tabId = id;
        tab.title = title;
        tab.namedStyles = slimr::Json::object_t{{"styles", slimr::Json::array_t{slimr::Json::object_t{
                                                               {"namedStyleType", "TITLE"},
                                                               {"textStyle", slimr::Json::object_t{{"weightedFontFamily", slimr::Json::object_t{{"fontFamily", font}}}}}}}}};
        model::ParagraphBlock p;
        p.key = id + "_p0";
        p.inlines.push_back(model::Inline::from_text("Body\n"));
        tab.blocks.emplace_back(std::move(p));
        return tab;
    };

    doc.tabs.push_back(makeTab("t.0", "First", "Georgia"));
    doc.tabs.push_back(makeTab("t.1", "Second", "Georgia"));
    doc.tabs.push_back(makeTab("t.2", "Third", "Roboto"));

    const auto exported = exportDocumentFile(doc);
    REQUIRE(exported.tabs.size() == 3u);
    CHECK(exported.tabs[0].readonlyAttributes.get_value("namedStyleSet") == std::optional<std::string>("1"));
    CHECK(exported.tabs[1].readonlyAttributes.get_value("namedStyleSet") == std::optional<std::string>("1"));
    CHECK(exported.tabs[2].readonlyAttributes.get_value("namedStyleSet") == std::optional<std::string>("2"));
    CHECK(exported.frontmatter.namedStyleSets.get<slimr::Json::object_t>().size() == 2u);
    CHECK(exported.frontmatter.namedStyleSets.contains("1"));
    CHECK(exported.frontmatter.namedStyleSets.contains("2"));
    CHECK(exported.frontmatter.cellStyles.empty());
    const auto markdown = lens::documentFileExport(exported);
    CHECK(markdown.find("cellStyles:") == std::string::npos);
}

TEST_CASE("export - image downloads select IDs and propagate failures") {
    auto tmpDir = std::filesystem::temp_directory_path() / ("gdocsmith-export-images-" + detail::unique_tmp_suffix());
    std::filesystem::create_directories(tmpDir);

    model::DocModel doc;
    doc.docId = "doc_export_images";
    doc.revisionId = "r1";
    model::TabModel tab;
    tab.tabId = "t.0";
    model::ParagraphBlock paragraph;
    paragraph.inlines.push_back(model::Inline::from_text("Synthetic image context\n"));
    model::Atom image;
    image.key = "image_synthetic";
    image.type = "image";
    paragraph.inlines.push_back(model::Inline::from_atom(std::move(image)));
    tab.blocks.emplace_back(std::move(paragraph));
    for (const auto &id : {"image_synthetic", "image_second"}) {
        tab.inlineObjects[id] = slimr::Json::object_t{
            {"inlineObjectProperties", slimr::Json::object_t{
                                           {"embeddedObject", slimr::Json::object_t{
                                                                  {"imageProperties", slimr::Json::object_t{{"contentUri", std::string("https://images/") + id}}}}}}}};
    }
    doc.tabs.push_back(std::move(tab));
    FileServices services;
    services.root = tmpDir.string();

    std::vector<std::string> downloads;
    services.imageDownload = [&](const std::string &uri) {
        downloads.push_back(uri);
        return DownloadedImage{.bytes = std::string("image\0bytes", 11), .extension = "png"};
    };
    for (bool selectIds : {false, true}) {
        downloads.clear();
        ExportOptions options;
        options.imagesDownload = true;
        if (selectIds)
            options.imageIds = std::vector<std::string>{"image_synthetic"};
        auto written = exportModel(doc, {}, services, options);
        checkPartialExportDisclosure(written);
        REQUIRE(written.images.has_value());
        REQUIRE(written.imagesDir.has_value());
        CHECK(downloads.size() == (selectIds ? 1u : 2u));
        CHECK(written.images->size() == downloads.size());
        for (const auto &[id, path] : *written.images) {
            CHECK(detail::file_text_read(path) == std::optional<std::string>(std::string("image\0bytes", 11)));
            struct stat st{};
            REQUIRE(stat(path.c_str(), &st) == 0);
            CHECK((st.st_mode & 0777) == 0600);
        }
        CHECK(written.markdown.find("https://images/") == std::string::npos);
        CHECK(std::filesystem::exists(written.exportPath));
        CHECK(written.markdown.find("Synthetic image context") != std::string::npos);
    }
    ExportOptions options;
    options.imagesDownload = true;
    options.imageIds = std::vector<std::string>{"missing"};
    downloads.clear();
    CHECK_THROWS_AS(exportModel(doc, {}, services, options), model::CoreError);
    CHECK(downloads.empty());
    options.imageIds = std::vector<std::string>{};
    auto empty = exportModel(doc, {}, services, options);
    REQUIRE(empty.images.has_value());
    CHECK(empty.images->empty());
    CHECK(!empty.imagesDir.has_value());
    CHECK(downloads.empty());
    options.imageIds = std::vector<std::string>{"image_synthetic"};
    services.imageDownload = [](const std::string &) -> DownloadedImage {
        throw model::CoreError("operationFailed", "synthetic download failure");
    };
    CHECK_THROWS_AS(exportModel(doc, {}, services, options), model::CoreError);

    const auto imageDirectory = std::filesystem::path(empty.folderPath) / "images";
    std::filesystem::remove_all(imageDirectory);
    const auto symlinkTarget = tmpDir / "symlink-target";
    std::filesystem::create_directory(symlinkTarget);
    std::filesystem::create_directory_symlink(symlinkTarget, imageDirectory);
    downloads.clear();
    services.imageDownload = [&](const std::string &uri) {
        downloads.push_back(uri);
        return DownloadedImage{.bytes = "image", .extension = "png"};
    };
    CHECK_THROWS_AS(exportModel(doc, {}, services, options), model::CoreError);
    CHECK(downloads.empty());
    CHECK(std::filesystem::is_empty(symlinkTarget));

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

} // namespace gdocsmith::engine::test
