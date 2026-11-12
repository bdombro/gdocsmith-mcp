#pragma once

#include "model/keys.hpp"
#include "model/lists.hpp"
#include "model/object_ids.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/suggestions.hpp"
#include "model/types.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::model {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

struct DocModelParseOptions {
    std::string docId;
    KeyAllocator keys;
    std::optional<std::string> suggestionsViewMode;
};

inline const std::vector<std::string> SEGMENT_STYLE_FIELDS = {
    "defaultHeaderId",
    "firstPageHeaderId",
    "evenPageHeaderId",
    "defaultFooterId",
    "firstPageFooterId",
    "evenPageFooterId"
};

inline std::string segmentTabId(std::string_view tabId, std::string_view segmentId) {
    return std::format("{}::{}", tabId, segmentId);
}

Block blockParse(const JsonObject& el, KeyAllocator& keys);

inline Range requireRange(const JsonObject& el, std::string_view what) {
    if (!el.contains("startIndex") || !el.contains("endIndex")) {
        throw ParseError(std::format("{} element is missing startIndex/endIndex", what));
    }
    int start = el["startIndex"].holds<int64_t>() ? static_cast<int>(el["startIndex"].get<int64_t>()) : static_cast<int>(el["startIndex"].get<double>());
    int end = el["endIndex"].holds<int64_t>() ? static_cast<int>(el["endIndex"].get<int64_t>()) : static_cast<int>(el["endIndex"].get<double>());
    return Range{.end = end, .start = start};
}

inline Atom atomFromElement(const JsonObject& el, KeyAllocator& keys) {
    static const std::vector<std::pair<std::string, std::string>> VARIANTS = {
        {"person", "person"},
        {"richLink", "richLink"},
        {"dateElement", "date"},
        {"dropdown", "dropdown"},
        {"inlineObjectElement", "image"},
        {"footnoteReference", "footnoteRef"},
        {"horizontalRule", "horizontalRule"},
        {"pageBreak", "pageBreak"},
        {"columnBreak", "columnBreak"},
        {"equation", "equation"},
        {"autoText", "autoText"}
    };

    Range rng = requireRange(el, "atom");
    for (const auto& [jsonKey, typeName] : VARIANTS) {
        if (el.contains(jsonKey)) {
            const auto& payload = el[jsonKey];
            Atom atom;
            atom.kind = "atom";
            atom.key = keys.next("o");
            atom.type = typeName;
            atom.length = rng.end - rng.start;

            if (payload.contains("textStyle")) {
                atom.style = styleCanonical(payload["textStyle"]);
            }
            JsonObject raw = el;
            if (raw.holds<slimr::Json::object_t>()) {
                auto& map = raw.get<slimr::Json::object_t>();
                map.erase("startIndex");
                map.erase("endIndex");
            }
            atom.raw = raw;
            return atom;
        }
    }
    throw ParseError("paragraph element has no recognized non-text variant");
}

inline std::vector<Inline> mergeAdjacentTextRuns(const std::vector<Inline>& inlines) {
    std::vector<Inline> out;
    for (const auto& inlineItem : inlines) {
        if (!out.empty() && out.back().is_text() && inlineItem.is_text()) {
            auto& prev = out.back();
            if (styleEqual(prev.style, inlineItem.style) &&
                suggestionIdsEqual(prev.suggestedDeletionIds, inlineItem.suggestedDeletionIds) &&
                suggestionIdsEqual(prev.suggestedInsertionIds, inlineItem.suggestedInsertionIds) &&
                styleEqual(prev.suggestedTextStyleChanges.value_or(make_json_object()),
                           inlineItem.suggestedTextStyleChanges.value_or(make_json_object()))) {
                prev.text += inlineItem.text;
                continue;
            }
        }
        out.push_back(inlineItem);
    }
    return out;
}

