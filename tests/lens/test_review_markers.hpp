#pragma once

#include "lens/review_markers.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("reviewMarkersStrip removes inline markers and records positions") {
    std::vector<FileProblem> problems;
    std::string md =
        "Intro paragraph with :comment{id=c1 start}marked text:comment{id=c1 end}.\n"
        "Another line with :suggestion{id=s1 start}inserted:suggestion{id=s1 end} and point :suggestion{id=s2}.\n\n"
        "```\n"
        "Code with :comment{id=c2 start} preserved\n"
        "```\n\n"
        ":::comment{id=c1}\n"
        "Comment body\n"
        ":::\n";

    auto marks = reviewMarkersStrip(md, problems, 1);
    CHECK(problems.size() == 0u);
    CHECK(marks.markers.size() == 5u);

    CHECK(marks.markers[0].name == "comment");
    CHECK(marks.markers[0].id == "c1");
    CHECK(marks.markers[0].edge == "start");

    CHECK(marks.markers[1].name == "comment");
    CHECK(marks.markers[1].id == "c1");
    CHECK(marks.markers[1].edge == "end");

    CHECK(marks.markers[2].name == "suggestion");
    CHECK(marks.markers[2].id == "s1");
    CHECK(marks.markers[2].edge == "start");

    CHECK(marks.markers[3].name == "suggestion");
    CHECK(marks.markers[3].id == "s1");
    CHECK(marks.markers[3].edge == "end");

    CHECK(marks.markers[4].name == "suggestion");
    CHECK(marks.markers[4].id == "s2");
    CHECK(marks.markers[4].edge == "point");

    // Inside code block, `:comment{id=c2 start}` is preserved
    CHECK(marks.text.find(":comment{id=c2 start}") != std::string::npos);
    // Outside code block, inline markers are stripped
    CHECK(marks.text.find("marked text.") != std::string::npos);
    CHECK(marks.text.find("inserted and point .") != std::string::npos);
}

} // namespace gdocsmith::lens::test
