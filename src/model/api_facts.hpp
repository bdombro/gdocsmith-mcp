#pragma once

#include "model/raw_json.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model {

inline JsonObject make_link_chrome_color() {
    return slimr::Json::object_t{
        {"color", slimr::Json::object_t{
            {"rgbColor", slimr::Json::object_t{
                {"blue", 0.8},
                {"green", 0.33333334},
                {"red", 0.06666667}
            }}
        }}
    };
}

inline const std::vector<std::string> TEXT_STYLE_INHERIT_DROP_FIELDS = {
    "foregroundColor", "underline"
};

inline JsonObject make_table_row_style_default() {
    return slimr::Json::object_t{
        {"minRowHeight", slimr::Json::object_t{{"unit", "PT"}}}
    };
}

inline JsonObject make_table_cell_style_default() {
    return slimr::Json::object_t{
        {"backgroundColor", slimr::Json::object_t{}},
        {"columnSpan", static_cast<int64_t>(1)},
        {"contentAlignment", "TOP"},
        {"paddingBottom", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(5)}, {"unit", "PT"}}},
        {"paddingLeft", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(5)}, {"unit", "PT"}}},
        {"paddingRight", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(5)}, {"unit", "PT"}}},
        {"paddingTop", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(5)}, {"unit", "PT"}}},
        {"rowSpan", static_cast<int64_t>(1)}
    };
}

inline JsonObject make_cell_paragraph_border() {
    return slimr::Json::object_t{
        {"color", slimr::Json::object_t{}},
        {"dashStyle", "SOLID"},
        {"padding", slimr::Json::object_t{{"unit", "PT"}}},
        {"width", slimr::Json::object_t{{"unit", "PT"}}}
    };
}

inline JsonObject make_table_cell_paragraph_style_default() {
    auto border = make_cell_paragraph_border();
    return slimr::Json::object_t{
        {"alignment", "START"},
        {"avoidWidowAndOrphan", false},
        {"borderBetween", border},
        {"borderBottom", border},
        {"borderLeft", border},
        {"borderRight", border},
        {"borderTop", border},
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

inline std::vector<JsonObject> tableColumnPropertiesDefault(int columns) {
    double width = (columns == 1) ? 500.0 : (columns == 2) ? 250.0 : (columns == 3) ? 175.0 : (500.0 / columns);
    std::vector<JsonObject> out;
    out.reserve(columns);
    for (int i = 0; i < columns; ++i) {
        out.push_back(slimr::Json::object_t{
            {"width", slimr::Json::object_t{{"magnitude", width}, {"unit", "PT"}}},
            {"widthType", "FIXED_WIDTH"}
        });
    }
    return out;
}

inline std::vector<JsonObject> nestedTableColumnPropertiesDefault(int columns) {
    std::vector<JsonObject> out;
    out.reserve(columns);
    for (int i = 0; i < columns; ++i) {
        out.push_back(slimr::Json::object_t{
            {"widthType", "EVENLY_DISTRIBUTED"}
        });
    }
    return out;
}

inline const std::vector<std::string> MERGE_MOVED_PARAGRAPH_FIELDS = {
    "avoidWidowAndOrphan", "direction", "indentStart", "lineSpacing", "namedStyleType", "spaceBelow"
};

inline JsonObject make_merged_row_min_height() {
    return slimr::Json::object_t{
        {"magnitude", static_cast<int64_t>(21)},
        {"unit", "PT"}
    };
}

inline JsonObject mergedRowStyle(const JsonObject& style) {
    double floor = 21.0;
    if (style.contains("minRowHeight")) {
        const auto& mrh = style["minRowHeight"];
        if (mrh.contains("magnitude")) {
            double h = 0.0;
            if (mrh["magnitude"].holds<int64_t>()) h = static_cast<double>(mrh["magnitude"].get<int64_t>());
            else if (mrh["magnitude"].holds<double>()) h = mrh["magnitude"].get<double>();
            if (h >= floor) return style;
        }
    }
    JsonObject res = style;
    res["minRowHeight"] = make_merged_row_min_height();
    return res;
}

inline JsonObject make_section_style_default() {
    return slimr::Json::object_t{
        {"columnSeparatorStyle", "NONE"},
        {"contentDirection", "LEFT_TO_RIGHT"}
    };
}

inline JsonObject make_date_element_defaults() {
    return slimr::Json::object_t{
        {"dateFormat", "DATE_FORMAT_MONTH_DAY_YEAR_ABBREVIATED"},
        {"locale", "en"},
        {"timeFormat", "TIME_FORMAT_DISABLED"}
    };
}

inline JsonObject tabDocumentStyleNew(const std::optional<JsonObject>& first) {
    if (first.has_value()) {
        return *first;
    }
    return slimr::Json::object_t{
        {"documentFormat", slimr::Json::object_t{{"documentMode", "PAGELESS"}}},
        {"marginTop", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(72)}, {"unit", "PT"}}},
        {"marginBottom", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(72)}, {"unit", "PT"}}},
        {"marginRight", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(72)}, {"unit", "PT"}}},
        {"marginLeft", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(72)}, {"unit", "PT"}}},
        {"pageSize", slimr::Json::object_t{
            {"height", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(792)}, {"unit", "PT"}}},
            {"width", slimr::Json::object_t{{"magnitude", static_cast<int64_t>(612)}, {"unit", "PT"}}}
        }}
    };
}

} // namespace gdocsmith::model
