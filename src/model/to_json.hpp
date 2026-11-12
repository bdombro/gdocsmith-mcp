#pragma once

#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model {

/** Pure builders for Google Docs batchUpdate request objects. */
class RequestBuilder {
public:
    static JsonObject make_range(int startIndex, int endIndex, std::string_view tabId = "", std::string_view segmentId = "") {
        slimr::Json::object_t r{
            {"startIndex", static_cast<int64_t>(startIndex)},
            {"endIndex", static_cast<int64_t>(endIndex)}
        };
        if (!tabId.empty()) r["tabId"] = std::string(tabId);
        if (!segmentId.empty()) r["segmentId"] = std::string(segmentId);
        return r;
    }

    static JsonObject make_location(int index, std::string_view tabId = "", std::string_view segmentId = "") {
        slimr::Json::object_t loc{
            {"index", static_cast<int64_t>(index)}
        };
        if (!tabId.empty()) loc["tabId"] = std::string(tabId);
        if (!segmentId.empty()) loc["segmentId"] = std::string(segmentId);
        return loc;
    }

    static JsonObject insertText(int index, std::string_view text, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"insertText", slimr::Json::object_t{
                {"location", make_location(index, tabId, segmentId)},
                {"text", std::string(text)}
            }}
        };
    }

    static JsonObject deleteContentRange(int startIndex, int endIndex, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"deleteContentRange", slimr::Json::object_t{
                {"range", make_range(startIndex, endIndex, tabId, segmentId)}
            }}
        };
    }

    static JsonObject updateTextStyle(int startIndex, int endIndex, const JsonObject& style, std::string_view fields, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"updateTextStyle", slimr::Json::object_t{
                {"range", make_range(startIndex, endIndex, tabId, segmentId)},
                {"textStyle", style},
                {"fields", std::string(fields)}
            }}
        };
    }

    static JsonObject updateParagraphStyle(int startIndex, int endIndex, const JsonObject& style, std::string_view fields, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"updateParagraphStyle", slimr::Json::object_t{
                {"range", make_range(startIndex, endIndex, tabId, segmentId)},
                {"paragraphStyle", style},
                {"fields", std::string(fields)}
            }}
        };
    }

    static JsonObject createParagraphBullets(int startIndex, int endIndex, std::string_view preset, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"createParagraphBullets", slimr::Json::object_t{
                {"bulletPreset", std::string(preset)},
                {"range", make_range(startIndex, endIndex, tabId, segmentId)}
            }}
        };
    }

    static JsonObject deleteParagraphBullets(int startIndex, int endIndex, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"deleteParagraphBullets", slimr::Json::object_t{
                {"range", make_range(startIndex, endIndex, tabId, segmentId)}
            }}
        };
    }

    static JsonObject insertTable(int index, int rows, int columns, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"insertTable", slimr::Json::object_t{
                {"columns", static_cast<int64_t>(columns)},
                {"rows", static_cast<int64_t>(rows)},
                {"location", make_location(index, tabId, segmentId)}
            }}
        };
    }

    static JsonObject insertPageBreak(int index, std::string_view tabId = "", std::string_view segmentId = "") {
        return slimr::Json::object_t{
            {"insertPageBreak", slimr::Json::object_t{
                {"location", make_location(index, tabId, segmentId)}
            }}
        };
    }

    static JsonObject insertSectionBreak(int index, std::string_view sectionType = "NEXT_PAGE", std::string_view tabId = "") {
        return slimr::Json::object_t{
            {"insertSectionBreak", slimr::Json::object_t{
                {"location", make_location(index, tabId)},
                {"sectionType", std::string(sectionType)}
            }}
        };
    }
};

} // namespace gdocsmith::model
