#pragma once

#include "diff/myers.hpp"
#include "lens/tokens.hpp"
#include "lens/atom_put.hpp"
#include "model/edit.hpp"
#include "model/suggestions.hpp"
#include "model/symbols.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gdocsmith::engine {

namespace suggestion_rule_detail {

inline bool hits(const std::optional<std::vector<std::string>>& marks, const std::set<std::string>& ids) {
    return marks && std::any_of(marks->begin(), marks->end(), [&](const auto& id) { return ids.contains(id); });
}

inline bool hits(const model::SuggestionMarks& marks, const std::set<std::string>& ids) {
    const auto present = model::marksIds(marks);
    return std::any_of(present.begin(), present.end(), [&](const auto& id) { return ids.contains(id); });
}

inline void strip(model::NewlineMark& newline, const std::set<std::string>& ids) {
    model::SuggestionMarks marks{.suggestedDeletionIds = newline.suggestedDeletionIds,
        .suggestedInsertionIds = newline.suggestedInsertionIds,
        .suggestedTextStyleChanges = newline.suggestedTextStyleChanges};
    model::marksStrip(marks, ids);
    newline.suggestedDeletionIds = std::move(marks.suggestedDeletionIds);
    newline.suggestedInsertionIds = std::move(marks.suggestedInsertionIds);
    newline.suggestedTextStyleChanges = std::move(marks.suggestedTextStyleChanges);
}

inline bool paragraphLevelSuggested(const model::ParagraphBlock& paragraph) {
    if (!paragraph.suggestionsRaw) return false;
    if (!paragraph.suggestionsRaw->is_object()) return true;
    for (const auto& [field, value] : paragraph.suggestionsRaw->get<slimr::Json::object_t>()) {
        (void)field;
        if (value.is_object()) {
            if (!value.get<slimr::Json::object_t>().empty()) return true;
        } else if (value.is_array()) {
            if (!value.get<slimr::Json::array_t>().empty()) return true;
        } else if (!value.is_null()) return true;
    }
    return false;
}

inline void protectionRefresh(std::vector<model::Block>& blocks) {
    for (auto& block : blocks) {
        if (block.is_paragraph()) {
            auto& paragraph = block.as_paragraph();
            if (paragraph.suggestionsRaw && paragraph.suggestionsRaw->is_object() &&
                paragraph.suggestionsRaw->get<slimr::Json::object_t>().empty()) paragraph.suggestionsRaw.reset();
            bool protectedNow = paragraphLevelSuggested(paragraph) || !model::marksKey(paragraph.newline).empty();
            for (const auto& sym : model::paragraphSymbols(paragraph)) {
                protectedNow = protectedNow || !model::marksKey(model::symMarks(sym)).empty();
            }
            paragraph.is_protected = protectedNow;
        } else if (block.is_table()) {
            auto& table = block.as_table();
            for (auto& row : table.rows) for (auto& cell : row.cells) protectionRefresh(cell.blocks);
            model::TabModel only;
            only.blocks.push_back(model::Block(table));
            table.is_protected = !model::tabSuggestions(only).empty();
        }
    }
}

inline void tombstone(model::EditTarget target, const std::string& key, const std::string& kind) {
    target.ctx.tombstones.push_back({.key = key, .kind = kind, .stamp = target.ctx.stamp, .tabId = target.tab.tabId});
}

inline void rebuildRow(model::EditTarget target, model::TableBlock& table, size_t index,
                       const std::set<std::string>& ids) {
    const auto& row = table.rows[index];
    model::TabModel nested;
    nested.blocks.push_back(model::Block(table));
    auto& only = nested.blocks.front().as_table();
    only.rows = {row};
    for (const auto& [id, parts] : model::tabSuggestions(nested)) {
        (void)parts;
        if (!ids.contains(id)) throw model::CoreError("suggestionConflict",
            "Rejecting this table row also removes suggestion " + id + "; add reject to its block too.");
    }
    auto span = [](const model::JsonObject& style, const char* field) {
        return style.contains(field) && style[field].is_number() ? style[field].as<int>() : 1;
    };
    for (size_t r = 0; r <= index; ++r) {
        for (const auto& cell : table.rows[r].cells) {
            if ((r < index && span(cell.style, "rowSpan") > static_cast<int>(index - r)) ||
                (r == index && (span(cell.style, "rowSpan") > 1 || span(cell.style, "columnSpan") > 1 ||
                 cell.blocks.size() != 1 || !cell.blocks.front().is_paragraph()))) {
                throw model::CoreError("suggestionConflict", "Rejecting this row needs unmerged cells with one text paragraph.");
            }
        }
    }
    auto fresh = row;
    fresh.key = target.ctx.keys.next("n");
    fresh.origin.reset();
    fresh.suggestedDeletionIds.reset();
    fresh.suggestedInsertionIds.reset();
    fresh.suggestedStyleChanges.reset();
    for (auto& cell : fresh.cells) {
        cell.key = target.ctx.keys.next("n");
        cell.origin.reset();
        cell.suggestedDeletionIds.reset();
        cell.suggestedInsertionIds.reset();
        cell.suggestedStyleChanges.reset();
        cell.style["columnSpan"] = 1;
        cell.style["rowSpan"] = 1;
        auto& paragraph = cell.blocks.front().as_paragraph();
        for (const auto& item : paragraph.inlines) {
            if (item.is_atom()) throw model::CoreError("suggestionConflict", "Rejecting this row cannot recreate an object in its cell.");
        }
        auto syms = model::paragraphSymbols(paragraph);
        for (auto& sym : syms) {
            sym.suggestedDeletionIds.reset();
            sym.suggestedInsertionIds.reset();
            sym.suggestedTextStyleChanges.reset();
        }
        paragraph.key = target.ctx.keys.next("n");
        paragraph.origin.reset();
        paragraph.stamp = target.ctx.stamp;
        paragraph.suggestionsRaw.reset();
        paragraph.newline = model::NewlineMark{.style = paragraph.newline.style};
        model::paragraphSymbolsSet(paragraph, syms);
        cell.blocks.front().key = paragraph.key;
        cell.blocks.front().origin.reset();
    }
    tombstone(target, row.key, "row");
    table.rows[index] = std::move(fresh);
}

inline void rejectsInBlocks(model::EditTarget target, std::vector<model::Block>& blocks,
                            const std::set<std::string>& ids, const model::DocModel& document,
                            lens::AtomPutReport& report, std::map<std::string, std::string>& aliases) {
    for (auto& block : blocks) {
        if (!block.is_table()) continue;
        auto& table = block.as_table();
        for (size_t r = table.rows.size(); r > 0; --r) {
            auto& row = table.rows[r - 1];
            if (hits(row.suggestedInsertionIds, ids)) {
                tombstone(target, row.key, "row");
                table.rows.erase(table.rows.begin() + static_cast<std::ptrdiff_t>(r - 1));
            } else {
                bool styled = false;
                for (const auto& cell : row.cells) {
                    if (cell.suggestedStyleChanges && cell.suggestedStyleChanges->is_object()) {
                        for (const auto& [id, value] : cell.suggestedStyleChanges->get<slimr::Json::object_t>()) {
                            (void)value;
                            styled = styled || ids.contains(id);
                        }
                    }
                }
                if (hits(row.suggestedDeletionIds, ids) || styled) rebuildRow(target, table, r - 1, ids);
            }
        }
        for (auto& row : table.rows) for (auto& cell : row.cells) rejectsInBlocks(target, cell.blocks, ids, document, report, aliases);
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!blocks[i].is_paragraph()) continue;
        auto& paragraph = blocks[i].as_paragraph();
        auto syms = model::paragraphSymbols(paragraph);
        std::vector<model::Sym> kept;
        bool changed = false;
        for (auto sym : syms) {
            auto marks = model::symMarks(sym);
            if (hits(marks.suggestedInsertionIds, ids)) {
                if (sym.atom) tombstone(target, sym.atom->key, "atom");
                changed = true;
                continue;
            }
            if (hits(marks, ids)) {
                if (sym.atom) {
                    if (sym.atom->type != "image") throw model::CoreError("suggestionConflict",
                        "Rejecting this object would recreate an unsupported atom; handle it in Google Docs.");
                    try {
                        const auto original = *sym.atom;
                        sym.atom = lens::atomRecreate(target, original, document, report);
                        const auto oldId = lens::atomTokenProject(original, target.tab).attrs.get_value("id");
                        const auto newId = lens::atomTokenProject(*sym.atom, target.tab).attrs.get_value("id");
                        if (oldId && newId) aliases[*oldId] = *newId;
                        tombstone(target, original.key, "atom");
                    } catch (const model::CoreError& error) {
                        throw model::CoreError("suggestionConflict", std::string("This image suggestion cannot be recreated: ") + error.what());
                    }
                    kept.push_back(std::move(sym));
                    changed = true;
                    continue;
                }
                model::marksStrip(marks, ids);
                sym.suggestedDeletionIds = std::move(marks.suggestedDeletionIds);
                sym.suggestedInsertionIds = std::move(marks.suggestedInsertionIds);
                sym.suggestedTextStyleChanges = std::move(marks.suggestedTextStyleChanges);
                changed = true;
            }
            kept.push_back(std::move(sym));
        }
        if (changed) {
            model::paragraphSymbolsSet(paragraph, kept);
            paragraph.stamp = target.ctx.stamp;
        }
        if (hits(paragraph.newline.suggestedInsertionIds, ids)) {
            if (paragraph.inlines.empty()) {
                tombstone(target, paragraph.key, "paragraph");
                blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(i));
                --i;
                continue;
            }
            if (i + 1 >= blocks.size() || !blocks[i + 1].is_paragraph() || paragraphLevelSuggested(paragraph) ||
                paragraphLevelSuggested(blocks[i + 1].as_paragraph())) {
                throw model::CoreError("suggestionConflict", "Rejecting a suggested paragraph break needs plain paragraphs on both sides.");
            }
            const auto next = blocks[i + 1].as_paragraph();
            auto joined = model::paragraphSymbols(paragraph);
            auto tail = model::paragraphSymbols(next);
            joined.insert(joined.end(), tail.begin(), tail.end());
            model::paragraphSymbolsSet(paragraph, joined);
            paragraph.newline = next.newline;
            paragraph.stamp = target.ctx.stamp;
            tombstone(target, next.key, "paragraph");
            blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(i + 1));
            --i;
            continue;
        }
        bool styleHit = false;
        if (paragraph.suggestionsRaw && paragraph.suggestionsRaw->contains("suggestedParagraphStyleChanges")) {
            const auto& changes = (*paragraph.suggestionsRaw)["suggestedParagraphStyleChanges"];
            if (changes.is_object()) for (const auto& [id, value] : changes.get<slimr::Json::object_t>()) {
                (void)value;
                styleHit = styleHit || ids.contains(id);
            }
        }
        const auto newline = model::SuggestionMarks{.suggestedDeletionIds = paragraph.newline.suggestedDeletionIds,
            .suggestedInsertionIds = paragraph.newline.suggestedInsertionIds,
            .suggestedTextStyleChanges = paragraph.newline.suggestedTextStyleChanges};
        if (!styleHit && !hits(newline, ids)) continue;
        if (i + 1 >= blocks.size() || !blocks[i + 1].is_paragraph()) {
            throw model::CoreError("suggestionConflict", "Rejecting this paragraph suggestion needs a following plain paragraph.");
        }
        const auto& next = blocks[i + 1].as_paragraph();
        if (paragraph.bullet || next.bullet || next.headingId || paragraphLevelSuggested(next)) {
            throw model::CoreError("suggestionConflict", "Rejecting this paragraph suggestion needs a following non-list, non-heading plain paragraph.");
        }
        if (styleHit) {
            auto& raw = paragraph.suggestionsRaw->get<slimr::Json::object_t>();
            auto& changes = raw.at("suggestedParagraphStyleChanges").get<slimr::Json::object_t>();
            for (const auto& id : ids) changes.erase(id);
            if (changes.empty()) raw.erase("suggestedParagraphStyleChanges");
            if (raw.empty()) paragraph.suggestionsRaw.reset();
        }
        strip(paragraph.newline, ids);
        paragraph.stamp = target.ctx.stamp;
    }
}

} // namespace suggestion_rule_detail

