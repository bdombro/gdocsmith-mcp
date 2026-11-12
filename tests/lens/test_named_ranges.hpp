#pragma once

#include "lens/named_ranges.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("RangeMark equality and fields") {
    RangeMark m1{.edge = "start", .key = "p1", .name = "scope", .offset = 0, .same = true};
    RangeMark m2{.edge = "start", .key = "p1", .name = "scope", .offset = 0, .same = true};
    CHECK(m1 == m2);

    RangeMark m3{.edge = "end", .key = "p1", .name = "scope", .offset = 5, .same = true};
    CHECK(!(m1 == m3));
}

TEST_CASE("RangeCreate equality and fields") {
    RangeCreate rc1{
        .start = {.key = "p1", .offset = 0},
        .end = {.key = "p1", .offset = 10},
        .name = "my_range"
    };
    RangeCreate rc2{
        .start = {.key = "p1", .offset = 0},
        .end = {.key = "p1", .offset = 10},
        .name = "my_range"
    };
    CHECK(rc1 == rc2);
}

} // namespace gdocsmith::lens::test
