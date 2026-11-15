#pragma once

#include "lens/parse.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("Node parse282 directional merge chains move the sole content to the rectangular head") {
    const auto parsed = markdownParse(
        "| A | :merge{left} | C |\n| --- | --- | --- |\n"
        "| :merge{down} | :merge{left} | x |\n| B | :merge{up} | y |");
    const auto& table = *parsed.blocks[0].table;
    CHECK(table.merges == std::vector<TableMerge>{
        {.column=0,.columnSpan=2,.row=0,.rowSpan=1},
        {.column=0,.columnSpan=2,.row=1,.rowSpan=2}});
    CHECK(table.rows[1][0][0].text == "B");
    CHECK(table.rows[2][0].empty());
    const auto chain = markdownParse("| :merge{down} |\n| --- |\n| :merge{down} |\n| content |");
    CHECK(chain.blocks[0].table->merges[0].rowSpan == 3);
    CHECK(chain.blocks[0].table->rows[0][0][0].text == "content");
    const auto escaped = markdownParse("| \\:merge{up} | b |\n| --- | --- |");
    CHECK(escaped.blocks[0].table->merges.empty());
    CHECK(escaped.blocks[0].table->rows[0][0][0].text == ":merge{up}");
    for (const auto* bad : {
        "| :merge{up} | b |\n| --- | --- |",
        "| :merge{down} |\n| --- |\n| :merge{up} |",
        "| a | :merge{left} |\n| --- | --- |\n| :merge{up} | b |",
        "| mixed :merge{down} |\n| --- |\n| content |",
        "| content |\n| --- |\n| :merge{down} |"})
        CHECK_THROWS_AS(markdownParse(bad), model::CoreError);
}

TEST_CASE("parseInlines extracts image URLs without consuming trailing Markdown") {
    for (const auto& text : {"![alt](https://example.com/image.png)",
                             "![alt](https://example.com/image.png) tail",
                             "![alt](https://example.com/image.png){width=20} tail"}) {
        const auto spans = parseInlines(text);
        REQUIRE(!spans.empty());
        REQUIRE(spans[0].token.has_value());
        CHECK(spans[0].token->attrs.get_value("src") ==
              std::optional<std::string>("https://example.com/image.png"));
        CHECK(spans[0].token->label == std::optional<std::string>("alt"));
        if (std::string_view(text).ends_with(" tail")) {
            REQUIRE(spans.size() == 2u);
            CHECK(spans[1].text == " tail");
        }
    }
}

TEST_CASE("parseInlines keeps malformed images as text") {
    for (const auto& text : {"![alt](missing tail", "![alt] tail"}) {
        const auto spans = parseInlines(text);
        REQUIRE(spans.size() == 1u);
        CHECK(spans[0].kind == "text");
        CHECK(spans[0].text == text);
    }
}

TEST_CASE("markdownParse parses headings, lists, and formatted inlines") {
    std::string md =
        "# Main Title\n\n"
        "This is **bold**, *italic*, `code`, and [Google](https://google.com).\n\n"
        "Here is styled text: :style[Accent]{.accent}.\n\n"
        "- Item 1\n"
        "- Item 2 :p{.boxed}\n";

    auto proj = markdownParse(md);
    CHECK(proj.blocks.size() == 5u);

    // Heading
    CHECK(proj.blocks[0].kind == "heading");
    CHECK(proj.blocks[0].headingLevel == std::optional<int>(1));
    CHECK(proj.blocks[0].spans[0].text == "Main Title");

    // Paragraph with formatting
    CHECK(proj.blocks[1].kind == "paragraph");
    bool has_bold = false;
    bool has_italic = false;
    bool has_code = false;
    bool has_link = false;
    for (const auto& span : proj.blocks[1].spans) {
        if (span.marks.bold && span.text == "bold") has_bold = true;
        if (span.marks.italic && span.text == "italic") has_italic = true;
        if (span.marks.code && span.text == "code") has_code = true;
        if (span.marks.link == std::optional<std::string>("https://google.com")) has_link = true;
    }
    CHECK(has_bold);
    CHECK(has_italic);
    CHECK(has_code);
    CHECK(has_link);

    // Styled text directive
    CHECK(proj.blocks[2].kind == "paragraph");
    bool has_directive = false;
    for (const auto& span : proj.blocks[2].spans) {
        if (span.directive == std::optional<std::string>("accent") && span.text == "Accent") {
            has_directive = true;
        }
    }
    CHECK(has_directive);

    // List item with paragraph style marker
    CHECK(proj.blocks[3].kind == "listItem");
    CHECK(proj.blocks[4].kind == "listItem");
    CHECK(proj.blocks[4].paragraphStyle == std::optional<std::string>("boxed"));
}

TEST_CASE("markdownParse parses tables, merges, and referenced cells") {
    std::string md =
        "| Header 1 | Header 2 |\n"
        "| --- | --- |\n"
        "| Cell 1 | :merge{left} |\n"
        "| :cell{ref=t1r2c1} | Cell 4 |\n\n"
        "::cell{id=t1r2c1 start}\n"
        "Multi-block content.\n\n"
        "- List in cell\n"
        "::cell{id=t1r2c1 end}\n";

    auto proj = markdownParse(md);
    CHECK(proj.blocks.size() == 1u);
    CHECK(proj.blocks[0].kind == "table");
    const auto& t = *proj.blocks[0].table;
    CHECK(t.rows.size() == 3u);
    CHECK(t.merges.size() == 1u);
    CHECK(t.merges[0].row == 1);
    CHECK(t.merges[0].column == 0);
    CHECK(t.merges[0].columnSpan == 2);

    CHECK(t.cells[2][0].size() >= 1);
}

} // namespace gdocsmith::lens::test
