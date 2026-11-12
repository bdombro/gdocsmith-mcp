#pragma once

#include "model/api_facts.hpp"
#include "model/effective_style.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/suggestions.hpp"
#include "model/types.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gdocsmith::model {

struct CompareOptions {
    std::vector<IdentityTransfer> identityTransfers;
    std::set<std::string> listsRebuilt;
    std::map<std::string, std::string> tabIdMap;
    bool skipSegments = false;
};

struct CompareResult {
    std::vector<std::string> diffs;
    bool equal = true;
};

struct TabCompareOptions : CompareOptions {
    const TabModel* actualTab = nullptr;
};

inline JsonObject gaps(const JsonObject& style) {
    if (!style.contains("columnProperties") || !style["columnProperties"].holds<slimr::Json::array_t>()) {
        return style;
    }
    slimr::Json::array_t arr;
    for (const auto& col : style["columnProperties"].get<slimr::Json::array_t>()) {
        JsonObject c = col;
        if (c.holds<slimr::Json::object_t>()) {
            c.get<slimr::Json::object_t>().erase("width");
        }
        arr.push_back(std::move(c));
    }
    JsonObject out = style;
    out["columnProperties"] = arr;
    return out;
}

inline void styleCompare(
    std::string_view path,
    std::string_view label,
    const JsonObject& expected,
    const JsonObject& actual,
    std::vector<std::string>& diffs
) {
    if (styleEqual(expected, actual)) return;

    std::set<std::string> allKeys;
    if (expected.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : expected.get<slimr::Json::object_t>()) allKeys.insert(k);
    }
    if (actual.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : actual.get<slimr::Json::object_t>()) allKeys.insert(k);
    }
    std::vector<std::string> fields(allKeys.begin(), allKeys.end());
    std::vector<std::string> changed = styleFieldsChanged(expected, actual, fields);

    std::string changedList;
    for (size_t i = 0; i < changed.size(); ++i) {
        if (i > 0) changedList += ", ";
        changedList += changed[i];
    }
    diffs.push_back(std::format("{}: {} differs ({})", path, label, changedList));
}

inline void idsCompare(
    std::string_view path,
    std::string_view label,
    const std::optional<JsonObject>& expected,
    const std::optional<JsonObject>& actual,
    std::vector<std::string>& diffs
) {
    auto getKeys = [](const std::optional<JsonObject>& obj) {
        std::vector<std::string> keys;
        if (obj.has_value() && obj->holds<slimr::Json::object_t>()) {
            for (const auto& [k, _] : obj->get<slimr::Json::object_t>()) keys.push_back(k);
            std::sort(keys.begin(), keys.end());
        }
        std::string s;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) s += ", ";
            s += keys[i];
        }
        return s;
    };
    std::string want = getKeys(expected);
    std::string got = getKeys(actual);
    if (want != got) {
        diffs.push_back(std::format("{}: {} expected [{}], got [{}]", path, label, want, got));
    }
}

inline void marksCompare(
    std::string_view path,
    std::string_view label,
    const SuggestionMarks& expected,
    const SuggestionMarks& actual,
    std::vector<std::string>& diffs
) {
    std::string want = marksKey(expected);
    std::string got = marksKey(actual);
    if (want != got) {
        diffs.push_back(std::format("{}: {} suggestions expected {}, got {}", path, label, want.empty() ? "none" : want, got.empty() ? "none" : got));
    }
}

inline JsonObject inheritedFontEquivalent(
    const JsonObject& expected,
    const JsonObject& actual,
    const JsonObject& inherited
) {
    JsonObject normalized = actual;
    std::vector<std::string> checkFields = {"fontSize", "weightedFontFamily"};
    checkFields.insert(checkFields.end(), TEXT_STYLE_INHERIT_DROP_FIELDS.begin(), TEXT_STYLE_INHERIT_DROP_FIELDS.end());

    for (const auto& field : checkFields) {
        if (actual.contains(field) || !expected.contains(field) || !inherited.contains(field)) continue;
        JsonObject defaultValue = (field == "weightedFontFamily")
                                      ? slimr::Json::object_t{{"weight", static_cast<int64_t>(400)}}
                                      : inherited[field];
        if (field == "weightedFontFamily" && inherited[field].holds<slimr::Json::object_t>()) {
            for (const auto& [k, v] : inherited[field].get<slimr::Json::object_t>()) defaultValue[k] = v;
        }
        JsonObject expectedValue = (field == "weightedFontFamily")
                                       ? slimr::Json::object_t{{"weight", static_cast<int64_t>(400)}}
                                       : expected[field];
        if (field == "weightedFontFamily" && expected[field].holds<slimr::Json::object_t>()) {
            for (const auto& [k, v] : expected[field].get<slimr::Json::object_t>()) expectedValue[k] = v;
        }
        if (styleEqual(expectedValue, defaultValue)) {
            normalized[field] = expected[field];
        }
    }
    return normalized;
}

