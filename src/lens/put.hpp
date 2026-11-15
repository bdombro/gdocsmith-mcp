#pragma once

#include "diff/myers.hpp"
#include "json.hpp"
#include "lens/atom_put.hpp"
#include "lens/json_helper.hpp"
#include "lens/links.hpp"
#include "model/edit.hpp"
#include "model/edit_tables.hpp"
#include "parse.hpp"
#include "project.hpp"
#include "render.hpp"
#include "tokens.hpp"
#include <functional>
#include <string>
#include <vector>

namespace gdocsmith::lens {

struct WriteReport {
    bool changed{false};
    std::vector<std::string> createdKeys;
    std::vector<std::string> notes;
    std::vector<std::string> managedNamedRangeIds;
    std::vector<ImageSource> recreatedImages;
};

struct PutOptions {
    bool allowTokenOmission{false};
    // When set, hidden insertions keep their gaps and this rule classifies review-span edits
    // (base is the tab before the write) before review markers are checked.
    std::function<void(const model::TabModel &base, model::TabModel &written)> carryReview;
    // Paragraph keys this write's suggestion rejections emptied (some inlines before, none after).
    std::set<std::string> rejectionEmptied;
};

namespace put_detail {

struct PutState : WriteReport {
    std::map<std::string, std::vector<RangeMark>> markers;
    Projection oldProjection;
    Projection newProjection;
    AtomPutOptions atomOptions;
    AtomPutReport atomReport;
    std::map<std::string, model::Atom> atoms;
    std::set<std::string> recreatedLocalImages;
    bool carryReview{false};
    std::set<std::string> rejectionEmptied;
};

inline void imageTokensCollect(const std::vector<ProjBlock> &blocks,
                               std::map<std::string, std::vector<TokenRef>> &tokens) {
    auto spans = [&](const std::vector<ProjSpan> &values) {
        for (const auto &span : values) {
            if (span.token && span.token->kind == "image" && span.token->attrs.get_value("id"))
                tokens[*span.token->attrs.get_value("id")].push_back(*span.token);
        }
    };
    for (const auto &block : blocks) {
        spans(block.spans);
        if (!block.table)
            continue;
        const auto &table = *block.table;
        for (size_t r = 0; r < table.rows.size(); ++r) {
            for (size_t c = 0; c < table.rows[r].size(); ++c) {
                if (r < table.cells.size() && c < table.cells[r].size() && !table.cells[r][c].empty())
                    imageTokensCollect(table.cells[r][c], tokens);
                else
                    spans(table.rows[r][c]);
            }
        }
    }
}

inline void tokenIdentitiesCollect(const std::vector<ProjBlock> &blocks, std::set<std::string> &identities) {
    for (const auto &block : blocks) {
        if (block.token)
            identities.insert(atomTokenIdentity(*block.token));
        for (const auto &span : block.spans)
            if (span.token)
                identities.insert(atomTokenIdentity(*span.token));
        if (!block.table)
            continue;
        for (const auto &row : block.table->rows)
            for (const auto &cell : row)
                for (const auto &span : cell)
                    if (span.token)
                        identities.insert(atomTokenIdentity(*span.token));
        for (const auto &row : block.table->cells)
            for (const auto &cell : row)
                tokenIdentitiesCollect(cell, identities);
    }
}

inline void tokenOmissionsApply(std::vector<ProjBlock> &blocks, const std::set<std::string> &identities,
                                const AtomPutOptions &atoms, const PutOptions &options,
                                std::vector<std::string> &notes) {
    auto omit = [&](const TokenRef &token) {
        const auto id = token.attrs.get_value("id");
        static const std::set<std::string> opaque = {
            "autotext", "chart", "columnbreak", "drawing", "equation", "footnote",
            "hr", "image", "namedrange", "positioned", "toc"};
        if (!id || !opaque.contains(token.kind) || identities.contains(atomTokenIdentity(token)))
            return false;
        if (token.kind == "image" && (token.attrs.get_value("src") || (atoms.images && atoms.images(*id))))
            return false;
        if (!options.allowTokenOmission)
            throw model::CoreError("anchorNotFound", "there's no " + token.kind + " with id=" + *id + " in this document");
        notes.push_back("Omitted " + token.kind + (token.label && !token.label->empty() ? " \"" + *token.label + "\"" : "") +
                        " (id=" + *id + "): this object cannot transfer; use file copy to preserve it.");
        return true;
    };
    auto spans = [&](std::vector<ProjSpan> &values) {
        std::erase_if(values, [&](const auto &span) { return span.token && omit(*span.token); });
    };
    std::erase_if(blocks, [&](auto &block) {
        if (block.token && omit(*block.token))
            return true;
        spans(block.spans);
        if (block.table) {
            auto &table = *block.table;
            for (size_t r = 0; r < table.rows.size(); ++r) {
                for (size_t c = 0; c < table.rows[r].size(); ++c) {
                    if (r < table.cells.size() && c < table.cells[r].size() && !table.cells[r][c].empty()) {
                        tokenOmissionsApply(table.cells[r][c], identities, atoms, options, notes);
                        table.rows[r][c].clear();
                        if (table.cells[r][c].size() == 1)
                            table.rows[r][c] = table.cells[r][c][0].spans;
                    } else
                        spans(table.rows[r][c]);
                }
            }
        }
        return false;
    });
}

inline void refuse() {
    throw model::CoreError("unsupportedSyntax",
                           "This Markdown edit would change an unsupported protected object, review construct, or table structure.");
}

inline std::string canonical(const ProjBlock &block) {
    Projection projection;
    projection.blocks.push_back(block);
    return markdownRender(projection);
}

inline model::JsonObject paragraphPatch(const std::optional<std::string> &name, const Projection &projection,
                                        const Projection *fallback = nullptr) {
    if (!name)
        return model::make_json_object();
    const auto found = projection.paragraphStyles.find(*name);
    if (found != projection.paragraphStyles.end())
        return paragraphDeclarationPatch(found->second);
    if (fallback)
        return paragraphPatch(name, *fallback);
    refuse();
    return {};
}

inline model::JsonObject cellPatch(const std::optional<std::string> &name, const Projection &projection,
                                   const Projection *fallback = nullptr) {
    if (!name)
        return model::make_json_object();
    const auto found = projection.cellStyles.find(*name);
    if (found != projection.cellStyles.end())
        return cellDeclarationPatch(found->second);
    if (fallback)
        return cellPatch(name, *fallback);
    refuse();
    return {};
}

inline void declarationApply(model::JsonObject &style, const model::JsonObject &before,
                             const model::JsonObject &after, bool cell = false) {
    if (!style.is_object())
        style = model::make_json_object();
    auto patch = after;
    for (const auto &[field, value] : before.get<slimr::Json::object_t>()) {
        (void)value;
        if (!patch.contains(field))
            patch[field] = cell && field.starts_with("padding")
                               ? model::dimensionFromPt(DEFAULT_PADDING)
                               : slimr::Json(nullptr);
    }
    for (const auto &[field, value] : patch.get<slimr::Json::object_t>()) {
        if (value.is_null())
            style.get<slimr::Json::object_t>().erase(field);
        else
            style[field] = value;
    }
}

inline ProjBlock resolveTextStyles(ProjBlock block, const Projection &projection,
                                   const Projection *fallback = nullptr) {
    for (auto &span : block.spans)
        if (span.directive) {
            auto found = projection.styles.find(*span.directive);
            const DirectiveAttrs *attrs = found == projection.styles.end() ? nullptr : &found->second;
            if (!attrs && fallback) {
                const auto prior = fallback->styles.find(*span.directive);
                if (prior != fallback->styles.end())
                    attrs = &prior->second;
            }
            if (!attrs)
                refuse();
            for (const auto &[key, value] : attrs->values) {
                if (!span.formatting.has_value(key))
                    span.formatting.values.emplace_back(key, value);
            }
            span.directive.reset();
        }
    return block;
}

inline std::string comparisonKey(const ProjBlock &block, const Projection &projection,
                                 const Projection *fallback = nullptr) {
    auto key = canonical(resolveTextStyles(block, projection, fallback));
    key += slimr::write_json(paragraphPatch(block.paragraphStyle, projection, fallback));
    if (block.table) {
        for (const auto &row : block.table->cellStyles)
            for (const auto &name : row) {
                key += slimr::write_json(cellPatch(name, projection, fallback));
            }
        for (const auto &row : block.table->cells)
            for (const auto &cell : row) {
                for (const auto &child : cell)
                    key += comparisonKey(child, projection, fallback);
            }
    }
    return key;
}

inline bool review(const model::ParagraphBlock &paragraph) {
    if (paragraph.suggestionsRaw || paragraph.newline.suggestedDeletionIds ||
        paragraph.newline.suggestedInsertionIds || paragraph.newline.suggestedTextStyleChanges)
        return true;
    for (const auto &item : paragraph.inlines) {
        if (item.suggestedDeletionIds || item.suggestedInsertionIds || item.suggestedTextStyleChanges)
            return true;
        if (item.is_atom() && !model::marksKey(model::atomSuggestion(item.to_atom())).empty())
            return true;
    }
    return false;
}

inline void deletable(const model::Block &block) {
    if (block.is_paragraph()) {
        const auto &paragraph = block.as_paragraph();
        if (paragraph.is_protected || review(paragraph) ||
            (paragraph.positionedObjectIds && !paragraph.positionedObjectIds->empty()))
            refuse();
    } else if (block.is_table()) {
        const auto &table = block.as_table();
        if (table.is_protected || table.suggestedDeletionIds || table.suggestedInsertionIds)
            refuse();
        for (const auto &row : table.rows) {
            if (row.suggestedDeletionIds || row.suggestedInsertionIds || row.suggestedStyleChanges)
                refuse();
            for (const auto &cell : row.cells) {
                if (cell.suggestedDeletionIds || cell.suggestedInsertionIds || cell.suggestedStyleChanges)
                    refuse();
                for (const auto &child : cell.blocks)
                    deletable(child);
            }
        }
    } else if (!block.is_section_break())
        refuse();
}

struct WrittenSym {
    std::string id;
    ProjSpan span;
};

inline std::vector<WrittenSym> writtenSymbols(const ProjBlock &block) {
    std::vector<WrittenSym> result;
    for (const auto &span : block.spans) {
        if (span.kind == "token" && span.token && span.token->kind == "namedrange") {
            if (span.token->label || !span.token->attrs.classes.empty() ||
                span.token->attrs.values.size() != 1 || span.token->attrs.flags.size() != 1)
                refuse();
            continue;
        }
        if (span.kind == "token" && span.token) {
            result.push_back({std::string("\0", 1) + atomTokenIdentity(*span.token), span});
        } else if (span.kind == "text") {
            for (const auto &cp : model::utf8CodePoints(span.text)) {
                auto item = span;
                item.text = cp;
                result.push_back({cp, std::move(item)});
            }
        } else
            refuse();
    }
    return result;
}

inline void applyMarks(model::JsonObject &style, const ProjSpan &span, const ProjSpan *before = nullptr,
                       std::string_view tabId = {}) {
    if (!style.is_object())
        style = model::make_json_object();
    for (const auto &[name, value] : {
             std::pair{"bold", span.marks.bold}, {"italic", span.marks.italic}, {"underline", span.marks.underline}, {"strikethrough", span.marks.strikethrough}, {"smallCaps", span.marks.smallCaps}}) {
        auto prior = before ? before->marks : Marks{};
        bool oldValue = std::string_view(name) == "bold" ? prior.bold : std::string_view(name) == "italic"      ? prior.italic
                                                                    : std::string_view(name) == "underline"     ? prior.underline
                                                                    : std::string_view(name) == "strikethrough" ? prior.strikethrough
                                                                                                                : prior.smallCaps;
        if (!before || value != oldValue) {
            if (value || oldValue || (style.contains(name) && style[name].is_boolean()))
                style[name] = value;
        }
    }
    if (!before || before->marks.link != span.marks.link) {
        if (span.marks.link)
            style["link"] = documentLinkStyle(*span.marks.link, tabId);
        else if (before || (style.contains("link") && style["link"].contains("url"))) {
            style.get<slimr::Json::object_t>().erase("link");
        }
    }
    if (before) {
        for (const auto &[name, value] : before->formatting.values) {
            (void)value;
            if (span.formatting.has_value(name))
                continue;
            if (name == "fontFamily" || name == "fontWeight") {
                if (style.contains("weightedFontFamily") && style["weightedFontFamily"].is_object()) {
                    style["weightedFontFamily"].get<slimr::Json::object_t>().erase(
                        name == "fontFamily" ? "fontFamily" : "weight");
                }
            } else
                style.get<slimr::Json::object_t>().erase(name);
        }
    }
    if (span.marks.code)
        style["weightedFontFamily"]["fontFamily"] = "Roboto Mono";
    if (!before || before->formatting != span.formatting)
        textStyleApply(style, span.formatting);
    if (span.directive)
        refuse();
}

inline void paragraphPut(model::ParagraphBlock &paragraph, const ProjBlock &before,
                         const ProjBlock &after, model::EditTarget target, PutState &state) {
    if (paragraph.is_protected && !review(paragraph))
        refuse();
    const auto all = model::paragraphSymbols(paragraph);
    // Under carryReview, align visible symbols only; hidden insertions are re-placed by gap below.
    std::vector<size_t> visible;
    for (size_t i = 0; i < all.size(); ++i)
        if (!state.carryReview || !model::symHidden(all[i]))
            visible.push_back(i);
    std::vector<model::Sym> old;
    for (auto i : visible)
        old.push_back(all[i]);
    const auto resolvedBefore = resolveTextStyles(before, state.oldProjection);
    const auto oldWritten = writtenSymbols(resolvedBefore);
    auto written = resolveTextStyles(after, state.newProjection, &state.oldProjection);
    if (written.kind == "codeLine")
        for (auto &span : written.spans)
            span.marks.code = true;
    const auto next = writtenSymbols(written);
    if (old.size() != oldWritten.size())
        refuse();
    std::vector<std::string> oldIds, nextIds;
    for (const auto &item : oldWritten)
        oldIds.push_back(item.id);
    for (const auto &item : next)
        nextIds.push_back(item.id);
    auto pairs = std::find(oldIds.begin(), oldIds.end(), "\uE907") == oldIds.end()
                     ? diff::matchesMyers<std::string>(oldIds, nextIds)
                     : diff::lcsPairs(oldIds, nextIds, nullptr, nullptr, [&](int i) {
                           return oldIds[i] == "\uE907" ? static_cast<double>(oldIds.size()) + static_cast<double>(nextIds.size()) + 1 : 1;
                       });
    std::vector<int> source(next.size(), -1);
    std::vector<bool> retained(old.size(), false);
    for (auto [a, b] : pairs) {
        source[b] = a;
        retained[a] = true;
    }
    for (size_t i = 0; i < old.size() && !state.carryReview; ++i)
        if (!retained[i] && !model::marksKey(model::symMarks(old[i])).empty())
            throw model::CoreError("suggestionConflict", "This edit changes text carrying a pending suggestion; reject it first.");
    const auto oldMarkers = rangeMarks(before.spans);
    auto markers = rangeMarks(after.spans);
    for (auto &marker : markers) {
        std::optional<int> originalOffset;
        if (marker.edge == "start") {
            if (marker.offset == static_cast<int>(next.size()))
                originalOffset = static_cast<int>(old.size());
            else if (marker.offset >= 0 && marker.offset < static_cast<int>(source.size()) &&
                     source[marker.offset] >= 0)
                originalOffset = source[marker.offset];
        } else {
            if (marker.offset == 0)
                originalOffset = 0;
            else if (marker.offset > 0 && marker.offset <= static_cast<int>(source.size()) &&
                     source[marker.offset - 1] >= 0)
                originalOffset = source[marker.offset - 1] + 1;
        }
        marker.key = paragraph.key;
        marker.same = originalOffset && std::any_of(oldMarkers.begin(), oldMarkers.end(), [&](const auto &prior) {
                          return prior.name == marker.name && prior.edge == marker.edge && prior.offset == *originalOffset;
                      });
    }
    state.markers[paragraph.key] = std::move(markers);
    std::vector<model::Sym> result;
    // Hidden gap g sits between visible g-1 and g. Matching the original modelMap, it stays after a
    // replacement ending there and before one starting there, follows a pure insertion, and is dropped
    // when both neighbors are replaced (the caller's rule then refuses the lost suggestion).
    const auto gapCount = static_cast<int>(old.size());
    auto afterPrevious = [&](int g) { return g < gapCount && !retained[g] && (g == 0 || retained[g - 1]); };
    auto gapEmit = [&](int g) {
        if (!state.carryReview || (g > 0 && g < gapCount && !retained[g - 1] && !retained[g]))
            return;
        const size_t lo = g == 0 ? 0 : visible[g - 1] + 1;
        const size_t hi = g == gapCount ? all.size() : visible[g];
        for (size_t k = lo; k < hi; ++k)
            result.push_back(all[k]);
    };
    if (afterPrevious(0))
        gapEmit(0);
    for (size_t i = 0; i < next.size(); ++i) {
        if (source[i] >= 0) {
            if (!afterPrevious(source[i]))
                gapEmit(source[i]);
            auto sym = old[source[i]];
            if (sym.is_char())
                applyMarks(sym.style, next[i].span, &oldWritten[source[i]].span, target.tab.tabId);
            else if (sym.atom && next[i].span.token && oldWritten[source[i]].span.token) {
                const auto &prior = *oldWritten[source[i]].span.token;
                const auto &incoming = *next[i].span.token;
                bool recreate = false;
                for (const auto *field : {"email", "url", "date", "time", "tz", "format", "locale"}) {
                    if (prior.attrs.get_value(field) != incoming.attrs.get_value(field))
                        recreate = true;
                }
                if (recreate) {
                    auto atom = atomTokenCreate(target, incoming, state.atomOptions, state.atomReport);
                    atom.style = sym.atom->style;
                    sym = model::Sym(std::move(atom));
                } else
                    atomTokenKeep(*sym.atom, prior, incoming, target.tab, state.atomOptions);
            }
            result.push_back(std::move(sym));
            if (afterPrevious(source[i] + 1))
                gapEmit(source[i] + 1);
        } else {
            if (next[i].span.token) {
                const auto &token = *next[i].span.token;
                const auto snapshot = state.atoms.find(atomTokenIdentity(token));
                auto atom = snapshot == state.atoms.end()
                                ? atomTokenCreate(target, token, state.atomOptions, state.atomReport)
                                : atomRecreate(target, snapshot->second, state.atomReport);
                if (snapshot != state.atoms.end() && snapshot->second.type == "image")
                    state.recreatedLocalImages.insert(snapshot->second.key);
                if (snapshot != state.atoms.end() && token.attrs.get_value("src") && atom.create && atom.type == "image") {
                    atom.create->uri = imageUriResolve(*token.attrs.get_value("src"), state.atomOptions.baseDir);
                }
                result.emplace_back(std::move(atom));
                continue;
            }
            if (next[i].span.kind != "text")
                refuse();
            if (next[i].span.text == "\uE907")
                throw model::CoreError("unrecreatableMove",
                                       "a hidden chip or variable can't be created or moved (Google drops the character); keep its :chip or :variable directive in place, or delete it");
            model::CharSym sym;
            sym.ch = next[i].span.text;
            sym.style = result.empty() ? (old.empty() ? paragraph.newline.style : old.front().style)
                                       : (result.back().is_char() ? result.back().style : paragraph.newline.style);
            if (!sym.style.is_object())
                sym.style = model::make_json_object();
            for (const auto &field : {"bold", "italic", "underline", "strikethrough", "smallCaps",
                                      "fontSize", "foregroundColor", "backgroundColor", "baselineOffset"}) {
                sym.style.get<slimr::Json::object_t>().erase(field);
            }
            if (sym.style.contains("weightedFontFamily") && sym.style["weightedFontFamily"].is_object()) {
                auto &font = sym.style["weightedFontFamily"].get<slimr::Json::object_t>();
                font.erase("fontFamily");
                font.erase("weight");
                if (font.empty())
                    sym.style.get<slimr::Json::object_t>().erase("weightedFontFamily");
            }
            const auto named = model::textStyleEffective(target.tab, paragraph, model::make_json_object());
            for (const auto &field : {"bold", "italic", "underline", "strikethrough", "smallCaps"}) {
                if (named.contains(field) && named[field].is_boolean() && named[field].get<bool>()) {
                    sym.style[field] = false;
                }
            }
            if (!next[i].span.marks.link && projectionLink(sym.style, target.tab, nullptr)) {
                sym.style.get<slimr::Json::object_t>().erase("link");
            }
            applyMarks(sym.style, next[i].span, nullptr, target.tab.tabId);
            result.emplace_back(std::move(sym));
        }
    }
    if (!afterPrevious(gapCount))
        gapEmit(gapCount);
    model::paragraphSymbolsSet(paragraph, result);
    declarationApply(paragraph.style, paragraphPatch(before.paragraphStyle, state.oldProjection),
                     paragraphPatch(after.paragraphStyle, state.newProjection, &state.oldProjection));
    if (after.kind == "heading") {
        if (!after.headingLevel || *after.headingLevel < 1 || *after.headingLevel > 6)
            refuse();
        paragraph.style["namedStyleType"] = "HEADING_" + std::to_string(*after.headingLevel);
    } else if (before.kind == "heading") {
        paragraph.style["namedStyleType"] = "NORMAL_TEXT";
    }
    if (after.kind == "listItem") {
        if (after.listDepth < 0 || after.listDepth > 8)
            refuse();
        const std::string kind = after.listKind == "numbered" ? "number" : "bullet";
        const auto preset = after.listPreset.value_or(model::defaultPresetForKind(kind));
        if (!model::listPresetTable().contains(preset) ||
            preset.starts_with("NUMBERED_") != (kind == "number"))
            refuse();
        std::string listId;
        if (paragraph.bullet) {
            const auto found = target.tab.lists.find(paragraph.bullet->listId);
            if (found != target.tab.lists.end() && before.listPreset == after.listPreset &&
                ((before.kind == "listItem" && before.listKind == after.listKind &&
                  before.listDepth == after.listDepth) ||
                 model::listKind(found->second, after.listDepth) == kind))
                listId = found->first;
        }
        if (listId.empty()) {
            for (const auto &[id, definition] : target.tab.lists) {
                if (model::listKind(definition, after.listDepth) == kind && definition.preset == preset) {
                    listId = id;
                    break;
                }
            }
        }
        if (listId.empty()) {
            listId = "new:list:" + target.ctx.keys.next("n");
            model::ListDef definition;
            definition.isNew = true;
            definition.preset = preset;
            definition.nestingLevels = model::listPresetTable().at(*definition.preset);
            target.tab.lists.emplace(listId, std::move(definition));
        }
        if (paragraph.bullet && paragraph.bullet->listId == listId) {
            paragraph.bullet->nestingLevel = after.listDepth;
        } else
            paragraph.bullet = model::BulletRef{.listId = listId, .nestingLevel = after.listDepth};
    } else
        paragraph.bullet.reset();
    paragraph.stamp = target.ctx.stamp;
}

inline bool paragraphKind(const ProjBlock &block) {
    return block.kind == "paragraph" || block.kind == "heading" ||
           block.kind == "listItem" || block.kind == "codeLine";
}

inline void containerPut(std::vector<model::Block> &blocks, const std::vector<ProjBlock> &desired,
                         model::EditTarget target, PutState &report,
                         const std::vector<int> *desiredLines = nullptr);

inline std::vector<std::string> tableList(const std::string &value) {
    std::vector<std::string> result;
    std::istringstream input(value);
    std::string item;
    while (std::getline(input, item, ','))
        result.push_back(item);
    if (value.empty() || value.back() == ',')
        refuse();
    return result;
}

inline double tableNumber(const std::string &value, bool integer = false) {
    size_t count = 0;
    double number = 0;
    try {
        number = std::stod(value, &count);
    } catch (const std::exception &) {
        refuse();
    }
    if (count != value.size() || !std::isfinite(number) || number < 0 ||
        (integer && (number != std::floor(number) || number > std::numeric_limits<int>::max())))
        refuse();
    return number;
}

inline void tableSettingsPut(model::TableBlock &table, const DirectiveAttrs &before, const DirectiveAttrs &after) {
    if (!after.flags.empty() || !after.classes.empty())
        refuse();
    for (const auto &[key, value] : after.values) {
        (void)value;
        if (key != "widths" && key != "heights" && key != "headerRows" && key != "preventOverflow")
            refuse();
    }
    const auto changed = [&](const char *name) { return before.get_value(name) != after.get_value(name); };
    if (changed("widths")) {
        auto values = after.get_value("widths") ? tableList(*after.get_value("widths"))
                                                : std::vector<std::string>(table.columns.size(), "auto");
        if (values.size() != table.columns.size())
            refuse();
        for (size_t c = 0; c < values.size(); ++c) {
            table.columns[c].props = slimr::Json::object_t{{"widthType", values[c] == "auto" ? "EVENLY_DISTRIBUTED" : "FIXED_WIDTH"}};
            if (values[c] != "auto")
                table.columns[c].props["width"] = model::dimensionFromPt(tableNumber(values[c]));
        }
    }
    if (changed("heights")) {
        auto values = after.get_value("heights") ? tableList(*after.get_value("heights"))
                                                 : std::vector<std::string>(table.rows.size(), "0");
        if (values.size() != table.rows.size())
            refuse();
        for (size_t r = 0; r < values.size(); ++r) {
            const auto height = tableNumber(values[r]);
            table.rows[r].style["minRowHeight"] = height ? model::dimensionFromPt(height)
                                                         : slimr::Json(slimr::Json::object_t{{"unit", "PT"}});
        }
    }
    if (changed("headerRows")) {
        const auto count = after.get_value("headerRows") ? tableNumber(*after.get_value("headerRows"), true) : 0;
        if (count > static_cast<double>(table.rows.size()))
            refuse();
        for (size_t r = 0; r < table.rows.size(); ++r) {
            if (static_cast<double>(r) < count)
                table.rows[r].style["tableHeader"] = true;
            else
                table.rows[r].style.get<slimr::Json::object_t>().erase("tableHeader");
        }
    }
    if (changed("preventOverflow")) {
        std::set<int> selected;
        if (const auto value = after.get_value("preventOverflow"))
            for (const auto &item : tableList(*value)) {
                const auto row = tableNumber(item, true);
                if (row < 1 || row > static_cast<double>(table.rows.size()))
                    refuse();
                selected.insert(static_cast<int>(row));
            }
        for (size_t r = 0; r < table.rows.size(); ++r) {
            if (selected.contains(static_cast<int>(r + 1)))
                table.rows[r].style["preventOverflow"] = true;
            else
                table.rows[r].style.get<slimr::Json::object_t>().erase("preventOverflow");
        }
    }
}

inline std::vector<std::vector<std::string>> tableText(const ProjTable &table) {
    std::vector<std::vector<std::string>> result;
    for (size_t r = 0; r < table.rows.size(); ++r) {
        std::vector<std::string> row;
        for (size_t c = 0; c < table.rows[r].size(); ++c) {
            std::string text;
            if (r < table.cells.size() && c < table.cells[r].size() && !table.cells[r][c].empty()) {
                for (const auto &block : table.cells[r][c]) {
                    for (const auto &span : block.spans)
                        text += span.kind == "text" ? span.text : "\xEF\xBF\xBC";
                    text += " ";
                }
            } else
                for (const auto &span : table.rows[r][c])
                    text += span.kind == "text" ? span.text : "\xEF\xBF\xBC";
            row.push_back(std::move(text));
        }
        result.push_back(std::move(row));
    }
    return result;
}

inline void tablePut(model::TableBlock &table, const ProjBlock &before, const ProjBlock &after,
                     model::EditTarget target, PutState &report) {
    if (!before.table || !after.table || table.is_protected ||
        table.suggestedDeletionIds || table.suggestedInsertionIds)
        refuse();
    const auto &old = *before.table;
    const auto &next = *after.table;
    for (const auto &row : table.rows) {
        if (row.suggestedDeletionIds || row.suggestedInsertionIds || row.suggestedStyleChanges)
            refuse();
        for (const auto &cell : row.cells) {
            if (cell.suggestedDeletionIds || cell.suggestedInsertionIds || cell.suggestedStyleChanges)
                refuse();
        }
    }
    const auto align = tableAlign(tableText(old), tableText(next));
    std::vector<std::optional<int>> rows, columns;
    for (const auto &row : align.rows)
        if (row.n)
            rows.push_back(row.o);
    for (const auto &col : align.columns)
        if (col.n)
            columns.push_back(col.o);
    const auto original = table;
    for (const auto &row : align.rows)
        if (row.o && !row.n) {
            for (const auto &cell : original.rows[*row.o].cells)
                for (const auto &child : cell.blocks)
                    deletable(child);
        }
    for (const auto &col : align.columns)
        if (col.o && !col.n) {
            for (const auto &row : original.rows)
                for (const auto &child : row.cells[*col.o].blocks)
                    deletable(child);
        }
    model::tableGeometrySet(target, table, rows, columns);
    const auto covered = mergeCovered(next.merges);
    std::map<std::tuple<size_t, size_t, std::string>, slimr::Json> borderEdits;
    for (size_t r = 0; r < table.rows.size(); ++r) {
        auto &row = table.rows[r];
        for (size_t c = 0; c < row.cells.size(); ++c) {
            auto &cell = row.cells[c];
            if (cell.style.contains("rowSpan"))
                cell.style["rowSpan"] = 1;
            if (cell.style.contains("columnSpan"))
                cell.style["columnSpan"] = 1;
            const auto at = [](const auto &values, size_t ri, size_t ci) -> std::optional<std::string> {
                return ri < values.size() && ci < values[ri].size() ? values[ri][ci] : std::nullopt;
            };
            const auto oldStyle = rows[r] && columns[c] ? at(old.cellStyles, *rows[r], *columns[c]) : std::nullopt;
            if (!covered.contains({static_cast<int>(r), static_cast<int>(c)})) {
                const auto beforePatch = cellPatch(oldStyle, report.oldProjection);
                const auto afterPatch = cellPatch(at(next.cellStyles, r, c), report.newProjection, &report.oldProjection);
                declarationApply(cell.style, beforePatch, afterPatch, true);
                for (const auto *side : {"borderTop", "borderRight", "borderBottom", "borderLeft"}) {
                    if (beforePatch.contains(side) || afterPatch.contains(side)) {
                        borderEdits[{r, c, side}] = afterPatch.contains(side) ? afterPatch[side] : slimr::Json(nullptr);
                    }
                }
            }
            std::vector<ProjBlock> desired;
            if (r < next.cells.size() && c < next.cells[r].size() &&
                !next.cells[r][c].empty())
                desired = next.cells[r][c];
            else {
                ProjBlock paragraph;
                paragraph.spans = next.rows[r][c];
                paragraph.paragraphStyle = at(next.paragraphStyles, r, c);
                desired.push_back(std::move(paragraph));
            }
            const bool referencedBefore = rows[r] && columns[c] && static_cast<size_t>(*rows[r]) < old.cells.size() &&
                                          static_cast<size_t>(*columns[c]) < old.cells[*rows[r]].size() && !old.cells[*rows[r]][*columns[c]].empty();
            const bool referencedAfter = r < next.cells.size() && c < next.cells[r].size() && !next.cells[r][c].empty();
            if (!referencedBefore && !referencedAfter && !cell.blocks.empty() && cell.blocks[0].is_paragraph()) {
                auto projected = markdownParse(markdownRender(projectionBuild(std::vector<model::Block>{cell.blocks[0]}, target.tab)));
                const ProjBlock emptyBefore;
                const auto &beforeBlock = projected.blocks.empty() ? emptyBefore : projected.blocks.front();
                if (!covered.contains({static_cast<int>(r), static_cast<int>(c)})) {
                    paragraphPut(cell.blocks[0].as_paragraph(), beforeBlock, desired.front(), target, report);
                }
            } else if (!covered.contains({static_cast<int>(r), static_cast<int>(c)})) {
                containerPut(cell.blocks, desired, target, report);
            }
            if (c < next.alignments.size() && (!columns[c] || static_cast<size_t>(*columns[c]) >= old.alignments.size() ||
                                               next.alignments[c] != old.alignments[*columns[c]])) {
                for (auto &child : cell.blocks)
                    if (child.is_paragraph())
                        child.as_paragraph().style["alignment"] = next.alignments[c];
            }
        }
        for (const auto &[edge, value] : borderEdits) {
            const auto &[r, c, side] = edge;
            const int nr = static_cast<int>(r) + (side == "borderBottom" ? 1 : side == "borderTop" ? -1
                                                                                                   : 0);
            const int nc = static_cast<int>(c) + (side == "borderRight" ? 1 : side == "borderLeft" ? -1
                                                                                                   : 0);
            if (nr < 0 || nc < 0 || static_cast<size_t>(nr) >= table.rows.size() || static_cast<size_t>(nc) >= table.columns.size())
                continue;
            const auto opposite = sharedSide(side);
            const auto other = borderEdits.find({static_cast<size_t>(nr), static_cast<size_t>(nc), opposite});
            if (other != borderEdits.end() && !model::styleEqual(other->second, value)) {
                throw model::CoreError("unsupportedSyntax", "Conflicting shared table-cell borders.");
            }
            auto &style = table.rows[nr].cells[nc].style;
            if (value.is_null())
                style.get<slimr::Json::object_t>().erase(opposite);
            else
                style[opposite] = value;
        }
    }
    for (const auto &merge : next.merges) {
        if (merge.row < 0 || merge.column < 0 || merge.rowSpan < 1 || merge.columnSpan < 1 ||
            static_cast<size_t>(merge.row) + merge.rowSpan > table.rows.size() ||
            static_cast<size_t>(merge.column) + merge.columnSpan > table.columns.size())
            refuse();
        auto &head = table.rows[merge.row].cells[merge.column];
        if (merge.rowSpan > 1 || head.style.contains("rowSpan"))
            head.style["rowSpan"] = merge.rowSpan;
        if (merge.columnSpan > 1 || head.style.contains("columnSpan"))
            head.style["columnSpan"] = merge.columnSpan;
        if (std::find(old.merges.begin(), old.merges.end(), merge) == old.merges.end()) {
            for (int r = merge.row; r < merge.row + merge.rowSpan; ++r) {
                table.rows[r].style = model::mergedRowStyle(table.rows[r].style);
            }
        }
        for (int r = merge.row; r < merge.row + merge.rowSpan; ++r)
            for (int c = merge.column; c < merge.column + merge.columnSpan; ++c) {
                if (r == merge.row && c == merge.column)
                    continue;
                auto &cell = table.rows[r].cells[c];
                if (cell.blocks.empty() || !cell.blocks[0].is_paragraph())
                    refuse();
                for (const auto &child : cell.blocks)
                    deletable(child);
                for (size_t i = 1; i < cell.blocks.size(); ++i)
                    model::tombstonesAdd(target, cell.blocks[i]);
                cell.blocks.resize(1);
                model::paragraphSymbolsSet(cell.blocks[0].as_paragraph(), {});
            }
    }
    tableSettingsPut(table, old.settings, next.settings);
    table.stamp = target.ctx.stamp;
}

inline model::Block create(const ProjBlock &block, model::EditTarget target, PutState &report) {
    if (block.kind == "token" && block.token && block.token->kind == "pagebreak") {
        ProjBlock paragraph;
        ProjSpan span;
        span.kind = "token";
        span.token = block.token;
        paragraph.spans.push_back(std::move(span));
        return create(paragraph, target, report);
    }
    if (paragraphKind(block)) {
        auto paragraph = model::paragraphEmptyCreate(target.ctx.keys, std::nullopt, target.ctx.stamp);
        ProjBlock empty;
        auto written = block;
        if (written.kind == "codeLine") {
            for (auto &span : written.spans)
                span.marks.code = true;
        }
        paragraphPut(paragraph, empty, written, target, report);
        report.createdKeys.push_back(paragraph.key);
        return model::Block(std::move(paragraph));
    }
    if (block.kind == "token" && block.token && block.token->kind == "section") {
        model::SectionBreakBlock section;
        section.key = target.ctx.keys.next("n");
        const auto &attrs = block.token->attrs;
        const std::string type = attrs.has_flag("continuous") ? "CONTINUOUS" : "NEXT_PAGE";
        section.create = model::SectionBreakBlock::Create{.sectionType = type};
        section.sectionStyle = sectionStyleWith(slimr::Json::object_t{{"sectionType", type}},
                                                sectionDirectiveSettings(attrs));
        report.createdKeys.push_back(section.key);
        return model::Block(std::move(section));
    }
    if (block.kind == "table" && block.table) {
        model::TabModel scratch;
        model::EditTarget scratchTarget{target.ctx, scratch};
        model::EditTableSpec spec;
        for (const auto &row : block.table->rows)
            spec.rows.emplace_back(row.size());
        model::tableCreate(scratchTarget, 0, spec);
        const auto created = std::find_if(scratch.blocks.begin(), scratch.blocks.end(),
                                          [](const auto &item) { return item.is_table(); });
        if (created == scratch.blocks.end())
            refuse();
        auto result = std::move(*created);
        auto before = markdownParse(markdownRender(projectionBuild(std::vector<model::Block>{result}, target.tab)));
        const auto projected = std::find_if(before.blocks.begin(), before.blocks.end(),
                                            [](const auto &item) { return item.kind == "table"; });
        if (projected == before.blocks.end())
            refuse();
        auto desired = block;
        desired.table->cells.resize(desired.table->rows.size());
        for (size_t r = 0; r < desired.table->rows.size(); ++r) {
            desired.table->cells[r].resize(desired.table->rows[r].size());
            for (size_t c = 0; c < desired.table->rows[r].size(); ++c) {
                auto &cell = desired.table->cells[r][c];
                if (cell.empty()) {
                    ProjBlock paragraph;
                    paragraph.spans = desired.table->rows[r][c];
                    cell.push_back(std::move(paragraph));
                }
                if (!cell[0].paragraphStyle && r < projected->table->cells.size() &&
                    c < projected->table->cells[r].size() && !projected->table->cells[r][c].empty()) {
                    cell[0].paragraphStyle = projected->table->cells[r][c][0].paragraphStyle;
                }
            }
        }
        tablePut(result.as_table(), *projected, desired, target, report);
        report.createdKeys.push_back(result.key);
        return result;
    }
    refuse();
    return {};
}

inline void containerPut(std::vector<model::Block> &blocks, const std::vector<ProjBlock> &desired,
                         model::EditTarget target, PutState &report, const std::vector<int> *desiredLines) {
    // First failing desired block wins; its source line goes on content-code problems
    auto located = [&](size_t index, const std::function<void()> &step) {
        if (!desiredLines) {
            step();
            return;
        }
        std::vector<model::FileProblem> problems;
        model::FileProblemWhere where;
        if (index < desiredLines->size())
            where.line = (*desiredLines)[index];
        model::problemCollect(problems, where, step);
        if (!problems.empty())
            throw model::FileProblems(std::move(problems));
    };
    std::vector<std::string> originalKeys;
    for (const auto &block : blocks)
        originalKeys.push_back(block.key);
    std::vector<ProjBlock> before;
    std::vector<size_t> visibleIndices;
    std::vector<size_t> visibleEnds;
    std::vector<std::vector<std::string>> joinedSuggestions;
    std::optional<size_t> firstSkipped;
    size_t firstSkippedEnd = 0;
    std::vector<std::string> firstSkippedJoined;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].is_paragraph() && model::paragraphInserted(blocks[i].as_paragraph()))
            continue;
        size_t end = i;
        std::vector<std::string> joined;
        while (blocks[end].is_paragraph() && model::newlineInserted(blocks[end].as_paragraph().newline)) {
            size_t next = end + 1;
            while (next < blocks.size() && blocks[next].is_paragraph() &&
                   model::paragraphInserted(blocks[next].as_paragraph()))
                ++next;
            if (next >= blocks.size() || !blocks[next].is_paragraph())
                break;
            for (const auto &id : blocks[end].as_paragraph().newline.suggestedInsertionIds.value_or(std::vector<std::string>{}))
                if (std::find(joined.begin(), joined.end(), id) == joined.end())
                    joined.push_back(id);
            end = next;
        }
        std::vector<model::Block> group(blocks.begin() + static_cast<std::ptrdiff_t>(i),
                                        blocks.begin() + static_cast<std::ptrdiff_t>(end + 1));
        auto visible = markdownParse(markdownRender(projectionBuild(group, target.tab)));
        if (visible.blocks.empty()) {
            if (report.carryReview && blocks[i].is_paragraph() && report.rejectionEmptied.contains(blocks[i].key)) {
                before.push_back(ProjBlock{});
                visibleIndices.push_back(i);
                visibleEnds.push_back(end + 1);
                joinedSuggestions.push_back(std::move(joined));
            } else if (!firstSkipped && blocks[i].is_paragraph()) {
                firstSkipped = i;
                firstSkippedEnd = end + 1;
                firstSkippedJoined = std::move(joined);
            }
            i = end;
            continue;
        }
        if (visible.blocks.size() != 1)
            refuse();
        before.push_back(std::move(visible.blocks.front()));
        visibleIndices.push_back(i);
        visibleEnds.push_back(end + 1);
        joinedSuggestions.push_back(std::move(joined));
        i = end;
    }
    if (before.empty() && !desired.empty() && firstSkipped) {
        before.push_back(ProjBlock{});
        visibleIndices.push_back(*firstSkipped);
        visibleEnds.push_back(firstSkippedEnd);
        joinedSuggestions.push_back(std::move(firstSkippedJoined));
    }
    std::vector<std::string> a, b;
    for (const auto &block : before)
        a.push_back(comparisonKey(block, report.oldProjection));
    for (const auto &block : desired)
        b.push_back(comparisonKey(block, report.newProjection, &report.oldProjection));
    const auto hunks = diff::diffHunks(a, b);
    auto soleParagraph = [&](size_t visible, size_t modelIndex) {
        if (visibleEnds[visible] != modelIndex + 1)
            return false;
        for (size_t other = 0; other < blocks.size(); ++other) {
            if (other != modelIndex && blocks[other].is_paragraph())
                return false;
        }
        return std::none_of(desired.begin(), desired.end(), [](const auto &block) { return paragraphKind(block); });
    };
    for (auto h = hunks.rbegin(); h != hunks.rend(); ++h) {
        std::vector<model::Block> replacement;
        const auto paired = std::min(h->aEnd - h->aStart, h->bEnd - h->bStart);
        for (int i = 0; i < paired; ++i)
            located(static_cast<size_t>(h->bStart + i), [&] {
                const auto &joined = joinedSuggestions[h->aStart + i];
                if (!joined.empty()) {
                    std::string ids;
                    for (const auto &id : joined)
                        ids += (ids.empty() ? "" : ", ") + id;
                    throw model::CoreError("suggestionConflict", "this paragraph holds a pending suggested paragraph break (" +
                                                                     ids + "), so it can't be edited; add reject to its suggestion block first");
                }
                const auto modelIndex = visibleIndices[h->aStart + i];
                if (i > 0) {
                    for (auto hidden = visibleIndices[h->aStart + i - 1] + 1; hidden < modelIndex; ++hidden) {
                        replacement.push_back(blocks[hidden]);
                    }
                }
                auto block = blocks[modelIndex];
                const auto &next = desired[h->bStart + i];
                if (block.is_paragraph() && paragraphKind(next)) {
                    paragraphPut(block.as_paragraph(), before[h->aStart + i], next, target, report);
                } else if (block.is_table() && next.kind == "table") {
                    tablePut(block.as_table(), before[h->aStart + i], next, target, report);
                } else if (block.is_section_break() && next.token && next.token->kind == "section") {
                    auto &section = block.as_section_break();
                    const auto &attrs = next.token->attrs;
                    const std::string type = attrs.has_flag("continuous") ? "CONTINUOUS" : "NEXT_PAGE";
                    if (sectionTypeFlag(section.sectionStyle) != (type == "CONTINUOUS" ? "continuous" : "nextPage"))
                        refuse();
                    const auto oldId = before[h->aStart + i].token->attrs.get_value("id");
                    if (attrs.get_value("id") != oldId)
                        refuse();
                    section.sectionStyle = sectionStyleWith(section.sectionStyle, sectionDirectiveSettings(attrs));
                } else {
                    deletable(block);
                    model::tombstonesAdd(target, block);
                    block = create(next, target, report);
                }
                replacement.push_back(std::move(block));
            });
        for (int i = h->aStart + paired; i < h->aEnd; ++i) {
            const auto modelIndex = visibleIndices[i];
            if (i > h->aStart) {
                for (auto hidden = visibleIndices[i - 1] + 1; hidden < modelIndex; ++hidden) {
                    replacement.push_back(blocks[hidden]);
                }
            }
            // Node treats a rejection-emptied paragraph as owned and invisible; deleting the container's only
            // paragraph would re-key it through containerNormalize and restyle it, so keep it in place.
            if (report.carryReview && blocks[modelIndex].is_paragraph() &&
                report.rejectionEmptied.contains(blocks[modelIndex].key) &&
                !blocks[modelIndex].as_paragraph().is_protected &&
                soleParagraph(static_cast<size_t>(i), modelIndex)) {
                replacement.push_back(blocks[modelIndex]);
                continue;
            }
            deletable(blocks[modelIndex]);
            model::tombstonesAdd(target, blocks[modelIndex]);
        }
        for (int i = h->bStart + paired; i < h->bEnd; ++i)
            located(static_cast<size_t>(i), [&] {
                replacement.push_back(create(desired[i], target, report));
            });
        const auto start = h->aStart < static_cast<int>(visibleIndices.size())
                               ? visibleIndices[h->aStart]
                               : blocks.size();
        const auto end = h->aEnd > h->aStart ? visibleEnds[h->aEnd - 1] : start;
        blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(start),
                     blocks.begin() + static_cast<std::ptrdiff_t>(end));
        blocks.insert(blocks.begin() + static_cast<std::ptrdiff_t>(start), replacement.begin(), replacement.end());
    }
    const bool structural = blocks.size() != originalKeys.size() ||
                            !std::equal(blocks.begin(), blocks.end(), originalKeys.begin(), originalKeys.end(),
                                        [](const auto &block, const auto &key) { return block.key == key; });
    if (structural)
        blocks = model::containerNormalize(blocks, target.ctx.keys, target.ctx.stamp);
}

} // namespace put_detail

