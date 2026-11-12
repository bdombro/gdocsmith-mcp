#pragma once

#include "model/style_values.hpp"
#include "cell_styles.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <cmath>

namespace gdocsmith::lens {

inline constexpr std::array<std::string_view, 5> PARAGRAPH_BORDERS = {
    "borderBetween", "borderBottom", "borderLeft", "borderRight", "borderTop"
};

inline constexpr std::array<std::string_view, 5> PARAGRAPH_DIMENSIONS = {
    "indentEnd", "indentFirstLine", "indentStart", "spaceAbove", "spaceBelow"
};

inline constexpr std::array<std::string_view, 4> PARAGRAPH_FLAGS = {
    "avoidWidowAndOrphan", "keepLinesTogether", "keepWithNext", "pageBreakBefore"
};

struct ParagraphBorder {
    std::optional<std::string> color;
    std::string dashStyle{"SOLID"};
    double padding{0.0};
    double width{0.0};

    bool operator==(const ParagraphBorder& other) const = default;
};

struct ParagraphDeclaration {
    std::optional<std::string> alignment;
    std::optional<bool> avoidWidowAndOrphan;
    std::optional<ParagraphBorder> borderBetween;
    std::optional<ParagraphBorder> borderBottom;
    std::optional<ParagraphBorder> borderLeft;
    std::optional<ParagraphBorder> borderRight;
    std::optional<ParagraphBorder> borderTop;
    std::optional<std::string> direction;
    std::optional<double> indentEnd;
    std::optional<double> indentFirstLine;
    std::optional<double> indentStart;
    std::optional<bool> keepLinesTogether;
    std::optional<bool> keepWithNext;
    std::optional<double> lineSpacing;
    std::optional<std::string> namedStyle;
    std::optional<bool> pageBreakBefore;
    std::optional<std::string> shading;
    std::optional<double> spaceAbove;
    std::optional<double> spaceBelow;
    std::optional<std::string> spacingMode;