inline ParagraphBlock paragraphParse(
    const JsonObject& p,
    const Range& origin,
    KeyAllocator& keys
) {
    if (!p.contains("elements") || !p["elements"].holds<slimr::Json::array_t>()) {
        throw ParseError("paragraph has no elements");
    }
    const auto& elements = p["elements"].get<slimr::Json::array_t>();
    if (elements.empty()) throw ParseError("paragraph has no elements");

    std::vector<Inline> rawInlines;
    std::optional<NewlineMark> newline;

    for (size_t i = 0; i < elements.size(); ++i) {
        bool isLast = (i == elements.size() - 1);
        const auto& el = elements[i];
        if (!el.contains("textRun")) {
            rawInlines.push_back(Inline(atomFromElement(el, keys)));
            continue;
        }
        const auto& tr = el["textRun"];
        std::string content = tr.contains("content") && tr["content"].holds<std::string>() ? tr["content"].get<std::string>() : "";
        JsonObject style = tr.contains("textStyle") ? styleCanonical(tr["textStyle"]) : make_json_object();

        std::optional<std::vector<std::string>> sugDel;
        if (tr.contains("suggestedDeletionIds") && tr["suggestedDeletionIds"].holds<slimr::Json::array_t>()) {
            std::vector<std::string> ids;
            for (const auto& item : tr["suggestedDeletionIds"].get<slimr::Json::array_t>()) {
                if (item.holds<std::string>()) ids.push_back(item.get<std::string>());
            }
            if (!ids.empty()) sugDel = ids;
        }
        std::optional<std::vector<std::string>> sugIns;
        if (tr.contains("suggestedInsertionIds") && tr["suggestedInsertionIds"].holds<slimr::Json::array_t>()) {
            std::vector<std::string> ids;
            for (const auto& item : tr["suggestedInsertionIds"].get<slimr::Json::array_t>()) {
                if (item.holds<std::string>()) ids.push_back(item.get<std::string>());
            }
            if (!ids.empty()) sugIns = ids;
        }
        std::optional<JsonObject> sugStyle;
        if (tr.contains("suggestedTextStyleChanges")) {
            sugStyle = tr["suggestedTextStyleChanges"];
        }

        if (!isLast) {
            TextRun r;
            r.kind = "text";
            r.text = content;
            r.style = style;
            r.suggestedDeletionIds = sugDel;
            r.suggestedInsertionIds = sugIns;
            r.suggestedTextStyleChanges = sugStyle;
            rawInlines.push_back(Inline(std::move(r)));
            continue;
        }

        if (!content.ends_with('\n')) {
            throw ParseError("paragraph's last element does not end with a newline");
        }
        newline = NewlineMark{
            .style = style,
            .suggestedDeletionIds = sugDel,
            .suggestedInsertionIds = sugIns,
            .suggestedTextStyleChanges = sugStyle,
        };
        std::string visible = content.substr(0, content.size() - 1);
        if (!visible.empty()) {
            TextRun r;
            r.kind = "text";
            r.text = visible;
            r.style = style;
            r.suggestedDeletionIds = sugDel;
            r.suggestedInsertionIds = sugIns;
            r.suggestedTextStyleChanges = sugStyle;
            rawInlines.push_back(Inline(std::move(r)));
        }
    }

    if (!newline) throw ParseError("paragraph has no trailing newline");

    auto inlines = mergeAdjacentTextRuns(rawInlines);
    JsonObject rawStyle = p.contains("paragraphStyle") ? p["paragraphStyle"] : make_json_object();
    std::optional<std::string> headingId;
    if (rawStyle.contains("headingId") && rawStyle["headingId"].holds<std::string>()) {
        headingId = rawStyle["headingId"].get<std::string>();
        if (rawStyle.holds<slimr::Json::object_t>()) {
            rawStyle.get<slimr::Json::object_t>().erase("headingId");
        }
    }

    std::optional<BulletRef> bullet;
    if (p.contains("bullet") && p["bullet"].holds<slimr::Json::object_t>()) {
        const auto& b = p["bullet"];
        BulletRef br;
        if (b.contains("listId") && b["listId"].holds<std::string>()) br.listId = b["listId"].get<std::string>();
        if (b.contains("nestingLevel")) {
            if (b["nestingLevel"].holds<int64_t>()) br.nestingLevel = static_cast<int>(b["nestingLevel"].get<int64_t>());
            else if (b["nestingLevel"].holds<double>()) br.nestingLevel = static_cast<int>(b["nestingLevel"].get<double>());
        }
        if (b.contains("textStyle")) br.textStyle = styleCanonical(b["textStyle"]);
        bullet = br;
    }

    auto marked = [](const auto& item) {
        return (item.suggestedDeletionIds && !item.suggestedDeletionIds->empty()) ||
               (item.suggestedInsertionIds && !item.suggestedInsertionIds->empty()) ||
               (item.suggestedTextStyleChanges && item.suggestedTextStyleChanges->template holds<slimr::Json::object_t>() &&
                !item.suggestedTextStyleChanges->template get<slimr::Json::object_t>().empty());
    };

    bool protectedFlag = p.contains("suggestedParagraphStyleChanges") ||
                         p.contains("suggestedBulletChanges") ||
                         p.contains("suggestedPositionedObjectIds") ||
                         marked(*newline);
    if (!protectedFlag) {
        for (const auto& inlineItem : inlines) {
            if (inlineItem.is_text()) {
                if (marked(inlineItem)) { protectedFlag = true; break; }
            } else {
                auto marks = atomSuggestion(inlineItem.to_atom());
                if (marked(marks)) { protectedFlag = true; break; }
            }
        }
    }

    ParagraphBlock block;
    block.kind = "paragraph";
    block.key = keys.next("o");
    block.inlines = std::move(inlines);
    block.newline = std::move(*newline);
    block.style = styleCanonical(rawStyle);
    block.bullet = std::move(bullet);
    block.headingId = std::move(headingId);
    block.origin = origin;
    block.is_protected = protectedFlag;

    if (p.contains("positionedObjectIds") && p["positionedObjectIds"].holds<slimr::Json::array_t>()) {
        std::vector<std::string> ids;
        for (const auto& item : p["positionedObjectIds"].get<slimr::Json::array_t>()) {
            if (item.holds<std::string>()) ids.push_back(item.get<std::string>());
        }
        if (!ids.empty()) block.positionedObjectIds = ids;
    }

    if (protectedFlag) {
        slimr::Json::object_t rawSug;
        if (p.contains("suggestedBulletChanges")) rawSug["suggestedBulletChanges"] = p["suggestedBulletChanges"];
        if (p.contains("suggestedParagraphStyleChanges")) rawSug["suggestedParagraphStyleChanges"] = p["suggestedParagraphStyleChanges"];
        if (p.contains("suggestedPositionedObjectIds")) rawSug["suggestedPositionedObjectIds"] = p["suggestedPositionedObjectIds"];
        JsonObject obj;
        obj.data = std::move(rawSug);
        block.suggestionsRaw = obj;
    }

    return block;
}

