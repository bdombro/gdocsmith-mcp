#pragma once

#include "json.hpp"
#include "model/list_presets.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace gdocsmith::emulator {

enum class EmulatorErrorCode {
    DELETE_LAST_NEWLINE,
    DELETE_NEWLINE_BEFORE_STRUCTURE,
    DELETE_PARTIAL_STRUCTURE,
    INSERT_IN_TABLE,
    INSERT_OUTSIDE_PARAGRAPH,
    INVALID_FIELD,
    RANGE_AT_SEGMENT_END,
    SURROGATE_SPLIT,
    TAB_REQUIRED,
    TAB_TITLE_TAKEN,
    UNKNOWN_REQUEST,
    UNMODELED
};

inline std::string error_code_to_string(EmulatorErrorCode code) {
    switch (code) {
        case EmulatorErrorCode::DELETE_LAST_NEWLINE: return "DELETE_LAST_NEWLINE";
        case EmulatorErrorCode::DELETE_NEWLINE_BEFORE_STRUCTURE: return "DELETE_NEWLINE_BEFORE_STRUCTURE";
        case EmulatorErrorCode::DELETE_PARTIAL_STRUCTURE: return "DELETE_PARTIAL_STRUCTURE";
        case EmulatorErrorCode::INSERT_IN_TABLE: return "INSERT_IN_TABLE";
        case EmulatorErrorCode::INSERT_OUTSIDE_PARAGRAPH: return "INSERT_OUTSIDE_PARAGRAPH";
        case EmulatorErrorCode::INVALID_FIELD: return "INVALID_FIELD";
        case EmulatorErrorCode::RANGE_AT_SEGMENT_END: return "RANGE_AT_SEGMENT_END";
        case EmulatorErrorCode::SURROGATE_SPLIT: return "SURROGATE_SPLIT";
        case EmulatorErrorCode::TAB_REQUIRED: return "TAB_REQUIRED";
        case EmulatorErrorCode::TAB_TITLE_TAKEN: return "TAB_TITLE_TAKEN";
        case EmulatorErrorCode::UNKNOWN_REQUEST: return "UNKNOWN_REQUEST";
        case EmulatorErrorCode::UNMODELED: return "UNMODELED";
    }
    return "UNKNOWN";
}

class EmulatorError : public std::runtime_error {
public:
    EmulatorErrorCode code;
    int request_index;

    EmulatorError(EmulatorErrorCode code, int request_index, const std::string& message)
        : std::runtime_error(message), code(code), request_index(request_index) {}
};

struct IdCounters {
    int chip = 0;
    int heading = 0;
    int image = 0;
    int list = 0;
    int segment = 0;
    int tab = 0;
};

struct EmulateContext {
    IdCounters id_counters;
    int request_index = 0;
};

struct MintPrefixes {
    static constexpr std::string_view chip = "emu.chip.";
    static constexpr std::string_view heading = "emu.h.";
    static constexpr std::string_view image = "emu.io.";
    static constexpr std::string_view list = "emu.list.";
    static constexpr std::string_view segment = "emu.kix.";
    static constexpr std::string_view tab = "emu.t.";
};

// Suggestion metadata attached to a char, newline, row, or cell tape cell
struct Sugg {
    std::vector<std::string> del_ids;
    std::vector<std::string> ins_ids;
    slimr::Json style_changes;

    bool empty() const {
        bool sc_empty = true;
        if (style_changes.is_object()) {
            sc_empty = style_changes.get<slimr::Json::object_t>().empty();
        }
        return del_ids.empty() && ins_ids.empty() && sc_empty;
    }
};

// Paragraph metadata attached to a newline cell
struct NlPara {
    std::optional<slimr::Json> bullet;
    std::optional<std::string> heading_id;
    std::vector<std::string> positioned_object_ids;
    slimr::Json style;
    std::optional<slimr::Json> suggestions_raw;
};

enum class TapeCellType {
    Char,
    Atom,
    AtomCont,
    Nl,
    SectionBreak,
    TableStart,
    RowStart,
    CellStart,
    TableEnd,
    TocStart,
    TocEnd
};

struct TapeCell {
    TapeCellType type = TapeCellType::Char;
    std::string ch; // for Char (a UTF-16 code unit represented as UTF-8 string)
    char16_t u16 = 0; // UTF-16 code unit
    slimr::Json style; // for Char, Atom, Nl, RowStart, CellStart
    std::optional<Sugg> sugg;
    slimr::Json raw; // for Atom, SectionBreak, TableStart, TocStart
    int span = 1; // for Atom
    NlPara para; // for Nl
};