inline JsonObject inheritedParagraphEquivalent(
    const JsonObject& expected,
    const ParagraphBlock& actual,
    const TabModel& tab
) {
    std::string type = "NORMAL_TEXT";
    if (actual.style.contains("namedStyleType") && actual.style["namedStyleType"].holds<std::string>()) {
        type = actual.style["namedStyleType"].get<std::string>();
    }
    JsonObject inherited = namedTextStyle(tab, "NORMAL_TEXT").paragraphStyle;
    if (type != "NORMAL_TEXT") {
        JsonObject extra = namedTextStyle(tab, type).paragraphStyle;
        if (extra.holds<slimr::Json::object_t>()) {
            for (const auto& [k, v] : extra.get<slimr::Json::object_t>()) inherited[k] = v;
        }
    }
    JsonObject normalized = actual.style;
    if (expected.holds<slimr::Json::object_t>()) {
        for (const auto& [field, value] : expected.get<slimr::Json::object_t>()) {
            if (!normalized.contains(field) && inherited.contains(field) && styleEqual(value, inherited[field])) {
                normalized[field] = value;
            }
        }
    }
    return normalized;
}

inline JsonObject paragraphStyleDefaulted(const JsonObject& style) {
    JsonObject out = style;
    if (!out.contains("direction")) out["direction"] = "LEFT_TO_RIGHT";
    if (!out.contains("namedStyleType")) out["namedStyleType"] = "NORMAL_TEXT";
    if (!out.contains("pageBreakBefore")) out["pageBreakBefore"] = false;
    return out;
}

inline JsonObject cellStyleDefaulted(const JsonObject& style) {
    JsonObject out = style;
    if (!out.contains("columnSpan")) out["columnSpan"] = static_cast<int64_t>(1);
    if (!out.contains("rowSpan")) out["rowSpan"] = static_cast<int64_t>(1);
    return out;
}

inline bool atomCreateMatches(const AtomCreate& create, const Atom& actual) {
    if (actual.create.has_value()) {
        return create == *actual.create;
    }
    if (create.type == "person") {
        if (actual.raw.has_value() && actual.raw->contains("person") &&
            actual.raw->operator[]("person").contains("personProperties") &&
            actual.raw->operator[]("person")["personProperties"].contains("email") &&
            actual.raw->operator[]("person")["personProperties"]["email"].holds<std::string>()) {
            return actual.raw->operator[]("person")["personProperties"]["email"].get<std::string>() == create.email.value_or("");
        }
        return false;
    }
    if (create.type == "date") {
        if (actual.raw.has_value() && actual.raw->contains("dateElement") &&
            actual.raw->operator[]("dateElement").contains("dateElementProperties") &&
            actual.raw->operator[]("dateElement")["dateElementProperties"].contains("timestamp") &&
            actual.raw->operator[]("dateElement")["dateElementProperties"]["timestamp"].holds<std::string>()) {
            return actual.raw->operator[]("dateElement")["dateElementProperties"]["timestamp"].get<std::string>() == create.timestamp.value_or("");
        }
        return false;
    }
    if (create.type == "richLink") {
        if (actual.raw.has_value() && actual.raw->contains("richLink") &&
            actual.raw->operator[]("richLink").contains("richLinkProperties") &&
            actual.raw->operator[]("richLink")["richLinkProperties"].contains("uri") &&
            actual.raw->operator[]("richLink")["richLinkProperties"]["uri"].holds<std::string>()) {
            return actual.raw->operator[]("richLink")["richLinkProperties"]["uri"].get<std::string>() == create.uri.value_or("");
        }
        return false;
    }
    return actual.type == create.type;
}

