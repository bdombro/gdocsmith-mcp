#pragma once

#include "model/style_values.hpp"
#include "test.hpp"

#include <algorithm>
#include <vector>

namespace gdocsmith::model::test {

TEST_CASE("styleValues - Dimension without magnitude equals 0") {
    using namespace gdocsmith::model;
    CHECK(dimensionPt(JsonObject{}) == 0.0);
    JsonObject dim1 = slimr::Json::object_t{{"unit", "PT"}};
    CHECK(dimensionPt(dim1) == 0.0);
    JsonObject dim2 = slimr::Json::object_t{{"magnitude", static_cast<int64_t>(12)}, {"unit", "PT"}};
    CHECK(dimensionPt(dim2) == 12.0);
    JsonObject dimFrom = dimensionFromPt(12.0);
    CHECK(dimFrom["magnitude"].get<double>() == 12.0);
    CHECK(dimFrom["unit"].get<std::string>() == "PT");
}

TEST_CASE("styleValues - rgbColor missing channels equals black") {
    using namespace gdocsmith::model;
    JsonObject opt1 = slimr::Json::object_t{{"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{}}}}};
    CHECK(colorHexFromOptional(opt1) == "#000000");

    JsonObject opt2 = slimr::Json::object_t{{"color", slimr::Json::object_t{}}};
    CHECK(colorHexFromOptional(opt2) == "#000000");

    CHECK(colorHexFromOptional(JsonObject{}) == std::nullopt);

    JsonObject optHex = colorOptionalFromHex("#1155CC");
    CHECK(optHex.contains("color"));
    CHECK(optHex["color"].contains("rgbColor"));
    auto blue = optHex["color"]["rgbColor"]["blue"].get<double>();
    auto green = optHex["color"]["rgbColor"]["green"].get<double>();
    auto red = optHex["color"]["rgbColor"]["red"].get<double>();
    CHECK(std::abs(blue - (0xCC / 255.0)) < 1e-4);
    CHECK(std::abs(green - (0x55 / 255.0)) < 1e-4);
    CHECK(std::abs(red - (0x11 / 255.0)) < 1e-4);
}

TEST_CASE("styleValues - styleEqual ignores key order") {
    using namespace gdocsmith::model;
    JsonObject a = slimr::Json::object_t{{"a", static_cast<int64_t>(1)}, {"b", static_cast<int64_t>(2)}};
    JsonObject b = slimr::Json::object_t{{"b", static_cast<int64_t>(2)}, {"a", static_cast<int64_t>(1)}};
    CHECK(styleEqual(a, b));

    JsonObject c = slimr::Json::object_t{{"a", static_cast<int64_t>(2)}};
    CHECK(!styleEqual(a, c));

    JsonObject fs1 = slimr::Json::object_t{{"fontSize", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(12)}, {"unit", "PT"}}}};
    JsonObject fs2 = slimr::Json::object_t{{"fontSize", slimr::Json::object_t{{"unit", "PT"}, {"magnitude", static_cast<int64_t>(12)}}}};
    CHECK(styleEqual(fs1, fs2));

    JsonObject fs3 = slimr::Json::object_t{{"fontSize", slimr::Json::object_t{}}};
    CHECK(!styleEqual(fs1, fs3));
}

TEST_CASE("styleValues - styleFieldsChanged reports added, removed, and changed fields") {
    using namespace gdocsmith::model;
    JsonObject a = slimr::Json::object_t{
        {"bold", true},
        {"foregroundColor", slimr::Json::object_t{{"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{{"red", 1.0}}}}}}}
    };
    JsonObject b = slimr::Json::object_t{
        {"bold", true},
        {"italic", true},
        {"foregroundColor", slimr::Json::object_t{{"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{{"red", 0.5}}}}}}}
    };
    std::vector<std::string> fields = {"bold", "italic", "foregroundColor"};
    auto changed = styleFieldsChanged(a, b, fields);
    std::sort(changed.begin(), changed.end());
    std::vector<std::string> expected = {"foregroundColor", "italic"};
    CHECK(changed == expected);

    auto changedBold = styleFieldsChanged(a, b, {"bold"});
    CHECK(changedBold.empty());
}

TEST_CASE("styleValues - monospaceFontIs accepts Courier New and Roboto Mono, rejects Arial") {
    using namespace gdocsmith::model;
    CHECK(monospaceFontIs("Courier New"));
    CHECK(monospaceFontIs("Roboto Mono"));
    CHECK(!monospaceFontIs("Arial"));
    CHECK(!monospaceFontIs(std::nullopt));
}

} // namespace gdocsmith::model::test