struct TabState {
    std::string tab_id;
    std::string title;
    std::optional<std::string> parent_tab_id;
    std::optional<std::string> icon_emoji;
    std::vector<TapeCell> tape;
    slimr::Json document_style;
    slimr::Json dropdown_definitions = slimr::Json::object_t{};
    slimr::Json footers = slimr::Json::object_t{};
    slimr::Json footnotes = slimr::Json::object_t{};
    slimr::Json headers = slimr::Json::object_t{};
    slimr::Json inline_objects = slimr::Json::object_t{};
    slimr::Json lists = slimr::Json::object_t{};
    slimr::Json named_ranges = slimr::Json::object_t{};
    slimr::Json named_styles = slimr::Json::object_t{};
    slimr::Json positioned_objects = slimr::Json::object_t{};
    slimr::Json suggested = slimr::Json::object_t{};
};

struct EmulatorState {
    std::string document_id;
    std::string revision_id;
    std::string title;
    std::vector<TabState> tabs;
};

// API Facts & Constants
inline slimr::Json make_link_chrome_color() {
    slimr::Json rgb = slimr::Json::object_t{
        {"blue", 0.8},
        {"green", 0.33333334},
        {"red", 0.06666667}
    };
    return slimr::Json::object_t{
        {"color", slimr::Json::object_t{{"rgbColor", std::move(rgb)}}}
    };
}

inline const std::vector<std::string> TEXT_STYLE_INHERIT_DROP_FIELDS = {
    "foregroundColor", "underline"
};

inline slimr::Json make_table_row_style_default() {
    return slimr::Json::object_t{
        {"minRowHeight", slimr::Json::object_t{{"unit", "PT"}}}
    };
}

inline slimr::Json make_table_cell_style_default() {
    return slimr::Json::object_t{
        {"backgroundColor", slimr::Json::object_t{}},
        {"columnSpan", static_cast<int64_t>(1)},
        {"contentAlignment", "TOP"},
        {"paddingBottom", slimr::Json::object_t{{"magnitude", 5.0}, {"unit", "PT"}}},
        {"paddingLeft", slimr::Json::object_t{{"magnitude", 5.0}, {"unit", "PT"}}},
        {"paddingRight", slimr::Json::object_t{{"magnitude", 5.0}, {"unit", "PT"}}},
        {"paddingTop", slimr::Json::object_t{{"magnitude", 5.0}, {"unit", "PT"}}},
        {"rowSpan", static_cast<int64_t>(1)}
    };
}

inline slimr::Json make_cell_paragraph_border() {
    return slimr::Json::object_t{
        {"color", slimr::Json::object_t{}},
        {"dashStyle", "SOLID"},
        {"padding", slimr::Json::object_t{{"unit", "PT"}}},
        {"width", slimr::Json::object_t{{"unit", "PT"}}}
    };
}

inline slimr::Json make_table_cell_paragraph_style_default() {
    auto b = make_cell_paragraph_border();
    return slimr::Json::object_t{
        {"alignment", "START"},
        {"avoidWidowAndOrphan", false},
        {"borderBetween", b},
        {"borderBottom", b},
        {"borderLeft", b},
        {"borderRight", b},
        {"borderTop", b},
        {"direction", "LEFT_TO_RIGHT"},
        {"indentEnd", slimr::Json::object_t{{"unit", "PT"}}},
        {"indentFirstLine", slimr::Json::object_t{{"unit", "PT"}}},
        {"indentStart", slimr::Json::object_t{{"unit", "PT"}}},
        {"keepLinesTogether", false},
        {"keepWithNext", false},
        {"lineSpacing", static_cast<int64_t>(100)},
        {"namedStyleType", "NORMAL_TEXT"},
        {"pageBreakBefore", false},
        {"shading", slimr::Json::object_t{{"backgroundColor", slimr::Json::object_t{}}}},
        {"spaceAbove", slimr::Json::object_t{{"unit", "PT"}}},
        {"spaceBelow", slimr::Json::object_t{{"unit", "PT"}}},
        {"spacingMode", "COLLAPSE_LISTS"}
    };
}

