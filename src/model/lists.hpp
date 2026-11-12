#pragma once

#include "model/list_presets.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gdocsmith::model {

inline const std::vector<BulletPreset> BULLET_PRESETS = {
    "BULLET_DISC_CIRCLE_SQUARE",
    "BULLET_DIAMONDX_ARROW3D_SQUARE",
    "BULLET_CHECKBOX",
    "BULLET_ARROW_DIAMOND_DISC",
    "BULLET_STAR_CIRCLE_SQUARE",
    "BULLET_ARROW3D_CIRCLE_SQUARE",
    "BULLET_LEFTTRIANGLE_DIAMOND_DISC",
    "BULLET_DIAMONDX_HOLLOWDIAMOND_SQUARE",
    "BULLET_DIAMOND_CIRCLE_SQUARE",
    "NUMBERED_DECIMAL_ALPHA_ROMAN",
    "NUMBERED_DECIMAL_ALPHA_ROMAN_PARENS",
    "NUMBERED_DECIMAL_NESTED",
    "NUMBERED_UPPERALPHA_ALPHA_ROMAN",
    "NUMBERED_UPPERROMAN_UPPERALPHA_DECIMAL",
    "NUMBERED_ZERODECIMAL_ALPHA_ROMAN",
};

inline BulletPreset defaultPresetForKind(std::string_view kind) {
    if (kind == "check") return "BULLET_CHECKBOX";
    if (kind == "number") return "NUMBERED_DECIMAL_ALPHA_ROMAN";
    return "BULLET_DISC_CIRCLE_SQUARE";
}

using PresetLevels = std::vector<RawListNestingLevel>;
using PresetTable = std::unordered_map<std::string, PresetLevels>;

inline const PresetTable& listPresetTable() {
    static const auto table = []() {
        PresetTable t;
        const auto& json = listPresetsJson();
        if (json.holds<slimr::Json::object_t>()) {
            for (const auto& [name, levelsJson] : json.get<slimr::Json::object_t>()) {
                if (levelsJson.holds<slimr::Json::array_t>()) {
                    PresetLevels lvls;
                    for (const auto& item : levelsJson.get<slimr::Json::array_t>()) {
                        lvls.push_back(item);
                    }
                    t[name] = std::move(lvls);
                }
            }
        }
        return t;
    }();
    return table;
}

inline const std::vector<std::string> GLYPH_FIELDS = {
    "bulletAlignment", "glyphFormat", "glyphSymbol", "glyphType"
};

inline bool glyphFieldsEqual(const RawListNestingLevel& a, const RawListNestingLevel& b) {
    int startA = 1;
    if (a.contains("startNumber")) {
        const auto& sn = a["startNumber"];
        if (sn.holds<int64_t>()) startA = static_cast<int>(sn.get<int64_t>());
        else if (sn.holds<double>()) startA = static_cast<int>(sn.get<double>());
    }
    int startB = 1;
    if (b.contains("startNumber")) {
        const auto& sn = b["startNumber"];
        if (sn.holds<int64_t>()) startB = static_cast<int>(sn.get<int64_t>());
        else if (sn.holds<double>()) startB = static_cast<int>(sn.get<double>());
    }
    if (startA != startB) return false;

    for (const auto& field : GLYPH_FIELDS) {
        bool has_a = a.contains(field);
        bool has_b = b.contains(field);
        if (has_a != has_b) return false;
        if (has_a && !styleEqual(a[field], b[field])) return false;
    }
    return true;
}

inline std::optional<BulletPreset> listPresetInfer(
    const std::vector<RawListNestingLevel>& levels,
    const PresetTable& table = listPresetTable()
) {
    for (const auto& preset : BULLET_PRESETS) {
        auto it = table.find(preset);
        if (it == table.end()) continue;
        const auto& presetLevels = it->second;
        if (levels.size() > presetLevels.size()) continue;

        bool all_match = true;
        for (size_t i = 0; i < levels.size(); ++i) {
            if (!glyphFieldsEqual(levels[i], presetLevels[i])) {
                all_match = false;
                break;
            }
        }
        if (all_match) return preset;
    }
    return std::nullopt;
}

inline std::string listKind(const ListDef& def, int level) {
    if (def.preset.has_value()) {
        if (*def.preset == "BULLET_CHECKBOX") return "check";
        if (def.preset->starts_with("NUMBERED_")) return "number";
        return "bullet";
    }
    if (level < static_cast<int>(def.nestingLevels.size())) {
        const auto& raw = def.nestingLevels[level];
        if (raw.contains("glyphSymbol") && raw["glyphSymbol"].holds<std::string>()) {
            const auto& sym = raw["glyphSymbol"].get<std::string>();
            if (sym == "☐" || sym == "☑") return "check";
        }
        if (raw.contains("glyphType") && raw["glyphType"].holds<std::string>()) {
            const auto& gt = raw["glyphType"].get<std::string>();
            if (!gt.empty() && gt != "GLYPH_TYPE_UNSPECIFIED") return "number";
        }
    }
    return "bullet";
}

struct LevelIndent {
    double indentFirstLine = 0.0;
    double indentStart = 0.0;
};

inline LevelIndent listLevelIndent(const ListDef& /*def*/, int level) {
    double indentStart = 36.0 * (level + 1);
    return LevelIndent{
        .indentFirstLine = indentStart - 18.0,
        .indentStart = indentStart,
    };
}

inline void listPresetsApply(DocModel& doc) {
    const auto& table = listPresetTable();
    for (auto& tab : doc.tabs) {
        for (auto& [_, list] : tab.lists) {
            list.preset = listPresetInfer(list.nestingLevels, table);
        }
    }
}

} // namespace gdocsmith::model
