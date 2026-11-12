#pragma once

#include "emulator/emulate.hpp"
#include "model/api_facts.hpp"
#include "model/edit.hpp"
#include "model/edit_lists.hpp"
#include "model/edit_tables.hpp"
#include "model/equivalence.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/list_presets.hpp"
#include "model/lists.hpp"
#include "model/style_values.hpp"
#include "model/test_docs.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"
#include "reconcile/container.hpp"
#include "reconcile/context.hpp"
#include "reconcile/lists.hpp"
#include "reconcile/reconcile.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::reconcile::test {

using namespace model;

struct DifferentialResult {
    CompareResult compare;
    ReconcileContext ctx;
    DocModel final;
    JsonObject json;
    std::vector<JsonObject> requests;
};

inline std::vector<std::string> requestKinds(const std::vector<JsonObject>& reqs) {
    std::vector<std::string> res;
    for (const auto& r : reqs) {
        if (r.holds<slimr::Json::object_t>() && !r.get<slimr::Json::object_t>().empty()) {
            res.push_back(r.get<slimr::Json::object_t>().begin()->first);
        }
    }
    return res;
}

inline BlockSpec paraSpec(std::vector<ParagraphContentSpec> content, std::optional<JsonObject> style = std::nullopt) {
    ParagraphSpec p;
    p.content = std::move(content);
    p.style = std::move(style);
    return BlockSpec::make_paragraph(std::move(p));
}

inline BlockSpec itemSpec(std::string text, int nestingLevel = 0, std::string listId = "kix.p") {
    ParagraphSpec p;
    p.content = {std::move(text)};
    p.bullet = ParagraphSpec::Bullet{.listId = std::move(listId), .nestingLevel = nestingLevel};
    p.style = slimr::Json::object_t{
        {"indentFirstLine", dimensionFromPt(36 * (nestingLevel + 1) - 18)},
        {"indentStart", dimensionFromPt(36 * (nestingLevel + 1))}
    };
    return BlockSpec::make_paragraph(std::move(p));
}

inline BlockSpec headingSpec(std::string text, std::string id, std::string type = "HEADING_1") {
    ParagraphSpec p;
    p.content = {std::move(text)};
    p.headingId = std::move(id);
    p.style = slimr::Json::object_t{{"namedStyleType", std::move(type)}};
    return BlockSpec::make_paragraph(std::move(p));
}

inline BlockSpec cellBlock(std::string text) {
    return paraSpec(text.empty() ? std::vector<ParagraphContentSpec>{} : std::vector<ParagraphContentSpec>{text});
}

inline BlockSpec tableSpec(const std::vector<std::vector<std::vector<BlockSpec>>>& cells) {
    TableBlockSpec t;
    t.cells = cells;
    return BlockSpec::make_table(std::move(t));
}

inline JsonObject stylePatchApply(const JsonObject& style, const JsonObject& patch) {
    JsonObject next = style;
    if (!next.holds<slimr::Json::object_t>()) next = slimr::Json::object_t{};
    if (patch.holds<slimr::Json::object_t>()) {
        auto& nextObj = next.get<slimr::Json::object_t>();
        for (const auto& [key, value] : patch.get<slimr::Json::object_t>()) {
            if (value.is_null()) {
                nextObj.erase(key);
            } else {
                nextObj[key] = value;
            }
        }
    }
    return styleCanonical(next);
}

inline bool styleMatches(const JsonObject& style, const JsonObject& where) {
    if (!where.holds<slimr::Json::object_t>()) return true;
    for (const auto& [k, v] : where.get<slimr::Json::object_t>()) {
        if (!style.contains(k) || !styleEqual(style[k], v)) return false;
    }
    return true;
}

inline BulletPreset listDefaultPreset(std::string_view kind) {
    if (kind == "number") return "NUMBERED_DECIMAL_ALPHA_ROMAN";
    if (kind == "check") return "BULLET_CHECKBOX";
    return "BULLET_DISC_CIRCLE_SQUARE";
}

inline ParagraphBlock& paragraphRequire(TabModel& tab, std::string_view key) {
    auto found = blockFind(tab, key);
    if (!found || !found->block->is_paragraph()) {
        throw CoreError("internal", std::format("no paragraph with key {}", key));
    }
    return found->block->as_paragraph();
}

