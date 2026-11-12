#pragma once

#include "lens/sections.hpp"
#include "lens/json_helper.hpp"
#include "test.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace gdocsmith::lens::test {

inline slimr::Json dim(double magnitude) {
    return slimr::Json::object_t{{"magnitude", magnitude}};
}

TEST_CASE("sectionTypeFlag maps CONTINUOUS and defaults to nextPage") {
    slimr::Json cont = slimr::json_obj();
    cont["sectionType"] = "CONTINUOUS";
    CHECK(sectionTypeFlag(cont) == "continuous");

    slimr::Json next = slimr::json_obj();
    next["sectionType"] = "NEXT_PAGE";
    CHECK(sectionTypeFlag(next) == "nextPage");

    slimr::Json empty = slimr::json_obj();
    CHECK(sectionTypeFlag(empty) == "nextPage");

    CHECK(sectionTypeFlag(slimr::Json("not an object")) == "nextPage");
}

TEST_CASE("sectionSettingsAttrs renders values and flags") {
    SectionSettings s;
    s.columns = 2;
    s.columnGap = 40.0;
    s.margins = std::array<double, 4>{72.0, 54.0, 72.0, 54.0};
    s.pageNumberStart = 3;
    s.columnSeparator = true;
    s.firstPage = true;
    s.landscape = true;
    s.rtl = true;

    auto attrs = sectionSettingsAttrs(s);
    CHECK(attrs.get_value("columns") == std::optional<std::string>("2"));
    CHECK(attrs.get_value("columnGap") == std::optional<std::string>("40"));
    CHECK(attrs.get_value("margins") == std::optional<std::string>("72,54,72,54"));
    CHECK(attrs.get_value("pageNumberStart") == std::optional<std::string>("3"));
    CHECK(attrs.has_flag("columnSeparator"));
    CHECK(attrs.has_flag("firstPage"));
    CHECK(attrs.has_flag("landscape"));
    CHECK(attrs.has_flag("rtl"));
}

TEST_CASE("sectionSettingsAttrs omits unset fields") {
    SectionSettings s;
    auto attrs = sectionSettingsAttrs(s);
    CHECK(!attrs.has_value("columns"));
    CHECK(!attrs.has_value("columnGap"));
    CHECK(!attrs.has_value("margins"));
    CHECK(!attrs.has_value("pageNumberStart"));
    CHECK(!attrs.has_flag("columnSeparator"));
    CHECK(!attrs.has_flag("firstPage"));
    CHECK(!attrs.has_flag("landscape"));
    CHECK(!attrs.has_flag("rtl"));
}

TEST_CASE("sectionSettingsOf reads columns, separator, orientation, margins and rtl") {
    slimr::Json style = slimr::json_obj();

    slimr::Json colProps = slimr::json_arr();
    slimr::Json c0 = slimr::json_obj();
    c0["paddingEnd"] = dim(40.0);
    slimr::Json c1 = slimr::json_obj();
    c1["paddingEnd"] = dim(40.0);
    json::push_back(colProps, std::move(c0));
    json::push_back(colProps, std::move(c1));
    style["columnProperties"] = std::move(colProps);

    style["columnSeparatorStyle"] = "BETWEEN_EACH_COLUMN";
    style["useFirstPageHeaderFooter"] = true;
    style["flipPageOrientation"] = true;
    style["marginTop"] = dim(72.0);
    style["marginRight"] = dim(54.0);
    style["marginBottom"] = dim(72.0);
    style["marginLeft"] = dim(54.0);
    style["pageNumberStart"] = 3;
    style["contentDirection"] = "RIGHT_TO_LEFT";

    SectionSettings expected;
    expected.columns = 2;
    expected.columnGap = 40.0;
    expected.columnSeparator = true;
    expected.firstPage = true;
    expected.landscape = true;
    expected.margins = std::array<double, 4>{72.0, 54.0, 72.0, 54.0};
    expected.pageNumberStart = 3;
    expected.rtl = true;

    CHECK(sectionSettingsOf(style, slimr::json_obj()) == expected);
}

