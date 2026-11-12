#pragma once

#include "lens/parse.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

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