/** Body-only typed putback. The caller validates the document-file revision/envelope. */
inline WriteReport markdownPut(model::EditTarget target, const Projection &input, const AtomPutOptions &incomingAtomOptions = {},
                               const PutOptions &putOptions = {}) {
    auto atomOptions = incomingAtomOptions;
    if (atomOptions.images) {
        auto cache = std::make_shared<std::map<std::string, std::optional<ImageSource>>>();
        atomOptions.images = [resolver = atomOptions.images, cache](const std::string &id) {
            const auto found = cache->find(id);
            if (found != cache->end())
                return found->second;
            auto source = resolver(id);
            cache->emplace(id, source);
            return source;
        };
    }
    auto parsed = input;
    if (parsed.unsupportedReview)
        put_detail::refuse();
    const auto oldProjection = projectionBuild(target.tab);
    std::set<std::string> identities;
    put_detail::tokenIdentitiesCollect(oldProjection.blocks, identities);
    std::vector<std::string> omissionNotes;
    std::vector<int> lines;
    if (atomOptions.locateBlockProblems && parsed.blockLines.lines.size() == parsed.blocks.size()) {
        lines = parsed.blockLines.lines;
        std::vector<ProjBlock> kept;
        std::vector<int> keptLines;
        for (size_t j = 0; j < parsed.blocks.size(); ++j) {
            std::vector<ProjBlock> one{std::move(parsed.blocks[j])};
            std::vector<model::FileProblem> problems;
            model::problemCollect(problems, {.line = lines[j]}, [&] {
                put_detail::tokenOmissionsApply(one, identities, atomOptions, putOptions, omissionNotes);
            });
            if (!problems.empty())
                throw model::FileProblems(std::move(problems));
            if (one.empty())
                continue;
            kept.push_back(std::move(one.front()));
            keptLines.push_back(lines[j]);
        }
        parsed.blocks = std::move(kept);
        lines = std::move(keptLines);
    } else {
        put_detail::tokenOmissionsApply(parsed.blocks, identities, atomOptions, putOptions, omissionNotes);
    }
    auto withOpening = [&](Projection projection, const model::TabModel &tab) {
        if (!parsed.blocks.empty() && parsed.blocks.front().token &&
            parsed.blocks.front().token->kind == "section" &&
            parsed.blocks.front().token->attrs.get_value("id") == std::optional<std::string>{"s1"} &&
            (projection.blocks.empty() || !projection.blocks.front().token ||
             projection.blocks.front().token->kind != "section" ||
             projection.blocks.front().token->attrs.get_value("id") != std::optional<std::string>{"s1"})) {
            auto opening = parsed.blocks.front();
            const auto actual = sectionSettingsOf(tab.leadingSectionStyle, tab.documentStyle);
            SectionSettings settings;
            settings.columns = actual.columns;
            settings.columnGap = actual.columnGap;
            settings.columnSeparator = actual.columnSeparator;
            settings.rtl = actual.rtl;
            opening.token->attrs = sectionSettingsAttrs(settings);
            opening.token->attrs.values.emplace_back("id", "s1");
            projection.blocks.insert(projection.blocks.begin(), std::move(opening));
        }
        return projection;
    };
    const auto before = markdownParse(markdownRender(withOpening(oldProjection, target.tab)));
    if (markdownRender(before) == markdownRender(parsed) && before.reviewMarkers == parsed.reviewMarkers &&
        parsed.styles.empty() && parsed.paragraphStyles.empty() && parsed.cellStyles.empty() &&
        (!putOptions.carryReview || putOptions.rejectionEmptied.empty()))
        return WriteReport{.notes = std::move(omissionNotes)};
    model::TabModel candidate = target.tab;
    model::EditContext context = target.ctx;
    put_detail::PutState report;
    report.oldProjection = oldProjection;
    report.newProjection = parsed;
    report.atomOptions = atomOptions;
    report.carryReview = static_cast<bool>(putOptions.carryReview);
    report.rejectionEmptied = putOptions.rejectionEmptied;
    report.notes = std::move(omissionNotes);
    std::map<std::string, std::vector<TokenRef>> imageTokens;
    put_detail::imageTokensCollect(parsed.blocks, imageTokens);
    for (const auto &[id, occurrences] : imageTokens) {
        (void)id;
        if (occurrences.size() < 2)
            continue;
        for (const auto &token : occurrences) {
            if (!token.attrs.flags.empty() || std::any_of(token.attrs.values.begin(), token.attrs.values.end(),
                                                          [](const auto &attr) { return attr.first != "id"; }))
                throw model::CoreError("unsupportedSyntax", "A copied existing image accepts only its id, not replacement attributes.");
        }
    }
    std::vector<RangeMark> oldMarkers;
    for (const auto &paragraph : model::paragraphsDeep(target.tab.blocks)) {
        auto projection = projectionBuild(std::vector<model::Block>{model::Block(paragraph)}, target.tab);
        auto visible = markdownParse(markdownRender(projection));
        if (!visible.blocks.empty()) {
            const auto written = put_detail::writtenSymbols(visible.blocks.front());
            const auto symbols = model::paragraphSymbols(paragraph);
            if (written.size() == symbols.size()) {
                for (size_t i = 0; i < symbols.size(); ++i) {
                    if (symbols[i].atom && written[i].span.token) {
                        auto snapshot = *symbols[i].atom;
                        if (snapshot.type == "footnoteRef" && written[i].span.token->label) {
                            snapshot.create = model::AtomCreate{.type = "footnoteRef", .label = written[i].span.token->label};
                        }
                        report.atoms[atomTokenIdentity(*written[i].span.token)] = std::move(snapshot);
                    }
                }
            }
        }
        auto markers = visible.blocks.empty() ? std::vector<RangeMark>{} : rangeMarks(visible.blocks.front().spans);
        for (auto &marker : markers) {
            marker.key = paragraph.key;
            marker.same = true;
        }
        oldMarkers.insert(oldMarkers.end(), markers.begin(), markers.end());
        report.markers[paragraph.key] = std::move(markers);
    }
    auto desired = parsed.blocks;
    {
        if (!desired.empty() && desired.front().token && desired.front().token->kind == "section" &&
            desired.front().token->attrs.get_value("id") == std::optional<std::string>{"s1"}) {
            const auto requested = sectionDirectiveSettings(desired.front().token->attrs);
            auto settings = sectionSettingsOf(candidate.leadingSectionStyle, candidate.documentStyle);
            const auto previous = settings;
            settings.columns = requested.columns;
            settings.columnGap = requested.columnGap;
            settings.columnSeparator = requested.columnSeparator;
            settings.rtl = requested.rtl;
            if (settings != previous)
                candidate.leadingSectionStyle = sectionStyleWith(candidate.leadingSectionStyle, settings);
            desired.erase(desired.begin());
            if (!lines.empty())
                lines.erase(lines.begin());
        }
    }
    put_detail::containerPut(candidate.blocks, desired, {context, candidate}, report, lines.empty() ? nullptr : &lines);
    if (putOptions.carryReview)
        putOptions.carryReview(target.tab, candidate);
    if (candidate.blocks.empty()) {
        candidate.blocks.emplace_back(model::paragraphEmptyCreate(context.keys, std::nullopt, context.stamp));
    }
    std::vector<RangeMark> writtenMarkers;
    for (const auto &paragraph : model::paragraphsDeep(candidate.blocks)) {
        const auto found = report.markers.find(paragraph.key);
        if (found != report.markers.end()) {
            writtenMarkers.insert(writtenMarkers.end(), found->second.begin(), found->second.end());
        }
    }
    if (!oldMarkers.empty() || !writtenMarkers.empty()) {
        if (candidate.tabId.find("::") != std::string::npos)
            put_detail::refuse();
        const auto shown = projectionShownRanges(target.tab);
        const auto managed = namedRangesApply(candidate, oldMarkers, writtenMarkers, shown,
                                              [&] { return "new:range:" + context.keys.next("n"); });
        report.managedNamedRangeIds.assign(managed.begin(), managed.end());
    }
    const auto expected = markdownParse(markdownRender(withOpening(projectionBuild(candidate), candidate)));
    if (expected.reviewMarkers != parsed.reviewMarkers)
        put_detail::refuse();
    report.changed = candidate != target.tab;
    if (!atomOptions.allowLossyImageMove) {
        std::set<std::string> retainedAtoms;
        for (const auto &paragraph : model::paragraphsDeep(candidate.blocks)) {
            for (const auto &sym : model::paragraphSymbols(paragraph))
                if (sym.atom)
                    retainedAtoms.insert(sym.atom->key);
        }
        for (const auto &source : report.atomReport.recreated) {
            if (!source.lost.empty() && report.recreatedLocalImages.contains(source.source.key) &&
                !retainedAtoms.contains(source.source.key))
                throw model::CoreError("lossyImageMove", "Recreating this image loses properties; explicit permission is required.");
        }
    }
    report.recreatedImages = std::move(report.atomReport.recreated);
    report.notes.insert(report.notes.end(), report.atomReport.notes.begin(), report.atomReport.notes.end());
    if (report.changed) {
        target.tab = std::move(candidate);
        target.ctx.tombstones = std::move(context.tombstones);
    }
    return report;
}

