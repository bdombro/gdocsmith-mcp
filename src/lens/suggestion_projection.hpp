#pragma once

#include "lens/project.hpp"
#include "lens/render.hpp"
#include "lens/suggestion_blocks.hpp"
#include "model/suggestions.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace gdocsmith::lens {
namespace suggestion_detail {

inline bool has(const std::optional<std::vector<std::string>>& ids, const std::string& id) {
    return ids && std::find(ids->begin(), ids->end(), id) != ids->end();
}

inline const model::JsonObject* change(const std::optional<model::JsonObject>& changes,
                                       const std::string& id) {
    return changes && changes->is_object() && changes->contains(id) ? &(*changes)[id] : nullptr;
}

inline model::JsonObject merge(model::JsonObject base, const model::JsonObject& patch) {
    if (!base.is_object()) base = model::make_json_object();
    if (patch.is_object()) {
        for (const auto& [field, value] : patch.get<slimr::Json::object_t>()) base[field] = value;
    }
    return base;
}

inline model::Sym marker(bool start) {
    model::Atom atom;
    atom.key = start ? "review-start" : "review-end";
    atom.type = start ? "gdocsmithReviewStart" : "gdocsmithReviewEnd";
    return model::Sym(std::move(atom));
}

inline bool marked(const model::Sym& sym, const std::string& id) {
    const auto marks = model::symMarks(sym);
    return has(marks.suggestedInsertionIds, id) || has(marks.suggestedDeletionIds, id) ||
           change(marks.suggestedTextStyleChanges, id);
}

inline void clearMarks(model::Sym& sym) {
    sym.suggestedInsertionIds.reset();
    sym.suggestedDeletionIds.reset();
    sym.suggestedTextStyleChanges.reset();
    if (sym.atom && sym.atom->raw && sym.atom->raw->is_object()) {
        for (auto& [field, payload] : sym.atom->raw->get<slimr::Json::object_t>()) {
            (void)field;
            if (!payload.is_object()) continue;
            auto& object = payload.get<slimr::Json::object_t>();
            object.erase("suggestedInsertionIds");
            object.erase("suggestedDeletionIds");
            object.erase("suggestedTextStyleChanges");
        }
    }
}

inline bool visible(const model::SuggestionMarks& marks, const std::string& id, bool after) {
    if (after && has(marks.suggestedDeletionIds, id)) return false;
    if (marks.suggestedInsertionIds) {
        for (const auto& inserted : *marks.suggestedInsertionIds) {
            if (!after || inserted != id) return false;
        }
    }
    return true;
}

inline std::string jsonContext(const model::JsonObject& value) {
    ProjSpan span;
    span.text = slimr::write_json(value);
    span.marks.code = true;
    return ":change[" + renderSpans({span}) + "]";
}

inline void paragraphApply(model::ParagraphBlock& paragraph, model::TabModel& context,
                           const std::string& id, bool after, std::vector<std::string>& details) {
    const auto old = model::paragraphSymbols(paragraph);
    int first = -1, last = -1;
    for (size_t i = 0; i < old.size(); ++i) {
        if (marked(old[i], id)) {
            if (first < 0) first = static_cast<int>(i);
            last = static_cast<int>(i);
        }
    }
    const model::JsonObject* paragraphChange = nullptr;
    const model::JsonObject* bulletChange = nullptr;
    if (paragraph.suggestionsRaw && paragraph.suggestionsRaw->is_object()) {
        const auto& raw = *paragraph.suggestionsRaw;
        if (raw.contains("suggestedParagraphStyleChanges") &&
            raw["suggestedParagraphStyleChanges"].contains(id)) {
            paragraphChange = &raw["suggestedParagraphStyleChanges"][id];
        }
        if (raw.contains("suggestedBulletChanges") && raw["suggestedBulletChanges"].contains(id)) {
            bulletChange = &raw["suggestedBulletChanges"][id];
        }
    }
    const bool newlineHit = has(paragraph.newline.suggestedInsertionIds, id) ||
                            has(paragraph.newline.suggestedDeletionIds, id) ||
                            change(paragraph.newline.suggestedTextStyleChanges, id);
    std::vector<model::Sym> result;
    if (first < 0 && (paragraphChange || bulletChange)) result.push_back(marker(true));
    for (size_t i = 0; i < old.size(); ++i) {
        if (static_cast<int>(i) == first) result.push_back(marker(true));
        auto sym = old[i];
        const auto marks = model::symMarks(sym);
        if (visible(marks, id, after)) {
            if (after) {
                if (const auto* proposed = change(marks.suggestedTextStyleChanges, id);
                    proposed && proposed->contains("textStyle")) {
                    if (sym.is_char()) sym.style = (*proposed)["textStyle"];
                    else if (sym.atom) sym.atom->style = (*proposed)["textStyle"];
                }
            }
            clearMarks(sym);
            result.push_back(std::move(sym));
        }
        if (static_cast<int>(i) == last) result.push_back(marker(false));
    }
    if (first < 0 && (paragraphChange || bulletChange)) result.push_back(marker(false));
    else if (first < 0 && newlineHit) {
        result.push_back(marker(true));
        result.push_back(marker(false));
    }
    model::paragraphSymbolsSet(paragraph, result);
    if (paragraphChange && paragraphChange->contains("paragraphStyle")) {
        const auto proposed = (*paragraphChange)["paragraphStyle"];
        details.push_back("Paragraph style: " + jsonContext(after ? proposed : paragraph.style));
        if (after) {
            paragraph.style = proposed;
            if (paragraph.style.is_object()) paragraph.style.get<slimr::Json::object_t>().erase("headingId");
        }
    }
    if (bulletChange && after) {
        if (bulletChange->contains("bullet")) {
            const auto& bullet = (*bulletChange)["bullet"];
            if (bullet.contains("listId") && bullet["listId"].is_string() &&
                context.lists.contains(bullet["listId"].get<std::string>())) {
                model::BulletRef value;
                value.listId = bullet["listId"].get<std::string>();
                if (bullet.contains("nestingLevel") && bullet["nestingLevel"].is_number()) {
                    value.nestingLevel = static_cast<int>(bullet["nestingLevel"].get<int64_t>());
                }
                paragraph.bullet = std::move(value);
            } else paragraph.bullet.reset();
        } else paragraph.bullet.reset();
    }
    if (newlineHit) {
        auto state = model::make_json_object();
        state["present"] = after ? !has(paragraph.newline.suggestedDeletionIds, id)
                                 : !has(paragraph.newline.suggestedInsertionIds, id);
        if (const auto* proposed = change(paragraph.newline.suggestedTextStyleChanges, id);
            after && proposed && proposed->contains("textStyle")) {
            state["style"] = (*proposed)["textStyle"];
        } else state["style"] = paragraph.newline.style;
        details.push_back("Paragraph break: " + jsonContext(state));
    }
    paragraph.suggestionsRaw.reset();
    paragraph.newline.suggestedInsertionIds.reset();
    paragraph.newline.suggestedDeletionIds.reset();
    paragraph.newline.suggestedTextStyleChanges.reset();
}

inline void markCell(model::CellModel& cell) {
    for (auto& block : cell.blocks) {
        if (!block.is_paragraph()) continue;
        auto syms = model::paragraphSymbols(block.as_paragraph());
        const bool marked = std::any_of(syms.begin(), syms.end(), [](const auto& sym) {
            return sym.atom && (sym.atom->type == "gdocsmithReviewStart" ||
                                sym.atom->type == "gdocsmithReviewEnd");
        });
        if (!marked) {
            syms.insert(syms.begin(), marker(true));
            syms.push_back(marker(false));
            model::paragraphSymbolsSet(block.as_paragraph(), syms);
        }
        break;
    }
}

inline void blocksApply(std::vector<model::Block>& blocks, model::TabModel& context,
                        const std::string& id, bool after, std::vector<std::string>& details) {
    for (auto& block : blocks) {
        if (block.is_paragraph()) {
            paragraphApply(block.as_paragraph(), context, id, after, details);
        } else if (block.is_table()) {
            auto& table = block.as_table();
            const bool tableHit = has(table.suggestedInsertionIds, id) || has(table.suggestedDeletionIds, id);
            const bool tableVisible = visible({.suggestedDeletionIds = table.suggestedDeletionIds,
                                               .suggestedInsertionIds = table.suggestedInsertionIds}, id, after);
            std::vector<model::RowModel> rows;
            for (auto row : table.rows) {
                const bool rowHit = has(row.suggestedInsertionIds, id) || has(row.suggestedDeletionIds, id);
                const bool rowVisible = visible({.suggestedDeletionIds = row.suggestedDeletionIds,
                                                 .suggestedInsertionIds = row.suggestedInsertionIds}, id, after);
                if (!rowVisible) {
                    if (rowHit) details.push_back("Table row: :change[]");
                    continue;
                }
                if (const auto* proposed = change(row.suggestedStyleChanges, id);
                    proposed && proposed->contains("tableRowStyle")) {
                    const auto style = merge(row.style, (*proposed)["tableRowStyle"]);
                    details.push_back("Row style: " + jsonContext(after ? style : row.style));
                    if (after) row.style = style;
                }
                for (auto& cell : row.cells) {
                    const bool cellHit = has(cell.suggestedInsertionIds, id) || has(cell.suggestedDeletionIds, id);
                    const bool cellVisible = visible({.suggestedDeletionIds = cell.suggestedDeletionIds,
                                                      .suggestedInsertionIds = cell.suggestedInsertionIds}, id, after);
                    if (!cellVisible) cell.blocks.clear();
                    else blocksApply(cell.blocks, context, id, after, details);
                    if (cell.blocks.empty()) {
                        model::ParagraphBlock empty;
                        empty.key = cell.key + ":review";
                        cell.blocks.emplace_back(empty);
                    }
                    if (const auto* proposed = change(cell.suggestedStyleChanges, id);
                        proposed && proposed->contains("tableCellStyle")) {
                        const auto style = merge(cell.style, (*proposed)["tableCellStyle"]);
                        details.push_back("Cell style: " + jsonContext(after ? style : cell.style));
                        if (after) cell.style = style;
                        markCell(cell);
                    }
                    if (rowHit || tableHit || cellHit) markCell(cell);
                    cell.suggestedInsertionIds.reset();
                    cell.suggestedDeletionIds.reset();
                    cell.suggestedStyleChanges.reset();
                }
                row.suggestedInsertionIds.reset();
                row.suggestedDeletionIds.reset();
                row.suggestedStyleChanges.reset();
                rows.push_back(std::move(row));
            }
            if (!tableVisible || rows.empty()) {
                model::ParagraphBlock empty;
                empty.key = table.key + ":review";
                model::paragraphSymbolsSet(empty, {marker(true), marker(false)});
                block = model::Block(std::move(empty));
            } else {
                table.rows = std::move(rows);
                table.suggestedInsertionIds.reset();
                table.suggestedDeletionIds.reset();
            }
        }
    }
}

inline void markerProjection(std::vector<ProjBlock>& blocks) {
    auto spans = [](std::vector<ProjSpan>& items) {
        for (auto& span : items) {
            if (!span.token) continue;
            if (span.token->kind == "gdocsmithReviewStart") span.kind = "reviewStart";
            else if (span.token->kind == "gdocsmithReviewEnd") span.kind = "reviewEnd";
        }
    };
    for (auto& block : blocks) {
        spans(block.spans);
        if (!block.table) continue;
        for (auto& row : block.table->rows) for (auto& cell : row) spans(cell);
        for (auto& row : block.table->cells) for (auto& cell : row) markerProjection(cell);
    }
}

inline std::string snippet(const model::TabModel& source, const std::string& id, bool after) {
    model::TabModel context = source;
    context.blocks.clear();
    context.segments.clear();
    context.namedRanges.clear();
    auto carries = [&](const model::Block& block) {
        std::set<std::string> found;
        model::blocksSuggestions({block}, [&](const auto& ids, std::string_view) {
            if (ids) found.insert(ids->begin(), ids->end());
        });
        return found.contains(id);
    };
    size_t first = source.blocks.size(), last = 0;
    for (size_t i = 0; i < source.blocks.size(); ++i) {
        if (carries(source.blocks[i])) {
            first = std::min(first, i);
            last = i;
        }
    }
    if (first == source.blocks.size()) return {};
    context.blocks.assign(source.blocks.begin() + static_cast<std::ptrdiff_t>(first),
                          source.blocks.begin() + static_cast<std::ptrdiff_t>(last + 1));
    std::vector<std::string> details;
    blocksApply(context.blocks, context, id, after, details);
    auto projection = projectionBuild(context);
    markerProjection(projection.blocks);
    auto result = markdownRender(projection);
    for (const auto& detail : details) result += detail + "\n\n";
    return suggestionSnippetTrim(result);
}

inline std::string kind(const std::set<model::SuggestionPart>& parts) {
    for (const auto& [part, name] : {
            std::pair{"table", "table"}, {"row", "table"}, {"cell", "table"}, {"rowStyle", "table"},
            {"cellStyle", "cellStyle"}, {"list", "list"}, {"paragraphStyle", "paragraphStyle"},
            {"object", "object"}, {"insertion", "text"}, {"deletion", "text"},
            {"newlineInsertion", "text"}, {"newlineDeletion", "text"}}) {
        if (parts.contains(part)) return name;
    }
    return "textStyle";
}

} // namespace suggestion_detail

