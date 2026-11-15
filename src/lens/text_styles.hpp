#pragma once

#include "lens/directives.hpp"
#include "model/style_values.hpp"

#include <cmath>
#include <string>

namespace gdocsmith::lens {

inline DirectiveAttrs textStyleExport(const model::JsonObject &style) {
    DirectiveAttrs attrs;
    if (!style.is_object())
        return attrs;
    for (const auto &name : {"foregroundColor", "backgroundColor"}) {
        if (style.contains(name)) {
            if (auto color = model::colorHexFromOptional(style[name])) {
                attrs.values.emplace_back(name, *color);
            }
        }
    }
    if (style.contains("fontSize")) {
        attrs.values.emplace_back("fontSize", std::to_string(model::dimensionPt(style["fontSize"])));
    }
    if (style.contains("baselineOffset") && style["baselineOffset"].is_string()) {
        attrs.values.emplace_back("baselineOffset", style["baselineOffset"].get<std::string>());
    }
    if (style.contains("weightedFontFamily")) {
        const auto &font = style["weightedFontFamily"];
        if (font.contains("fontFamily") && font["fontFamily"].is_string()) {
            attrs.values.emplace_back("fontFamily", font["fontFamily"].get<std::string>());
        }
        if (font.contains("weight")) {
            attrs.values.emplace_back("fontWeight", slimr::write_json(font["weight"]));
        }
    }
    return attrs;
}

inline void textStyleApply(model::JsonObject &style, const DirectiveAttrs &attrs) {
    for (const auto &[name, value] : attrs.values) {
        if (value == "null") {
            if (name == "fontFamily" || name == "fontWeight") {
                if (style.contains("weightedFontFamily") && style["weightedFontFamily"].is_object()) {
                    style["weightedFontFamily"].get<slimr::Json::object_t>().erase(
                        name == "fontFamily" ? "fontFamily" : "weight");
                }
            } else
                style.get<slimr::Json::object_t>().erase(name);
        } else if (name == "bold" || name == "italic" || name == "smallCaps" ||
                   name == "strikethrough" || name == "underline") {
            if (value != "true" && value != "false")
                throw model::CoreError("unsupportedSyntax", "Invalid text flag.");
            style[name] = value == "true";
        } else if (name == "foregroundColor" || name == "backgroundColor") {
            if (value.size() != 7 || value[0] != '#' ||
                value.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) {
                throw model::CoreError("unsupportedSyntax", "Invalid text color.");
            }
            style[name] = model::colorOptionalFromHex(value);
        } else if (name == "fontFamily") {
            style["weightedFontFamily"]["fontFamily"] = value;
        } else if (name == "baselineOffset") {
            if (value != "NORMAL" && value != "SUPERSCRIPT" && value != "SUBSCRIPT") {
                throw model::CoreError("unsupportedSyntax", "Invalid baseline offset.");
            }
            style[name] = value;
        } else if (name == "fontSize" || name == "fontWeight") {
            size_t end = 0;
            double number = 0;
            try {
                number = std::stod(value, &end);
            } catch (const std::exception &) {
                throw model::CoreError("unsupportedSyntax", "Invalid text style number.");
            }
            if (end != value.size() || !std::isfinite(number) ||
                (name == "fontSize" ? number <= 0 : number < 1 || number > 1000)) {
                throw model::CoreError("unsupportedSyntax", "Invalid text style number.");
            }
            if (name == "fontSize")
                style[name] = model::dimensionFromPt(number);
            else
                style["weightedFontFamily"]["weight"] = number;
        } else {
            throw model::CoreError("unsupportedSyntax", "Unsupported text style attribute: " + name);
        }
    }
}

} // namespace gdocsmith::lens