inline const ParagraphBlock& paragraphRequire(const TabModel& tab, std::string_view key) {
    for (const auto& b : tab.blocks) {
        if (b.key == key && b.is_paragraph()) return b.as_paragraph();
    }
    for (const auto& table : tablesDeep(tab.blocks)) {
        for (const auto& row : table.rows) {
            for (const auto& cell : row.cells) {
                for (const auto& b : cell.blocks) {
                    if (b.key == key && b.is_paragraph()) return b.as_paragraph();
                }
            }
        }
    }
    throw CoreError("internal", std::format("no paragraph with key {}", key));
}

inline TableBlock& tableRequire(EditTarget& target, std::string_view key) {
    auto found = blockFind(target.tab, key);
    if (!found || !found->block->is_table()) {
        throw CoreError("internal", std::format("no table with key {}", key));
    }
    return found->block->as_table();
}

inline void textStyleUpdate(
    EditTarget& target,
    std::string_view key,
    int from,
    int to,
    const JsonObject& patch,
    const std::optional<JsonObject>& where = std::nullopt
) {
    auto& paragraph = paragraphRequire(target.tab, key);
    auto syms = paragraphSymbols(paragraph);
    if (from < 0 || to > static_cast<int>(syms.size()) || from > to) {
        throw CoreError("internal", std::format("style range [{}, {}) is outside the paragraph ({} symbols)", from, to, syms.size()));
    }
    bool changedLast = !syms.empty() && to == static_cast<int>(syms.size()) && from < to;
    for (int i = from; i < to; ++i) {
        auto& sym = syms[i];
        if (where && !styleMatches(sym.style, *where)) {
            if (i == static_cast<int>(syms.size()) - 1) changedLast = false;
            continue;
        }
        sym.style = stylePatchApply(sym.style, patch);
    }
    paragraphSymbolsSet(paragraph, syms);
    if (changedLast) {
        JsonObject newlinePatch = patch;
        if (newlinePatch.holds<slimr::Json::object_t>()) {
            newlinePatch.get<slimr::Json::object_t>().erase("link");
        }
        paragraph.newline.style = stylePatchApply(paragraph.newline.style, newlinePatch);
    }
    paragraph.stamp = target.ctx.stamp;
}

inline void paragraphStyleUpdate(
    EditTarget& target,
    std::string_view key,
    const JsonObject& patch
) {
    if (patch.contains("headingId")) throw CoreError("internal", "headingId is read-only");
    auto& paragraph = paragraphRequire(target.tab, key);
    std::string wasType = paragraph.style.contains("namedStyleType") && paragraph.style["namedStyleType"].is_string()
        ? paragraph.style["namedStyleType"].get<std::string>() : "NORMAL_TEXT";
    bool wasHeading = headingStyleIs(wasType);
    paragraph.style = stylePatchApply(paragraph.style, patch);
    std::string isType = paragraph.style.contains("namedStyleType") && paragraph.style["namedStyleType"].is_string()
        ? paragraph.style["namedStyleType"].get<std::string>() : "NORMAL_TEXT";
    bool isHeading = headingStyleIs(isType);
    if (isHeading) {
        paragraph.bullet = std::nullopt;
        if (!wasHeading) paragraph.headingId = std::nullopt;
    } else {
        paragraph.headingId = std::nullopt;
    }
    paragraph.stamp = target.ctx.stamp;
}

inline ListKind itemKind(const TabModel& tab, const ParagraphBlock& item) {
    if (!item.bullet) return "bullet";
    auto it = tab.lists.find(item.bullet->listId);
    if (it != tab.lists.end()) {
        return listKind(it->second, item.bullet->nestingLevel);
    }
    return "bullet";
}

inline std::string listCreate(EditTarget& target, BulletPreset preset) {
    std::string listId = std::format("new:list:{}", target.ctx.keys.next("n"));
    ListDef def;
    def.isNew = true;
    auto presetList = listPresetTable().find(preset);
    if (presetList != listPresetTable().end()) {
        def.nestingLevels = presetList->second;
    }
    def.preset = preset;
    target.tab.lists[listId] = std::move(def);
    return listId;
}