inline CellModel cellParse(const JsonObject& cell, KeyAllocator& keys) {
    std::vector<Block> blocks;
    if (cell.contains("content") && cell["content"].holds<slimr::Json::array_t>()) {
        for (const auto& el : cell["content"].get<slimr::Json::array_t>()) {
            blocks.push_back(blockParse(el, keys));
        }
    }
    CellModel cm;
    cm.key = keys.next("o");
    cm.blocks = std::move(blocks);
    if (cell.contains("tableCellStyle")) {
        cm.style = styleCanonical(cell["tableCellStyle"]);
    }
    if (cell.contains("startIndex") && cell.contains("endIndex")) {
        cm.origin = requireRange(cell, "tableCell");
    }
    if (cell.contains("suggestedTableCellStyleChanges")) {
        cm.suggestedStyleChanges = cell["suggestedTableCellStyleChanges"];
    }
    return cm;
}

inline RowModel rowParse(const JsonObject& row, KeyAllocator& keys) {
    std::vector<CellModel> cells;
    if (row.contains("tableCells") && row["tableCells"].holds<slimr::Json::array_t>()) {
        for (const auto& c : row["tableCells"].get<slimr::Json::array_t>()) {
            cells.push_back(cellParse(c, keys));
        }
    }
    RowModel rm;
    rm.key = keys.next("o");
    rm.cells = std::move(cells);
    if (row.contains("tableRowStyle")) {
        rm.style = styleCanonical(row["tableRowStyle"]);
    }
    if (row.contains("startIndex") && row.contains("endIndex")) {
        rm.origin = requireRange(row, "tableRow");
    }
    if (row.contains("suggestedTableRowStyleChanges")) {
        rm.suggestedStyleChanges = row["suggestedTableRowStyleChanges"];
    }
    return rm;
}