inline std::vector<std::string> suggestionRejectsApply(model::EditTarget target,
                                                       const std::set<std::string>& requested,
                                                       const model::DocModel& document,
                                                       lens::AtomPutReport& report,
                                                       std::map<std::string, std::string>* atomAliases = nullptr) {
    std::set<std::string> ids;
    for (const auto& [id, parts] : model::tabSuggestions(target.tab)) {
        if (!requested.contains(id)) continue;
        for (const auto* unsupported : {"pageLayout", "namedStyle", "list", "rowStyle", "table"}) {
            if (parts.contains(unsupported)) throw model::CoreError("suggestionConflict",
                "Suggestion " + id + " is a " + unsupported + " change, which import cannot reject.");
        }
        if (parts.contains("cell") && !parts.contains("row")) {
            throw model::CoreError("suggestionConflict", "Suggestion " + id + " is a column suggestion, which import cannot reject.");
        }
        ids.insert(id);
    }
    std::map<std::string, std::string> aliases;
    suggestion_rule_detail::rejectsInBlocks(target, target.tab.blocks, ids, document, report, aliases);
    suggestion_rule_detail::protectionRefresh(target.tab.blocks);
    const auto remaining = model::tabSuggestions(target.tab);
    for (const auto& id : ids) if (remaining.contains(id)) {
        throw model::CoreError("suggestionConflict", "Suggestion " + id +
            " includes a shape import cannot reject; handle it in Google Docs.");
    }
    if (atomAliases) *atomAliases = std::move(aliases);
    return {ids.begin(), ids.end()};
}