inline void membershipSet(
    EditTarget& target,
    const std::vector<Block>& blocks,
    ParagraphBlock& paragraph,
    const std::string& listId,
    int nesting
) {
    auto it = target.tab.lists.find(listId);
    const ListDef* def = (it != target.tab.lists.end()) ? &it->second : nullptr;
    JsonObject level = (def && nesting < static_cast<int>(def->nestingLevels.size())) ? def->nestingLevels[nesting] : make_json_object();

    BulletRef br{.listId = listId, .nestingLevel = nesting};
    if (level.contains("textStyle") && level["textStyle"].is_object()) {
        br.textStyle = styleCanonical(level["textStyle"]);
    }
    paragraph.bullet = std::move(br);

    const ParagraphBlock* neighbor = nullptr;
    for (const auto& b : blocks) {
        if (b.key != paragraph.key && b.is_paragraph()) {
            const auto& p = b.as_paragraph();
            if (p.bullet && p.bullet->listId == listId && p.bullet->nestingLevel == nesting && p.style.contains("indentStart")) {
                neighbor = &p;
                break;
            }
        }
    }

    JsonObject style = paragraph.style;
    if (neighbor) {
        style["indentStart"] = neighbor->style["indentStart"];
        if (!neighbor->style.contains("indentFirstLine")) {
            if (style.holds<slimr::Json::object_t>()) style.get<slimr::Json::object_t>().erase("indentFirstLine");
        } else {
            style["indentFirstLine"] = neighbor->style["indentFirstLine"];
        }
    } else if (def) {
        auto indent = listLevelIndent(*def, nesting);
        style["indentStart"] = dimensionFromPt(indent.indentStart);
        style["indentFirstLine"] = dimensionFromPt(indent.indentFirstLine);
    }
    paragraph.style = style;
    paragraph.stamp = target.ctx.stamp;
}

struct RunGroup {
    std::vector<std::string> keys;
    ContainerRef ref;
};

inline std::vector<RunGroup> runsGroup(TabModel& tab, const std::vector<std::string>& keys) {
    struct Located {
        std::string container;
        BlockLocation found;
        std::string key;
    };
    std::vector<Located> located;
    for (const auto& key : keys) {
        auto found = blockFind(tab, key);
        if (!found) throw CoreError("internal", std::format("no block with key {}", key));
        std::string cName = found->container.is_body() ? "body" : found->container.cellKey;
        located.push_back(Located{.container = cName, .found = *found, .key = key});
    }
    std::sort(located.begin(), located.end(), [](const Located& a, const Located& b) {
        if (a.container == b.container) return a.found.index < b.found.index;
        return a.container < b.container;
    });

    struct InternalRun {
        std::vector<std::string> keys;
        int lastIndex = 0;
        std::string name;
        ContainerRef ref;
    };
    std::vector<InternalRun> runs;
    for (const auto& item : located) {
        if (!runs.empty() && runs.back().name == item.container && runs.back().lastIndex + 1 == item.found.index) {
            runs.back().keys.push_back(item.key);
            runs.back().lastIndex = item.found.index;
        } else {
            runs.push_back(InternalRun{
                .keys = {item.key},
                .lastIndex = item.found.index,
                .name = item.container,
                .ref = item.found.container
            });
        }
    }
    std::vector<RunGroup> res;
    for (auto& r : runs) {
        res.push_back(RunGroup{.keys = std::move(r.keys), .ref = std::move(r.ref)});
    }
    return res;
}

