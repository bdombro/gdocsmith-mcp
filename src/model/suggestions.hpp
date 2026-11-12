#pragma once

#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/types.hpp"

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace gdocsmith::model {

/** The suggestion marks one character, newline or atom carries. */
struct SuggestionMarks {
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedTextStyleChanges;

    bool operator==(const SuggestionMarks&) const = default;
};

/** An atom's suggestion marks, read from its element payload. */
inline SuggestionMarks atomSuggestion(const Atom& atom) {
    if (!atom.raw.has_value() || !atom.raw->holds<slimr::Json::object_t>()) {
        return {};
    }
    const auto& map = atom.raw->get<slimr::Json::object_t>();
    if (map.empty()) return {};
    const auto& payload = map.begin()->second;
    if (!payload.holds<slimr::Json::object_t>()) return {};

    SuggestionMarks marks;
    if (payload.contains("suggestedDeletionIds")) {
        const auto& del = payload["suggestedDeletionIds"];
        if (del.holds<slimr::Json::array_t>() && !del.get<slimr::Json::array_t>().empty()) {
            std::vector<std::string> ids;
            for (const auto& item : del.get<slimr::Json::array_t>()) {
                if (item.holds<std::string>()) ids.push_back(item.get<std::string>());
            }
            if (!ids.empty()) marks.suggestedDeletionIds = std::move(ids);
        }
    }
    if (payload.contains("suggestedInsertionIds")) {
        const auto& ins = payload["suggestedInsertionIds"];
        if (ins.holds<slimr::Json::array_t>() && !ins.get<slimr::Json::array_t>().empty()) {
            std::vector<std::string> ids;
            for (const auto& item : ins.get<slimr::Json::array_t>()) {
                if (item.holds<std::string>()) ids.push_back(item.get<std::string>());
            }
            if (!ids.empty()) marks.suggestedInsertionIds = std::move(ids);
        }
    }
    return marks;
}

/**
 * A canonical key for a set of marks: equal keys mean the same pending suggestions; "" is plain.
 */
inline std::string marksKey(const SuggestionMarks& marks) {
    std::vector<std::string> del = marks.suggestedDeletionIds.value_or(std::vector<std::string>{});
    std::vector<std::string> ins = marks.suggestedInsertionIds.value_or(std::vector<std::string>{});
    std::sort(del.begin(), del.end());
    std::sort(ins.begin(), ins.end());

    std::vector<std::string> changes;
    if (marks.suggestedTextStyleChanges.has_value() && marks.suggestedTextStyleChanges->holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : marks.suggestedTextStyleChanges->get<slimr::Json::object_t>()) {
            changes.push_back(k);
        }
        std::sort(changes.begin(), changes.end());
    }

    if (del.empty() && ins.empty() && changes.empty()) {
        return "";
    }

    slimr::Json::array_t arr;
    slimr::Json::array_t del_arr;
    for (const auto& s : del) del_arr.push_back(s);
    slimr::Json::array_t ins_arr;
    for (const auto& s : ins) ins_arr.push_back(s);
    slimr::Json::array_t chg_arr;
    for (const auto& s : changes) chg_arr.push_back(s);
    arr.push_back(std::move(del_arr));
    arr.push_back(std::move(ins_arr));
    arr.push_back(std::move(chg_arr));

    JsonObject out;
    out.data = std::move(arr);
    return out.dump().value_or("");
}

inline std::string marksKey(const NewlineMark& nl) {
    return marksKey(SuggestionMarks{
        .suggestedDeletionIds = nl.suggestedDeletionIds,
        .suggestedInsertionIds = nl.suggestedInsertionIds,
        .suggestedTextStyleChanges = nl.suggestedTextStyleChanges,
    });
}

inline std::string marksKey(const TextRun& tr) {
    return marksKey(SuggestionMarks{
        .suggestedDeletionIds = tr.suggestedDeletionIds,
        .suggestedInsertionIds = tr.suggestedInsertionIds,
        .suggestedTextStyleChanges = tr.suggestedTextStyleChanges,
    });
}

/** Every suggestion id the marks name. */
inline std::vector<std::string> marksIds(const SuggestionMarks& marks) {
    std::vector<std::string> out;
    if (marks.suggestedDeletionIds) {
        out.insert(out.end(), marks.suggestedDeletionIds->begin(), marks.suggestedDeletionIds->end());
    }
    if (marks.suggestedInsertionIds) {
        out.insert(out.end(), marks.suggestedInsertionIds->begin(), marks.suggestedInsertionIds->end());
    }
    if (marks.suggestedTextStyleChanges && marks.suggestedTextStyleChanges->holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : marks.suggestedTextStyleChanges->get<slimr::Json::object_t>()) {
            out.push_back(k);
        }
    }
    return out;
}