inline TableBlock tableParse(
    const JsonObject& t,
    const Range& origin,
    KeyAllocator& keys
) {
    std::vector<RowModel> rows;
    if (t.contains("tableRows") && t["tableRows"].holds<slimr::Json::array_t>()) {
        for (const auto& r : t["tableRows"].get<slimr::Json::array_t>()) {
            rows.push_back(rowParse(r, keys));
        }
    }
    int colCount = 0;
    if (t.contains("columns")) {
        colCount = t["columns"].holds<int64_t>() ? static_cast<int>(t["columns"].get<int64_t>()) : static_cast<int>(t["columns"].get<double>());
    } else if (!rows.empty()) {
        colCount = static_cast<int>(rows[0].cells.size());
    }

    std::vector<JsonObject> colProps;
    if (t.contains("tableStyle") && t["tableStyle"].contains("tableColumnProperties") &&
        t["tableStyle"]["tableColumnProperties"].holds<slimr::Json::array_t>()) {
        for (const auto& cp : t["tableStyle"]["tableColumnProperties"].get<slimr::Json::array_t>()) {
            colProps.push_back(cp);
        }
    }

    std::vector<ColumnModel> columns;
    for (int i = 0; i < colCount; ++i) {
        ColumnModel cm;
        cm.key = keys.next("o");
        cm.props = (i < static_cast<int>(colProps.size())) ? colProps[i] : make_json_object();
        columns.push_back(std::move(cm));
    }

    TableBlock tb;
    tb.kind = "table";
    tb.key = keys.next("o");
    tb.columns = std::move(columns);
    tb.rows = std::move(rows);
    tb.origin = origin;

    auto hasSug = [](const auto& el) {
        return (el.suggestedDeletionIds && !el.suggestedDeletionIds->empty()) ||
               (el.suggestedInsertionIds && !el.suggestedInsertionIds->empty());
    };
    bool protectedFlag = hasSug(tb);
    if (!protectedFlag) {
        for (const auto& r : tb.rows) {
            if (hasSug(r)) { protectedFlag = true; break; }
            for (const auto& c : r.cells) {
                if (hasSug(c)) { protectedFlag = true; break; }
                for (const auto& b : c.blocks) {
                    if ((b.is_paragraph() && b.as_paragraph().is_protected) ||
                        (b.is_table() && b.as_table().is_protected)) {
                        protectedFlag = true;
                        break;
                    }
                }
                if (protectedFlag) break;
            }
            if (protectedFlag) break;
        }
    }
    tb.is_protected = protectedFlag;
    return tb;
}

inline TocBlock tocParse(const JsonObject& el, KeyAllocator& keys) {
    Range origin = requireRange(el, "tableOfContents");
    TocBlock tb;
    tb.kind = "toc";
    tb.key = keys.next("o");
    tb.length = origin.end - origin.start;
    tb.origin = origin;
    tb.raw = el.contains("tableOfContents") ? el["tableOfContents"] : make_json_object();
    return tb;
}

inline SectionBreakBlock sectionBreakParse(
    const JsonObject& sb,
    const Range& origin,
    KeyAllocator& keys
) {
    SectionBreakBlock sbb;
    sbb.kind = "sectionBreak";
    sbb.key = keys.next("o");
    sbb.origin = origin;
    if (sb.contains("sectionStyle")) {
        sbb.sectionStyle = styleCanonical(sb["sectionStyle"]);
    }
    return sbb;
}

inline Block blockParse(const JsonObject& el, KeyAllocator& keys) {
    if (el.contains("paragraph")) {
        return Block(paragraphParse(el["paragraph"], requireRange(el, "paragraph"), keys));
    }
    if (el.contains("table")) {
        return Block(tableParse(el["table"], requireRange(el, "table"), keys));
    }
    if (el.contains("tableOfContents")) {
        return Block(tocParse(el, keys));
    }
    if (el.contains("sectionBreak")) {
        return Block(sectionBreakParse(el["sectionBreak"], requireRange(el, "sectionBreak"), keys));
    }
    throw ParseError("structural element has none of paragraph, table, tableOfContents, sectionBreak");
}