inline void bulletableAssert(const ParagraphBlock& paragraph) {
    std::string namedStyle = paragraph.style.contains("namedStyleType") && paragraph.style["namedStyleType"].is_string()
        ? paragraph.style["namedStyleType"].get<std::string>() : "NORMAL_TEXT";
    if (headingStyleIs(namedStyle)) {
        throw CoreError("headingBullet", "headings can't be list items; lists are NORMAL_TEXT paragraphs");
    }
    for (const auto& i : paragraph.inlines) {
        if (i.is_atom() && i.type == "pageBreak") {
            throw CoreError("invalidAtom", "a page break can't be a list item");
        }
    }
    if (paragraph.inlines.empty()) {
        throw CoreError("listItemEmpty", "an empty list item can swallow the next heading; give it visible text");
    }
    std::string text = paragraph.inlines[0].is_text() ? paragraph.inlines[0].text : "";
    if (text.starts_with("\t")) {
        throw CoreError("fakeBulletPrefix", "leading tabs would be read as nesting; indent the markdown list item instead");
    }
    static const std::regex fakeBullet(R"(^(?:[-*•]\s+|\d+[.)]\s+))");
    if (std::regex_search(text, fakeBullet)) {
        throw CoreError("fakeBulletPrefix", "text starts with a typed bullet or number; use a markdown list marker instead");
    }
}

inline std::string listRunsResolve(
    EditTarget& target,
    const ContainerRef& ref,
    const std::vector<std::string>& keys,
    const std::vector<BulletIntent>& intents
) {
    auto& blocks = containerOf(target.tab, ref);
    int first = -1;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].key == keys[0]) {
            first = static_cast<int>(i);
            break;
        }
    }
    ListKind kind = intents[0].kind;
    const ParagraphBlock* before = (first > 0 && blocks[first - 1].is_paragraph() && blocks[first - 1].as_paragraph().bullet)
        ? &blocks[first - 1].as_paragraph() : nullptr;
    int afterIdx = first + static_cast<int>(keys.size());
    const ParagraphBlock* after = (afterIdx < static_cast<int>(blocks.size()) && blocks[afterIdx].is_paragraph() && blocks[afterIdx].as_paragraph().bullet)
        ? &blocks[afterIdx].as_paragraph() : nullptr;

    std::optional<std::string> listId;
    if (before && itemKind(target.tab, *before) == kind) {
        auto it = target.tab.lists.find(before->bullet->listId);
        const ListDef* def = (it != target.tab.lists.end()) ? &it->second : nullptr;
        bool presetOk = !intents[0].preset || (def && def->preset == intents[0].preset);
        bool allSameNesting = std::all_of(intents.begin(), intents.end(), [&](const BulletIntent& i) {
            return i.nesting == before->bullet->nestingLevel;
        });
        if (presetOk && (allSameNesting || (def && def->preset.has_value()))) {
            listId = before->bullet->listId;
        }
    }
    if (!listId && after && itemKind(target.tab, *after) == kind) {
        auto it = target.tab.lists.find(after->bullet->listId);
        const ListDef* def = (it != target.tab.lists.end()) ? &it->second : nullptr;
        bool presetOk = !intents[0].preset || (def && def->preset == intents[0].preset);
        bool allSameNesting = std::all_of(intents.begin(), intents.end(), [&](const BulletIntent& i) {
            return i.nesting == after->bullet->nestingLevel;
        });
        if (presetOk && (allSameNesting || (def && def->preset.has_value()))) {
            listId = after->bullet->listId;
        }
    }
    if (!listId) {
        BulletPreset preset = intents[0].preset.value_or(listDefaultPreset(kind));
        listId = listCreate(target, preset);
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        auto& p = paragraphRequire(target.tab, keys[i]);
        membershipSet(target, blocks, p, *listId, intents[i].nesting);
    }
    return *listId;
}

inline void bulletsSet(
    EditTarget& target,
    const std::vector<std::string>& keys,
    const std::optional<BulletIntent>& spec
) {
    if (!spec.has_value()) {
        for (const auto& key : keys) {
            auto& paragraph = paragraphRequire(target.tab, key);
            if (!paragraph.bullet) continue;
            paragraph.bullet = std::nullopt;
            JsonObject style = paragraph.style;
            style["indentStart"] = dimensionFromPt(0);
            if (style.holds<slimr::Json::object_t>()) style.get<slimr::Json::object_t>().erase("indentFirstLine");
            paragraph.style = style;
            paragraph.stamp = target.ctx.stamp;
        }
        return;
    }
    std::vector<std::string> pending;
    for (const auto& key : keys) {
        const auto& paragraph = paragraphRequire(target.tab, key);
        bulletableAssert(paragraph);
        if (!paragraph.bullet) {
            pending.push_back(key);
            continue;
        }
        auto it = target.tab.lists.find(paragraph.bullet->listId);
        const ListDef* def = (it != target.tab.lists.end()) ? &it->second : nullptr;
        bool sameKind = itemKind(target.tab, paragraph) == spec->kind;
        if (!sameKind || (spec->preset && def && def->preset != spec->preset)) {
            pending.push_back(key);
        }
    }
    for (const auto& run : runsGroup(target.tab, pending)) {
        std::vector<BulletIntent> intents;
        for (const auto& key : run.keys) {
            const auto& p = paragraphRequire(target.tab, key);
            intents.push_back(BulletIntent{
                .kind = spec->kind,
                .nesting = p.bullet ? p.bullet->nestingLevel : 0,
                .preset = spec->preset
            });
        }
        listRunsResolve(target, run.ref, run.keys, intents);
    }
}