    bool operator==(const ParagraphDeclaration& other) const = default;
    bool empty() const {
        return !alignment && !avoidWidowAndOrphan && !borderBetween && !borderBottom &&
               !borderLeft && !borderRight && !borderTop && !direction && !indentEnd &&
               !indentFirstLine && !indentStart && !keepLinesTogether && !keepWithNext &&
               !lineSpacing && !namedStyle && !pageBreakBefore && !shading && !spaceAbove &&
               !spaceBelow && !spacingMode;
    }
};

inline slimr::Json paragraphBorderPatch(const ParagraphBorder& b) {
    slimr::Json::object_t j{
        {"dashStyle", b.dashStyle},
        {"padding", dimensionFromPt(b.padding)},
        {"width", dimensionFromPt(b.width)}
    };
    if (b.color) {
        j["color"] = colorOptionalFromHex(*b.color);
    } else {
        j["color"] = slimr::Json::object_t{};
    }
    return j;
}

inline slimr::Json paragraphDeclarationPatch(const ParagraphDeclaration& decl) {
    slimr::Json::object_t patch;
    if (decl.alignment) patch["alignment"] = *decl.alignment;
    if (decl.avoidWidowAndOrphan) patch["avoidWidowAndOrphan"] = *decl.avoidWidowAndOrphan;
    if (decl.borderBetween) patch["borderBetween"] = paragraphBorderPatch(*decl.borderBetween);
    if (decl.borderBottom) patch["borderBottom"] = paragraphBorderPatch(*decl.borderBottom);
    if (decl.borderLeft) patch["borderLeft"] = paragraphBorderPatch(*decl.borderLeft);
    if (decl.borderRight) patch["borderRight"] = paragraphBorderPatch(*decl.borderRight);
    if (decl.borderTop) patch["borderTop"] = paragraphBorderPatch(*decl.borderTop);
    if (decl.direction) patch["direction"] = *decl.direction;
    if (decl.indentEnd) patch["indentEnd"] = dimensionFromPt(*decl.indentEnd);
    if (decl.indentFirstLine) patch["indentFirstLine"] = dimensionFromPt(*decl.indentFirstLine);
    if (decl.indentStart) patch["indentStart"] = dimensionFromPt(*decl.indentStart);
    if (decl.keepLinesTogether) patch["keepLinesTogether"] = *decl.keepLinesTogether;
    if (decl.keepWithNext) patch["keepWithNext"] = *decl.keepWithNext;
    if (decl.lineSpacing) patch["lineSpacing"] = *decl.lineSpacing;
    if (decl.namedStyle) patch["namedStyleType"] = *decl.namedStyle;
    if (decl.pageBreakBefore) patch["pageBreakBefore"] = *decl.pageBreakBefore;
    if (decl.shading) {
        patch["shading"] = slimr::Json::object_t{
            {"backgroundColor", colorOptionalFromHex(*decl.shading)}
        };
    }
    if (decl.spaceAbove) patch["spaceAbove"] = dimensionFromPt(*decl.spaceAbove);
    if (decl.spaceBelow) patch["spaceBelow"] = dimensionFromPt(*decl.spaceBelow);
    if (decl.spacingMode) patch["spacingMode"] = *decl.spacingMode;

    return patch;
}

inline ParagraphDeclaration paragraphDeclarationExport(
    const slimr::Json& style,
    bool list = false,
    const slimr::Json& inherited = slimr::Json::object_t{}
) {
    ParagraphDeclaration out;
    if (!style.is_object()) return out;

    auto layout_val = [](std::string_view field, const slimr::Json& raw) -> slimr::Json {
        if (field == "direction") {
            if (raw.holds<std::string>()) return raw;
            return "LEFT_TO_RIGHT";
        }
        if (!field.starts_with("border")) {
            if (raw.holds<bool>()) return raw;
            return false;
        }
        slimr::Json::object_t b;
        if (raw.is_object()) {
            if (raw.contains("color")) {
                auto col = colorHexFromOptional(raw["color"]);
                if (col) b["color"] = *col;
            }
            std::string dash = "SOLID";
            if (raw.contains("dashStyle") && raw["dashStyle"].holds<std::string>()) {
                dash = raw["dashStyle"].get<std::string>();
            }
            b["dashStyle"] = dash;
            b["padding"] = raw.contains("padding") ? dimensionPt(raw["padding"]) : 0.0;
            b["width"] = raw.contains("width") ? dimensionPt(raw["width"]) : 0.0;
        } else {
            b["dashStyle"] = "SOLID";
            b["padding"] = 0.0;
            b["width"] = 0.0;
        }
        return b;
    };

    auto check_diff = [&](std::string_view field) -> bool {
        std::string f(field);
        if (!style.contains(f)) return false;
        slimr::Json empty_raw;
        const auto& val = style[f];
        const auto& inh_val = inherited.contains(f) ? inherited[f] : empty_raw;
        auto v = layout_val(field, val);
        auto inh_v = layout_val(field, inh_val);
        return v.data != inh_v.data;
    };

    if (check_diff("direction")) {
        const auto& dir = style["direction"];
        out.direction = dir.holds<std::string>() ? dir.get<std::string>() : "LEFT_TO_RIGHT";
    }
    if (check_diff("avoidWidowAndOrphan")) {
        const auto& v = style["avoidWidowAndOrphan"];
        if (v.holds<bool>()) out.avoidWidowAndOrphan = v.get<bool>();
    }
    if (check_diff("keepLinesTogether")) {
        const auto& v = style["keepLinesTogether"];
        if (v.holds<bool>()) out.keepLinesTogether = v.get<bool>();
    }
    if (check_diff("keepWithNext")) {
        const auto& v = style["keepWithNext"];
        if (v.holds<bool>()) out.keepWithNext = v.get<bool>();
    }
    if (check_diff("pageBreakBefore")) {
        const auto& v = style["pageBreakBefore"];
        if (v.holds<bool>()) out.pageBreakBefore = v.get<bool>();
    }

    auto export_border = [&](std::string_view field) -> std::optional<ParagraphBorder> {
        if (!check_diff(field)) return std::nullopt;
        std::string f(field);
        const auto& raw = style[f];
        if (!raw.is_object()) return std::nullopt;
        ParagraphBorder pb;
        if (raw.contains("color")) pb.color = colorHexFromOptional(raw["color"]);
        pb.dashStyle = (raw.contains("dashStyle") && raw["dashStyle"].holds<std::string>()) ? raw["dashStyle"].get<std::string>() : "SOLID";
        pb.padding = raw.contains("padding") ? dimensionPt(raw["padding"]) : 0.0;
        pb.width = raw.contains("width") ? dimensionPt(raw["width"]) : 0.0;
        return pb;
    };

    out.borderBetween = export_border("borderBetween");
    out.borderBottom = export_border("borderBottom");
    out.borderLeft = export_border("borderLeft");
    out.borderRight = export_border("borderRight");
    out.borderTop = export_border("borderTop");

    auto get_dim = [&](std::string_view field) -> std::optional<double> {
        std::string f(field);
        if (style.contains(f)) return dimensionPt(style[f]);
        return std::nullopt;
    };

    if (!list) {
        out.indentStart = get_dim("indentStart");
        out.indentFirstLine = get_dim("indentFirstLine");
    }
    out.indentEnd = get_dim("indentEnd");
    out.spaceAbove = get_dim("spaceAbove");
    out.spaceBelow = get_dim("spaceBelow");

    if (style.contains("lineSpacing")) {
        const auto& ls = style["lineSpacing"];
        if (ls.holds<double>()) out.lineSpacing = ls.get<double>();
        else if (ls.holds<int64_t>()) out.lineSpacing = static_cast<double>(ls.get<int64_t>());
    }

    if (style.contains("alignment") && style["alignment"].holds<std::string>()) {
        std::string s = style["alignment"].get<std::string>();
        if (s == "CENTER" || s == "END" || s == "JUSTIFIED" || s == "START") out.alignment = s;
    }

    if (style.contains("spacingMode") && style["spacingMode"].holds<std::string>()) {
        std::string s = style["spacingMode"].get<std::string>();
        if (s == "COLLAPSE_LISTS" || s == "NEVER_COLLAPSE") out.spacingMode = s;
    }

    if (style.contains("namedStyleType") && style["namedStyleType"].holds<std::string>()) {
        std::string s = style["namedStyleType"].get<std::string>();
        if (s == "TITLE" || s == "SUBTITLE") out.namedStyle = s;
    }

    if (style.contains("shading") && style["shading"].is_object()) {
        const auto& shading = style["shading"];
        if (shading.contains("backgroundColor")) {
            auto c = colorHexFromOptional(shading["backgroundColor"]);
            if (c) out.shading = *c;
        }
    }

    return out;
}

} // namespace gdocsmith::lens
