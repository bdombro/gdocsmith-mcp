#pragma once

#include "json.hpp"
#include "lens/cell_styles.hpp"
#include "lens/directives.hpp"
#include "lens/json_helper.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::lens {

struct SectionSettings {
    std::optional<double> columnGap;
    std::optional<int> columns;
    bool columnSeparator{false};
    bool firstPage{false};
    bool landscape{false};
    std::optional<std::array<double, 4>> margins; // top, right, bottom, left
    std::optional<int> pageNumberStart;
    bool rtl{false};

    bool operator==(const SectionSettings &other) const = default;
};

inline constexpr std::array<std::string_view, 4> SECTION_FLAGS = {
    "columnSeparator", "firstPage", "landscape", "rtl"};

inline constexpr std::array<std::string_view, 4> SECTION_VALUES = {
    "columnGap", "columns", "margins", "pageNumberStart"};

inline std::string sectionTypeFlag(const slimr::Json &style) {
    if (style.is_object()) {
        if (auto *st = json::get(style, "sectionType")) {
            if (st->is_string() && st->get<std::string>() == "CONTINUOUS") {
                return "continuous";
            }
        }
    }
    return "nextPage";
}

inline DirectiveAttrs sectionSettingsAttrs(const SectionSettings &settings) {
    DirectiveAttrs attrs;
    if (settings.columns) {
        attrs.values.push_back({"columns", std::to_string(*settings.columns)});
    }
    if (settings.columnGap) {
        std::ostringstream ss;
        ss << *settings.columnGap;
        attrs.values.push_back({"columnGap", ss.str()});
    }
    if (settings.margins) {
        const auto &m = *settings.margins;
        std::ostringstream ss;
        ss << m[0] << "," << m[1] << "," << m[2] << "," << m[3];
        attrs.values.push_back({"margins", ss.str()});
    }
    if (settings.pageNumberStart) {
        attrs.values.push_back({"pageNumberStart", std::to_string(*settings.pageNumberStart)});
    }
    for (std::string_view flag : SECTION_FLAGS) {
        if (flag == "columnSeparator" && settings.columnSeparator)
            attrs.flags.push_back(std::string(flag));
        else if (flag == "firstPage" && settings.firstPage)
            attrs.flags.push_back(std::string(flag));
        else if (flag == "landscape" && settings.landscape)
            attrs.flags.push_back(std::string(flag));
        else if (flag == "rtl" && settings.rtl)
            attrs.flags.push_back(std::string(flag));
    }
    return attrs;
}

inline SectionSettings sectionSettingsOf(const slimr::Json &style, const slimr::Json &tabStyle) {
    SectionSettings settings;
    if (!style.is_object())
        return settings;

    auto pt = [](const slimr::Json *v) -> std::optional<double> {
        if (!v || !v->is_object())
            return std::nullopt;
        auto *mag = json::get(*v, "magnitude");
        if (mag && mag->is_number())
            return mag->as<double>();
        return std::nullopt;
    };

    auto *cols = json::get(style, "columnProperties");
    if (cols && cols->is_array() && cols->get<slimr::Json::array_t>().size() > 1) {
        const auto &arr = cols->get<slimr::Json::array_t>();
        settings.columns = static_cast<int>(arr.size());
        auto gap = pt(json::get(arr[0], "paddingEnd"));
        if (gap && std::abs(*gap - 36.0) > 1e-4) {
            settings.columnGap = *gap;
        }
    }

    if (auto *cs = json::get(style, "columnSeparatorStyle")) {
        if (cs->is_string() && cs->get<std::string>() == "BETWEEN_EACH_COLUMN") {
            settings.columnSeparator = true;
        }
    }
    if (auto *fp = json::get(style, "useFirstPageHeaderFooter")) {
        if (fp->is_boolean() && fp->get<bool>()) {
            settings.firstPage = true;
        }
    }
    if (auto *fo = json::get(style, "flipPageOrientation")) {
        if (fo->is_boolean() && fo->get<bool>()) {
            settings.landscape = true;
        }
    }

    static const std::array<std::string_view, 4> sides = {"marginTop", "marginRight", "marginBottom", "marginLeft"};
    bool has_any_margin = false;
    for (std::string_view side : sides) {
        if (pt(json::get(style, side))) {
            has_any_margin = true;
            break;
        }
    }
    if (has_any_margin) {
        std::array<double, 4> m{};
        for (size_t i = 0; i < 4; i++) {
            auto s_val = pt(json::get(style, sides[i]));
            auto t_val = pt(json::get(tabStyle, sides[i]));
            m[i] = s_val.value_or(t_val.value_or(72.0));
        }
        settings.margins = m;
    }

    if (auto *pns = json::get(style, "pageNumberStart")) {
        if (pns->is_number())
            settings.pageNumberStart = static_cast<int>(pns->as<int64_t>());
    }

    if (auto *cd = json::get(style, "contentDirection")) {
        if (cd->is_string() && cd->get<std::string>() == "RIGHT_TO_LEFT") {
            settings.rtl = true;
        }
    }
    return settings;
}

