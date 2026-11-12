#pragma once

#include "model/raw_json.hpp"
#include "model/to_json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::reconcile {

using JsonObject = model::JsonObject;

struct TableRangeOpts {
    int columnIndex = 0;
    int columnSpan = 1;
    int rowIndex = 0;
    int rowSpan = 1;
    std::string tabId;
    int tableStart = 0;
};

class RequestBuilder : public model::RequestBuilder {
public:
    static JsonObject loc(int index, std::string_view segmentId = "", std::string_view tabId = "") {
        return make_location(index, tabId, segmentId);
    }

    static JsonObject rng(int startIndex, int endIndex, std::string_view segmentId = "", std::string_view tabId = "") {
        return make_range(startIndex, endIndex, tabId, segmentId);
    }

    static JsonObject pt(double magnitude) {
        return slimr::Json::object_t{
            {"magnitude", magnitude},
            {"unit", "PT"}
        };
    }

    static JsonObject createParagraphBullets(
        int startIndex,
        int endIndex,
        std::string_view bulletPreset,
        std::string_view segmentId = "",
        std::string_view tabId = ""
    ) {
        return slimr::Json::object_t{
            {"createParagraphBullets", slimr::Json::object_t{
                {"bulletPreset", std::string(bulletPreset)},
                {"range", rng(startIndex, endIndex, segmentId, tabId)}
            }}
        };
    }

    static JsonObject deleteParagraphBullets(
        int startIndex,
        int endIndex,
        std::string_view segmentId = "",
        std::string_view tabId = ""
    ) {
        return slimr::Json::object_t{
            {"deleteParagraphBullets", slimr::Json::object_t{
                {"range", rng(startIndex, endIndex, segmentId, tabId)}
            }}
        };
    }

    static JsonObject insertPageBreak(int index, std::string_view segmentId = "", std::string_view tabId = "") {
        return slimr::Json::object_t{
            {"insertPageBreak", slimr::Json::object_t{
                {"location", loc(index, segmentId, tabId)}
            }}
        };
    }

    static JsonObject segmentCreate(std::string_view kind, std::string_view tabId = "", int breakIndex = 0) {
        slimr::Json::object_t body{
            {"type", "DEFAULT"}
        };
        if (!tabId.empty() || breakIndex > 0) {
            slimr::Json::object_t secLoc{
                {"index", static_cast<int64_t>(breakIndex)}
            };
            if (!tabId.empty()) secLoc["tabId"] = std::string(tabId);
            body["sectionBreakLocation"] = std::move(secLoc);
        }
        if (kind == "header") {
            return slimr::Json::object_t{{"createHeader", std::move(body)}};
        } else {
            return slimr::Json::object_t{{"createFooter", std::move(body)}};
        }
    }

    static JsonObject segmentDelete(std::string_view kind, std::string_view segmentId, std::string_view tabId = "") {
        slimr::Json::object_t body;
        if (kind == "header") {
            body["headerId"] = std::string(segmentId);
            if (!tabId.empty()) body["tabId"] = std::string(tabId);
            return slimr::Json::object_t{{"deleteHeader", std::move(body)}};
        } else {
            body["footerId"] = std::string(segmentId);
            if (!tabId.empty()) body["tabId"] = std::string(tabId);
            return slimr::Json::object_t{{"deleteFooter", std::move(body)}};
        }
    }

    static JsonObject deleteTab(std::string_view tabId) {
        return slimr::Json::object_t{
            {"deleteTab", slimr::Json::object_t{
                {"tabId", std::string(tabId)}
            }}
        };
    }

    struct TableRowLocationOpts {
        int columnIndex = 0;
        bool insertBelow = true;
        int rowIndex = 0;
        std::string segmentId;
        std::string tabId;
        int tableStart = 0;
    };

    static JsonObject insertTableRow(const TableRowLocationOpts& opts) {
        return slimr::Json::object_t{
            {"insertTableRow", slimr::Json::object_t{
                {"insertBelow", opts.insertBelow},
                {"tableCellLocation", slimr::Json::object_t{
                    {"columnIndex", static_cast<int64_t>(opts.columnIndex)},
                    {"rowIndex", static_cast<int64_t>(opts.rowIndex)},
                    {"tableStartLocation", loc(opts.tableStart, opts.segmentId, opts.tabId)}
                }}
            }}
        };
    }