namespace suggestion_rule_detail {

// JS /\s/ code points.
inline bool jsSpace(const std::string& ch) {
    static const std::set<std::string> spaces = {"\t", "\n", "\v", "\f", "\r", " ", "\u00A0", "\u1680",
        "\u2000", "\u2001", "\u2002", "\u2003", "\u2004", "\u2005", "\u2006", "\u2007", "\u2008", "\u2009",
        "\u200A", "\u2028", "\u2029", "\u202F", "\u205F", "\u3000", "\uFEFF"};
    return spaces.contains(ch);
}

struct Removed {
    std::vector<std::string> order;
    std::set<std::string> seen;
    void add(const std::string& id) { if (seen.insert(id).second) order.push_back(id); }
};

inline void paragraphRuleApply(const model::ParagraphBlock& original, model::ParagraphBlock& final, Removed& removed) {
    const auto oSyms = model::paragraphSymbols(original);
    auto fSyms = model::paragraphSymbols(final);
    std::vector<std::string> oIds, fIds;
    for (const auto& sym : oSyms) oIds.push_back(model::symbolId(sym));
    for (const auto& sym : fSyms) fIds.push_back(model::symbolId(sym));
    const auto hunks = diff::diffHunks(oIds, fIds);
    bool marked = false;
    auto mark = [&](int from, int to, const std::optional<model::SuggestionMarks>& marks) {
        if (!marks || model::marksKey(*marks).empty()) return;
        for (int i = from; i < to; ++i) {
            auto& sym = fSyms[i];
            if (!sym.is_char()) continue;
            auto copy = model::marksCopy(*marks);
            sym.suggestedDeletionIds = std::move(copy.suggestedDeletionIds);
            sym.suggestedInsertionIds = std::move(copy.suggestedInsertionIds);
            sym.suggestedTextStyleChanges = std::move(copy.suggestedTextStyleChanges);
            marked = true;
        }
    };
    auto removeIds = [&](const model::Sym& sym) { for (const auto& id : model::marksIds(model::symMarks(sym))) removed.add(id); };
    auto keyAt = [&](int i) { return model::marksKey(model::symMarks(oSyms[i])); };
    // Replacements separated only by kept whitespace are one edit: "Fast and" -> "Quick or" diffs as two
    // hunks around the space, and splitting it between a suggestion and plain text would be a guess.
    std::vector<std::vector<diff::Hunk>> groups;
    for (const auto& h : hunks) {
        bool joins = false;
        if (!groups.empty()) {
            const auto& last = groups.back().back();
            joins = last.aEnd > last.aStart && h.aEnd > h.aStart;
            for (int i = last.aEnd; joins && i < h.aStart; ++i) joins = oSyms[i].is_char() && jsSpace(oSyms[i].ch);
        }
        if (joins) groups.back().push_back(h);
        else groups.push_back({h});
    }
    for (const auto& group : groups) {
        const auto& first = group.front();
        const auto& last = group.back();
        for (const auto& h : group)
            for (int i = h.aStart; i < h.aEnd; ++i) if (model::symHidden(oSyms[i])) removeIds(oSyms[i]);
        if (first.aEnd == first.aStart) {
            mark(first.bStart, first.bEnd, model::joinMarks(oSyms, original.newline, first.aStart));
            continue;
        }
        if (std::all_of(group.begin(), group.end(), [](const auto& h) { return h.bEnd == h.bStart; })) continue;
        std::set<std::string> keys;
        for (int i = first.aStart; i < last.aEnd; ++i) keys.insert(keyAt(i));
        if (keys.size() == 1 && !keys.begin()->empty()) {
            const auto& only = *keys.begin();
            int from = first.aStart;
            while (from > 0 && keyAt(from - 1) == only) --from;
            int to = last.aEnd;
            while (to < static_cast<int>(oSyms.size()) && keyAt(to) == only) ++to;
            // A suggestion of two or more symbols has an interior the edit can carry; one symbol cannot.
            if (to - from >= 2) for (const auto& h : group) mark(h.bStart, h.bEnd, model::symMarks(oSyms[first.aStart]));
            else removeIds(oSyms[first.aStart]);
        } else if (std::any_of(keys.begin(), keys.end(), [](const auto& key) { return !key.empty(); })) {
            for (int i = first.aStart; i < last.aEnd; ++i) removeIds(oSyms[i]);
        }
    }
    if (marked) model::paragraphSymbolsSet(final, fSyms);
}

inline void paragraphsCollect(const std::vector<model::Block>& blocks, std::map<std::string, const model::ParagraphBlock*>& out,
                              Removed* order) {
    for (const auto& block : blocks) {
        if (block.is_paragraph()) {
            const auto& paragraph = block.as_paragraph();
            out.emplace(paragraph.key, &paragraph);
            if (order) {
                for (const auto& sym : model::paragraphSymbols(paragraph))
                    for (const auto& id : model::marksIds(model::symMarks(sym))) order->add(id);
                for (const auto& id : model::marksIds(model::SuggestionMarks{.suggestedDeletionIds = paragraph.newline.suggestedDeletionIds,
                         .suggestedInsertionIds = paragraph.newline.suggestedInsertionIds,
                         .suggestedTextStyleChanges = paragraph.newline.suggestedTextStyleChanges})) order->add(id);
            }
        } else if (block.is_table()) {
            for (const auto& row : block.as_table().rows)
                for (const auto& cell : row.cells) paragraphsCollect(cell.blocks, out, order);
        }
    }
}

inline void paragraphsRuleApply(std::vector<model::Block>& blocks, const std::map<std::string, const model::ParagraphBlock*>& before,
                                Removed& removed) {
    for (auto& block : blocks) {
        if (block.is_paragraph()) {
            auto& paragraph = block.as_paragraph();
            const auto found = before.find(paragraph.key);
            if (found != before.end()) paragraphRuleApply(*found->second, paragraph, removed);
        } else if (block.is_table()) {
            for (auto& row : block.as_table().rows)
                for (auto& cell : row.cells) paragraphsRuleApply(cell.blocks, before, removed);
        }
    }
}

} // namespace suggestion_rule_detail

