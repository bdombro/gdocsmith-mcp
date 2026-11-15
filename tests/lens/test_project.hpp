#pragma once

#include "lens/json_helper.hpp"
#include "lens/project.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("Node hidden-only paragraph content projects no false marker while current newline and real blank paragraphs survive") {
    model::TabModel tab;
    model::ParagraphBlock paragraph;
    paragraph.key = "hidden";
    for (const auto *text : {"first hidden run", "second hidden run"}) {
        auto item = model::Inline::from_text(text);
        item.suggestedInsertionIds = std::vector<std::string>{"s1"};
        paragraph.inlines.push_back(item);
    }
    model::Atom person{.key = "hidden-person", .type = "person", .raw = slimr::Json::object_t{{"person", slimr::Json::object_t{{"suggestedInsertionIds", slimr::Json::array_t{"s1"}}}}}};
    paragraph.inlines.emplace_back(person);
    tab.blocks.emplace_back(paragraph);
    const auto original = tab;
    CHECK(projectionBuild(tab).blocks.empty());
    CHECK(tab == original);
    model::ParagraphBlock prefix;
    prefix.key = "prefix";
    prefix.inlines.push_back(model::Inline::from_text("prior visible text"));
    tab.blocks.insert(tab.blocks.begin(), prefix);
    const auto anchored = projectionBuild(tab);
    REQUIRE(anchored.blocks.size() == 1);
    REQUIRE(anchored.blocks[0].spans.back().token);
    CHECK(anchored.blocks[0].spans.back().token->kind == "suggestion");
    CHECK(anchored.blocks[0].spans.back().token->attrs.get_value("id") == "s1");
    CHECK(anchored.blocks[0].spans.back().token->attrs.flags.empty());
    tab = original;
    model::ParagraphBlock blank;
    blank.key = "real-blank";
    tab.blocks.emplace_back(blank);
    const auto blanks = projectionBuild(tab);
    REQUIRE(blanks.blocks.size() == 1);
    CHECK(blanks.blocks[0].spans.empty());
    tab.blocks[0].as_paragraph().inlines.insert(tab.blocks[0].as_paragraph().inlines.begin(),
                                                model::Inline::from_text("visible prefix "));
    const auto mixed = projectionBuild(tab);
    REQUIRE(mixed.blocks.size() == 2);
    CHECK(std::any_of(mixed.blocks[0].spans.begin(), mixed.blocks[0].spans.end(), [](const auto &span) {
        return span.kind == "text" && span.text.find("visible") != std::string::npos;
    }));
    CHECK(std::any_of(mixed.blocks[0].spans.begin(), mixed.blocks[0].spans.end(), [](const auto &span) {
        return span.token && span.token->kind == "suggestion";
    }));
    tab = original;
    tab.blocks[0].as_paragraph().positionedObjectIds = std::vector<std::string>{"positioned"};
    const auto positioned = tab;
    CHECK(projectionBuild(tab).blocks.empty());
    CHECK(tab == positioned);
    tab = original;
    tab.blocks[0].as_paragraph().bullet = model::BulletRef{};
    CHECK(projectionBuild(tab).blocks.size() == 1);
}

TEST_CASE("Node native URL links pass through document-aware projection with external fallback") {
    model::DocModel document;
    document.docId = "synthetic";
    model::TabModel tab;
    tab.tabId = "main";
    model::ParagraphBlock heading;
    heading.key = "heading";
    heading.headingId = "provider-heading";
    heading.style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    heading.inlines.push_back(model::Inline::from_text("Goals"));
    tab.blocks.emplace_back(heading);
    for (const auto &url : {"https://docs.google.com/document/d/synthetic/edit?tab=main#heading=provider-heading",
                            "https://example.invalid/external"}) {
        model::ParagraphBlock paragraph;
        paragraph.key = url;
        paragraph.inlines.push_back(model::Inline::from_text("link",
                                                             slimr::Json::object_t{{"link", slimr::Json::object_t{{"url", url}}}}));
        tab.blocks.emplace_back(paragraph);
    }
    document.tabs.push_back(tab);
    const auto projection = projectionBuild(document.tabs[0], &document);
    CHECK(projection.blocks[1].spans[0].marks.link == "#goals");
    CHECK(projection.blocks[2].spans[0].marks.link == "https://example.invalid/external");
}
TEST_CASE("projectionBuild projects tab JSON into Projection") {
    slimr::Json tab = slimr::json_obj();
    slimr::Json blocks = slimr::json_arr();

    slimr::Json p = slimr::json_obj();
    p["kind"] = "paragraph";
    slimr::Json inlines = slimr::json_arr();
    slimr::Json t1 = slimr::json_obj();
    t1["kind"] = "text";
    t1["text"] = "Hello ";
    json::push_back(inlines, std::move(t1));

    slimr::Json t2 = slimr::json_obj();
    t2["kind"] = "text";
    t2["text"] = "World";
    slimr::Json st = slimr::json_obj();
    st["bold"] = true;
    t2["style"] = std::move(st);
    json::push_back(inlines, std::move(t2));

    p["inlines"] = std::move(inlines);
    json::push_back(blocks, std::move(p));
    tab["blocks"] = std::move(blocks);

    auto proj = projectionBuild(tab);
    CHECK(proj.blocks.size() == 1u);
    CHECK(proj.blocks[0].kind == "paragraph");
    CHECK(proj.blocks[0].spans.size() == 2u);
    CHECK(proj.blocks[0].spans[0].text == "Hello ");
    CHECK(proj.blocks[0].spans[1].text == "World");
    CHECK(proj.blocks[0].spans[1].marks.bold);
}

} // namespace gdocsmith::lens::test
