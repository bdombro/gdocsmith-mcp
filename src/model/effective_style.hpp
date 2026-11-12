#pragma once

#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gdocsmith::model {

inline std::optional<int> headingLevel(std::string_view namedStyleType) {
    if (namedStyleType == "TITLE") return 0;
    if (namedStyleType == "SUBTITLE" || namedStyleType == "HEADING_1") return 1;
    if (namedStyleType == "HEADING_2") return 2;
    if (namedStyleType == "HEADING_3") return 3;
    if (namedStyleType == "HEADING_4") return 4;
    if (namedStyleType == "HEADING_5") return 5;
    if (namedStyleType == "HEADING_6") return 6;
    return std::nullopt;
}

inline bool headingStyleIs(std::string_view namedStyleType) {
    return headingLevel(namedStyleType).has_value();
}

struct NamedTextStyle {
    JsonObject paragraphStyle;
    JsonObject textStyle;
};

inline NamedTextStyle namedTextStyle(const TabModel& tab, std::string_view type) {
    if (tab.namedStyles.contains("styles") && tab.namedStyles["styles"].holds<slimr::Json::array_t>()) {
        for (const auto& item : tab.namedStyles["styles"].get<slimr::Json::array_t>()) {
            if (item.contains("namedStyleType") && item["namedStyleType"].holds<std::string>()) {
                if (item["namedStyleType"].get<std::string>() == type) {
                    return NamedTextStyle{
                        .paragraphStyle = item.contains("paragraphStyle") ? item["paragraphStyle"] : make_json_object(),
                        .textStyle = item.contains("textStyle") ? item["textStyle"] : make_json_object(),
                    };
                }
            }
        }
    }
    return NamedTextStyle{.paragraphStyle = make_json_object(), .textStyle = make_json_object()};
}

inline JsonObject textStyleEffective(
    const TabModel& tab,
    const ParagraphBlock& paragraph,
    const JsonObject& runStyle
) {
    std::string namedType = "NORMAL_TEXT";
    if (paragraph.style.contains("namedStyleType") && paragraph.style["namedStyleType"].holds<std::string>()) {
        namedType = paragraph.style["namedStyleType"].get<std::string>();
    }
    auto base = namedTextStyle(tab, "NORMAL_TEXT").textStyle;
    auto named = (namedType == "NORMAL_TEXT") ? make_json_object() : namedTextStyle(tab, namedType).textStyle;

    slimr::Json::object_t merged;
    if (base.holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : base.get<slimr::Json::object_t>()) merged[k] = v;
    }
    if (named.holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : named.get<slimr::Json::object_t>()) merged[k] = v;
    }
    if (runStyle.holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : runStyle.get<slimr::Json::object_t>()) merged[k] = v;
    }
    JsonObject out;
    out.data = std::move(merged);
    return styleCanonical(out);
}

} // namespace gdocsmith::model