inline JsonObject unnumbered(const std::optional<JsonObject>& raw) {
    if (!raw.has_value()) return make_json_object();
    JsonObject out = *raw;
    if (out.contains("footnoteReference") && out["footnoteReference"].holds<slimr::Json::object_t>()) {
        out["footnoteReference"].get<slimr::Json::object_t>().erase("footnoteNumber");
    }
    return out;
}

void blockCompare(
    std::string_view path,
    const Block& expected,
    const Block& actual,
    const TabCompareOptions& opts,
    std::vector<std::string>& diffs
);

inline void inlineCompare(
    std::string_view path,
    const Inline& expected,
    const Inline& actual,
    std::vector<std::string>& diffs,
    const JsonObject& inherited
) {
    if (expected.kind != actual.kind) {
        diffs.push_back(std::format("{}: kind expected \"{}\", got \"{}\"", path, expected.kind, actual.kind));
        return;
    }
    if (expected.is_text() && actual.is_text()) {
        if (expected.text != actual.text) {
            diffs.push_back(std::format("{}: text expected \"{}\", got \"{}\"", path, expected.text, actual.text));
        }
        marksCompare(path, "text",
            SuggestionMarks{.suggestedDeletionIds = expected.suggestedDeletionIds, .suggestedInsertionIds = expected.suggestedInsertionIds, .suggestedTextStyleChanges = expected.suggestedTextStyleChanges},
            SuggestionMarks{.suggestedDeletionIds = actual.suggestedDeletionIds, .suggestedInsertionIds = actual.suggestedInsertionIds, .suggestedTextStyleChanges = actual.suggestedTextStyleChanges},
            diffs);
        styleCompare(path, "style", expected.style, inheritedFontEquivalent(expected.style, actual.style, inherited), diffs);
        return;
    }
    if (!expected.is_atom() || !actual.is_atom()) return;
    if (expected.type != actual.type) {
        diffs.push_back(std::format("{}: atom type expected \"{}\", got \"{}\"", path, expected.type, actual.type));
    }
    if (expected.create.has_value()) {
        if (!atomCreateMatches(*expected.create, actual.to_atom())) {
            diffs.push_back(std::format("{}: atom create fields differ", path));
        }
        return;
    }
    styleCompare(path, "atom payload", unnumbered(expected.raw), unnumbered(actual.raw), diffs);
}

inline void headingIdCompare(
    std::string_view path,
    const ParagraphBlock& expected,
    const ParagraphBlock& actual,
    const CompareOptions& opts,
    std::vector<std::string>& diffs
) {
    std::string namedType = "NORMAL_TEXT";
    if (expected.style.contains("namedStyleType") && expected.style["namedStyleType"].holds<std::string>()) {
        namedType = expected.style["namedStyleType"].get<std::string>();
    }
    bool isHeading = (namedType != "NORMAL_TEXT");
    if (!isHeading) {
        if (actual.headingId.has_value() && !actual.headingId->empty()) {
            diffs.push_back(std::format("{}: expected no headingId, got \"{}\"", path, *actual.headingId));
        }
        return;
    }
    for (const auto& t : opts.identityTransfers) {
        if (t.toKey == expected.key && t.headingId.has_value()) {
            if (actual.headingId != t.headingId) {
                diffs.push_back(std::format("{}: headingId expected \"{}\" (transferred), got \"{}\"", path, *t.headingId, actual.headingId.value_or("")));
            }
            return;
        }
    }
    if (expected.key.starts_with("n") || !expected.headingId.has_value() || expected.headingId->empty()) {
        if (!actual.headingId.has_value() || actual.headingId->empty()) {
            diffs.push_back(std::format("{}: new heading paragraph has no headingId", path));
        }
        return;
    }
    if (expected.headingId != actual.headingId) {
        diffs.push_back(std::format("{}: headingId expected \"{}\", got \"{}\"", path, expected.headingId.value_or(""), actual.headingId.value_or("")));
    }
}

inline void bulletCompare(
    std::string_view path,
    const std::optional<BulletRef>& expected,
    const std::optional<BulletRef>& actual,
    const CompareOptions& opts,
    std::vector<std::string>& diffs
) {
    if (!expected.has_value() && !actual.has_value()) return;
    if (!expected.has_value() || !actual.has_value()) {
        diffs.push_back(std::format("{}: bullet expected {}, got {}", path, expected ? "present" : "absent", actual ? "present" : "absent"));
        return;
    }
    if (expected->nestingLevel != actual->nestingLevel) {
        diffs.push_back(std::format("{}: bullet nestingLevel expected {}, got {}", path, expected->nestingLevel, actual->nestingLevel));
    }
    bool listsRenamed = expected->listId.starts_with("new:") || actual->listId.starts_with("new:") || opts.listsRebuilt.contains(expected->listId);
    if (!listsRenamed && expected->listId != actual->listId) {
        diffs.push_back(std::format("{}: bullet listId expected \"{}\", got \"{}\"", path, expected->listId, actual->listId));
    }
}

