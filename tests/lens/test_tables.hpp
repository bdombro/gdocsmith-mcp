#pragma once

#include "lens/tables.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("mergeCovered computes covered cells") {
    std::vector<TableMerge> merges = {
        TableMerge{.column = 0, .columnSpan = 2, .row = 0, .rowSpan = 2}
    };
    auto covered = mergeCovered(merges);
    CHECK(covered.size() == 3u);
    CHECK(!covered.contains({0, 0}));
    CHECK(covered.contains({0, 1}));
    CHECK(covered.contains({1, 0}));
    CHECK(covered.contains({1, 1}));
}

TEST_CASE("dice2 similarity metric") {
    CHECK(dice2("", "") == 1.0);
    CHECK(dice2("hello", "hello") == 1.0);
    CHECK(dice2("hello", "world") < 0.5);
    CHECK(dice2("table header", "table headers") > 0.8);
}

} // namespace gdocsmith::lens::test
