#pragma once

#include "model/effective_style.hpp"
#include "model/invariants.hpp"
#include "test.hpp"

#include <algorithm>

namespace gdocsmith::model::test {

inline Block foundation_paragraph(std::string key) {
    ParagraphBlock paragraph;
    paragraph.key = std::move(key);
    paragraph.style = make_json_object();
    paragraph.newline.style = make_json_object();
    return Block(std::move(paragraph));
}

inline Block foundation_table(std::string key) {
    TableBlock table;
    table.key = std::move(key);
    return Block(std::move(table));
}

TEST_CASE("foundation effective style uses normal then heading then explicit run overrides") {
    TabModel tab;
    tab.namedStyles = slimr::Json::object_t{{"styles", slimr::Json::array_t{
                                                           slimr::Json::object_t{
                                                               {"namedStyleType", "NORMAL_TEXT"},
                                                               {"paragraphStyle", slimr::Json::object_t{{"alignment", "START"}}},
                                                               {"textStyle", slimr::Json::object_t{
                                                                                 {"fontSize", slimr::Json::object_t{{"magnitude", 11}, {"unit", "PT"}}}}}},
                                                           slimr::Json::object_t{{"namedStyleType", "HEADING_1"}, {"paragraphStyle", slimr::Json::object_t{{"spaceAbove", slimr::Json::object_t{{"magnitude", 20}, {"unit", "PT"}}}}}, {"textStyle", slimr::Json::object_t{{"bold", true}, {"fontSize", slimr::Json::object_t{{"magnitude", 20}, {"unit", "PT"}}}}}}}}};
    auto paragraph = foundation_paragraph("o1").as_paragraph();
    paragraph.headingId = "h1";
    paragraph.style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    const JsonObject heading = slimr::Json::object_t{
        // Native style canonicalization represents dimensions as doubles (JS has one number type).
        {"bold", true},
        {"fontSize", slimr::Json::object_t{{"magnitude", 20.0}, {"unit", "PT"}}}};
    CHECK(textStyleEffective(tab, paragraph, make_json_object()) == heading);
    auto explicit_false = heading;
    explicit_false["bold"] = false;
    CHECK(textStyleEffective(tab, paragraph, slimr::Json::object_t{{"bold", false}}) == explicit_false);
    paragraph.style = make_json_object();
    const JsonObject normal = slimr::Json::object_t{
        {"fontSize", slimr::Json::object_t{{"magnitude", 11.0}, {"unit", "PT"}}}};
    CHECK(textStyleEffective(tab, paragraph, make_json_object()) == normal);
}

TEST_CASE("foundation heading classification retains title subtitle and absent distinctions") {
    CHECK(headingLevel("TITLE") == std::optional<int>{0});
    CHECK(headingLevel("SUBTITLE") == std::optional<int>{1});
    CHECK(headingLevel("HEADING_3") == std::optional<int>{3});
    CHECK(headingLevel("NORMAL_TEXT") == std::nullopt);
    // The native API uses an empty string_view for JS's absent optional style.
    CHECK(headingLevel({}) == std::nullopt);
    CHECK(headingStyleIs("HEADING_1"));
    CHECK_FALSE(headingStyleIs("NORMAL_TEXT"));
}

TEST_CASE("foundation container diagnostics distinguish terminal and adjacent tables") {
    const auto terminal = containerValidate({foundation_paragraph("o1"), foundation_table("o2")});
    CHECK(std::any_of(terminal.begin(), terminal.end(), [](const auto &problem) {
        return problem.find("V1") != std::string::npos;
    }));
    const auto adjacent = containerValidate({foundation_paragraph("o1"), foundation_table("o2"), foundation_table("o3"), foundation_paragraph("o4")});
    CHECK(std::any_of(adjacent.begin(), adjacent.end(), [](const auto &problem) {
        return problem.find("block 2 (table)") != std::string::npos;
    }));
    SectionBreakBlock section;
    section.key = "o4";
    section.sectionStyle = make_json_object();
    CHECK(containerValidate({foundation_paragraph("o1"), foundation_table("o2"), foundation_paragraph("o3"),
                             Block(section), foundation_paragraph("o5")})
              .empty());
}

TEST_CASE("foundation normalization repairs exact gaps and preserves stable keys of valid blocks") {
    KeyAllocator keys;
    const EditStamp stamp{.force = false, .tabIndex = 0};
    const std::vector<Block> invalid{foundation_table("o1"), foundation_table("o2")};
    const auto normalized = containerNormalize(invalid, keys, stamp);
    std::vector<std::string> kinds;
    for (const auto &block : normalized)
        kinds.push_back(block.kind);
    CHECK(kinds == std::vector<std::string>{"paragraph", "table", "paragraph", "table", "paragraph"});
    CHECK(containerValidate(normalized).empty());
    REQUIRE(normalized.size() == 5);
    CHECK(normalized[1] == invalid[0]);
    CHECK(normalized[3] == invalid[1]);
    for (const size_t index : {size_t{0}, size_t{2}, size_t{4}}) {
        CHECK(normalized[index].key.starts_with("n"));
        CHECK(normalized[index].as_paragraph().inlines.empty());
        CHECK(normalized[index].stamp == stamp);
    }
    CHECK(normalized[0].key != normalized[2].key);
    CHECK(normalized[2].key != normalized[4].key);
    const std::vector<Block> valid{
        foundation_paragraph("o1"), foundation_table("o2"), foundation_paragraph("o3")};
    // Native value semantics deep-copy blocks; identity is the unchanged stable key plus full value.
    CHECK(containerNormalize(valid, keys, stamp) == valid);
}

TEST_CASE("foundation empty paragraph and invisibility preserve heading list text table negatives") {
    KeyAllocator keys;
    const auto empty = paragraphEmptyCreate(keys);
    CHECK(empty.inlines.empty());
    CHECK_FALSE(empty.bullet.has_value());
    CHECK(empty.key.starts_with("n"));
    CHECK(blockInvisibleIs(foundation_paragraph("o1")));
    auto heading = foundation_paragraph("o1");
    heading.as_paragraph().style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    CHECK_FALSE(blockInvisibleIs(heading));
    auto list = foundation_paragraph("o1");
    list.as_paragraph().bullet = BulletRef{.listId = "l1", .nestingLevel = 0};
    CHECK_FALSE(blockInvisibleIs(list));
    auto text = foundation_paragraph("o1");
    text.as_paragraph().inlines.push_back(Inline::from_text("x"));
    CHECK_FALSE(blockInvisibleIs(text));
    CHECK_FALSE(blockInvisibleIs(foundation_table("o1")));
}

} // namespace gdocsmith::model::test
