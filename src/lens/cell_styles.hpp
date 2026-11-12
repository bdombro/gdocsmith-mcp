#pragma once

#include "model/style_values.hpp"
#include "directives.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <cmath>

namespace gdocsmith::lens {

using model::dimensionPt;
using model::dimensionFromPt;
using model::colorHexFromOptional;
using model::colorOptionalFromHex;
using model::rgbToHex;

inline constexpr std::array<std::string_view, 4> CELL_SIDES = {"Top", "Right", "Bottom", "Left"};
inline constexpr std::array<std::string_view, 3> CELL_DASH_STYLES = {"SOLID", "DOT", "DASH"};
inline constexpr std::array<std::string_view, 3> CELL_ALIGNMENTS = {"TOP", "MIDDLE", "BOTTOM"};
inline constexpr double DEFAULT_PADDING = 5.0;

struct CellBorder {
    std::string color;
    std::string dashStyle{"SOLID"};
    double width{0.0};

    bool operator==(const CellBorder& other) const = default;
};

struct CellDeclaration {
    std::optional<std::string> backgroundColor;
    std::optional<CellBorder> borderBottom;
    std::optional<CellBorder> borderLeft;
    std::optional<CellBorder> borderRight;
    std::optional<CellBorder> borderTop;
    std::optional<std::string> contentAlignment;
    std::optional<double> paddingBottom;
    std::optional<double> paddingLeft;
    std::optional<double> paddingRight;
    std::optional<double> paddingTop;

    bool operator==(const CellDeclaration& other) const = default;
    bool empty() const {
        return !backgroundColor && !borderBottom && !borderLeft && !borderRight && !borderTop &&
               !contentAlignment && !paddingBottom && !paddingLeft && !paddingRight && !paddingTop;
    }
};

inline CellDeclaration cellDeclarationExport(const slimr::Json& style) {
    CellDeclaration out;
    if (!style.is_object()) return out;

    if (style.contains("backgroundColor")) {
        auto bg_hex = colorHexFromOptional(style["backgroundColor"]);
        if (bg_hex) out.backgroundColor = *bg_hex;
    }

    if (style.contains("contentAlignment") && style["contentAlignment"].holds<std::string>()) {
        std::string align = style["contentAlignment"].get<std::string>();
        if (align != "TOP") out.contentAlignment = align;
    }

    for (std::string_view side : CELL_SIDES) {
        std::string pad_key = "padding" + std::string(side);
        if (style.contains(pad_key)) {
            double pt = dimensionPt(style[pad_key]);
            if (std::abs(pt - DEFAULT_PADDING) > 1e-4) {
                if (side == "Top") out.paddingTop = pt;
                else if (side == "Right") out.paddingRight = pt;
                else if (side == "Bottom") out.paddingBottom = pt;
                else if (side == "Left") out.paddingLeft = pt;
            }
        }

        std::string border_key = "border" + std::string(side);
        if (style.contains(border_key) && style[border_key].is_object()) {
            const auto& b = style[border_key];
            if (b.contains("color")) {
                auto col_hex = colorHexFromOptional(b["color"]);
                if (col_hex) {
                    std::string dash = "SOLID";
                    if (b.contains("dashStyle") && b["dashStyle"].holds<std::string>()) {
                        dash = b["dashStyle"].get<std::string>();
                    }
                    double width = 0.0;
                    if (b.contains("width")) width = dimensionPt(b["width"]);
                    CellBorder cb{.color = *col_hex, .dashStyle = std::move(dash), .width = width};
                    if (side == "Top") out.borderTop = cb;
                    else if (side == "Right") out.borderRight = cb;
                    else if (side == "Bottom") out.borderBottom = cb;
                    else if (side == "Left") out.borderLeft = cb;
                }
            }
        }
    }
    return out;
}

inline slimr::Json cellDeclarationPatch(const CellDeclaration& declaration) {
    slimr::Json patch = slimr::json_obj();
    if (declaration.backgroundColor) {
        patch["backgroundColor"] = colorOptionalFromHex(*declaration.backgroundColor);
    }
    if (declaration.contentAlignment) {
        patch["contentAlignment"] = *declaration.contentAlignment;
    }
    if (declaration.paddingTop) patch["paddingTop"] = dimensionFromPt(*declaration.paddingTop);
    if (declaration.paddingRight) patch["paddingRight"] = dimensionFromPt(*declaration.paddingRight);
    if (declaration.paddingBottom) patch["paddingBottom"] = dimensionFromPt(*declaration.paddingBottom);
    if (declaration.paddingLeft) patch["paddingLeft"] = dimensionFromPt(*declaration.paddingLeft);

    auto set_border = [&](std::string_view side, const std::optional<CellBorder>& b) {
        if (!b) return;
        slimr::Json bj = slimr::json_obj();
        if (!b->color.empty()) {
            bj["color"] = colorOptionalFromHex(b->color);
        }
        bj["dashStyle"] = b->dashStyle;
        bj["width"] = dimensionFromPt(b->width);
        patch["border" + std::string(side)] = std::move(bj);
    };

    set_border("Top", declaration.borderTop);
    set_border("Right", declaration.borderRight);
    set_border("Bottom", declaration.borderBottom);
    set_border("Left", declaration.borderLeft);

    return patch;
}

inline std::string sharedSide(std::string_view borderName) {
    if (borderName == "borderBottom") return "borderTop";
    if (borderName == "borderTop") return "borderBottom";
    if (borderName == "borderLeft") return "borderRight";
    if (borderName == "borderRight") return "borderLeft";
    return std::string(borderName);
}

} // namespace gdocsmith::lens