inline ListChangeResult nestingSet(
    EditTarget& target,
    const std::string& key,
    int level
) {
    if (level < 0 || level > 8) throw CoreError("internal", std::format("nesting level {} is outside 0–8", level));
    auto found = blockFind(target.tab, key);
    if (!found || !found->block->is_paragraph() || !found->block->as_paragraph().bullet) {
        throw CoreError("internal", std::format("no list item with key {}", key));
    }
    const auto& p = found->block->as_paragraph();
    auto it = target.tab.lists.find(p.bullet->listId);
    bool lossy = (it == target.tab.lists.end() || !it->second.preset.has_value());
    ListKind kind = itemKind(target.tab, p);
    listRunsResolve(target, found->container, {key}, {BulletIntent{.kind = kind, .nesting = level}});
    return ListChangeResult{.lossy = lossy};
}

inline ListChangeResult listKindSet(
    EditTarget& target,
    const std::vector<std::string>& keys,
    ListKind kind,
    std::optional<BulletPreset> preset = std::nullopt
) {
    bool lossy = false;
    for (const auto& key : keys) {
        const auto& p = paragraphRequire(target.tab, key);
        if (p.bullet) {
            auto it = target.tab.lists.find(p.bullet->listId);
            if (it == target.tab.lists.end() || !it->second.preset.has_value()) lossy = true;
        }
    }
    for (const auto& run : runsGroup(target.tab, keys)) {
        std::vector<BulletIntent> intents;
        for (const auto& key : run.keys) {
            const auto& p = paragraphRequire(target.tab, key);
            intents.push_back(BulletIntent{
                .kind = kind,
                .nesting = p.bullet ? p.bullet->nestingLevel : 0,
                .preset = preset
            });
        }
        listRunsResolve(target, run.ref, run.keys, intents);
    }
    return ListChangeResult{.lossy = lossy};
}

inline CellModel cellLike(EditTarget& target, const CellModel& ref) {
    auto p = paragraphEmptyCreate(target.ctx.keys, {}, target.ctx.stamp);
    if (!ref.blocks.empty() && ref.blocks[0].is_paragraph()) {
        p.style = ref.blocks[0].as_paragraph().style;
    }
    CellModel cell;
    cell.key = target.ctx.keys.next("n");
    cell.style = ref.style;
    cell.blocks.push_back(Block(std::move(p)));
    return cell;
}

inline void cellTombstonesAdd(EditTarget& target, const CellModel& cell) {
    for (const auto& b : cell.blocks) {
        tombstonesAdd(target, b);
    }
}

inline std::vector<std::string> rowsInsert(
    EditTarget& target,
    std::string_view tableKey,
    int at,
    int count
) {
    auto& table = tableRequire(target, tableKey);
    const RowModel* ref = (at > 0 && at - 1 < static_cast<int>(table.rows.size()))
        ? &table.rows[at - 1]
        : (at < static_cast<int>(table.rows.size()) ? &table.rows[at] : nullptr);
    if (!ref || at < 0 || at > static_cast<int>(table.rows.size())) {
        throw CoreError("internal", std::format("row index {} is out of range", at));
    }
    std::vector<std::string> keys;
    std::vector<RowModel> rows;
    for (int i = 0; i < count; ++i) {
        RowModel r;
        r.key = target.ctx.keys.next("n");
        r.style = ref->style;
        for (const auto& cell : ref->cells) {
            r.cells.push_back(cellLike(target, cell));
        }
        keys.push_back(r.key);
        rows.push_back(std::move(r));
    }
    table.rows.insert(table.rows.begin() + at, rows.begin(), rows.end());
    table.stamp = target.ctx.stamp;
    return keys;
}

