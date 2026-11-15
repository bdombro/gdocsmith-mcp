#pragma once

#include <string>
#include <vector>
#include <map>
#include "json.hpp"
#include "lens/json_helper.hpp"
#include "tokens.hpp"
#include "directives.hpp"
#include "frontmatter.hpp"
#include "paragraph_styles.hpp"
#include "cell_styles.hpp"
#include "model/lists.hpp"
#include "model/object_ids.hpp"
#include "text_styles.hpp"
#include "named_ranges.hpp"
#include "sections.hpp"
#include "model/api_facts.hpp"
#include "model/effective_style.hpp"
#include "lens/atom_put.hpp"
#include "lens/review_projection.hpp"
#include "lens/links.hpp"

namespace gdocsmith::lens {

inline Projection projectionBuild(const model::TabModel& tab, const model::DocModel* document = nullptr);

inline std::optional<std::string> projectionLink(const model::JsonObject& style, const model::TabModel& tab,
                                                const model::DocModel* document) {
    if (!style.contains("link") || !style["link"].is_object()) return std::nullopt;
    const auto& link = style["link"];
    if (link.contains("url") && link["url"].is_string()) {
        const auto url = link["url"].get<std::string>();
        if (document) {
            LinkTarget target;
            target.kind = "url";
            target.url = url;
            if (const auto href = documentLinkHref(target, *document, tab)) return href;
        }
        return url;
    }
    LinkTarget target;
    target.kind = "heading";
    if (link.contains("tabId") && link["tabId"].is_string()) target.tabId = link["tabId"].get<std::string>();
    if (link.contains("headingId") && link["headingId"].is_string()) target.headingId = link["headingId"].get<std::string>();
    if (link.contains("heading") && link["heading"].is_object()) {
        const auto& heading = link["heading"];
        if (heading.contains("key") && heading["key"].is_string()) target.key = heading["key"].get<std::string>();
        if (heading.contains("tabId") && heading["tabId"].is_string()) target.tabId = heading["tabId"].get<std::string>();
    }
    if (target.key && target.key->starts_with("file-link:")) target.kind = "pending";
    if (!target.key && !target.headingId) {
        if (!target.tabId) return std::nullopt;
        target.kind = "tab";
    }
    if (target.kind == "pending") return target.key->substr(10);
    const auto targetExists = [&](const model::TabModel& candidate) {
        if (target.kind == "tab") return true;
        return std::any_of(candidate.blocks.begin(), candidate.blocks.end(), [&](const auto& block) {
            return block.is_paragraph() && ((target.headingId && block.as_paragraph().headingId == target.headingId) ||
                                           (target.key && block.key == *target.key));
        });
    };
    if (document) {
        const auto candidate = std::find_if(document->tabs.begin(), document->tabs.end(), [&](const auto& item) {
            return item.tabId == target.tabId.value_or(tab.tabId);
        });
        if (candidate == document->tabs.end() || !targetExists(*candidate)) return std::nullopt;
        return documentLinkHref(target, *document, tab);
    }
    if (target.tabId && *target.tabId != tab.tabId) return std::nullopt;
    if (!targetExists(tab)) return std::nullopt;
    model::DocModel local;
    local.tabs.push_back(tab);
    return documentLinkHref(target, local, tab);
}

inline ParagraphDeclaration projectionParagraphStyle(const model::ParagraphBlock& paragraph, const model::TabModel& tab) {
    auto inherited = model::make_table_cell_paragraph_style_default();
    const auto kind = paragraph.style.contains("namedStyleType") ? paragraph.style["namedStyleType"] : slimr::Json("NORMAL_TEXT");
    if (tab.namedStyles.contains("styles") && tab.namedStyles["styles"].is_array()) {
        for (const auto& entry : tab.namedStyles["styles"].get<slimr::Json::array_t>()) {
            if (entry.contains("namedStyleType") && entry["namedStyleType"] == kind &&
                entry.contains("paragraphStyle") && entry["paragraphStyle"].is_object()) {
                for (const auto& [key, value] : entry["paragraphStyle"].get<slimr::Json::object_t>()) inherited[key] = value;
            }
        }
    }
    auto result = paragraphDeclarationExport(paragraph.style, paragraph.bullet.has_value(), inherited);
    const auto defaults = paragraphDeclarationExport(inherited, paragraph.bullet.has_value(), inherited);
    for (auto member : {&ParagraphDeclaration::alignment, &ParagraphDeclaration::direction,
                       &ParagraphDeclaration::namedStyle, &ParagraphDeclaration::shading, &ParagraphDeclaration::spacingMode}) {
        if (result.*member == defaults.*member) (result.*member).reset();
    }
    for (auto member : {&ParagraphDeclaration::indentEnd, &ParagraphDeclaration::indentFirstLine,
                       &ParagraphDeclaration::indentStart, &ParagraphDeclaration::lineSpacing,
                       &ParagraphDeclaration::spaceAbove, &ParagraphDeclaration::spaceBelow}) {
        if (result.*member == defaults.*member) (result.*member).reset();
    }
    return result;
}

inline std::string projectionStyleName(char prefix, std::string_view key) {
    std::string result(1, prefix);
    constexpr std::string_view hex = "0123456789abcdef";
    for (const unsigned char character : key) {
        result += hex[character >> 4];
        result += hex[character & 15];
    }
    return result;
}

inline std::optional<std::string> projectionFootnoteId(const model::Inline& item) {
    if (item.type != "footnoteRef" || !item.raw || !item.raw->contains("footnoteReference")) return std::nullopt;
    const auto& reference = (*item.raw)["footnoteReference"];
    if (!reference.contains("footnoteId") || !reference["footnoteId"].is_string()) return std::nullopt;
    return reference["footnoteId"].get<std::string>();
}

inline std::map<std::string, std::string> projectionFootnotes(const model::TabModel& tab) {
    std::map<std::string, std::string> labels;
    for (const auto& paragraph : model::paragraphsDeep(tab.blocks)) {
        for (const auto& item : paragraph.inlines) {
            const auto id = projectionFootnoteId(item);
            if (id && !labels.contains(*id)) labels[*id] = std::to_string(labels.size() + 1);
        }
    }
    for (const auto& segment : tab.segments) {
        if (segment.kind == "footnote" && !labels.contains(segment.id)) {
            labels[segment.id] = std::to_string(labels.size() + 1);
        }
    }
    return labels;
}

inline std::optional<int> paragraphRangeOffset(const model::ParagraphBlock& paragraph, int index) {
    if (!paragraph.origin || index < paragraph.origin->start) return std::nullopt;
    int current = paragraph.origin->start;
    const auto symbols = model::paragraphSymbols(paragraph);
    for (size_t i = 0; i <= symbols.size(); ++i) {
        if (current == index) return static_cast<int>(i);
        if (i < symbols.size()) {
            if (model::symHidden(symbols[i])) return std::nullopt;
            current += symbols[i].is_char() ? model::codePointUtf16Units(symbols[i].ch) : symbols[i].atom->length;
        }
    }
    return std::nullopt;
}

/** Only ranges with both edges representable in body text are shown/managed. */
inline std::vector<ShownRange> projectionShownRanges(const model::TabModel& tab) {
    if (tab.namedRanges.empty()) return {};
    auto shown = namedRangesShown(tab);
    const auto paragraphs = model::paragraphsDeep(tab.blocks);
    std::erase_if(shown, [&](const auto& entry) {
        const auto& range = entry.range.ranges[entry.segment];
        bool start = false, end = false;
        for (const auto& paragraph : paragraphs) {
            start |= paragraphRangeOffset(paragraph, range.start).has_value();
            end |= paragraphRangeOffset(paragraph, range.end).has_value();
        }
        return !start || !end;
    });
    return shown;
}

inline void projectionRangeMarkers(ProjBlock& projected, const model::ParagraphBlock& paragraph,
                                   const model::TabModel& tab) {
    if (tab.namedRanges.empty()) return;
    std::map<int, std::vector<TokenRef>> markers;
    for (const auto& entry : projectionShownRanges(tab)) {
        const auto& range = entry.range.ranges[entry.segment];
        for (const auto& [edge, index] : {std::pair{"start", range.start}, {"end", range.end}}) {
            const auto offset = paragraphRangeOffset(paragraph, index);
            if (!offset) continue;
            TokenRef token;
            token.kind = "namedrange";
            token.attrs.values.emplace_back("name", entry.range.name);
            token.attrs.flags.push_back(edge);
            markers[*offset].push_back(std::move(token));
        }
    }
    if (markers.empty()) return;
    std::vector<ProjSpan> spans;
    int offset = 0;
    auto emit = [&] {
        for (const auto& token : markers[offset]) {
            ProjSpan marker;
            marker.kind = "token";
            marker.token = token;
            spans.push_back(std::move(marker));
        }
    };
    for (const auto& span : projected.spans) {
        if (span.kind == "text") {
            for (const auto& cp : model::utf8CodePoints(span.text)) {
                emit();
                auto character = span;
                character.text = cp;
                if (!spans.empty() && spans.back().kind == "text" &&
                    spans.back().marks == character.marks && spans.back().formatting == character.formatting &&
                    spans.back().directive == character.directive) spans.back().text += cp;
                else spans.push_back(std::move(character));
                ++offset;
            }
        } else {
            emit();
            spans.push_back(span);
            ++offset;
        }
    }
    emit();
    projected.spans = std::move(spans);
}

inline void projectionReviewMarkers(ProjBlock& projected, const model::ParagraphBlock& paragraph) {
    std::map<std::string, std::pair<int, int>> ranges;
    int visibleOffset = 0;
    for (const auto& sym : model::paragraphSymbols(paragraph)) {
        const auto ids = model::marksIds(model::symMarks(sym));
        for (const auto& id : ids) {
            if (!ranges.contains(id)) ranges[id] = {visibleOffset, visibleOffset};
        }
        if (!model::symHidden(sym)) ++visibleOffset;
        for (const auto& id : ids) ranges[id].second = visibleOffset;
    }
    if (paragraph.suggestionsRaw && paragraph.suggestionsRaw->is_object()) {
        for (const auto& [field, changes] : paragraph.suggestionsRaw->get<slimr::Json::object_t>()) {
            (void)field;
            if (!changes.is_object()) continue;
            for (const auto& [id, value] : changes.get<slimr::Json::object_t>()) {
                (void)value;
                if (!ranges.contains(id)) ranges[id] = {0, visibleOffset};
            }
        }
    }
    const auto newlineIds = model::marksIds({
        .suggestedDeletionIds = paragraph.newline.suggestedDeletionIds,
        .suggestedInsertionIds = paragraph.newline.suggestedInsertionIds,
        .suggestedTextStyleChanges = paragraph.newline.suggestedTextStyleChanges});
    for (const auto& id : newlineIds) if (!ranges.contains(id)) ranges[id] = {visibleOffset, visibleOffset};
    if (ranges.empty()) return;
    std::map<int, std::vector<TokenRef>> markers;
    for (const auto& [id, edges] : ranges) {
        for (const auto& [edge, offset] : {std::pair{"start", edges.first}, {"end", edges.second}}) {
            TokenRef token;
            token.kind = "suggestion";
            token.attrs.values.emplace_back("id", id);
            token.attrs.flags.push_back(edge);
            markers[offset].push_back(std::move(token));
        }
    }
    std::vector<ProjSpan> spans;
    int offset = 0;
    auto emit = [&] {
        for (const auto& token : markers[offset]) {
            ProjSpan marker;
            marker.kind = "token";
            marker.token = token;
            spans.push_back(std::move(marker));
        }
        markers.erase(offset);
    };
    for (const auto& span : projected.spans) {
        if (span.kind == "text") {
            for (const auto& cp : model::utf8CodePoints(span.text)) {
                emit();
                auto character = span;
                character.text = cp;
                if (!spans.empty() && spans.back().kind == "text" &&
                    spans.back().marks == character.marks && spans.back().formatting == character.formatting &&
                    spans.back().directive == character.directive) spans.back().text += cp;
                else spans.push_back(std::move(character));
                ++offset;
            }
        } else {
            emit();
            spans.push_back(span);
            if (!span.token || span.token->kind != "namedrange") ++offset;
        }
    }
    emit();
    projected.spans = std::move(spans);
}

inline Projection projectionBuild(const std::vector<model::Block>& blocks, const model::TabModel& tab,
                                  const model::DocModel* document = nullptr) {
    Projection projection;
    for (const auto& block : blocks) {
        ProjBlock projected;
        if (block.is_paragraph()) {
            const auto& paragraph = block.as_paragraph();
            const auto declaration = projectionParagraphStyle(paragraph, tab);
            if (declaration != ParagraphDeclaration{}) {
                projected.paragraphStyle = projectionStyleName('p', paragraph.key);
                projection.paragraphStyles[*projected.paragraphStyle] = declaration;
            }
            if (paragraph.style.contains("namedStyleType") && paragraph.style["namedStyleType"].is_string()) {
                const auto& style = paragraph.style["namedStyleType"].get<std::string>();
                if (style.size() == 9 && style.starts_with("HEADING_") && style.back() >= '1' && style.back() <= '6') {
                    projected.kind = "heading";
                    projected.headingLevel = style.back() - '0';
                }
            }
            if (paragraph.bullet) {
                projected.kind = "listItem";
                projected.listDepth = paragraph.bullet->nestingLevel;
                const auto definition = tab.lists.find(paragraph.bullet->listId);
                projected.listKind = definition != tab.lists.end() &&
                                     model::listKind(definition->second, projected.listDepth) == "number"
                    ? "numbered" : "bullet";
                if (definition != tab.lists.end() && definition->second.preset &&
                    *definition->second.preset != "BULLET_CHECKBOX" &&
                    *definition->second.preset != model::defaultPresetForKind(
                        projected.listKind == "numbered" ? "number" : "bullet")) {
                    projected.listPreset = definition->second.preset;
                }
            }
            const auto baseAttrs = textStyleExport(model::textStyleEffective(tab, paragraph, model::make_json_object()));
            for (const auto& inlineItem : paragraph.inlines) {
                if ((inlineItem.suggestedInsertionIds && !inlineItem.suggestedInsertionIds->empty()) ||
                    (inlineItem.is_atom() && !model::atomSuggestion(inlineItem.to_atom())
                        .suggestedInsertionIds.value_or(std::vector<std::string>{}).empty())) continue;
                ProjSpan span;
                if (inlineItem.is_text()) {
                    span.text = inlineItem.text;
                    auto flag = [&](const std::string& field) {
                        return inlineItem.style.contains(field) && inlineItem.style[field].is_boolean() &&
                               inlineItem.style[field].get<bool>();
                    };
                    span.marks.bold = flag("bold");
                    span.marks.italic = flag("italic");
                    span.marks.underline = flag("underline");
                    span.marks.strikethrough = flag("strikethrough");
                    span.marks.smallCaps = flag("smallCaps");
                    const auto effectiveAttrs = textStyleExport(model::textStyleEffective(tab, paragraph, inlineItem.style));
                    for (const auto& [name, value] : effectiveAttrs.values) {
                        if (baseAttrs.get_value(name) != value) {
                            span.formatting.values.emplace_back(name, value);
                        }
                    }
                    span.marks.link = projectionLink(inlineItem.style, tab, document);
                } else {
                    span.kind = "token";
                    TokenRef token = atomTokenProject(inlineItem.to_atom(), tab);
                    if (const auto footnoteId = projectionFootnoteId(inlineItem)) {
                        const auto labels = projectionFootnotes(tab);
                        const auto found = labels.find(*footnoteId);
                        if (found != labels.end()) { token.label = found->second; token.attrs = {}; }
                    }
                    if (inlineItem.create && inlineItem.create->uri) {
                        token.attrs.values.emplace_back("src", *inlineItem.create->uri);
                    }
                    span.token = std::move(token);
                }
                projected.spans.push_back(std::move(span));
            }
            projectionRangeMarkers(projected, paragraph, tab);
            projectionReviewMarkers(projected, paragraph);
        } else if (block.is_table()) {
            projected.kind = "table";
            projected.table.emplace();
            const auto& table = block.as_table();
            std::string widths, heights, overflow;
            std::set<std::string> distinctWidths;
            for (const auto& column : table.columns) {
                if (!widths.empty()) widths += ",";
                const auto* width = json::get(column.props, "width");
                const auto value = width && column.props.contains("widthType") &&
                    column.props["widthType"] == slimr::Json("FIXED_WIDTH")
                    ? std::format("{}", std::round(dimensionPt(*width) * 100) / 100) : "auto";
                widths += value;
                distinctWidths.insert(value);
            }
            if (distinctWidths.size() > 1) projected.table->settings.values.emplace_back("widths", widths);
            int headerRows = 0;
            for (size_t r = 0; r < table.rows.size(); ++r) {
                const auto& row = table.rows[r];
                if (r > 0) heights += ",";
                const auto* height = json::get(row.style, "minRowHeight");
                heights += height ? std::format("{}", dimensionPt(*height)) : "0";
                if (row.style.contains("tableHeader") && row.style["tableHeader"] == slimr::Json(true) &&
                    headerRows == static_cast<int>(r)) ++headerRows;
                if (row.style.contains("preventOverflow") && row.style["preventOverflow"] == slimr::Json(true)) {
                    if (!overflow.empty()) overflow += ",";
                    overflow += std::to_string(r + 1);
                }
                std::vector<std::vector<ProjSpan>> spans;
                std::vector<std::vector<ProjBlock>> cells;
                std::vector<std::optional<std::string>> cellStyles;
                std::vector<std::optional<std::string>> paragraphStyles;
                for (size_t c = 0; c < row.cells.size(); ++c) {
                    const auto& cell = row.cells[c];
                    spans.emplace_back();
                    auto child = projectionBuild(cell.blocks, tab, document);
                    const bool inlineCell = !child.blocks.empty() && child.blocks.front().kind == "paragraph" &&
                        std::all_of(child.blocks.begin() + 1, child.blocks.end(), [](const auto& item) {
                            return item.kind == "paragraph" && item.spans.empty() && !item.paragraphStyle;
                        });
                    if (inlineCell) {
                        spans.back() = child.blocks.front().spans;
                        paragraphStyles.push_back(child.blocks.front().paragraphStyle);
                        cells.emplace_back();
                    } else {
                        paragraphStyles.emplace_back();
                        cells.push_back(std::move(child.blocks));
                    }
                    projection.paragraphStyles.insert(child.paragraphStyles.begin(), child.paragraphStyles.end());
                    projection.cellStyles.insert(child.cellStyles.begin(), child.cellStyles.end());
                    const auto declaration = cellDeclarationExport(cell.style);
                    if (declaration != CellDeclaration{}) {
                        cellStyles.push_back(projectionStyleName('c', cell.key));
                        projection.cellStyles[*cellStyles.back()] = declaration;
                    } else cellStyles.emplace_back();
                    const auto span = [&](const char* name, size_t remaining) {
                        if (!cell.style.contains(name)) return 1;
                        if (!cell.style[name].is_number()) throw CoreError("unsupportedSyntax", "Invalid table merge spans.");
                        const double value = cell.style[name].as<double>();
                        if (!std::isfinite(value) || value < 1 || value != std::floor(value) ||
                            value > static_cast<double>(remaining)) {
                            throw CoreError("unsupportedSyntax", "Invalid table merge spans.");
                        }
                        return static_cast<int>(value);
                    };
                    const int rowSpan = span("rowSpan", table.rows.size() - r);
                    const int columnSpan = span("columnSpan", row.cells.size() - c);
                    if (rowSpan < 1 || columnSpan < 1 || static_cast<size_t>(rowSpan) > table.rows.size() - r ||
                        static_cast<size_t>(columnSpan) > row.cells.size() - c) {
                        throw CoreError("unsupportedSyntax", "Invalid table merge spans.");
                    }
                    if (rowSpan > 1 || columnSpan > 1) {
                        projected.table->merges.push_back({.column = static_cast<int>(c), .columnSpan = columnSpan,
                            .row = static_cast<int>(r), .rowSpan = rowSpan});
                    }
                }
                projected.table->rows.push_back(std::move(spans));
                projected.table->cells.push_back(std::move(cells));
                projected.table->cellStyles.push_back(std::move(cellStyles));
                projected.table->paragraphStyles.push_back(std::move(paragraphStyles));
            }
            if (headerRows) projected.table->settings.values.emplace_back("headerRows", std::to_string(headerRows));
            heights.clear();
            bool nonzeroHeight = false;
            for (size_t r = 0; r < table.rows.size(); ++r) {
                double height = table.rows[r].style.contains("minRowHeight")
                    ? dimensionPt(table.rows[r].style["minRowHeight"]) : 0;
                const bool merged = std::any_of(projected.table->merges.begin(), projected.table->merges.end(), [&](const auto& merge) {
                    return static_cast<int>(r) >= merge.row && static_cast<int>(r) < merge.row + merge.rowSpan;
                });
                if (merged && height == 21) height = 0;
                if (r) heights += ",";
                heights += std::format("{}", std::round(height * 100) / 100);
                nonzeroHeight = nonzeroHeight || height > 0;
            }
            if (nonzeroHeight) {
                projected.table->settings.values.emplace_back("heights", heights);
            }
            if (!overflow.empty()) projected.table->settings.values.emplace_back("preventOverflow", overflow);
        } else {
            projected.kind = "token";
            TokenRef token;
            token.kind = block.is_toc() ? "toc" : "section";
            if (block.is_section_break()) {
                token.attrs = sectionSettingsAttrs(sectionSettingsOf(block.as_section_break().sectionStyle, tab.documentStyle));
                token.attrs.flags.push_back(sectionTypeFlag(block.as_section_break().sectionStyle));
            }
            std::string identity = block.key;
            if (block.is_section_break()) {
                int ordinal = 2;
                for (const auto& item : tab.blocks) {
                    if (item.key == block.key) break;
                    if (item.is_section_break()) ++ordinal;
                }
                identity = "s" + std::to_string(ordinal);
            }
            token.attrs.values.emplace_back("id", identity);
            projected.token = std::move(token);
        }
        projection.blocks.push_back(std::move(projected));
    }
    reviewProjectionNormalize(projection, blocks);
    return projection;
}

inline Projection projectionBuild(const model::TabModel& tab, const model::DocModel* document) {
    auto projection = projectionBuild(tab.blocks, tab, document);
    if (tab.leadingSectionStyle.is_object() && !tab.leadingSectionStyle.get<slimr::Json::object_t>().empty()) {
        ProjBlock opening;
        opening.kind = "token";
        TokenRef token;
        token.kind = "section";
        token.attrs = sectionSettingsAttrs(sectionSettingsOf(tab.leadingSectionStyle, tab.documentStyle));
        token.attrs.values.emplace_back("id", "s1");
        opening.token = std::move(token);
        projection.blocks.insert(projection.blocks.begin(), std::move(opening));
    }
    return projection;
}

inline Projection projectionBuild(const slimr::Json& tab) {
    Projection proj;
    auto* blocks = json::get(tab, "blocks");
    if (!blocks || !blocks->is_array()) return proj;

    for (const auto& b : blocks->get<slimr::Json::array_t>()) {
        std::string kind = "paragraph";
        if (auto* k = json::get(b, "kind")) {
            if (k->is_string()) kind = k->get<std::string>();
        }
        if (kind == "paragraph") {
            ProjBlock pb;
            pb.kind = "paragraph";
            if (auto* inlines = json::get(b, "inlines")) {
                if (inlines->is_array()) {
                    for (const auto& inline_elem : inlines->get<slimr::Json::array_t>()) {
                        ProjSpan span;
                        std::string ik = "text";
                        if (auto* ik_val = json::get(inline_elem, "kind")) {
                            if (ik_val->is_string()) ik = ik_val->get<std::string>();
                        }
                        if (ik == "text") {
                            span.kind = "text";
                            if (auto* t_val = json::get(inline_elem, "text")) {
                                if (t_val->is_string()) span.text = t_val->get<std::string>();
                            }
                            if (auto* st = json::get(inline_elem, "style")) {
                                if (st->is_object()) {
                                    if (auto* bold = json::get(*st, "bold")) {
                                        if (bold->is_boolean() && bold->get<bool>()) span.marks.bold = true;
                                    }
                                    if (auto* italic = json::get(*st, "italic")) {
                                        if (italic->is_boolean() && italic->get<bool>()) span.marks.italic = true;
                                    }
                                    if (auto* underline = json::get(*st, "underline")) {
                                        if (underline->is_boolean() && underline->get<bool>()) span.marks.underline = true;
                                    }
                                    if (auto* strikethrough = json::get(*st, "strikethrough")) {
                                        if (strikethrough->is_boolean() && strikethrough->get<bool>()) span.marks.strikethrough = true;
                                    }
                                }
                            }
                        } else if (ik == "atom") {
                            span.kind = "token";
                            TokenRef tr;
                            if (auto* type_val = json::get(inline_elem, "type")) {
                                if (type_val->is_string()) tr.kind = type_val->get<std::string>();
                            }
                            span.token = std::move(tr);
                        }
                        pb.spans.push_back(std::move(span));
                    }
                }
            }
            proj.blocks.push_back(std::move(pb));
        } else if (kind == "table") {
            ProjBlock tb;
            tb.kind = "table";
            ProjTable pt;
            if (auto* rows = json::get(b, "rows")) {
                if (rows->is_array()) {
                    for (const auto& r : rows->get<slimr::Json::array_t>()) {
                        std::vector<std::vector<ProjSpan>> row_spans;
                        if (auto* cells = json::get(r, "cells")) {
                            if (cells->is_array()) {
                                for (const auto& c : cells->get<slimr::Json::array_t>()) {
                                    std::vector<ProjSpan> cell_spans;
                                    if (auto* cell_blocks = json::get(c, "blocks")) {
                                        if (cell_blocks->is_array()) {
                                            for (const auto& cb : cell_blocks->get<slimr::Json::array_t>()) {
                                                if (auto* inlines = json::get(cb, "inlines")) {
                                                    if (inlines->is_array()) {
                                                        for (const auto& inline_elem : inlines->get<slimr::Json::array_t>()) {
                                                            ProjSpan span;
                                                            span.kind = "text";
                                                            if (auto* t_val = json::get(inline_elem, "text")) {
                                                                if (t_val->is_string()) span.text = t_val->get<std::string>();
                                                            }
                                                            cell_spans.push_back(std::move(span));
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    row_spans.push_back(std::move(cell_spans));
                                }
                            }
                        }
                        pt.rows.push_back(std::move(row_spans));
                    }
                }
            }
            tb.table = std::move(pt);
            proj.blocks.push_back(std::move(tb));
        }
    }
    return proj;
}

} // namespace gdocsmith::lens
