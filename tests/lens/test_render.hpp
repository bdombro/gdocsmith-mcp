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
    CHECK(rendered == "## Section Title\n\nA **bold** word.\n\n");
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

} // namespace gdocsmith::lens::test
