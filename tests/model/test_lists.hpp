#pragma once

#include "model/lists.hpp"
#include "test.hpp"

namespace gdocsmith::model::test {

inline PresetTable makeSyntheticTable() {
    PresetTable table;
    table["BULLET_CHECKBOX"] = {
        slimr::Json::object_t{{"glyphFormat", "%0"}, {"glyphSymbol", "☐"}, {"glyphType", "GLYPH_TYPE_UNSPECIFIED"}},
        slimr::Json::object_t{{"glyphFormat", "%1"}, {"glyphSymbol", "☐"}, {"glyphType", "GLYPH_TYPE_UNSPECIFIED"}}
    };
    table["BULLET_DISC_CIRCLE_SQUARE"] = {
        slimr::Json::object_t{{"glyphFormat", "%0"}, {"glyphSymbol", "●"}, {"glyphType", "GLYPH_TYPE_UNSPECIFIED"}},
        slimr::Json::object_t{{"glyphFormat", "%1"}, {"glyphSymbol", "○"}, {"glyphType", "GLYPH_TYPE_UNSPECIFIED"}},
        slimr::Json::object_t{{"glyphFormat", "%2"}, {"glyphSymbol", "■"}, {"glyphType", "GLYPH_TYPE_UNSPECIFIED"}}
    };
    table["NUMBERED_DECIMAL_ALPHA_ROMAN"] = {
        slimr::Json::object_t{{"glyphFormat", "%0."}, {"glyphType", "DECIMAL"}},
        slimr::Json::object_t{{"glyphFormat", "%1."}, {"glyphType", "ALPHA"}}
    };
    return table;
}

TEST_CASE("lists - infers preset from synthetic table") {
    using namespace gdocsmith::model;
    auto synTable = makeSyntheticTable();
    std::vector<RawListNestingLevel> levels1 = {
        slimr::Json::object_t{
            {"glyphFormat", "%0"},
            {"glyphSymbol", "●"},
            {"glyphType", "GLYPH_TYPE_UNSPECIFIED"},
            {"indentFirstLine", slimr::Json::object_t{}}
        }
    };
    CHECK(listPresetInfer(levels1, synTable) == "BULLET_DISC_CIRCLE_SQUARE");

    std::vector<RawListNestingLevel> levels2 = {
        slimr::Json::object_t{{"glyphFormat", "%0."}, {"glyphType", "DECIMAL"}}
    };
    CHECK(listPresetInfer(levels2, synTable) == "NUMBERED_DECIMAL_ALPHA_ROMAN");

    std::vector<RawListNestingLevel> levels3 = {
        slimr::Json::object_t{{"glyphFormat", "%0"}, {"glyphSymbol", "☃"}}
    };
    CHECK(listPresetInfer(levels3, synTable) == std::nullopt);
}

TEST_CASE("lists - ignores indents and textStyle when matching a preset") {
    using namespace gdocsmith::model;
    auto synTable = makeSyntheticTable();
    std::vector<RawListNestingLevel> levels = {
        slimr::Json::object_t{
            {"glyphFormat", "%0"},
            {"glyphSymbol", "●"},
            {"glyphType", "GLYPH_TYPE_UNSPECIFIED"},
            {"indentFirstLine", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(99)}}},
            {"textStyle", slimr::Json::object_t{{"bold", true}}}
        }
    };
    CHECK(listPresetInfer(levels, synTable) == "BULLET_DISC_CIRCLE_SQUARE");
}

TEST_CASE("lists - kind: check, number, bullet") {
    using namespace gdocsmith::model;
    ListDef check{.isNew = false, .preset = "BULLET_CHECKBOX"};
    ListDef number{.isNew = false, .preset = "NUMBERED_DECIMAL_ALPHA_ROMAN"};
    ListDef bullet{.isNew = false, .preset = "BULLET_DISC_CIRCLE_SQUARE"};
    CHECK(listKind(check, 0) == "check");
    CHECK(listKind(number, 0) == "number");
    CHECK(listKind(bullet, 0) == "bullet");
}

TEST_CASE("lists - kind falls back to raw glyph fields when the preset is unrecognized") {
    using namespace gdocsmith::model;
    ListDef unrec{.isNew = false, .nestingLevels = {slimr::Json::object_t{{"glyphType", "DECIMAL"}}}};
    CHECK(listKind(unrec, 0) == "number");
    ListDef plain{.isNew = false, .nestingLevels = {slimr::Json::object_t{{"glyphSymbol", "●"}}}};
    CHECK(listKind(plain, 0) == "bullet");
}

TEST_CASE("lists - listLevelIndent increases with nesting") {
    using namespace gdocsmith::model;
    ListDef def{.isNew = false};
    auto l0 = listLevelIndent(def, 0);
    CHECK(l0.indentFirstLine == 18.0);
    CHECK(l0.indentStart == 36.0);
    auto l1 = listLevelIndent(def, 1);
    CHECK(l1.indentStart > l0.indentStart);
}

} // namespace gdocsmith::model::test
