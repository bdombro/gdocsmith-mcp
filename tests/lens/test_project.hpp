#pragma once

#include "lens/project.hpp"
#include "lens/json_helper.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

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