    static JsonObject deleteTableRow(const TableRowLocationOpts& opts) {
        return slimr::Json::object_t{
            {"deleteTableRow", slimr::Json::object_t{
                {"tableCellLocation", slimr::Json::object_t{
                    {"columnIndex", static_cast<int64_t>(opts.columnIndex)},
                    {"rowIndex", static_cast<int64_t>(opts.rowIndex)},
                    {"tableStartLocation", loc(opts.tableStart, opts.segmentId, opts.tabId)}
                }}
            }}
        };
    }

    struct TableColumnLocationOpts {
        int columnIndex = 0;
        bool insertRight = true;
        int rowIndex = 0;
        std::string segmentId;
        std::string tabId;
        int tableStart = 0;
    };

    static JsonObject insertTableColumn(const TableColumnLocationOpts& opts) {
        return slimr::Json::object_t{
            {"insertTableColumn", slimr::Json::object_t{
                {"insertRight", opts.insertRight},
                {"tableCellLocation", slimr::Json::object_t{
                    {"columnIndex", static_cast<int64_t>(opts.columnIndex)},
                    {"rowIndex", static_cast<int64_t>(opts.rowIndex)},
                    {"tableStartLocation", loc(opts.tableStart, opts.segmentId, opts.tabId)}
                }}
            }}
        };
    }

    static JsonObject deleteTableColumn(const TableColumnLocationOpts& opts) {
        return slimr::Json::object_t{
            {"deleteTableColumn", slimr::Json::object_t{
                {"tableCellLocation", slimr::Json::object_t{
                    {"columnIndex", static_cast<int64_t>(opts.columnIndex)},
                    {"rowIndex", static_cast<int64_t>(opts.rowIndex)},
                    {"tableStartLocation", loc(opts.tableStart, opts.segmentId, opts.tabId)}
                }}
            }}
        };
    }

    struct PinHeaderRowsOpts {
        int pinnedHeaderRowsCount = 0;
        std::string segmentId;
        std::string tabId;
        int tableStart = 0;
    };

    static JsonObject pinTableHeaderRows(const PinHeaderRowsOpts& opts) {
        return slimr::Json::object_t{
            {"pinTableHeaderRows", slimr::Json::object_t{
                {"pinnedHeaderRowsCount", static_cast<int64_t>(opts.pinnedHeaderRowsCount)},
                {"tableStartLocation", loc(opts.tableStart, opts.segmentId, opts.tabId)}
            }}
        };
    }

    struct UpdateTableRowStyleOpts {
        std::string fields;
        std::optional<double> minRowHeight;
        std::optional<bool> preventOverflow;
        std::vector<int> rowIndices;
        std::string segmentId;
        std::string tabId;
        std::optional<bool> tableHeader;
        JsonObject tableRowStyle;
        int tableStart = 0;
    };

    static JsonObject updateTableRowStyle(const UpdateTableRowStyleOpts& opts) {
        JsonObject trs = opts.tableRowStyle;
        if (!trs.holds<slimr::Json::object_t>()) trs = slimr::Json::object_t{};
        if (opts.minRowHeight) trs["minRowHeight"] = pt(*opts.minRowHeight);
        if (opts.preventOverflow) trs["preventOverflow"] = *opts.preventOverflow;
        if (opts.tableHeader) trs["tableHeader"] = *opts.tableHeader;

        slimr::Json::object_t body{
            {"fields", opts.fields},
            {"tableRowStyle", std::move(trs)},
            {"tableStartLocation", loc(opts.tableStart, opts.segmentId, opts.tabId)}
        };
        if (!opts.rowIndices.empty()) {
            slimr::Json::array_t arr;
            for (int idx : opts.rowIndices) arr.push_back(static_cast<int64_t>(idx));
            body["rowIndices"] = std::move(arr);
        }
        return slimr::Json::object_t{{"updateTableRowStyle", std::move(body)}};
    }