inline WriteReport markdownPut(model::EditTarget target, std::string_view markdown, const AtomPutOptions &atomOptions = {},
                               const PutOptions &putOptions = {}) {
    return markdownPut(target, markdownParse(markdown), atomOptions, putOptions);
}

inline WriteReport markdownPut(slimr::Json &tab, const Projection &parsed) {
    WriteReport report;
    slimr::Json new_blocks = slimr::json_arr();

    for (const auto &b : parsed.blocks) {
        if (b.kind == "paragraph" || b.kind == "heading" || b.kind == "listItem") {
            slimr::Json pb = slimr::json_obj();
            pb["kind"] = "paragraph";
            slimr::Json inlines = slimr::json_arr();
            for (const auto &span : b.spans) {
                if (span.kind == "text") {
                    slimr::Json inj = slimr::json_obj();
                    inj["kind"] = "text";
                    inj["text"] = span.text;
                    if (!span.marks.empty()) {
                        slimr::Json st = slimr::json_obj();
                        if (span.marks.bold)
                            st["bold"] = true;
                        if (span.marks.italic)
                            st["italic"] = true;
                        if (span.marks.underline)
                            st["underline"] = true;
                        if (span.marks.strikethrough)
                            st["strikethrough"] = true;
                        inj["style"] = std::move(st);
                    }
                    json::push_back(inlines, std::move(inj));
                }
            }
            pb["inlines"] = std::move(inlines);
            json::push_back(new_blocks, std::move(pb));
        } else if (b.kind == "table" && b.table) {
            slimr::Json tb = slimr::json_obj();
            tb["kind"] = "table";
            slimr::Json rows = slimr::json_arr();
            for (const auto &r : b.table->rows) {
                slimr::Json rj = slimr::json_obj();
                slimr::Json cells = slimr::json_arr();
                for (const auto &c : r) {
                    slimr::Json cj = slimr::json_obj();
                    slimr::Json c_blocks = slimr::json_arr();
                    slimr::Json pb = slimr::json_obj();
                    pb["kind"] = "paragraph";
                    slimr::Json inlines = slimr::json_arr();
                    for (const auto &span : c) {
                        slimr::Json inj = slimr::json_obj();
                        inj["kind"] = "text";
                        inj["text"] = span.text;
                        json::push_back(inlines, std::move(inj));
                    }
                    pb["inlines"] = std::move(inlines);
                    json::push_back(c_blocks, std::move(pb));
                    cj["blocks"] = std::move(c_blocks);
                    json::push_back(cells, std::move(cj));
                }
                rj["cells"] = std::move(cells);
                json::push_back(rows, std::move(rj));
            }
            tb["rows"] = std::move(rows);
            json::push_back(new_blocks, std::move(tb));
        }
    }

    auto *cur_blocks = json::get(tab, "blocks");
    if (!cur_blocks || *cur_blocks != new_blocks) {
        tab["blocks"] = std::move(new_blocks);
        report.changed = true;
    }
    return report;
}

} // namespace gdocsmith::lens