inline SuggestionMarks marksCopy(const SuggestionMarks& marks) {
    SuggestionMarks out;
    if (marks.suggestedDeletionIds && !marks.suggestedDeletionIds->empty()) {
        out.suggestedDeletionIds = *marks.suggestedDeletionIds;
    }
    if (marks.suggestedInsertionIds && !marks.suggestedInsertionIds->empty()) {
        out.suggestedInsertionIds = *marks.suggestedInsertionIds;
    }
    if (marks.suggestedTextStyleChanges) {
        out.suggestedTextStyleChanges = *marks.suggestedTextStyleChanges;
    }
    return out;
}

inline void marksStrip(SuggestionMarks& marks, const std::set<std::string>& ids) {
    if (marks.suggestedDeletionIds) {
        std::vector<std::string> kept;
        for (const auto& id : *marks.suggestedDeletionIds) {
            if (!ids.contains(id)) kept.push_back(id);
        }
        if (!kept.empty()) marks.suggestedDeletionIds = kept;
        else marks.suggestedDeletionIds = std::nullopt;
    }
    if (marks.suggestedInsertionIds) {
        std::vector<std::string> kept;
        for (const auto& id : *marks.suggestedInsertionIds) {
            if (!ids.contains(id)) kept.push_back(id);
        }
        if (!kept.empty()) marks.suggestedInsertionIds = kept;
        else marks.suggestedInsertionIds = std::nullopt;
    }
    if (marks.suggestedTextStyleChanges && marks.suggestedTextStyleChanges->holds<slimr::Json::object_t>()) {
        slimr::Json::object_t kept;
        for (const auto& [k, v] : marks.suggestedTextStyleChanges->get<slimr::Json::object_t>()) {
            if (!ids.contains(k)) kept.emplace(k, v);
        }
        if (!kept.empty()) {
            JsonObject obj;
            obj.data = std::move(kept);
            marks.suggestedTextStyleChanges = obj;
        } else {
            marks.suggestedTextStyleChanges = std::nullopt;
        }
    }
}

inline bool newlineInserted(const NewlineMark& newline) {
    return newline.suggestedInsertionIds.has_value() && !newline.suggestedInsertionIds->empty();
}

inline bool paragraphInserted(const ParagraphBlock& paragraph) {
    if (!newlineInserted(paragraph.newline)) return false;
    for (const auto& inlineItem : paragraph.inlines) {
        if (inlineItem.is_text()) {
            if (!inlineItem.suggestedInsertionIds || inlineItem.suggestedInsertionIds->empty()) return false;
        } else {
            auto marks = atomSuggestion(inlineItem.to_atom());
            if (!marks.suggestedInsertionIds || marks.suggestedInsertionIds->empty()) return false;
        }
    }
    return true;
}

inline bool tableColumnSuggested(const TableBlock& table) {
    auto ids = [](const auto& elem) {
        return marksKey(SuggestionMarks{
            .suggestedDeletionIds = elem.suggestedDeletionIds,
            .suggestedInsertionIds = elem.suggestedInsertionIds,
        });
    };
    for (const auto& row : table.rows) {
        std::string row_id = ids(row);
        for (const auto& cell : row.cells) {
            if (ids(cell) != row_id) return true;
        }
    }
    return false;
}

using SuggestionPart = std::string;

namespace suggestion_parts {
inline constexpr std::string_view CELL = "cell";
inline constexpr std::string_view CELL_STYLE = "cellStyle";
inline constexpr std::string_view DELETION = "deletion";
inline constexpr std::string_view INSERTION = "insertion";
inline constexpr std::string_view LIST = "list";
inline constexpr std::string_view NAMED_STYLE = "namedStyle";
inline constexpr std::string_view NEWLINE_DELETION = "newlineDeletion";
inline constexpr std::string_view NEWLINE_INSERTION = "newlineInsertion";
inline constexpr std::string_view OBJECT = "object";
inline constexpr std::string_view PAGE_LAYOUT = "pageLayout";
inline constexpr std::string_view PARAGRAPH_STYLE = "paragraphStyle";
inline constexpr std::string_view ROW = "row";
inline constexpr std::string_view ROW_STYLE = "rowStyle";
inline constexpr std::string_view TABLE = "table";
inline constexpr std::string_view TEXT_STYLE = "textStyle";
} // namespace suggestion_parts

