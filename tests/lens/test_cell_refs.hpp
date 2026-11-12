#pragma once

#include "lens/cell_refs.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("cellRefRender and cellDefinitionLine") {
    CHECK(cellRefRender(1, 2, 2) == "t1r3c3");
    CHECK(cellRefRender(0, 0, 0) == "t0r1c1");
    CHECK(cellDefinitionLine("t1r2c2", "start") == "::cell{id=t1r2c2 start}");
    CHECK(cellDefinitionLine("t1r2c2", "end") == "::cell{id=t1r2c2 end}");
}

TEST_CASE("A1 notation conversions") {
    CHECK(toA1(0, 0) == "A1");
    CHECK(toA1(0, 25) == "Z1");
    CHECK(toA1(0, 26) == "AA1");
    CHECK(toA1(9, 1) == "B10");

    auto p1 = fromA1("A1");
    CHECK(p1.has_value());
    CHECK(p1->first == 0);
    CHECK(p1->second == 0);

    auto p2 = fromA1("Z1");
    CHECK(p2.has_value());
    CHECK(p2->first == 0);
    CHECK(p2->second == 25);

    auto p3 = fromA1("B10");
    CHECK(p3.has_value());
    CHECK(p3->first == 9);
    CHECK(p3->second == 1);

    CHECK(!fromA1("").has_value());
    CHECK(!fromA1("123").has_value());
    CHECK(!fromA1("ABC").has_value());
}

} // namespace gdocsmith::lens::test