inline std::vector<slimr::Json> table_column_properties_default(int columns) {
    double width = 500.0 / std::max(1, columns);
    if (columns == 1) width = 500.0;
    else if (columns == 2) width = 250.0;
    else if (columns == 3) width = 175.0;

    std::vector<slimr::Json> res;
    res.reserve(columns);
    for (int i = 0; i < columns; ++i) {
        res.push_back(slimr::Json::object_t{
            {"width", slimr::Json::object_t{{"magnitude", width}, {"unit", "PT"}}},
            {"widthType", "FIXED_WIDTH"}
        });
    }
    return res;
}

inline std::vector<slimr::Json> nested_table_column_properties_default(int columns) {
    std::vector<slimr::Json> res;
    res.reserve(columns);
    for (int i = 0; i < columns; ++i) {
        res.push_back(slimr::Json::object_t{
            {"widthType", "EVENLY_DISTRIBUTED"}
        });
    }
    return res;
}

inline const std::vector<std::string> MERGE_MOVED_PARAGRAPH_FIELDS = {
    "avoidWidowAndOrphan",
    "direction",
    "indentStart",
    "lineSpacing",
    "namedStyleType",
    "spaceBelow"
};

inline slimr::Json make_merged_row_min_height() {
    return slimr::Json::object_t{{"magnitude", 21.0}, {"unit", "PT"}};
}

inline slimr::Json merged_row_style(const slimr::Json& style) {
    double height = 0.0;
    if (style.is_object() && style.contains("minRowHeight") && style["minRowHeight"].is_object() &&
        style["minRowHeight"].contains("magnitude")) {
        height = style["minRowHeight"]["magnitude"].as<double>();
    }
    if (height >= 21.0) {
        return style;
    }
    slimr::Json copy = style.is_object() ? style : slimr::Json::object_t{};
    copy["minRowHeight"] = make_merged_row_min_height();
    return copy;
}

inline slimr::Json make_section_style_default() {
    return slimr::Json::object_t{
        {"columnSeparatorStyle", "NONE"},
        {"contentDirection", "LEFT_TO_RIGHT"}
    };
}

inline slimr::Json make_date_element_defaults() {
    return slimr::Json::object_t{
        {"dateFormat", "DATE_FORMAT_MONTH_DAY_YEAR_ABBREVIATED"},
        {"locale", "en"},
        {"timeFormat", "TIME_FORMAT_DISABLED"}
    };
}

inline std::vector<slimr::Json> make_footnote_blank_content() {
    return {
        slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(0)},
            {"endIndex", static_cast<int64_t>(2)},
            {"paragraph", slimr::Json::object_t{
                {"elements", slimr::Json::array_t{
                    slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(0)},
                        {"endIndex", static_cast<int64_t>(2)},
                        {"textRun", slimr::Json::object_t{
                            {"content", " \n"},
                            {"textStyle", slimr::Json::object_t{
                                {"fontSize", slimr::Json::object_t{{"magnitude", 10.0}, {"unit", "PT"}}}
                            }}
                        }}
                    }
                }},
                {"paragraphStyle", slimr::Json::object_t{
                    {"direction", "LEFT_TO_RIGHT"},
                    {"lineSpacing", static_cast<int64_t>(100)},
                    {"namedStyleType", "NORMAL_TEXT"},
                    {"spaceBelow", slimr::Json::object_t{{"unit", "PT"}}}
                }}
            }}
        }
    };
}