    struct UpdateNamedStyleOpts {
        std::vector<std::string> fields;
        JsonObject namedStyle;
        std::string tabId;
    };

    static JsonObject updateNamedStyle(const UpdateNamedStyleOpts& opts) {
        std::string fList = "namedStyleType";
        for (const auto& f : opts.fields) {
            fList += ",";
            fList += f;
        }
        slimr::Json::object_t body{
            {"fields", std::move(fList)},
            {"namedStyle", opts.namedStyle}
        };
        if (!opts.tabId.empty()) body["tabId"] = opts.tabId;
        return slimr::Json::object_t{{"updateNamedStyle", std::move(body)}};
    }

    struct UpdateDocumentStyleOpts {
        JsonObject documentStyle;
        std::string fields;
        std::string tabId;
    };

    static JsonObject updateDocumentStyle(const UpdateDocumentStyleOpts& opts) {
        slimr::Json::object_t body{
            {"documentStyle", opts.documentStyle},
            {"fields", opts.fields}
        };
        if (!opts.tabId.empty()) body["tabId"] = opts.tabId;
        return slimr::Json::object_t{{"updateDocumentStyle", std::move(body)}};
    }

    struct InsertSectionBreakOpts {
        bool endOfSegment = false;
        int index = 0;
        std::string sectionType = "NEXT_PAGE";
        std::string segmentId;
        std::string tabId;
    };

    static JsonObject insertSectionBreak(const InsertSectionBreakOpts& opts) {
        slimr::Json::object_t body{
            {"sectionType", opts.sectionType}
        };
        if (opts.endOfSegment) {
            body["endOfSegmentLocation"] = loc(opts.index, opts.segmentId, opts.tabId);
        } else {
            body["location"] = loc(opts.index, opts.segmentId, opts.tabId);
        }
        return slimr::Json::object_t{{"insertSectionBreak", std::move(body)}};
    }

    struct InsertPersonOpts {
        std::string email;
        int index = 0;
        std::string segmentId;
        std::string tabId;
    };

    static JsonObject insertPerson(const InsertPersonOpts& opts) {
        return slimr::Json::object_t{
            {"insertPerson", slimr::Json::object_t{
                {"location", loc(opts.index, opts.segmentId, opts.tabId)},
                {"personProperties", slimr::Json::object_t{{"email", opts.email}}}
            }}
        };
    }

    static JsonObject positionedObjectDelete(std::string_view objectId, std::string_view tabId = "") {
        slimr::Json::object_t body{{"objectId", std::string(objectId)}};
        if (!tabId.empty()) body["tabId"] = std::string(tabId);
        return slimr::Json::object_t{{"deletePositionedObject", std::move(body)}};
    }

    static JsonObject imageReplace(std::string_view imageObjectId, std::string_view uri, std::string_view tabId = "") {
        slimr::Json::object_t body{
            {"imageObjectId", std::string(imageObjectId)},
            {"imageReplaceMethod", "CENTER_CROP"},
            {"uri", std::string(uri)}
        };
        if (!tabId.empty()) body["tabId"] = std::string(tabId);
        return slimr::Json::object_t{{"replaceImage", std::move(body)}};
    }

    static JsonObject namedRangeCreate(std::string_view name, int startIndex, int endIndex, std::string_view tabId = "") {
        slimr::Json::object_t range{
            {"startIndex", static_cast<int64_t>(startIndex)},
            {"endIndex", static_cast<int64_t>(endIndex)}
        };
        if (!tabId.empty()) range["tabId"] = std::string(tabId);
        return slimr::Json::object_t{
            {"createNamedRange", slimr::Json::object_t{
                {"name", std::string(name)},
                {"range", std::move(range)}
            }}
        };
    }

    static JsonObject namedRangeDelete(std::string_view namedRangeId, std::string_view tabId = "") {
        slimr::Json::object_t body{{"namedRangeId", std::string(namedRangeId)}};
        if (!tabId.empty()) {
            body["tabsCriteria"] = slimr::Json::object_t{
                {"tabIds", slimr::Json::array_t{std::string(tabId)}}
            };
        }
        return slimr::Json::object_t{{"deleteNamedRange", std::move(body)}};
    }