/**
 * Classifies a written tab against `base` (after rejects, before the write): text inserted strictly
 * inside a suggestion, or replacing two-or-more-symbol suggestion text, takes its marks; any edit that
 * would remove a suggestion refuses with one aggregated suggestionConflict. Port of suggestionRuleApply.
 */
inline void suggestionRuleApply(const model::TabModel& base, model::TabModel& candidate) {
    std::map<std::string, const model::ParagraphBlock*> before;
    suggestion_rule_detail::Removed baseOrder;
    suggestion_rule_detail::paragraphsCollect(base.blocks, before, &baseOrder);
    suggestion_rule_detail::Removed removed;
    suggestion_rule_detail::paragraphsRuleApply(candidate.blocks, before, removed);
    const auto baseIds = model::tabSuggestions(base);
    std::vector<std::string> rest;
    for (const auto& [id, parts] : baseIds) {
        (void)parts;
        if (!baseOrder.seen.contains(id)) rest.push_back(id);
    }
    std::sort(rest.begin(), rest.end());
    for (const auto& id : rest) baseOrder.add(id);
    const auto after = model::tabSuggestions(candidate);
    for (const auto& id : baseOrder.order)
        if (baseIds.contains(id) && !after.contains(id)) removed.add(id);
    if (removed.order.empty()) return;
    std::string ids;
    for (const auto& id : removed.order) ids += (ids.empty() ? "" : ", ") + id;
    throw model::CoreError("suggestionConflict",
        "This edit removes pending suggestions " + ids + "; add reject to their suggestion blocks, then retry.");
}

} // namespace gdocsmith::engine