inline void rowsDelete(
    EditTarget& target,
    std::string_view tableKey,
    const std::vector<int>& rows
) {
    auto& table = tableRequire(target, tableKey);
    std::set<int> doomed(rows.begin(), rows.end());
    if (doomed.size() >= table.rows.size()) {
        throw CoreError("internal", "deleting every row deletes the table; use blocksDelete");
    }
    for (int r : doomed) {
        if (r < 0 || r >= static_cast<int>(table.rows.size())) {
            throw CoreError("internal", std::format("row {} does not exist", r));
        }
        for (const auto& cell : table.rows[r].cells) cellTombstonesAdd(target, cell);
    }
    std::vector<RowModel> kept;
    for (size_t r = 0; r < table.rows.size(); ++r) {
        if (!doomed.contains(static_cast<int>(r))) kept.push_back(std::move(table.rows[r]));
    }
    table.rows = std::move(kept);
    table.stamp = target.ctx.stamp;
}

inline void columnsInsert(
    EditTarget& target,
    std::string_view tableKey,
    int at,
    int count
) {
    auto& table = tableRequire(target, tableKey);
    int refIndex = at > 0 ? at - 1 : 0;
    if (at < 0 || at > static_cast<int>(table.columns.size())) {
        throw CoreError("internal", std::format("column index {} is out of range", at));
    }
    JsonObject refProps = (refIndex < static_cast<int>(table.columns.size())) ? table.columns[refIndex].props : make_json_object();
    std::vector<ColumnModel> newCols;
    for (int i = 0; i < count; ++i) {
        newCols.push_back(ColumnModel{.key = target.ctx.keys.next("n"), .props = refProps});
    }
    table.columns.insert(table.columns.begin() + at, newCols.begin(), newCols.end());
    for (auto& row : table.rows) {
        const CellModel& ref = row.cells[refIndex];
        std::vector<CellModel> newCells;
        for (int i = 0; i < count; ++i) {
            newCells.push_back(cellLike(target, ref));
        }
        row.cells.insert(row.cells.begin() + at, newCells.begin(), newCells.end());
    }
    table.stamp = target.ctx.stamp;
}

inline void columnsDelete(
    EditTarget& target,
    std::string_view tableKey,
    const std::vector<int>& columns
) {
    auto& table = tableRequire(target, tableKey);
    std::set<int> doomed(columns.begin(), columns.end());
    if (doomed.size() >= table.columns.size()) {
        throw CoreError("internal", "deleting every column deletes the table; use blocksDelete");
    }
    for (auto& row : table.rows) {
        for (int c : doomed) {
            if (c >= 0 && c < static_cast<int>(row.cells.size())) {
                cellTombstonesAdd(target, row.cells[c]);
            }
        }
        std::vector<CellModel> kept;
        for (size_t c = 0; c < row.cells.size(); ++c) {
            if (!doomed.contains(static_cast<int>(c))) kept.push_back(std::move(row.cells[c]));
        }
        row.cells = std::move(kept);
    }
    std::vector<ColumnModel> keptCols;
    for (size_t c = 0; c < table.columns.size(); ++c) {
        if (!doomed.contains(static_cast<int>(c))) keptCols.push_back(std::move(table.columns[c]));
    }
    table.columns = std::move(keptCols);
    table.stamp = target.ctx.stamp;
}