inline void paragraphCompare(
    std::string_view path,
    const ParagraphBlock& expected,
    const ParagraphBlock& actual,
    const TabCompareOptions& opts,
    std::vector<std::string>& diffs
) {
    styleCompare(
        path,
        "style",
        paragraphStyleDefaulted(expected.style),
        paragraphStyleDefaulted(inheritedParagraphEquivalent(expected.style, actual, *opts.actualTab)),
        diffs
    );
    headingIdCompare(path, expected, actual, opts, diffs);
    bulletCompare(path, expected.bullet, actual.bullet, opts, diffs);

    auto getSug = [](const auto& p, std::string_view k) -> std::optional<JsonObject> {
        if (p.suggestionsRaw && p.suggestionsRaw->contains(k)) return p.suggestionsRaw->operator[](k);
        return std::nullopt;
    };
    for (const auto& kind : {"suggestedBulletChanges", "suggestedParagraphStyleChanges", "suggestedPositionedObjectIds"}) {
        idsCompare(path, kind, getSug(expected, kind), getSug(actual, kind), diffs);
    }
    marksCompare(path, "newline",
        SuggestionMarks{.suggestedDeletionIds = expected.newline.suggestedDeletionIds, .suggestedInsertionIds = expected.newline.suggestedInsertionIds, .suggestedTextStyleChanges = expected.newline.suggestedTextStyleChanges},
        SuggestionMarks{.suggestedDeletionIds = actual.newline.suggestedDeletionIds, .suggestedInsertionIds = actual.newline.suggestedInsertionIds, .suggestedTextStyleChanges = actual.newline.suggestedTextStyleChanges},
        diffs);

    JsonObject inherited = textStyleEffective(*opts.actualTab, actual, make_json_object());
    styleCompare(path, "newline style", expected.newline.style, inheritedFontEquivalent(expected.newline.style, actual.newline.style, inherited), diffs);

    size_t expectedIndex = 0;
    size_t actualIndex = 0;
    size_t expectedOffset = 0;
    size_t actualOffset = 0;
    while (expectedIndex < expected.inlines.size() && actualIndex < actual.inlines.size()) {
        const auto& expIn = expected.inlines[expectedIndex];
        const auto& actIn = actual.inlines[actualIndex];
        if (expIn.is_text() && actIn.is_text()) {
            size_t len = std::min(expIn.text.size() - expectedOffset, actIn.text.size() - actualOffset);
            Inline subExp = expIn;
            subExp.text = expIn.text.substr(expectedOffset, len);
            Inline subAct = actIn;
            subAct.text = actIn.text.substr(actualOffset, len);
            inlineCompare(std::format("{} inline {}", path, expectedIndex), subExp, subAct, diffs, inherited);
            expectedOffset += len;
            actualOffset += len;
            if (expectedOffset == expIn.text.size()) { expectedIndex++; expectedOffset = 0; }
            if (actualOffset == actIn.text.size()) { actualIndex++; actualOffset = 0; }
        } else {
            inlineCompare(std::format("{} inline {}", path, expectedIndex), expIn, actIn, diffs, inherited);
            expectedIndex++;
            actualIndex++;
            expectedOffset = actualOffset = 0;
        }
    }
    if (expectedIndex != expected.inlines.size() || actualIndex != actual.inlines.size()) {
        diffs.push_back(std::format("{}: inline content length differs", path));
    }
}