/** Read-only, target-specific before/after context. No model mutation or review resolution. */
inline std::vector<SuggestionBlock> suggestionBlocksBuild(const model::DocModel& doc) {
    std::vector<SuggestionBlock> result;
    for (size_t index = 0; index < doc.tabs.size(); ++index) {
        const auto& tab = doc.tabs[index];
        auto add = [&](const model::TabModel& source, std::optional<std::string> segment) {
            std::map<std::string, std::set<model::SuggestionPart>> suggestions;
            for (const auto& [id, parts] : model::tabSuggestions(source)) suggestions.emplace(id, parts);
            for (const auto& [id, parts] : suggestions) {
                SuggestionBlock block;
                block.id = id;
                block.tabIndex = static_cast<int>(index);
                block.segment = segment;
                block.kind = suggestion_detail::kind(parts);
                if (const auto* change = suggestion_detail::change(source.suggestedDocumentStyleChanges, id);
                    change && change->contains("documentStyle")) {
                    block.kind = "pageLayout";
                    block.before = suggestion_detail::jsonContext(source.documentStyle);
                    block.after = suggestion_detail::jsonContext(
                        suggestion_detail::merge(source.documentStyle, (*change)["documentStyle"]));
                } else if (const auto* change = suggestion_detail::change(source.suggestedNamedStylesChanges, id);
                           change && change->contains("namedStyles")) {
                    block.kind = "namedStyle";
                    block.before = suggestion_detail::jsonContext(source.namedStyles);
                    block.after = suggestion_detail::jsonContext(
                        suggestion_detail::merge(source.namedStyles, (*change)["namedStyles"]));
                } else {
                    block.before = suggestion_detail::snippet(source, id, false);
                    block.after = suggestion_detail::snippet(source, id, true);
                }
                result.push_back(std::move(block));
            }
        };
        add(tab, std::nullopt);
        for (const auto& segment : tab.segments) {
            if (segment.tab) add(*segment.tab, segment.kind + " " + segment.type);
        }
    }
    return result;
}

} // namespace gdocsmith::lens
