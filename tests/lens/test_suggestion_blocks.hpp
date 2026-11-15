#pragma once

#include "lens/suggestion_blocks.hpp"
#include "lens/suggestion_projection.hpp"
#include "lens/document.hpp"
#include "lens/put.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("suggestionBlockRender renders block fences") {
    SuggestionBlock sb{
        .id = "s1",
        .kind = "text",
        .tabIndex = 0,
        .segment = "header default",
        .before = "before text",
        .after = "after text"
    };
    std::string rendered = suggestionBlockRender(sb);
    CHECK(rendered.find(":::suggestion{id=s1 kind=text segment=\"header default\"}") != std::string::npos);
    CHECK(rendered.find("::before\nbefore text\n") != std::string::npos);
    CHECK(rendered.find("::after\nafter text\n") != std::string::npos);
    CHECK(rendered.ends_with(":::\n"));
}

inline model::ParagraphBlock suggestionParagraph() {
    model::ParagraphBlock paragraph;
    paragraph.key = "review-context";
    paragraph.inlines.push_back(model::Inline::from_text("Before "));
    auto removed = model::Inline::from_text("old");
    removed.suggestedDeletionIds = std::vector<std::string>{"replace"};
    paragraph.inlines.push_back(removed);
    auto inserted = model::Inline::from_text("new");
    inserted.suggestedInsertionIds = std::vector<std::string>{"replace"};
    paragraph.inlines.push_back(inserted);
    paragraph.inlines.push_back(model::Inline::from_text(" after"));
    return paragraph;
}

TEST_CASE("review export renders actual replacement before and after with exact change markers") {
    model::DocModel doc;
    doc.docId = "synthetic";
    doc.title = "Synthetic";
    model::TabModel tab;
    tab.tabId = "t.review";
    tab.blocks.emplace_back(suggestionParagraph());
    doc.tabs.push_back(tab);
    const auto original = doc;
    const auto blocks = suggestionBlocksBuild(doc);
    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].before == std::optional<std::string>{"Before :change[old] after"});
    CHECK(blocks[0].after == std::optional<std::string>{"Before :change[new] after"});
    CHECK_FALSE(blocks[0].notice.has_value());
    CHECK(doc == original);
    const auto body = markdownRender(projectionBuild(tab));
    CHECK(body.find(":suggestion{id=replace start}old:suggestion{id=replace end}") != std::string::npos);
    CHECK(body.find("new") == std::string::npos);
    model::EditContext context;
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab}, body).changed);
    CHECK(tab == doc.tabs[0]);
    auto tampered = body;
    tampered.erase(tampered.find(":suggestion{id=replace start}"), 29);
    CHECK_THROWS_AS(markdownPut(model::EditTarget{context, tab}, tampered), model::CoreError);
    CHECK(tab == doc.tabs[0]);
}

TEST_CASE("review export applies only the selected text-style proposal and retains other pending insertions") {
    model::DocModel doc;
    model::TabModel tab;
    model::ParagraphBlock paragraph;
    paragraph.key = "styled-review";
    auto word = model::Inline::from_text("word");
    word.suggestedTextStyleChanges = slimr::Json::object_t{
        {"style-change", slimr::Json::object_t{{"textStyle", slimr::Json::object_t{{"bold", true}}}}}};
    paragraph.inlines.push_back(word);
    auto hidden = model::Inline::from_text("unrelated");
    hidden.suggestedInsertionIds = std::vector<std::string>{"other-change"};
    paragraph.inlines.push_back(hidden);
    tab.blocks.emplace_back(paragraph);
    doc.tabs.push_back(tab);
    const auto blocks = suggestionBlocksBuild(doc);
    const auto found = std::find_if(blocks.begin(), blocks.end(), [](const auto& block) {
        return block.id == "style-change";
    });
    REQUIRE(found != blocks.end());
    CHECK(found->before == std::optional<std::string>{":change[word]"});
    CHECK(found->after == std::optional<std::string>{":change[**word**]"});
    CHECK(found->after->find("unrelated") == std::string::npos);
}