inline void blocksSuggestions(
    const std::vector<Block>& blocks,
    const std::function<void(const std::optional<std::vector<std::string>>&, std::string_view)>& add
) {
    for (const auto& block : blocks) {
        if (block.is_paragraph()) {
            const auto& p = block.as_paragraph();
            for (const auto& inlineItem : p.inlines) {
                if (inlineItem.is_atom()) {
                    auto marks = atomSuggestion(inlineItem.to_atom());
                    add(marks.suggestedDeletionIds, suggestion_parts::OBJECT);
                    add(marks.suggestedInsertionIds, suggestion_parts::OBJECT);
                    continue;
                }
                add(inlineItem.suggestedDeletionIds, suggestion_parts::DELETION);
                add(inlineItem.suggestedInsertionIds, suggestion_parts::INSERTION);
                if (inlineItem.suggestedTextStyleChanges && inlineItem.suggestedTextStyleChanges->holds<slimr::Json::object_t>()) {
                    std::vector<std::string> keys;
                    for (const auto& [k, _] : inlineItem.suggestedTextStyleChanges->get<slimr::Json::object_t>()) {
                        keys.push_back(k);
                    }
                    add(keys, suggestion_parts::TEXT_STYLE);
                }
            }
            add(p.newline.suggestedDeletionIds, suggestion_parts::NEWLINE_DELETION);
            add(p.newline.suggestedInsertionIds, suggestion_parts::NEWLINE_INSERTION);
            if (p.newline.suggestedTextStyleChanges && p.newline.suggestedTextStyleChanges->holds<slimr::Json::object_t>()) {
                std::vector<std::string> keys;
                for (const auto& [k, _] : p.newline.suggestedTextStyleChanges->get<slimr::Json::object_t>()) {
                    keys.push_back(k);
                }
                add(keys, suggestion_parts::TEXT_STYLE);
            }
            if (p.suggestionsRaw && p.suggestionsRaw->holds<slimr::Json::object_t>()) {
                const auto& raw = p.suggestionsRaw->get<slimr::Json::object_t>();
                auto addObjKeys = [&](std::string_view field, std::string_view part) {
                    auto it = raw.find(field);
                    if (it != raw.end() && it->second.holds<slimr::Json::object_t>()) {
                        std::vector<std::string> keys;
                        for (const auto& [k, _] : it->second.get<slimr::Json::object_t>()) {
                            keys.push_back(k);
                        }
                        add(keys, part);
                    }
                };
                addObjKeys("suggestedParagraphStyleChanges", suggestion_parts::PARAGRAPH_STYLE);
                addObjKeys("suggestedBulletChanges", suggestion_parts::LIST);
                addObjKeys("suggestedPositionedObjectIds", suggestion_parts::OBJECT);
            }
        } else if (block.is_table()) {
            const auto& t = block.as_table();
            add(t.suggestedDeletionIds, suggestion_parts::TABLE);
            add(t.suggestedInsertionIds, suggestion_parts::TABLE);
            for (const auto& row : t.rows) {
                add(row.suggestedDeletionIds, suggestion_parts::ROW);
                add(row.suggestedInsertionIds, suggestion_parts::ROW);
                if (row.suggestedStyleChanges && row.suggestedStyleChanges->holds<slimr::Json::object_t>()) {
                    std::vector<std::string> keys;
                    for (const auto& [k, _] : row.suggestedStyleChanges->get<slimr::Json::object_t>()) {
                        keys.push_back(k);
                    }
                    add(keys, suggestion_parts::ROW_STYLE);
                }
                for (const auto& cell : row.cells) {
                    add(cell.suggestedDeletionIds, suggestion_parts::CELL);
                    add(cell.suggestedInsertionIds, suggestion_parts::CELL);
                    if (cell.suggestedStyleChanges && cell.suggestedStyleChanges->holds<slimr::Json::object_t>()) {
                        std::vector<std::string> keys;
                        for (const auto& [k, _] : cell.suggestedStyleChanges->get<slimr::Json::object_t>()) {
                            keys.push_back(k);
                        }
                        add(keys, suggestion_parts::CELL_STYLE);
                    }
                    blocksSuggestions(cell.blocks, add);
                }
            }
        }
    }
}

inline std::unordered_map<std::string, std::set<SuggestionPart>> tabSuggestions(const TabModel& tab) {
    std::unordered_map<std::string, std::set<SuggestionPart>> out;
    auto add = [&](const std::optional<std::vector<std::string>>& ids, std::string_view part) {
        if (!ids) return;
        for (const auto& id : *ids) {
            out[id].insert(std::string(part));
        }
    };
    blocksSuggestions(tab.blocks, add);
    if (tab.suggestedDocumentStyleChanges && tab.suggestedDocumentStyleChanges->holds<slimr::Json::object_t>()) {
        std::vector<std::string> keys;
        for (const auto& [k, _] : tab.suggestedDocumentStyleChanges->get<slimr::Json::object_t>()) {
            keys.push_back(k);
        }
        add(keys, suggestion_parts::PAGE_LAYOUT);
    }
    if (tab.suggestedNamedStylesChanges && tab.suggestedNamedStylesChanges->holds<slimr::Json::object_t>()) {
        std::vector<std::string> keys;
        for (const auto& [k, _] : tab.suggestedNamedStylesChanges->get<slimr::Json::object_t>()) {
            keys.push_back(k);
        }
        add(keys, suggestion_parts::NAMED_STYLE);
    }
    return out;
}

} // namespace gdocsmith::model