inline void cellCompare(
    std::string_view path,
    const CellModel& expected,
    const CellModel& actual,
    const TabCompareOptions& opts,
    std::vector<std::string>& diffs
) {
    styleCompare(path, "style", cellStyleDefaulted(expected.style), cellStyleDefaulted(actual.style), diffs);
    marksCompare(path, "cell",
        SuggestionMarks{.suggestedDeletionIds = expected.suggestedDeletionIds, .suggestedInsertionIds = expected.suggestedInsertionIds},
        SuggestionMarks{.suggestedDeletionIds = actual.suggestedDeletionIds, .suggestedInsertionIds = actual.suggestedInsertionIds},
        diffs);
    idsCompare(path, "cell-style suggestions", expected.suggestedStyleChanges, actual.suggestedStyleChanges, diffs);
    if (expected.blocks.size() != actual.blocks.size()) {
        diffs.push_back(std::format("{}: block count expected {}, got {}", path, expected.blocks.size(), actual.blocks.size()));
        return;
    }
    for (size_t i = 0; i < expected.blocks.size(); ++i) {
        blockCompare(std::format("{} block {}", path, i), expected.blocks[i], actual.blocks[i], opts, diffs);
    }
}

inline void rowCompare(
    std::string_view path,
    const RowModel& expected,
    const RowModel& actual,
    const TabCompareOptions& opts,
    std::vector<std::string>& diffs
) {
    styleCompare(path, "style", expected.style, actual.style, diffs);
    marksCompare(path, "row",
        SuggestionMarks{.suggestedDeletionIds = expected.suggestedDeletionIds, .suggestedInsertionIds = expected.suggestedInsertionIds},
        SuggestionMarks{.suggestedDeletionIds = actual.suggestedDeletionIds, .suggestedInsertionIds = actual.suggestedInsertionIds},
        diffs);
    idsCompare(path, "row-style suggestions", expected.suggestedStyleChanges, actual.suggestedStyleChanges, diffs);
    if (expected.cells.size() != actual.cells.size()) {
        diffs.push_back(std::format("{}: cell count expected {}, got {}", path, expected.cells.size(), actual.cells.size()));
        return;
    }
    for (size_t i = 0; i < expected.cells.size(); ++i) {
        cellCompare(std::format("{} cell {}", path, i), expected.cells[i], actual.cells[i], opts, diffs);
    }
}

inline void tableCompare(
    std::string_view path,
    const TableBlock& expected,
    const TableBlock& actual,
    const TabCompareOptions& opts,
    std::vector<std::string>& diffs
) {
    if (expected.columns.size() != actual.columns.size()) {
        diffs.push_back(std::format("{}: column count expected {}, got {}", path, expected.columns.size(), actual.columns.size()));
    } else {
        for (size_t i = 0; i < expected.columns.size(); ++i) {
            styleCompare(std::format("{} column {}", path, i), "props", expected.columns[i].props, actual.columns[i].props, diffs);
        }
    }
    marksCompare(path, "table",
        SuggestionMarks{.suggestedDeletionIds = expected.suggestedDeletionIds, .suggestedInsertionIds = expected.suggestedInsertionIds},
        SuggestionMarks{.suggestedDeletionIds = actual.suggestedDeletionIds, .suggestedInsertionIds = actual.suggestedInsertionIds},
        diffs);
    if (expected.rows.size() != actual.rows.size()) {
        diffs.push_back(std::format("{}: row count expected {}, got {}", path, expected.rows.size(), actual.rows.size()));
        return;
    }
    for (size_t i = 0; i < expected.rows.size(); ++i) {
        rowCompare(std::format("{} row {}", path, i), expected.rows[i], actual.rows[i], opts, diffs);
    }
}

inline void blockCompare(
    std::string_view path,
    const Block& expected,
    const Block& actual,
    const TabCompareOptions& opts,
    std::vector<std::string>& diffs
) {
    if (expected.kind != actual.kind) {
        diffs.push_back(std::format("{}: kind expected \"{}\", got \"{}\"", path, expected.kind, actual.kind));
        return;
    }
    if (expected.is_paragraph() && actual.is_paragraph()) {
        paragraphCompare(path, expected.as_paragraph(), actual.as_paragraph(), opts, diffs);
    } else if (expected.is_table() && actual.is_table()) {
        tableCompare(path, expected.as_table(), actual.as_table(), opts, diffs);
    } else if (expected.is_toc() && actual.is_toc()) {
        if (!styleEqual(expected.as_toc().raw, actual.as_toc().raw)) {
            diffs.push_back(std::format("{}: TOC content differs", path));
        }
    } else if (expected.is_section_break() && actual.is_section_break()) {
        styleCompare(path, "sectionStyle", gaps(expected.as_section_break().sectionStyle), gaps(actual.as_section_break().sectionStyle), diffs);
    }
}