TEST_CASE("sectionSettingsOf treats a 36pt column gap as the default") {
    slimr::Json style = slimr::json_obj();
    slimr::Json colProps = slimr::json_arr();
    slimr::Json c0 = slimr::json_obj();
    c0["paddingEnd"] = dim(36.0);
    slimr::Json c1 = slimr::json_obj();
    c1["paddingEnd"] = dim(36.0);
    json::push_back(colProps, std::move(c0));
    json::push_back(colProps, std::move(c1));
    style["columnProperties"] = std::move(colProps);

    auto s = sectionSettingsOf(style, slimr::json_obj());
    CHECK(s.columns == std::optional<int>(2));
    CHECK(!s.columnGap.has_value());
}

TEST_CASE("sectionSettingsOf fills missing margins from tab style then 72") {
    slimr::Json style = slimr::json_obj();
    style["marginTop"] = dim(100.0);

    slimr::Json tabStyle = slimr::json_obj();
    tabStyle["marginRight"] = dim(50.0);

    auto s = sectionSettingsOf(style, tabStyle);
    REQUIRE(s.margins.has_value());
    CHECK((*s.margins)[0] == 100.0);
    CHECK((*s.margins)[1] == 50.0);
    CHECK((*s.margins)[2] == 72.0);
    CHECK((*s.margins)[3] == 72.0);
}

TEST_CASE("sectionSettingsOf leaves margins unset when no side is present") {
    auto s = sectionSettingsOf(slimr::json_obj(), slimr::json_obj());
    CHECK(!s.margins.has_value());
    CHECK(!s.columns.has_value());
    CHECK(!s.columnSeparator);
    CHECK(!s.firstPage);
    CHECK(!s.landscape);
    CHECK(!s.rtl);
}

TEST_CASE("sectionSettingsParse reads directive values and flags") {
    auto s = sectionSettingsParse(
        {"columnSeparator", "landscape"},
        {{"columns", "2"}, {"columnGap", "40"}, {"margins", "72,54,72,54"}, {"pageNumberStart", "3"}}
    );
    CHECK(s.columns == std::optional<int>(2));
    CHECK(s.columnGap == std::optional<double>(40.0));
    REQUIRE(s.margins.has_value());
    CHECK((*s.margins)[0] == 72.0);
    CHECK((*s.margins)[1] == 54.0);
    CHECK((*s.margins)[2] == 72.0);
    CHECK((*s.margins)[3] == 54.0);
    CHECK(s.pageNumberStart == std::optional<int>(3));
    CHECK(s.columnSeparator);
    CHECK(s.landscape);
    CHECK(!s.firstPage);
    CHECK(!s.rtl);
}

TEST_CASE("sectionSettingsParse rejects invalid directive values") {
    auto throws_with = [](const std::vector<std::string>& flags,
                          const std::vector<std::pair<std::string, std::string>>& values,
                          std::string_view needle) {
        bool threw = false;
        try {
            sectionSettingsParse(flags, values);
        } catch (const CoreError& err) {
            threw = true;
            CHECK(err.code == std::string("unsupportedSyntax"));
            CHECK(std::string(err.what()).find(needle) != std::string::npos);
        }
        CHECK(threw);
    };

    throws_with({}, {{"columns", "4"}}, "columns must be 1, 2 or 3");
    throws_with({}, {{"columns", "0"}}, "columns must be 1, 2 or 3");
    throws_with({}, {{"columns", "-1"}}, "nonnegative");
    throws_with({}, {{"columnGap", "40"}}, "columnGap needs columns");
    throws_with({}, {{"margins", "72,54,72"}}, "margins must be four");
    throws_with({"columnSeparator"}, {}, "columnSeparator needs columns");
}

} // namespace gdocsmith::lens::test