inline TabModel tabParse(
    std::string_view tabId,
    std::string_view title,
    std::optional<std::string> parentTabId,
    const JsonObject& dt,
    KeyAllocator keys
) {
    std::vector<JsonObject> content;
    if (dt.contains("body") && dt["body"].contains("content") && dt["body"]["content"].holds<slimr::Json::array_t>()) {
        for (const auto& item : dt["body"]["content"].get<slimr::Json::array_t>()) {
            content.push_back(item);
        }
    }
    if (content.empty()) {
        throw ParseError(std::format("tab \"{}\" body does not start with a leading section break at index 0", tabId));
    }
    const auto& first = content[0];
    int startIdx = 0;
    if (first.contains("startIndex")) {
        startIdx = first["startIndex"].holds<int64_t>() ? static_cast<int>(first["startIndex"].get<int64_t>()) : static_cast<int>(first["startIndex"].get<double>());
    }
    if (!first.contains("sectionBreak") || startIdx != 0) {
        throw ParseError(std::format("tab \"{}\" body does not start with a leading section break at index 0", tabId));
    }

    JsonObject leadingSectionStyle = first["sectionBreak"].contains("sectionStyle")
                                         ? styleCanonical(first["sectionBreak"]["sectionStyle"])
                                         : make_json_object();

    std::vector<Block> blocks;
    for (size_t i = 1; i < content.size(); ++i) {
        blocks.push_back(blockParse(content[i], keys));
    }

    std::unordered_map<std::string, ListDef> lists;
    if (dt.contains("lists") && dt["lists"].holds<slimr::Json::object_t>()) {
        for (const auto& [lid, ldef] : dt["lists"].get<slimr::Json::object_t>()) {
            ListDef ld;
            ld.isNew = false;
            if (ldef.contains("listProperties") && ldef["listProperties"].contains("nestingLevels") &&
                ldef["listProperties"]["nestingLevels"].holds<slimr::Json::array_t>()) {
                for (const auto& nl : ldef["listProperties"]["nestingLevels"].get<slimr::Json::array_t>()) {
                    ld.nestingLevels.push_back(nl);
                }
            }
            lists[lid] = std::move(ld);
        }
    }

    std::vector<NamedRangeDef> namedRanges;
    if (dt.contains("namedRanges") && dt["namedRanges"].holds<slimr::Json::object_t>()) {
        for (const auto& [_, entries] : dt["namedRanges"].get<slimr::Json::object_t>()) {
            if (entries.contains("namedRanges") && entries["namedRanges"].holds<slimr::Json::array_t>()) {
                for (const auto& entry : entries["namedRanges"].get<slimr::Json::array_t>()) {
                    NamedRangeDef nrd;
                    if (entry.contains("name") && entry["name"].holds<std::string>()) nrd.name = entry["name"].get<std::string>();
                    else if (entries.contains("name") && entries["name"].holds<std::string>()) nrd.name = entries["name"].get<std::string>();
                    if (entry.contains("namedRangeId") && entry["namedRangeId"].holds<std::string>()) nrd.namedRangeId = entry["namedRangeId"].get<std::string>();

                    if (entry.contains("ranges") && entry["ranges"].holds<slimr::Json::array_t>()) {
                        for (const auto& r : entry["ranges"].get<slimr::Json::array_t>()) {
                            NamedRangeDef::RangePart rp;
                            if (r.contains("startIndex")) rp.start = r["startIndex"].holds<int64_t>() ? static_cast<int>(r["startIndex"].get<int64_t>()) : static_cast<int>(r["startIndex"].get<double>());
                            if (r.contains("endIndex")) rp.end = r["endIndex"].holds<int64_t>() ? static_cast<int>(r["endIndex"].get<int64_t>()) : static_cast<int>(r["endIndex"].get<double>());
                            if (r.contains("segmentId") && r["segmentId"].holds<std::string>()) rp.segmentId = r["segmentId"].get<std::string>();
                            nrd.ranges.push_back(std::move(rp));
                        }
                    }
                    namedRanges.push_back(std::move(nrd));
                }
            }
        }
    }

    TabModel tab;
    tab.tabId = std::string(tabId);
    tab.title = std::string(title);
    tab.parentTabId = parentTabId;
    tab.isNew = false;
    tab.blocks = std::move(blocks);
    tab.leadingSectionStyle = std::move(leadingSectionStyle);
    tab.lists = std::move(lists);
    tab.namedRanges = std::move(namedRanges);

    if (dt.contains("documentStyle")) tab.documentStyle = styleCanonical(dt["documentStyle"]);
    if (dt.contains("namedStyles")) tab.namedStyles = styleCanonical(dt["namedStyles"]);
    if (dt.contains("dropdownDefinitions") && dt["dropdownDefinitions"].holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : dt["dropdownDefinitions"].get<slimr::Json::object_t>()) tab.dropdownDefinitions[k] = v;
    }
    if (dt.contains("footnotes") && dt["footnotes"].holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : dt["footnotes"].get<slimr::Json::object_t>()) tab.footnotes[k] = v;
    }
    if (dt.contains("inlineObjects") && dt["inlineObjects"].holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : dt["inlineObjects"].get<slimr::Json::object_t>()) tab.inlineObjects[k] = v;
    }
    if (dt.contains("positionedObjects") && dt["positionedObjects"].holds<slimr::Json::object_t>()) {
        for (const auto& [k, v] : dt["positionedObjects"].get<slimr::Json::object_t>()) tab.positionedObjects[k] = v;
    }
    if (dt.contains("suggestedDocumentStyleChanges")) tab.suggestedDocumentStyleChanges = dt["suggestedDocumentStyleChanges"];
    if (dt.contains("suggestedNamedStylesChanges")) tab.suggestedNamedStylesChanges = dt["suggestedNamedStylesChanges"];

    return tab;
}

