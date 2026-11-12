#pragma once

#include "model/raw_json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::model {

inline JsonObject make_link_chrome() {
    return slimr::Json::object_t{
        {"foregroundColor", "#1155CC"},
        {"underline", true}
    };
}

inline constexpr std::string_view CODE_FONT = "Roboto Mono";

inline bool codeFontIs(const JsonObject& font) {
    if (!font.contains("fontFamily") || !font.contains("weight")) return false;
    std::string fam;
    if (font["fontFamily"].holds<std::string>()) fam = font["fontFamily"].get<std::string>();
    int weight = 0;
    if (font["weight"].holds<int64_t>()) weight = static_cast<int>(font["weight"].get<int64_t>());
    else if (font["weight"].holds<double>()) weight = static_cast<int>(font["weight"].get<double>());
    return fam == CODE_FONT && weight == 300;
}

inline const std::vector<std::string> DIRECTIVE_ATTRS = {
    "backgroundColor",
    "baselineOffset",
    "bold",
    "fontFamily",
    "fontSize",
    "fontWeight",
    "foregroundColor",
    "italic",
    "smallCaps",
    "strikethrough",
    "underline"
};

inline const std::vector<std::string> TEXT_STYLE_FIELDS = {
    "backgroundColor",
    "baselineOffset",
    "bold",
    "fontSize",
    "foregroundColor",
    "italic",
    "link",
    "smallCaps",
    "strikethrough",
    "underline",
    "weightedFontFamily"
};

inline const std::vector<std::string> PARAGRAPH_STYLE_FIELDS = {
    "alignment",
    "avoidWidowAndOrphan",
    "borderBetween",
    "borderBottom",
    "borderLeft",
    "borderRight",
    "borderTop",
    "direction",
    "indentEnd",
    "indentFirstLine",
    "indentStart",
    "keepLinesTogether",
    "keepWithNext",
    "lineSpacing",
    "namedStyleType",
    "pageBreakBefore",
    "shading",
    "spaceAbove",
    "spaceBelow",
    "spacingMode"
};

inline double dimensionPt(const std::optional<RawDimension>& dim) {
    if (!dim || !dim->magnitude) return 0.0;
    return *dim->magnitude;
}

inline double dimensionPt(const JsonObject& dim) {
    if (!dim.contains("magnitude")) return 0.0;
    const auto& m = dim["magnitude"];
    if (m.holds<int64_t>()) return static_cast<double>(m.get<int64_t>());
    if (m.holds<double>()) return m.get<double>();
    return 0.0;
}

inline JsonObject dimensionFromPt(double pt) {
    return slimr::Json::object_t{
        {"magnitude", pt},
        {"unit", "PT"}
    };
}

inline std::string rgbToHex(double red, double green, double blue) {
    auto ch = [](double n) -> std::string {
        int val = static_cast<int>(std::round(std::clamp(n, 0.0, 1.0) * 255.0));
        return std::format("{:02X}", val);
    };
    return "#" + ch(red) + ch(green) + ch(blue);
}

inline std::optional<std::string> colorHexFromOptional(const std::optional<RawOptionalColor>& optColor) {
    if (!optColor || !optColor->color) return std::nullopt;
    const auto& rgb = optColor->color->rgbColor;
    double r = rgb && rgb->red ? *rgb->red : 0.0;
    double g = rgb && rgb->green ? *rgb->green : 0.0;
    double b = rgb && rgb->blue ? *rgb->blue : 0.0;
    return rgbToHex(r, g, b);
}

inline std::optional<std::string> colorHexFromOptional(const JsonObject& optColor) {
    if (!optColor.contains("color")) return std::nullopt;
    const auto& color = optColor["color"];
    double r = 0.0, g = 0.0, b = 0.0;
    if (color.contains("rgbColor")) {
        const auto& rgb = color["rgbColor"];
        if (rgb.contains("red")) {
            if (rgb["red"].holds<double>()) r = rgb["red"].get<double>();
            else if (rgb["red"].holds<int64_t>()) r = static_cast<double>(rgb["red"].get<int64_t>());
        }
        if (rgb.contains("green")) {
            if (rgb["green"].holds<double>()) g = rgb["green"].get<double>();
            else if (rgb["green"].holds<int64_t>()) g = static_cast<double>(rgb["green"].get<int64_t>());
        }
        if (rgb.contains("blue")) {
            if (rgb["blue"].holds<double>()) b = rgb["blue"].get<double>();
            else if (rgb["blue"].holds<int64_t>()) b = static_cast<double>(rgb["blue"].get<int64_t>());
        }
    }
    return rgbToHex(r, g, b);
}