TEST_CASE("review export keeps table cell and nonbody context and round-trips immutable snippets") {
    model::DocModel doc;
    doc.docId = "synthetic";
    doc.title = "Synthetic";
    model::TabModel tab;
    tab.tabId = "t.review";
    model::CellModel cell;
    cell.key = "review-cell";
    cell.blocks.emplace_back(suggestionParagraph());
    model::RowModel row;
    row.cells.push_back(cell);
    model::TableBlock table;
    table.key = "review-table";
    table.rows.push_back(row);
    tab.blocks.emplace_back(table);
    model::SegmentModel segment;
    segment.id = "synthetic-header";
    segment.kind = "header";
    segment.type = "default";
    segment.tab = std::make_shared<model::TabModel>();
    segment.tab->blocks.emplace_back(suggestionParagraph());
    tab.segments.push_back(segment);
    doc.tabs.push_back(tab);
    const auto original = doc;
    const auto blocks = suggestionBlocksBuild(doc);
    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0].before.has_value());
    REQUIRE(blocks[0].after.has_value());
    CHECK(blocks[0].before->find(":change[old]") != std::string::npos);
    CHECK(blocks[0].after->find(":change[new]") != std::string::npos);
    CHECK(blocks[1].segment == std::optional<std::string>{"header default"});
    CHECK(blocks[1].before == std::optional<std::string>{"Before :change[old] after"});
    CHECK(blocks[1].after == std::optional<std::string>{"Before :change[new] after"});
    DocumentFile file;
    file.frontmatter.doc = doc.docId;
    file.frontmatter.title = doc.title;
    file.suggestions = blocks;
    DocumentFileTab body;
    body.id = tab.tabId;
    body.markdown = "::section{id=s1}\n\n" + markdownRender(projectionBuild(tab));
    file.tabs.push_back(body);
    const auto parsed = documentFileParse(documentFileExport(file));
    REQUIRE(parsed.suggestions.size() == blocks.size());
    for (size_t i = 0; i < blocks.size(); ++i) {
        CHECK(parsed.suggestions[i].before == blocks[i].before);
        CHECK(parsed.suggestions[i].after == blocks[i].after);
        CHECK(parsed.suggestions[i].segment == blocks[i].segment);
    }
    model::EditContext context;
    CHECK_FALSE(markdownPut(model::EditTarget{context, tab},
        documentTabMarkdown(parsed,parsed.tabs[0],doc,tab.tabId)).changed);
    CHECK(doc == original);
}

TEST_CASE("review export shows actual structural deletion and layout style values") {
    model::DocModel doc;
    model::TabModel tab;
    tab.documentStyle = slimr::Json::object_t{{"marginTop", model::dimensionFromPt(10)}};
    tab.suggestedDocumentStyleChanges = slimr::Json::object_t{
        {"layout", slimr::Json::object_t{{"documentStyle",
            slimr::Json::object_t{{"marginTop", model::dimensionFromPt(20)}}}}}};
    model::CellModel cell;
    cell.key = "cell";
    model::ParagraphBlock paragraph;
    paragraph.key = "row-content";
    paragraph.inlines.push_back(model::Inline::from_text("deleted row"));
    cell.blocks.emplace_back(paragraph);
    model::RowModel row;
    row.cells.push_back(cell);
    row.suggestedDeletionIds = std::vector<std::string>{"row-delete"};
    model::TableBlock table;
    table.rows.push_back(row);
    tab.blocks.emplace_back(table);
    doc.tabs.push_back(tab);
    const auto original = doc;
    const auto blocks = suggestionBlocksBuild(doc);
    const auto layout = std::find_if(blocks.begin(), blocks.end(), [](const auto& block) { return block.id == "layout"; });
    REQUIRE(layout != blocks.end());
    CHECK(layout->before->find("10") != std::string::npos);
    CHECK(layout->after->find("20") != std::string::npos);
    const auto deleted = std::find_if(blocks.begin(), blocks.end(), [](const auto& block) { return block.id == "row-delete"; });
    REQUIRE(deleted != blocks.end());
    CHECK(deleted->before->find(":change[deleted row]") != std::string::npos);
    CHECK(deleted->after->find("deleted row") == std::string::npos);
    CHECK(deleted->after->find(":change[]") != std::string::npos);
    CHECK(doc == original);
}

} // namespace gdocsmith::lens::test