inline void cellsMerge(
    EditTarget& target,
    std::string_view tableKey,
    const CellRange& range
) {
    auto& table = tableRequire(target, tableKey);
    int colSpan = std::max(1, range.columnSpan);
    int rowSpan = std::max(1, range.rowSpan);
    if (colSpan * rowSpan < 2) {
        throw CoreError("internal", "a merge needs at least two cells");
    }
    for (int r = range.row; r < range.row + rowSpan; ++r) {
        for (int c = range.column; c < range.column + colSpan; ++c) {
            if (r == range.row && c == range.column) continue;
            const auto& cell = table.rows[r].cells[c];
            bool blank = cell.blocks.size() == 1 && cell.blocks[0].is_paragraph() &&
                cell.blocks[0].as_paragraph().inlines.empty() && !cell.blocks[0].as_paragraph().bullet;
            if (!blank) {
                throw CoreError("mergeNonEmpty", "only the top-left cell of a merge may have content; empty the others first");
            }
        }
    }
    auto& head = table.rows[range.row].cells[range.column];
    head.style["columnSpan"] = static_cast<int64_t>(colSpan);
    head.style["rowSpan"] = static_cast<int64_t>(rowSpan);
    for (int r = range.row; r < range.row + rowSpan; ++r) {
        table.rows[r].style = mergedRowStyle(table.rows[r].style);
    }
    table.stamp = target.ctx.stamp;
}

inline std::string sectionBreakInsert(
    EditTarget& target,
    int index,
    const std::string& sectionType
) {
    SectionBreakBlock sb;
    sb.create = SectionBreakBlock::Create{.sectionType = sectionType};
    sb.key = target.ctx.keys.next("n");
    sb.kind = "sectionBreak";
    sb.sectionStyle = make_section_style_default();
    sb.sectionStyle["sectionType"] = sectionType;

    auto& blocks = target.tab.blocks;
    if (index < 0 || index > static_cast<int>(blocks.size())) {
        throw CoreError("internal", std::format("insert index {} is out of range", index));
    }
    blocks.insert(blocks.begin() + index, Block(std::move(sb)));
    containerRenormalize(target, ContainerRef{.kind = "body"});
    return blocks[index].key;
}

inline std::string pageBreakParagraphInsert(
    EditTarget& target,
    int index
) {
    if (index < 0 || index > static_cast<int>(target.tab.blocks.size())) {
        throw CoreError("internal", std::format("insert index {} is out of range", index));
    }
    auto paragraph = paragraphEmptyCreate(target.ctx.keys, make_json_object(), target.ctx.stamp);
    Atom atom;
    atom.key = target.ctx.keys.next("n");
    atom.type = "pageBreak";
    atom.create = AtomCreate{.type = "pageBreak"};
    paragraph.inlines = {Inline(std::move(atom))};
    target.tab.blocks.insert(target.tab.blocks.begin() + index, Block(std::move(paragraph)));
    containerRenormalize(target, ContainerRef{.kind = "body"});
    return target.tab.blocks[index].key;
}

inline DifferentialResult differentialRun(
    const JsonObject& docJson,
    std::function<void(EditTarget& target, const std::vector<std::string>& keys)> mutate
) {
    KeyAllocator keys;
    DocModel original = docModelParse(docJson, DocModelParseOptions{.docId = "d", .keys = keys});
    DocModel final = original;
    TabModel& tab = final.tabs[0];

    EditContext editCtx{
        .keys = keys,
        .stamp = EditStamp{.force = false, .tabIndex = 0},
        .tombstones = {}
    };
    EditTarget target{.ctx = editCtx, .tab = tab};

    std::vector<std::string> origKeys;
    for (const auto& b : tab.blocks) origKeys.push_back(b.key);
    mutate(target, origKeys);

    ReconcileContext ctx = reconcileContextCreate(tab.tabId);
    containerReconcile(original.tabs[0].blocks, tab.blocks, ctx);
    bulletsReconcile(tab, ctx);

    auto res = emulator::requestsEmulate(docJson, ctx.requests);
    DocModel actual = docModelParse(res.json, DocModelParseOptions{.docId = "d", .keys = KeyAllocator{}});

    std::set<std::string> listsRebuilt;
    for (const auto& r : ctx.listRebuilds) listsRebuilt.insert(r.listId);

    CompareResult compare = docModelsCompare(final, actual, CompareOptions{
        .identityTransfers = ctx.identityTransfers,
        .listsRebuilt = listsRebuilt
    });

    return DifferentialResult{
        .compare = std::move(compare),
        .ctx = std::move(ctx),
        .final = std::move(final),
        .json = std::move(res.json),
        .requests = ctx.requests
    };
}

} // namespace gdocsmith::reconcile::test