    static JsonObject dropdownDefinitionCreate(const JsonObject& dropdownDefinition, std::string_view tabId = "") {
        slimr::Json::object_t body{{"dropdownDefinition", dropdownDefinition}};
        if (!tabId.empty()) body["tabId"] = std::string(tabId);
        return slimr::Json::object_t{{"createDropdownDefinition", std::move(body)}};
    }

    static JsonObject dropdownDefinitionDelete(std::string_view dropdownDefinitionId, std::string_view tabId = "") {
        slimr::Json::object_t body{{"dropdownDefinitionId", std::string(dropdownDefinitionId)}};
        if (!tabId.empty()) body["tabId"] = std::string(tabId);
        return slimr::Json::object_t{{"deleteDropdownDefinition", std::move(body)}};
    }

    static JsonObject dropdownDefinitionUpdate(std::string_view dropdownDefinitionId, const JsonObject& properties, std::string_view tabId = "") {
        slimr::Json::object_t body{
            {"dropdownDefinitionId", std::string(dropdownDefinitionId)},
            {"dropdownDefinitionProperties", properties},
            {"fields", "title,options"}
        };
        if (!tabId.empty()) body["tabId"] = std::string(tabId);
        return slimr::Json::object_t{{"updateDropdownDefinitionProperties", std::move(body)}};
    }

    static JsonObject dropdownSelect(std::string_view dropdownId, std::string_view selectedOptionId, std::string_view tabId = "") {
        slimr::Json::object_t body{
            {"dropdownId", std::string(dropdownId)},
            {"dropdownProperties", slimr::Json::object_t{{"selectedOptionId", std::string(selectedOptionId)}}},
            {"fields", "selectedOptionId"}
        };
        if (!tabId.empty()) body["tabId"] = std::string(tabId);
        return slimr::Json::object_t{{"updateDropdownProperties", std::move(body)}};
    }

    struct InsertDropdownOpts {
        std::string definitionId;
        int index = 0;
        std::string optionId;
        std::string tabId;
    };

    static JsonObject insertDropdown(const InsertDropdownOpts& opts) {
        return slimr::Json::object_t{
            {"insertDropdown", slimr::Json::object_t{
                {"dropdownDefinitionId", opts.definitionId},
                {"location", loc(opts.index, "", opts.tabId)},
                {"selectedOptionId", opts.optionId}
            }}
        };
    }

    static JsonObject createFootnote(int index, std::string_view tabId = "") {
        return slimr::Json::object_t{
            {"createFootnote", slimr::Json::object_t{
                {"location", loc(index, "", tabId)}
            }}
        };
    }

    struct InsertRichLinkOpts {
        int index = 0;
        std::string segmentId;
        std::string tabId;
        std::string uri;
    };

    static JsonObject insertRichLink(const InsertRichLinkOpts& opts) {
        return slimr::Json::object_t{
            {"insertRichLink", slimr::Json::object_t{
                {"location", loc(opts.index, opts.segmentId, opts.tabId)},
                {"richLinkProperties", slimr::Json::object_t{{"uri", opts.uri}}}
            }}
        };
    }

    struct InsertDateOpts {
        std::optional<std::string> dateFormat;
        std::optional<std::string> displayText;
        int index = 0;
        std::optional<std::string> locale;
        std::string segmentId;
        std::string tabId;
        std::optional<std::string> timeFormat;
        std::optional<std::string> timeZoneId;
        std::optional<std::string> timestamp;
    };

    static JsonObject insertDate(const InsertDateOpts& opts) {
        slimr::Json::object_t props;
        if (opts.dateFormat) props["dateFormat"] = *opts.dateFormat;
        if (opts.displayText) props["displayText"] = *opts.displayText;
        if (opts.locale) props["locale"] = *opts.locale;
        if (opts.timeFormat) props["timeFormat"] = *opts.timeFormat;
        if (opts.timeZoneId) props["timeZoneId"] = *opts.timeZoneId;
        if (opts.timestamp) props["timestamp"] = *opts.timestamp;

        return slimr::Json::object_t{
            {"insertDate", slimr::Json::object_t{
                {"dateElementProperties", std::move(props)},
                {"location", loc(opts.index, opts.segmentId, opts.tabId)}
            }}
        };
    }

