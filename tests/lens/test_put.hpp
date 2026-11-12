#pragma once

#include "lens/put.hpp"
#include "lens/parse.hpp"
#include "lens/json_helper.hpp"
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

} // namespace gdocsmith::lens::test