// Canonical Blank Tab template
inline slimr::Json blank_tab_template() {
    static const auto tmpl = []() -> slimr::Json {
        const char* raw = R"json({"body":{"content":[{"endIndex":1,"sectionBreak":{"sectionStyle":{"columnSeparatorStyle":"NONE","contentDirection":"LEFT_TO_RIGHT","sectionType":"CONTINUOUS"}}},{"startIndex":1,"endIndex":2,"paragraph":{"elements":[{"startIndex":1,"endIndex":2,"textRun":{"content":"\n","textStyle":{}}}],"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT"}}}]},"documentStyle":{"background":{"color":{"color":{"rgbColor":{"red":1,"green":1,"blue":1}}}},"pageNumberStart":1,"marginTop":{"magnitude":72,"unit":"PT"},"marginBottom":{"magnitude":72,"unit":"PT"},"marginRight":{"magnitude":72,"unit":"PT"},"marginLeft":{"magnitude":72,"unit":"PT"},"pageSize":{"height":{"magnitude":792,"unit":"PT"},"width":{"magnitude":612,"unit":"PT"}},"marginHeader":{"magnitude":36,"unit":"PT"},"marginFooter":{"magnitude":36,"unit":"PT"},"useCustomHeaderFooterMargins":true,"documentFormat":{"documentMode":"PAGELESS"}},"namedStyles":{"styles":[{"namedStyleType":"NORMAL_TEXT","textStyle":{"bold":false,"italic":false,"underline":false,"strikethrough":false,"smallCaps":false,"backgroundColor":{},"foregroundColor":{"color":{"rgbColor":{}}},"fontSize":{"magnitude":11,"unit":"PT"},"weightedFontFamily":{"fontFamily":"Arial","weight":400},"baselineOffset":"NONE"},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","alignment":"START","lineSpacing":115,"direction":"LEFT_TO_RIGHT","spacingMode":"COLLAPSE_LISTS","spaceAbove":{"unit":"PT"},"spaceBelow":{"unit":"PT"},"borderBetween":{"color":{},"width":{"unit":"PT"},"padding":{"unit":"PT"},"dashStyle":"SOLID"},"borderTop":{"color":{},"width":{"unit":"PT"},"padding":{"unit":"PT"},"dashStyle":"SOLID"},"borderBottom":{"color":{},"width":{"unit":"PT"},"padding":{"unit":"PT"},"dashStyle":"SOLID"},"borderLeft":{"color":{},"width":{"unit":"PT"},"padding":{"unit":"PT"},"dashStyle":"SOLID"},"borderRight":{"color":{},"width":{"unit":"PT"},"padding":{"unit":"PT"},"dashStyle":"SOLID"},"indentFirstLine":{"unit":"PT"},"indentStart":{"unit":"PT"},"indentEnd":{"unit":"PT"},"keepLinesTogether":false,"keepWithNext":false,"avoidWidowAndOrphan":true,"shading":{"backgroundColor":{}},"pageBreakBefore":false}},{"namedStyleType":"HEADING_1","textStyle":{"fontSize":{"magnitude":20,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"magnitude":20,"unit":"PT"},"spaceBelow":{"magnitude":6,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"HEADING_2","textStyle":{"bold":false,"fontSize":{"magnitude":16,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"magnitude":18,"unit":"PT"},"spaceBelow":{"magnitude":6,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"HEADING_3","textStyle":{"bold":false,"foregroundColor":{"color":{"rgbColor":{"red":0.2627451,"green":0.2627451,"blue":0.2627451}}},"fontSize":{"magnitude":14,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"magnitude":16,"unit":"PT"},"spaceBelow":{"magnitude":4,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"HEADING_4","textStyle":{"foregroundColor":{"color":{"rgbColor":{"red":0.4,"green":0.4,"blue":0.4}}},"fontSize":{"magnitude":12,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"magnitude":14,"unit":"PT"},"spaceBelow":{"magnitude":4,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"HEADING_5","textStyle":{"foregroundColor":{"color":{"rgbColor":{"red":0.4,"green":0.4,"blue":0.4}}},"fontSize":{"magnitude":11,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"magnitude":12,"unit":"PT"},"spaceBelow":{"magnitude":4,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"HEADING_6","textStyle":{"italic":true,"foregroundColor":{"color":{"rgbColor":{"red":0.4,"green":0.4,"blue":0.4}}},"fontSize":{"magnitude":11,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"magnitude":12,"unit":"PT"},"spaceBelow":{"magnitude":4,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"TITLE","textStyle":{"fontSize":{"magnitude":26,"unit":"PT"}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"unit":"PT"},"spaceBelow":{"magnitude":3,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}},{"namedStyleType":"SUBTITLE","textStyle":{"italic":false,"foregroundColor":{"color":{"rgbColor":{"red":0.4,"green":0.4,"blue":0.4}}},"fontSize":{"magnitude":15,"unit":"PT"},"weightedFontFamily":{"fontFamily":"Arial","weight":400}},"paragraphStyle":{"namedStyleType":"NORMAL_TEXT","direction":"LEFT_TO_RIGHT","spaceAbove":{"unit":"PT"},"spaceBelow":{"magnitude":16,"unit":"PT"},"keepLinesTogether":true,"keepWithNext":true,"pageBreakBefore":false}}]}})json";
        auto parsed = slimr::parse_json<slimr::Json>(raw);
        if (!parsed) return slimr::Json::object_t{};
        return *parsed;
    }();
    return tmpl;
}

inline bool json_bool(const slimr::Json& val, bool fallback = false) {
    if (val.is_boolean()) return val.get<bool>();
    if (val.is_number()) return val.as<double>() != 0.0;
    return fallback;
}

inline double json_num(const slimr::Json& val, double fallback = 0.0) {
    if (val.is_number()) return val.as<double>();
    return fallback;
}

inline std::string json_str(const slimr::Json& val, const std::string& fallback = "") {
    if (val.is_string()) return val.get<std::string>();
    return fallback;
}

inline slimr::Json tab_document_style_new(const std::optional<slimr::Json>& first) {
    if (first && first->is_object()) {
        return *first;
    }
    return blank_tab_template()["documentStyle"];
}

// Canonicalization, field masks, and styles
inline std::vector<std::string> parse_fields(std::string_view fields) {
    std::vector<std::string> res;
    size_t start = 0;
    while (start < fields.size()) {
        size_t comma = fields.find(',', start);
        if (comma == std::string_view::npos) comma = fields.size();
        size_t s = start;
        while (s < comma && (fields[s] == ' ' || fields[s] == '\t' || fields[s] == '\n' || fields[s] == '\r')) s++;
        size_t e = comma;
        while (e > s && (fields[e - 1] == ' ' || fields[e - 1] == '\t' || fields[e - 1] == '\n' || fields[e - 1] == '\r')) e--;
        if (e > s) {
            res.emplace_back(fields.substr(s, e - s));
        }
        start = comma + 1;
    }
    return res;
}

inline float fround(double val) {
    return static_cast<float>(val);
}

inline slimr::Json style_canonical(const slimr::Json& obj) {
    if (!obj.is_object() && !obj.is_array()) {
        return obj;
    }
    if (obj.is_array()) {
        slimr::Json::array_t arr;
        for (const auto& item : obj.get<slimr::Json::array_t>()) {
            arr.push_back(style_canonical(item));
        }
        return arr;
    }
    // Object
    if (obj.contains("rgbColor")) {
        const auto& rgb = obj["rgbColor"];
        double b = 0.0, g = 0.0, r = 0.0;
        if (rgb.is_object()) {
            if (rgb.contains("blue")) b = rgb["blue"].as<double>();
            if (rgb.contains("green")) g = rgb["green"].as<double>();
            if (rgb.contains("red")) r = rgb["red"].as<double>();
        }
        slimr::Json rgb_canon = slimr::Json::object_t{
            {"blue", static_cast<double>(fround(b))},
            {"green", static_cast<double>(fround(g))},
            {"red", static_cast<double>(fround(r))}
        };
        return slimr::Json::object_t{{"rgbColor", std::move(rgb_canon)}};
    }
    if (obj.contains("magnitude") || obj.contains("unit")) {
        double mag = obj.contains("magnitude") ? obj["magnitude"].as<double>() : 0.0;
        std::string unit = obj.contains("unit") && obj["unit"].is_string() ? obj["unit"].get<std::string>() : "PT";
        return slimr::Json::object_t{
            {"magnitude", mag},
            {"unit", std::move(unit)}
        };
    }

    std::map<std::string, slimr::Json> sorted;
    const auto& map = obj.get<slimr::Json::object_t>();
    for (const auto& [k, v] : map) {
        sorted[std::string(k)] = style_canonical(v);
    }
    slimr::Json::object_t out;
    for (auto& [k, v] : sorted) {
        out.insert({k, std::move(v)});
    }
    return out;
}

inline bool style_equal(const slimr::Json& a, const slimr::Json& b) {
    return slimr::write_json(style_canonical(a)) == slimr::write_json(style_canonical(b));
}

inline bool suggestion_ids_equal(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    auto sa = a;
    auto sb = b;
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    return sa == sb;
}

inline slimr::Json apply_style_fields(const slimr::Json& base, const slimr::Json& patch,
                                      const std::vector<std::string>& fields) {
    slimr::Json target = base.is_object() ? base : slimr::Json::object_t{};
    for (const auto& field : fields) {
        if (!patch.is_object() || !patch.contains(field) || patch[field].is_null()) {
            if (target.is_object() && target.contains(field)) {
                target.get<slimr::Json::object_t>().erase(field);
            }
        } else {
            target[field] = style_canonical(patch[field]);
        }
    }
    return target;
}

inline bool sugg_equal(const std::optional<Sugg>& a, const std::optional<Sugg>& b) {
    const std::vector<std::string> empty_ids;
    const auto& a_del = a ? a->del_ids : empty_ids;
    const auto& b_del = b ? b->del_ids : empty_ids;
    if (!suggestion_ids_equal(a_del, b_del)) return false;

    const auto& a_ins = a ? a->ins_ids : empty_ids;
    const auto& b_ins = b ? b->ins_ids : empty_ids;
    if (!suggestion_ids_equal(a_ins, b_ins)) return false;

    slimr::Json empty_obj = slimr::Json::object_t{};
    const auto& a_sc = a && a->style_changes.is_object() ? a->style_changes : empty_obj;
    const auto& b_sc = b && b->style_changes.is_object() ? b->style_changes : empty_obj;
    return style_equal(a_sc, b_sc);
}

inline bool sugg_present(const std::optional<Sugg>& sugg) {
    return sugg.has_value() && !sugg->empty();
}

inline std::optional<Sugg> sugg_from_ids(const std::vector<std::string>& del,
                                         const std::vector<std::string>& ins,
                                         const slimr::Json& style_changes = slimr::Json{nullptr}) {
    bool has_del = !del.empty();
    bool has_ins = !ins.empty();
    bool has_sc = style_changes.is_object() && !style_changes.get<slimr::Json::object_t>().empty();
    if (!has_del && !has_ins && !has_sc) return std::nullopt;

    Sugg s;
    s.del_ids = del;
    s.ins_ids = ins;
    if (has_sc) s.style_changes = style_changes;
    return s;
}

inline void sugg_fields_for(const std::optional<Sugg>& sugg, slimr::Json& out,
                            const std::string& style_field = "suggestedTextStyleChanges") {
    if (!sugg) return;
    if (!sugg->del_ids.empty()) {
        slimr::Json::array_t arr;
        for (const auto& id : sugg->del_ids) arr.push_back(id);
        out["suggestedDeletionIds"] = std::move(arr);
    }
    if (!sugg->ins_ids.empty()) {
        slimr::Json::array_t arr;
        for (const auto& id : sugg->ins_ids) arr.push_back(id);
        out["suggestedInsertionIds"] = std::move(arr);
    }
    if (sugg->style_changes.is_object() && !sugg->style_changes.get<slimr::Json::object_t>().empty()) {
        out[style_field] = sugg->style_changes;
    }
}

// Bullet presets
using BulletPreset = std::string;

inline const std::map<BulletPreset, std::vector<slimr::Json>>& list_preset_table() {
    static const auto table = []() {
        std::map<BulletPreset, std::vector<slimr::Json>> tbl;
        const auto& json = gdocsmith::model::listPresetsJson();
        if (json.is_object()) {
            for (const auto& [preset_name, levels_val] : json.get<slimr::Json::object_t>()) {
                if (levels_val.is_array()) {
                    std::vector<slimr::Json> levels;
                    for (const auto& lvl : levels_val.get<slimr::Json::array_t>()) {
                        levels.push_back(lvl);
                    }
                    tbl[std::string(preset_name)] = std::move(levels);
                }
            }
        }
        return tbl;
    }();
    return table;
}

inline bool glyph_fields_equal(const slimr::Json& a, const slimr::Json& b) {
    auto a_start = a.contains("startNumber") ? a["startNumber"].as<int>() : 1;
    auto b_start = b.contains("startNumber") ? b["startNumber"].as<int>() : 1;
    if (a_start != b_start) return false;

    static const std::vector<std::string> GLYPH_FIELDS = {
        "bulletAlignment", "glyphFormat", "glyphSymbol", "glyphType"
    };
    for (const auto& field : GLYPH_FIELDS) {
        bool has_a = a.contains(field);
        bool has_b = b.contains(field);
        if (has_a != has_b) return false;
        if (has_a) {
            std::string val_a = a[field].is_string() ? a[field].get<std::string>() : a[field].dump().value_or("");
            std::string val_b = b[field].is_string() ? b[field].get<std::string>() : b[field].dump().value_or("");
            if (val_a != val_b) return false;
        }
    }
    return true;
}

inline BulletPreset list_preset_infer(const std::vector<slimr::Json>& levels,
                                      const std::map<BulletPreset, std::vector<slimr::Json>>& tbl) {
    if (levels.empty()) return "BULLET_DISC_CIRCLE_SQUARE";
    for (const auto& [preset, preset_levels] : tbl) {
        if (levels.size() <= preset_levels.size()) {
            bool matches = true;
            for (size_t i = 0; i < levels.size(); ++i) {
                if (!glyph_fields_equal(levels[i], preset_levels[i])) {
                    matches = false;
                    break;
                }
            }
            if (matches) return preset;
        }
    }
    return "BULLET_DISC_CIRCLE_SQUARE";
}

} // namespace gdocsmith::emulator
