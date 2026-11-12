#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include "json.hpp"
#include "lens/json_helper.hpp"
#include "lens/cell_styles.hpp"
#include "lens/paragraph_styles.hpp"
#include "lens/frontmatter.hpp"

namespace gdocsmith::lens {

struct NamedStyleDeclaration {
    StyleDeclaration text;
    ParagraphDeclaration paragraph;
};

using NamedStyleSet = std::unordered_map<std::string, NamedStyleDeclaration>;

struct NamedStyleEntry {
    std::string namedStyleType;
    slimr::Json paragraphStyle{slimr::json_obj()};
    slimr::Json textStyle{slimr::json_obj()};

    bool operator==(const NamedStyleEntry& other) const = default;
};

inline const std::vector<NamedStyleEntry>& defaultNamedStyles() {
    static const std::vector<NamedStyleEntry> defaults = []() {
        std::vector<NamedStyleEntry> list;
        auto add_entry = [&](std::string type, auto fn_text, auto fn_para) {
            NamedStyleEntry entry;
            entry.namedStyleType = type;
            slimr::Json ts = slimr::json_obj();
            fn_text(ts);
            slimr::Json ps = slimr::json_obj();
            fn_para(ps);
            entry.textStyle = std::move(ts);
            entry.paragraphStyle = std::move(ps);
            list.push_back(std::move(entry));
        };

        // NORMAL_TEXT
        add_entry("NORMAL_TEXT",
            [](slimr::Json& ts) {
                ts["fontSize"] = dimensionFromPt(11);
                slimr::Json font = slimr::json_obj();
                font["fontFamily"] = "Arial";
                font["weight"] = 400;
                ts["weightedFontFamily"] = std::move(font);
            },
            [](slimr::Json& ps) {
                ps["alignment"] = "START";
                ps["lineSpacing"] = 115;
                ps["direction"] = "LEFT_TO_RIGHT";
                ps["spacingMode"] = "COLLAPSE_LISTS";
                ps["avoidWidowAndOrphan"] = true;
            }
        );

        // HEADING_1
        add_entry("HEADING_1",
            [](slimr::Json& ts) { ts["fontSize"] = dimensionFromPt(20); },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(20);
                ps["spaceBelow"] = dimensionFromPt(6);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // HEADING_2
        add_entry("HEADING_2",
            [](slimr::Json& ts) { ts["fontSize"] = dimensionFromPt(16); },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(18);
                ps["spaceBelow"] = dimensionFromPt(6);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // HEADING_3
        add_entry("HEADING_3",
            [](slimr::Json& ts) {
                ts["fontSize"] = dimensionFromPt(14);
                ts["foregroundColor"] = colorOptionalFromHex("#434343");
            },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(16);
                ps["spaceBelow"] = dimensionFromPt(4);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // HEADING_4
        add_entry("HEADING_4",
            [](slimr::Json& ts) {
                ts["fontSize"] = dimensionFromPt(12);
                ts["foregroundColor"] = colorOptionalFromHex("#666666");
            },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(14);
                ps["spaceBelow"] = dimensionFromPt(4);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // HEADING_5
        add_entry("HEADING_5",
            [](slimr::Json& ts) {
                ts["fontSize"] = dimensionFromPt(11);
                ts["foregroundColor"] = colorOptionalFromHex("#666666");
            },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(12);
                ps["spaceBelow"] = dimensionFromPt(4);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // HEADING_6
        add_entry("HEADING_6",
            [](slimr::Json& ts) {
                ts["fontSize"] = dimensionFromPt(11);
                ts["italic"] = true;
                ts["foregroundColor"] = colorOptionalFromHex("#666666");
            },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(12);
                ps["spaceBelow"] = dimensionFromPt(4);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // TITLE
        add_entry("TITLE",
            [](slimr::Json& ts) { ts["fontSize"] = dimensionFromPt(26); },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(0);
                ps["spaceBelow"] = dimensionFromPt(3);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        // SUBTITLE
        add_entry("SUBTITLE",
            [](slimr::Json& ts) {
                ts["fontSize"] = dimensionFromPt(15);
                ts["foregroundColor"] = colorOptionalFromHex("#666666");
                slimr::Json font = slimr::json_obj();
                font["fontFamily"] = "Arial";
                font["weight"] = 400;
                ts["weightedFontFamily"] = std::move(font);
            },
            [](slimr::Json& ps) {
                ps["spaceAbove"] = dimensionFromPt(0);
                ps["spaceBelow"] = dimensionFromPt(16);
                ps["keepLinesTogether"] = true;
                ps["keepWithNext"] = true;
            }
        );

        return list;
    }();
    return defaults;
}

inline NamedStyleSet namedStyleSetMerge(const NamedStyleSet& base, const NamedStyleSet& overlay) {
    NamedStyleSet merged;
    for (const auto& type : NAMED_STYLE_TYPES) {
        NamedStyleDeclaration decl;
        auto it_b = base.find(type);
        auto it_o = overlay.find(type);
        if (it_b != base.end()) decl = it_b->second;
        if (it_o != overlay.end()) {
            const auto& o_decl = it_o->second;
            if (o_decl.text.backgroundColor) decl.text.backgroundColor = o_decl.text.backgroundColor;
            if (o_decl.text.bold) decl.text.bold = o_decl.text.bold;
            if (o_decl.text.fontFamily) decl.text.fontFamily = o_decl.text.fontFamily;
            if (o_decl.text.fontSize) decl.text.fontSize = o_decl.text.fontSize;
            if (o_decl.text.fontWeight) decl.text.fontWeight = o_decl.text.fontWeight;
            if (o_decl.text.foregroundColor) decl.text.foregroundColor = o_decl.text.foregroundColor;
            if (o_decl.text.italic) decl.text.italic = o_decl.text.italic;
            if (o_decl.text.smallCaps) decl.text.smallCaps = o_decl.text.smallCaps;
            if (o_decl.text.strikethrough) decl.text.strikethrough = o_decl.text.strikethrough;
            if (o_decl.text.underline) decl.text.underline = o_decl.text.underline;

            if (o_decl.paragraph.alignment) decl.paragraph.alignment = o_decl.paragraph.alignment;
            if (o_decl.paragraph.lineSpacing) decl.paragraph.lineSpacing = o_decl.paragraph.lineSpacing;
            if (o_decl.paragraph.spaceAbove) decl.paragraph.spaceAbove = o_decl.paragraph.spaceAbove;
            if (o_decl.paragraph.spaceBelow) decl.paragraph.spaceBelow = o_decl.paragraph.spaceBelow;
            if (o_decl.paragraph.shading) decl.paragraph.shading = o_decl.paragraph.shading;
        }
        merged[type] = decl;
    }
    return merged;
}

} // namespace gdocsmith::lens
