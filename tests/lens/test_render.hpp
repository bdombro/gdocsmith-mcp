#pragma once

#include "lens/render.hpp"
#include "lens/parse.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("markdownRender renders blocks and formatted inlines") {
    Projection proj;

    ProjBlock h;
    h.kind = "heading";
    h.headingLevel = 2;
    ProjSpan hs;
    hs.kind = "text";
    hs.text = "Section Title";
    h.spans.push_back(hs);
    proj.blocks.push_back(h);

    ProjBlock p;
    p.kind = "paragraph";
    ProjSpan ps1;
    ps1.kind = "text";
    ps1.text = "A ";
    ProjSpan ps2;
    ps2.kind = "text";
    ps2.text = "bold";
    ps2.marks.bold = true;
    ProjSpan ps3;
    ps3.kind = "text";
    ps3.text = " word.";
    p.spans.push_back(ps1);
    p.spans.push_back(ps2);
    p.spans.push_back(ps3);
    proj.blocks.push_back(p);

    std::string rendered = markdownRender(proj);
    CHECK(rendered == "## Section Title\n\nA **bold** word.\n");
}

TEST_CASE("Node markdownRender final separator omits empty unstyled paragraphs") {
    Projection projection;
    ProjBlock first;
    first.spans.push_back(ProjSpan{.text = "first"});
    ProjBlock styledEmpty;
    styledEmpty.paragraphStyle = "p1";
    projection.blocks = {first, ProjBlock{}, styledEmpty};
    const auto rendered = markdownRender(projection);
    CHECK(rendered == "first\n\n\n\n :p{.p1}\n");
    CHECK(rendered.find(":style[]{}") == std::string::npos);
    const auto reparsed = markdownParse(rendered);
    REQUIRE(reparsed.blocks.size() == 2);
    CHECK(reparsed.blocks[1].paragraphStyle == std::optional<std::string>("p1"));
    CHECK(reparsed.blocks[1].spans.empty());
}

TEST_CASE("markdownTextEscape escapes colons only before directive collisions") {
    CHECK(markdownTextEscape("Launch date: March 3") == "Launch date: March 3");
    CHECK(markdownTextEscape("foo::section") == "foo::section");
    CHECK(markdownTextEscape("::sections") == "::sections");
    CHECK(markdownTextEscape("::section") == "\\::section");
    CHECK(markdownTextEscape(" ::pagebreak") == " \\::pagebreak");
    CHECK(markdownTextEscape("::toc") == "\\::toc");
    CHECK(markdownTextEscape("::section{id=s1}") == "\\::section\\{id=s1\\}");
    CHECK(markdownTextEscape("\\::section") == "\\\\::section");
}

TEST_CASE("markdownRender and markdownParse round trip") {
    std::string md =
        "# Heading 1\n\n"
        "Paragraph with **bold** text.\n\n"
        "- List item 1\n"
        "- List item 2\n\n";

    auto proj = markdownParse(md);
    std::string rendered = markdownRender(proj);
    auto proj2 = markdownParse(rendered);
    CHECK(proj.blocks.size() == proj2.blocks.size());
    for (size_t i = 0; i < proj.blocks.size(); i++) {
        CHECK(proj.blocks[i].kind == proj2.blocks[i].kind);
    }
}

TEST_CASE("rich span marks and literal Markdown survive render and parse") {
    ProjSpan span;
    span.text = "literal **bold** [link] :chip{}";
    span.marks.bold = true;
    span.marks.italic = true;
    span.marks.underline = true;
    span.marks.smallCaps = true;
    span.marks.link = "https://example.com";
    const auto parsed = parseInlines(renderSpans({span}));
    REQUIRE(parsed.size() == 1u);
    CHECK(parsed[0] == span);
}

TEST_CASE("multiple tables retain independent referenced cell contents") {
    Projection projection;
    for (const auto& text : {"first", "second"}) {
        ProjSpan span;
        span.text = text;
        ProjBlock cell;
        cell.spans.push_back(span);
        ProjBlock table;
        table.kind = "table";
        table.table.emplace();
        table.table->rows = {{{}}};
        table.table->cells = {{{cell}}};
        projection.blocks.push_back(std::move(table));
    }
    const auto parsed = markdownParse(markdownRender(projection));
    REQUIRE(parsed.blocks.size() == 2u);
    CHECK(parsed.blocks[0].table->cells[0][0][0].spans[0].text == "first");
    CHECK(parsed.blocks[1].table->cells[0][0][0].spans[0].text == "second");
}

} // namespace gdocsmith::lens::test