    struct InsertInlineImageOpts {
        std::optional<double> heightPt;
        int index = 0;
        std::string segmentId;
        std::string tabId;
        std::string uri;
        std::optional<double> widthPt;
    };

    static JsonObject insertInlineImage(const InsertInlineImageOpts& opts) {
        slimr::Json::object_t body{
            {"location", loc(opts.index, opts.segmentId, opts.tabId)},
            {"uri", opts.uri}
        };
        if (opts.widthPt || opts.heightPt) {
            slimr::Json::object_t sz;
            if (opts.widthPt) sz["width"] = pt(*opts.widthPt);
            if (opts.heightPt) sz["height"] = pt(*opts.heightPt);
            body["objectSize"] = std::move(sz);
        }
        return slimr::Json::object_t{{"insertInlineImage", std::move(body)}};
    }

    static JsonObject insertTextAt(int index, std::string_view text, std::string_view tabId) {
        return insertText(index, text, tabId);
    }

    static JsonObject contentRangeDelete(int startIndex, int endIndex, std::string_view tabId) {
        return deleteContentRange(startIndex, endIndex, tabId);
    }

    static JsonObject textStyleUpdate(
        int startIndex,
        int endIndex,
        const JsonObject& textStyle,
        const std::vector<std::string>& fields,
        std::string_view tabId
    ) {
        std::string fList;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += fields[i];
        }
        return updateTextStyle(startIndex, endIndex, textStyle, fList, tabId);
    }

    static JsonObject paragraphStyleUpdate(
        int startIndex,
        int endIndex,
        const JsonObject& paragraphStyle,
        const std::vector<std::string>& fields,
        std::string_view tabId
    ) {
        std::string fList;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += fields[i];
        }
        return updateParagraphStyle(startIndex, endIndex, paragraphStyle, fList, tabId);
    }

    static JsonObject tableInsert(int index, int rows, int columns, std::string_view tabId) {
        return slimr::Json::object_t{
            {"insertTable", slimr::Json::object_t{
                {"columns", static_cast<int64_t>(columns)},
                {"location", loc(index, "", tabId)},
                {"rows", static_cast<int64_t>(rows)}
            }}
        };
    }

    static JsonObject tableRange(const TableRangeOpts& opts) {
        return slimr::Json::object_t{
            {"columnSpan", static_cast<int64_t>(opts.columnSpan)},
            {"rowSpan", static_cast<int64_t>(opts.rowSpan)},
            {"tableCellLocation", slimr::Json::object_t{
                {"columnIndex", static_cast<int64_t>(opts.columnIndex)},
                {"rowIndex", static_cast<int64_t>(opts.rowIndex)},
                {"tableStartLocation", loc(opts.tableStart, "", opts.tabId)}
            }}
        };
    }

    static JsonObject tableCellsMerge(const TableRangeOpts& opts) {
        return slimr::Json::object_t{
            {"mergeTableCells", slimr::Json::object_t{
                {"tableRange", tableRange(opts)}
            }}
        };
    }

    static JsonObject tableCellsUnmerge(const TableRangeOpts& opts) {
        return slimr::Json::object_t{
            {"unmergeTableCells", slimr::Json::object_t{
                {"tableRange", tableRange(opts)}
            }}
        };
    }

    struct TableCellStyleUpdateOpts : TableRangeOpts {
        std::vector<std::string> fields;
        JsonObject tableCellStyle;
    };

    static JsonObject tableCellStyleUpdate(const TableCellStyleUpdateOpts& opts) {
        std::string fList;
        for (size_t i = 0; i < opts.fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += opts.fields[i];
        }
        return slimr::Json::object_t{
            {"updateTableCellStyle", slimr::Json::object_t{
                {"fields", std::move(fList)},
                {"tableCellStyle", opts.tableCellStyle},
                {"tableRange", tableRange(opts)}
            }}
        };
    }

    struct TableColumnPropertiesUpdateOpts {
        std::vector<int> columnIndices;
        std::vector<std::string> fields;
        std::string tabId;
        JsonObject tableColumnProperties;
        int tableStart = 0;
    };

    static JsonObject tableColumnPropertiesUpdate(const TableColumnPropertiesUpdateOpts& opts) {
        std::string fList;
        for (size_t i = 0; i < opts.fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += opts.fields[i];
        }
        slimr::Json::array_t cols;
        for (int c : opts.columnIndices) cols.push_back(static_cast<int64_t>(c));

        return slimr::Json::object_t{
            {"updateTableColumnProperties", slimr::Json::object_t{
                {"columnIndices", std::move(cols)},
                {"fields", std::move(fList)},
                {"tableColumnProperties", opts.tableColumnProperties},
                {"tableStartLocation", loc(opts.tableStart, "", opts.tabId)}
            }}
        };
    }

    struct TableRowStyleUpdateOpts {
        std::vector<std::string> fields;
        std::vector<int> rowIndices;
        std::string tabId;
        JsonObject tableRowStyle;
        int tableStart = 0;
    };

    static JsonObject tableRowStyleUpdate(const TableRowStyleUpdateOpts& opts) {
        std::string fList;
        for (size_t i = 0; i < opts.fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += opts.fields[i];
        }
        slimr::Json::array_t rows;
        for (int r : opts.rowIndices) rows.push_back(static_cast<int64_t>(r));

        return slimr::Json::object_t{
            {"updateTableRowStyle", slimr::Json::object_t{
                {"fields", std::move(fList)},
                {"rowIndices", std::move(rows)},
                {"tableRowStyle", opts.tableRowStyle},
                {"tableStartLocation", loc(opts.tableStart, "", opts.tabId)}
            }}
        };
    }

    static JsonObject sectionStyleUpdate(
        int startIndex,
        int endIndex,
        const JsonObject& sectionStyle,
        const std::vector<std::string>& fields,
        std::string_view tabId
    ) {
        std::string fList;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += fields[i];
        }
        return slimr::Json::object_t{
            {"updateSectionStyle", slimr::Json::object_t{
                {"fields", std::move(fList)},
                {"range", rng(startIndex, endIndex, "", tabId)},
                {"sectionStyle", sectionStyle}
            }}
        };
    }

    struct DocumentTabPropertiesUpdateOpts {
        std::optional<std::string> iconEmoji;
        std::optional<int> index;
        std::optional<std::string> parentTabId;
        std::optional<std::string> title;
    };

    static JsonObject documentTabPropertiesUpdate(
        std::string_view tabId,
        const DocumentTabPropertiesUpdateOpts& properties,
        const std::vector<std::string>& fields
    ) {
        std::string fList;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) fList += ",";
            fList += fields[i];
        }
        slimr::Json::object_t tp{
            {"tabId", std::string(tabId)}
        };
        if (properties.iconEmoji) tp["iconEmoji"] = *properties.iconEmoji;
        if (properties.index) tp["index"] = static_cast<int64_t>(*properties.index);
        if (properties.parentTabId) tp["parentTabId"] = *properties.parentTabId;
        if (properties.title) tp["title"] = *properties.title;

        return slimr::Json::object_t{
            {"updateDocumentTabProperties", slimr::Json::object_t{
                {"fields", std::move(fList)},
                {"tabProperties", std::move(tp)}
            }}
        };
    }

    struct DocumentTabAddOpts {
        std::optional<int> index;
        std::optional<std::string> parentTabId;
    };

    static JsonObject documentTabAdd(std::string_view title, const DocumentTabAddOpts& opts = {}) {
        slimr::Json::object_t tp{
            {"title", std::string(title)}
        };
        if (opts.index) tp["index"] = static_cast<int64_t>(*opts.index);
        if (opts.parentTabId) tp["parentTabId"] = *opts.parentTabId;

        return slimr::Json::object_t{
            {"addDocumentTab", slimr::Json::object_t{
                {"tabProperties", std::move(tp)}
            }}
        };
    }
};

} // namespace gdocsmith::reconcile