inline void tabCompare(
    const TabModel& expected,
    const TabModel& actual,
    const CompareOptions& opts,
    std::vector<std::string>& diffs
) {
    std::string path = std::format("tab \"{}\"", expected.title);
    if (expected.title != actual.title) {
        diffs.push_back(std::format("{}: title expected \"{}\", got \"{}\"", path, expected.title, actual.title));
    }
    std::optional<std::string> expectedParent = expected.parentTabId;
    if (expectedParent && opts.tabIdMap.contains(*expectedParent)) {
        expectedParent = opts.tabIdMap.at(*expectedParent);
    }
    if (expectedParent != actual.parentTabId) {
        diffs.push_back(std::format("{}: parentTabId expected {}, got {}", path, expectedParent ? *expectedParent : "none", actual.parentTabId ? *actual.parentTabId : "none"));
    }
    styleCompare(path, "documentStyle", expected.documentStyle, actual.documentStyle, diffs);
    styleCompare(path, "namedStyles", expected.namedStyles, actual.namedStyles, diffs);
    idsCompare(path, "page-layout suggestions", expected.suggestedDocumentStyleChanges, actual.suggestedDocumentStyleChanges, diffs);
    idsCompare(path, "named-style suggestions", expected.suggestedNamedStylesChanges, actual.suggestedNamedStylesChanges, diffs);

    if (!opts.skipSegments) {
        auto sameSeg = [](const SegmentModel& a, const SegmentModel& b) {
            return a.kind == b.kind && (a.kind == "footnote" ? a.id == b.id : a.type == b.type && a.section == b.section);
        };
        for (const auto& seg : expected.segments) {
            auto it = std::find_if(actual.segments.begin(), actual.segments.end(), [&](const SegmentModel& s) {
                return sameSeg(s, seg);
            });
            if (it == actual.segments.end()) {
                diffs.push_back(std::format("{}: {} expected, missing", path, seg.kind));
            } else if (seg.tab && it->tab) {
                tabCompare(*seg.tab, *it->tab, opts, diffs);
            }
        }
        for (const auto& seg : actual.segments) {
            auto it = std::find_if(expected.segments.begin(), expected.segments.end(), [&](const SegmentModel& s) {
                return sameSeg(s, seg);
            });
            if (it == expected.segments.end()) {
                diffs.push_back(std::format("{}: {} unexpected", path, seg.kind));
            }
        }
    }

    JsonObject expDd = slimr::Json::object_t{};
    for (const auto& [k, v] : expected.dropdownDefinitions) expDd[k] = v;
    JsonObject actDd = slimr::Json::object_t{};
    for (const auto& [k, v] : actual.dropdownDefinitions) actDd[k] = v;
    styleCompare(path, "dropdownDefinitions", expDd, actDd, diffs);

    styleCompare(path, "leadingSectionStyle", gaps(expected.leadingSectionStyle), gaps(actual.leadingSectionStyle), diffs);

    if (expected.blocks.size() != actual.blocks.size()) {
        diffs.push_back(std::format("{}: block count expected {}, got {}", path, expected.blocks.size(), actual.blocks.size()));
        return;
    }
    TabCompareOptions tabOpts;
    static_cast<CompareOptions&>(tabOpts) = opts;
    tabOpts.actualTab = &actual;
    for (size_t i = 0; i < expected.blocks.size(); ++i) {
        blockCompare(std::format("{} block {}", path, i), expected.blocks[i], actual.blocks[i], tabOpts, diffs);
    }
}

/**
 * Compares two `DocModel`s under G3's equivalence contract: structure and content must match,
 * but revision ids, named ranges, and concrete id values of anything newly created are ignored.
 */
inline CompareResult docModelsCompare(
    const DocModel& expected,
    const DocModel& actual,
    const CompareOptions& opts = {}
) {
    std::vector<std::string> diffs;
    if (expected.tabs.size() != actual.tabs.size()) {
        diffs.push_back(std::format("tab count: expected {}, got {}", expected.tabs.size(), actual.tabs.size()));
        return CompareResult{.diffs = std::move(diffs), .equal = false};
    }
    for (size_t i = 0; i < expected.tabs.size(); ++i) {
        tabCompare(expected.tabs[i], actual.tabs[i], opts, diffs);
    }
    return CompareResult{.diffs = diffs, .equal = diffs.empty()};
}

} // namespace gdocsmith::model