inline std::vector<TabModel> tabsFlattenDfs(const JsonObject& raw, KeyAllocator& keys) {
    if (!raw.contains("tabProperties") || !raw["tabProperties"].contains("tabId")) {
        throw ParseError("tab is missing tabProperties.tabId");
    }
    const auto& props = raw["tabProperties"];
    std::string tabId = props["tabId"].holds<std::string>() ? props["tabId"].get<std::string>() : "";
    std::string title = props.contains("title") && props["title"].holds<std::string>() ? props["title"].get<std::string>() : "";
    std::optional<std::string> parentTabId;
    if (props.contains("parentTabId") && props["parentTabId"].holds<std::string>()) {
        parentTabId = props["parentTabId"].get<std::string>();
    }

    JsonObject dt = raw.contains("documentTab") ? raw["documentTab"] : make_json_object();
    auto tab = tabParse(tabId, title, parentTabId, dt, keys.tabScope(tabId));
    if (props.contains("iconEmoji") && props["iconEmoji"].holds<std::string>()) {
        tab.iconEmoji = props["iconEmoji"].get<std::string>();
    }

    std::vector<TabModel> out;
    out.push_back(std::move(tab));

    if (raw.contains("childTabs") && raw["childTabs"].holds<slimr::Json::array_t>()) {
        for (const auto& child : raw["childTabs"].get<slimr::Json::array_t>()) {
            auto sub = tabsFlattenDfs(child, keys);
            for (auto& st : sub) out.push_back(std::move(st));
        }
    }
    return out;
}

/**
 * Parses a fetched document into a `DocModel`: every tab's content, styles, and lists, in depth-first order.
 */
inline DocModel docModelParse(const JsonObject& json, DocModelParseOptions opts) {
    const auto& raw = rawDocumentOf(json);
    std::vector<TabModel> tabs;
    if (raw.contains("tabs") && raw["tabs"].holds<slimr::Json::array_t>() && !raw["tabs"].get<slimr::Json::array_t>().empty()) {
        for (const auto& tabJson : raw["tabs"].get<slimr::Json::array_t>()) {
            auto sub = tabsFlattenDfs(tabJson, opts.keys);
            for (auto& t : sub) tabs.push_back(std::move(t));
        }
    } else {
        std::string title = raw.contains("title") && raw["title"].holds<std::string>() ? raw["title"].get<std::string>() : "";
        tabs.push_back(tabParse("t.0", title, std::nullopt, raw, opts.keys.tabScope("t.0")));
    }

    DocModel doc;
    doc.docId = std::move(opts.docId);
    doc.isNew = false;
    doc.title = raw.contains("title") && raw["title"].holds<std::string>() ? raw["title"].get<std::string>() : "";
    if (raw.contains("revisionId") && raw["revisionId"].holds<std::string>()) {
        doc.revisionId = raw["revisionId"].get<std::string>();
    }
    doc.suggestionsViewMode = opts.suggestionsViewMode;
    doc.tabs = std::move(tabs);

    listPresetsApply(doc);
    objectIdsAssign(doc);
    return doc;
}

} // namespace gdocsmith::model