inline JsonObject colorOptionalFromHex(std::string_view hex) {
    std::string h(hex);
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() != 6) throw std::runtime_error("Color must be #RRGGBB, got " + std::string(hex));
    int r = std::stoi(h.substr(0, 2), nullptr, 16);
    int g = std::stoi(h.substr(2, 2), nullptr, 16);
    int b = std::stoi(h.substr(4, 2), nullptr, 16);
    return slimr::Json::object_t{
        {"color", slimr::Json::object_t{
            {"rgbColor", slimr::Json::object_t{
                {"blue", b / 255.0},
                {"green", g / 255.0},
                {"red", r / 255.0}
            }}
        }}
    };
}

inline JsonObject rgbCanonical(const JsonObject& rgb) {
    auto readChan = [](const JsonObject& obj, std::string_view k) -> float {
        if (!obj.contains(k)) return 0.0f;
        const auto& val = obj[k];
        if (val.holds<double>()) return static_cast<float>(val.get<double>());
        if (val.holds<int64_t>()) return static_cast<float>(val.get<int64_t>());
        return 0.0f;
    };
    return slimr::Json::object_t{
        {"blue", static_cast<double>(readChan(rgb, "blue"))},
        {"green", static_cast<double>(readChan(rgb, "green"))},
        {"red", static_cast<double>(readChan(rgb, "red"))}
    };
}

inline JsonObject styleCanonical(const JsonObject& value) {
    if (value.holds<slimr::Json::array_t>()) {
        slimr::Json::array_t arr;
        for (const auto& item : value.get<slimr::Json::array_t>()) {
            arr.push_back(styleCanonical(item));
        }
        JsonObject out;
        out.data = std::move(arr);
        return out;
    }
    if (!value.holds<slimr::Json::object_t>()) {
        return value;
    }
    if (value.contains("rgbColor")) {
        return slimr::Json::object_t{
            {"rgbColor", rgbCanonical(value["rgbColor"])}
        };
    }
    if (value.contains("magnitude") || value.contains("unit")) {
        double mag = 0.0;
        if (value.contains("magnitude")) {
            const auto& m = value["magnitude"];
            if (m.holds<int64_t>()) mag = static_cast<double>(m.get<int64_t>());
            else if (m.holds<double>()) mag = m.get<double>();
        }
        std::string unit = "PT";
        if (value.contains("unit") && value["unit"].holds<std::string>()) {
            unit = value["unit"].get<std::string>();
        }
        return slimr::Json::object_t{
            {"magnitude", mag},
            {"unit", unit}
        };
    }

    const auto& map = value.get<slimr::Json::object_t>();
    std::vector<std::string> keys;
    for (const auto& [k, _] : map) keys.push_back(k);
    std::sort(keys.begin(), keys.end());

    slimr::Json::object_t out_map;
    for (const auto& k : keys) {
        auto it = map.find(k);
        if (it != map.end()) {
            out_map.emplace(k, styleCanonical(it->second));
        }
    }
    JsonObject out;
    out.data = std::move(out_map);
    return out;
}

inline JsonObject stylePick(const JsonObject& style, const std::vector<std::string>& fields) {
    slimr::Json::object_t out;
    for (const auto& field : fields) {
        if (style.contains(field)) {
            out.emplace(field, style[field]);
        }
    }
    return out;
}

inline bool styleEqual(const JsonObject& a, const JsonObject& b) {
    auto ca = styleCanonical(a);
    auto cb = styleCanonical(b);
    return ca.dump().value_or("") == cb.dump().value_or("");
}

inline std::vector<std::string> styleFieldsChanged(
    const JsonObject& a,
    const JsonObject& b,
    const std::vector<std::string>& fields
) {
    std::vector<std::string> changed;
    for (const auto& field : fields) {
        bool has_a = a.contains(field);
        bool has_b = b.contains(field);
        if (has_a != has_b) {
            changed.push_back(field);
        } else if (has_a) {
            if (!styleEqual(a[field], b[field])) {
                changed.push_back(field);
            }
        }
    }
    return changed;
}

inline bool suggestionIdsEqual(
    const std::optional<std::vector<std::string>>& a,
    const std::optional<std::vector<std::string>>& b
) {
    std::vector<std::string> sa = a.value_or(std::vector<std::string>{});
    std::vector<std::string> sb = b.value_or(std::vector<std::string>{});
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    return sa == sb;
}

inline bool monospaceFontIs(const std::optional<std::string>& rawFont) {
    if (!rawFont || rawFont->empty()) return false;
    std::string font;
    for (char c : *rawFont) {
        if (c != '\'' && c != '"') font += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // trim
    size_t start = font.find_first_not_of(" \t\r\n");
    size_t end = font.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return false;
    font = font.substr(start, end - start + 1);

    static const std::set<std::string> MONOSPACE_FONTS = {
        "consolas", "courier", "courier new", "fira code", "inconsolata",
        "menlo", "monaco", "monospace", "pt mono", "roboto mono",
        "source code pro", "space mono", "ubuntu mono"
    };

    if (MONOSPACE_FONTS.contains(font)) return true;
    return font.contains("mono") || font.contains("code") || font.contains("courier") || font.contains("console");
}

} // namespace gdocsmith::model