inline SectionSettings sectionSettingsParse(const std::vector<std::string> &flags, const std::vector<std::pair<std::string, std::string>> &values) {
    auto fail = [](std::string_view msg) {
        throw CoreError("unsupportedSyntax", "::section " + std::string(msg));
    };

    auto get_val = [&](std::string_view name) -> std::optional<std::string> {
        for (const auto &[k, v] : values) {
            if (k == name)
                return v;
        }
        return std::nullopt;
    };

    auto num_val = [&](std::string_view name, bool integer) -> double {
        auto s = get_val(name);
        if (!s || s->empty()) {
            fail(std::string(name) + " must be a nonnegative " + (integer ? "whole number" : "number of points"));
        }
        try {
            size_t idx = 0;
            double v = std::stod(*s, &idx);
            if (idx != s->size() || !std::isfinite(v) || v < 0 ||
                (integer && (v != std::floor(v) || v > std::numeric_limits<int>::max()))) {
                fail(std::string(name) + " must be a nonnegative " + (integer ? "whole number" : "number of points"));
            }
            return v;
        } catch (...) {
            fail(std::string(name) + " must be a nonnegative " + (integer ? "whole number" : "number of points"));
        }
        return 0;
    };

    SectionSettings settings;
    if (get_val("columns")) {
        int cols = static_cast<int>(num_val("columns", true));
        if (cols < 1 || cols > 3)
            fail("columns must be 1, 2 or 3");
        if (cols > 1)
            settings.columns = cols;
    }
    if (get_val("columnGap")) {
        if (!settings.columns)
            fail("columnGap needs columns=2 or 3");
        settings.columnGap = num_val("columnGap", false);
    }
    if (get_val("margins")) {
        std::string raw = *get_val("margins");
        std::stringstream ss(raw);
        std::string item;
        std::vector<double> sides;
        while (std::getline(ss, item, ',')) {
            try {
                size_t idx = 0;
                double v = std::stod(item, &idx);
                if (idx != item.size() || !std::isfinite(v) || v < 0) {
                    fail("margins must be four nonnegative point values: margins=top,right,bottom,left");
                }
                sides.push_back(v);
            } catch (...) {
                fail("margins must be four nonnegative point values: margins=top,right,bottom,left");
            }
        }
        if (sides.size() != 4) {
            fail("margins must be four nonnegative point values: margins=top,right,bottom,left");
        }
        settings.margins = std::array<double, 4>{sides[0], sides[1], sides[2], sides[3]};
    }
    if (get_val("pageNumberStart")) {
        settings.pageNumberStart = static_cast<int>(num_val("pageNumberStart", true));
    }
    for (const auto &flag : flags) {
        if (flag == "columnSeparator")
            settings.columnSeparator = true;
        else if (flag == "firstPage")
            settings.firstPage = true;
        else if (flag == "landscape")
            settings.landscape = true;
        else if (flag == "rtl")
            settings.rtl = true;
    }
    if (settings.columnSeparator && !settings.columns) {
        fail("columnSeparator needs columns=2 or 3");
    }
    return settings;
}

inline slimr::Json sectionStyleWith(const slimr::Json &style, const SectionSettings &settings) {
    auto out = style.is_object() ? style : slimr::json_obj();
    const auto set = [&](const std::string &field, const std::optional<slimr::Json> &value) {
        if (value)
            out[field] = *value;
        else if (out.contains(field)) {
            if (out[field].is_boolean())
                out[field] = false;
            else
                throw CoreError("unsupportedSyntax", "Google can't clear a section's " + field + " once set.");
        }
    };
    if (settings.columns || out.contains("columnProperties")) {
        slimr::Json columns = slimr::json_arr();
        const int count = settings.columns.value_or(1);
        for (int i = 0; i < count; ++i) {
            columns.get<slimr::Json::array_t>().emplace_back(slimr::Json::object_t{
                {"paddingEnd", dimensionFromPt(i + 1 < count ? settings.columnGap.value_or(36) : 0)}});
        }
        out["columnProperties"] = std::move(columns);
    }
    out["columnSeparatorStyle"] = settings.columnSeparator ? "BETWEEN_EACH_COLUMN" : "NONE";
    set("useFirstPageHeaderFooter", settings.firstPage ? std::optional<slimr::Json>{true} : std::nullopt);
    set("flipPageOrientation", settings.landscape ? std::optional<slimr::Json>{true} : std::nullopt);
    constexpr std::array<const char *, 4> sides = {"marginTop", "marginRight", "marginBottom", "marginLeft"};
    for (size_t i = 0; i < sides.size(); ++i) {
        set(sides[i], settings.margins
                          ? std::optional<slimr::Json>{dimensionFromPt((*settings.margins)[i])}
                          : std::nullopt);
    }
    set("pageNumberStart", settings.pageNumberStart
                               ? std::optional<slimr::Json>{*settings.pageNumberStart}
                               : std::nullopt);
    if (settings.rtl)
        out["contentDirection"] = "RIGHT_TO_LEFT";
    else if (out.contains("contentDirection") && out["contentDirection"] == slimr::Json("RIGHT_TO_LEFT")) {
        out["contentDirection"] = "LEFT_TO_RIGHT";
    }
    return out;
}

inline SectionSettings sectionDirectiveSettings(const DirectiveAttrs &attrs) {
    if (!attrs.classes.empty())
        throw CoreError("unsupportedSyntax", "Invalid section class.");
    for (const auto &flag : attrs.flags) {
        if (flag != "continuous" && flag != "nextPage" &&
            std::find(SECTION_FLAGS.begin(), SECTION_FLAGS.end(), flag) == SECTION_FLAGS.end()) {
            throw CoreError("unsupportedSyntax", "Unknown section flag.");
        }
    }
    for (const auto &[key, value] : attrs.values) {
        (void)value;
        if (key != "id" && std::find(SECTION_VALUES.begin(), SECTION_VALUES.end(), key) == SECTION_VALUES.end()) {
            throw CoreError("unsupportedSyntax", "Unknown section attribute.");
        }
    }
    if (attrs.has_flag("continuous") && attrs.has_flag("nextPage")) {
        throw CoreError("unsupportedSyntax", "A section can have only one break type.");
    }
    return sectionSettingsParse(attrs.flags, attrs.values);
}

} // namespace gdocsmith::lens
