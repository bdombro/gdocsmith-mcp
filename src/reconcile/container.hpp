#pragma once

#include "model/api_facts.hpp"
#include "model/errors.hpp"
#include "model/layout.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/suggestions.hpp"
#include "model/symbols.hpp"
#include "model/types.hpp"
#include "reconcile/context.hpp"
#include "reconcile/paragraph.hpp"
#include "reconcile/requests.hpp"
#include "reconcile/tables.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::reconcile {

using namespace model;

struct KeptPair {
    Block f;
    Block o;
    int fi = 0;
    int oi = 0;
};

struct KeptOverride {
    bool forceFullStyle = false;
    ParagraphBlock o;
};

struct InsertLocation {
    std::string kind; // "after" | "before" | "reuse"
    int pos = 0;
    std::optional<ParagraphBlock> inherit;
    std::optional<std::string> reusedKey;
};

inline Range originOf(const Block& block) {
    if (!block.origin) {
        throw CoreError("internal", std::format("block {} has no original range", block.key));
    }
    return *block.origin;
}

inline bool pageBreakParagraphIs(const ParagraphBlock& p) {
    return p.inlines.size() == 1 && p.inlines[0].is_atom() && p.inlines[0].type == "pageBreak";
}

inline void rangeDelete(int start, int end, ReconcileContext& ctx, const RequestOrigin& origin) {
    if (end <= start) return;
    requestPush(ctx, RequestBuilder::contentRangeDelete(start, end, ctx.tabId), origin);
    ctx.deletedRanges.push_back(Range{.end = end, .start = start});
}

inline void sectionStyleEmit(
    const JsonObject& before,
    const SectionBreakBlock& f,
    int index,
    ReconcileContext& ctx
) {
    std::set<std::string> allKeys;
    if (before.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : before.get<slimr::Json::object_t>()) allKeys.insert(k);
    }
    if (f.sectionStyle.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : f.sectionStyle.get<slimr::Json::object_t>()) allKeys.insert(k);
    }
    std::vector<std::string> fields;
    for (const auto& k : allKeys) {
        if (k == "sectionType") continue;
        if (k.ends_with("HeaderId") || k.ends_with("FooterId")) continue;
        JsonObject bVal = before.contains(k) ? before[k] : make_json_object();
        JsonObject fVal = f.sectionStyle.contains(k) ? f.sectionStyle[k] : make_json_object();
        if (!styleEqual(bVal, fVal)) {
            fields.push_back(k);
        }
    }
    std::sort(fields.begin(), fields.end());
    if (fields.empty()) return;

    JsonObject style = slimr::Json::object_t{};
    for (const auto& field : fields) {
        if (f.sectionStyle.contains(field)) style[field] = f.sectionStyle[field];
    }
    requestPush(ctx, RequestBuilder::sectionStyleUpdate(index + 1, index + 2, style, fields, ctx.tabId), RequestOrigin{.key = f.key});
}

inline void paragraphTextInsert(
    int pos,
    const std::vector<Sym>& syms,
    bool leadingNewline,
    bool trailingNewline,
    ReconcileContext& ctx,
    const RequestOrigin& origin
) {
    bool allChar = std::all_of(syms.begin(), syms.end(), [](const Sym& s) { return s.is_char(); });
    if (allChar) {
        std::string full;
        if (leadingNewline) full += "\n";
        for (const auto& s : syms) full += s.ch;
        if (trailingNewline) full += "\n";
        if (!full.empty()) {
            requestPush(ctx, RequestBuilder::insertTextAt(pos, full, ctx.tabId), origin);
        }
        return;
    }

    if (trailingNewline) requestPush(ctx, RequestBuilder::insertTextAt(pos, "\n", ctx.tabId), origin);
    symbolsInsertEmit(pos, syms, ctx, origin);
    if (leadingNewline) requestPush(ctx, RequestBuilder::insertTextAt(pos, "\n", ctx.tabId), origin);
}

void streamInsert(const std::vector<Block>& blocks, const InsertLocation& loc, ReconcileContext& ctx);

inline CellFill cellFillFor(ReconcileContext& ctx) {
    return [&ctx](const CellModel& cell, int contentStart) {
        inCellRun(ctx, [&]() {
            streamInsert(
                cell.blocks,
                InsertLocation{
                    .kind = "reuse",
                    .pos = contentStart,
                    .inherit = cellEmptyParagraph(cell.key, contentStart),
                    .reusedKey = std::nullopt
                },
                ctx
            );
        });
    };
}

inline void streamInsert(
    const std::vector<Block>& blocks,
    const InsertLocation& loc,
    ReconcileContext& ctx
) {
    int last = static_cast<int>(blocks.size()) - 1;
    bool fillSplit = false;

    for (int i = last; i >= 0; --i) {
        const auto& b = blocks[i];
        if (b.is_table() || b.is_section_break()) {
            if (loc.kind == "after") {
                throw CoreError("internal", "structure can't go in after a paragraph's text");
            }
            if (b.is_table()) {
                tableNewEmit(b.as_table(), loc.pos, ctx, cellFillFor(ctx));
            } else {
                const auto& sb = b.as_section_break();
                std::string sectionType = "CONTINUOUS";
                if (sb.create) {
                    sectionType = sb.create->sectionType;
                } else if (sb.sectionStyle.contains("sectionType") && sb.sectionStyle["sectionType"].holds<std::string>()) {
                    sectionType = sb.sectionStyle["sectionType"].get<std::string>();
                }
                requestPush(ctx, RequestBuilder::insertSectionBreak(RequestBuilder::InsertSectionBreakOpts{
                    .index = loc.pos,
                    .sectionType = sectionType,
                    .tabId = ctx.tabId
                }), RequestOrigin{.key = b.key});
                JsonObject defStyle = make_section_style_default();
                defStyle["sectionType"] = sectionType;
                sectionStyleEmit(defStyle, sb, loc.pos + 1, ctx);
            }
            fillSplit = true;
            continue;
        }

        if (!b.is_paragraph()) {
            throw CoreError("internal", "a table of contents can't be created");
        }

        const auto& p = b.as_paragraph();
        RequestOrigin origin{.key = b.key, .tabIndex = b.stamp ? std::optional<int>(b.stamp->tabIndex) : std::nullopt};
        auto syms = paragraphSymbols(p);
        bool pageBreak = pageBreakParagraphIs(p);

        if (fillSplit) {
            if (pageBreak) throw CoreError("internal", "a new table or section break can't follow a page break (S1)");
            paragraphTextInsert(loc.pos, syms, false, false, ctx, origin);
            fillSplit = false;
        } else if (loc.kind == "after") {
            if (pageBreak) {
                if (i == last) throw CoreError("internal", "a new page break can't be the last paragraph inserted after another");
                requestPush(ctx, RequestBuilder::insertPageBreak(loc.pos + 1, "", ctx.tabId), origin);
            } else {
                paragraphTextInsert(loc.pos, syms, true, false, ctx, origin);
            }
        } else if (pageBreak) {
            if (loc.kind == "reuse" && i == last) {
                throw CoreError("internal", "a new page break can't reuse a deleted paragraph's newline");
            }
            requestPush(ctx, RequestBuilder::insertPageBreak(loc.pos, "", ctx.tabId), origin);
        } else {
            bool trailingNewline = !(loc.kind == "reuse" && i == last);
            paragraphTextInsert(loc.pos, syms, false, trailingNewline, ctx, origin);
        }
    }

    if (fillSplit) {
        throw CoreError("internal", "a new table or section break must follow a new paragraph (S1)");
    }

    const auto& lastBlock = blocks[last];
    if (loc.kind == "reuse" && loc.reusedKey && loc.inherit && loc.inherit->headingId && lastBlock.is_paragraph()) {
        ctx.identityTransfers.push_back(IdentityTransfer{
            .fromKey = *loc.reusedKey,
            .headingId = loc.inherit->headingId,
            .tabId = ctx.tabId,
            .toKey = lastBlock.key
        });
    }

    int start = (loc.kind == "after") ? loc.pos + 1 : loc.pos;
    for (const auto& b : blocks) {
        if (b.is_paragraph()) {
            const auto& p = b.as_paragraph();
            ctx.bulletsNow[p.key] = loc.inherit ? loc.inherit->bullet : std::nullopt;
            newParagraphStylesEmit(start, p, ctx);
        }
        start += blockLength(b);
    }
}

struct NewlineJoinResult {
    ParagraphBlock next;
    ParagraphBlock o;
};

inline std::optional<NewlineJoinResult> newlineJoin(
    const KeptPair* a,
    const Block& nextBlock
) {
    if (!a || !a->o.is_paragraph() || !a->f.is_paragraph() || !nextBlock.is_paragraph() || a->o.as_paragraph().inlines.empty()) {
        return std::nullopt;
    }
    const auto& aO = a->o.as_paragraph();
    const auto& aF = a->f.as_paragraph();
    const auto& next = nextBlock.as_paragraph();

    auto ids = [](const std::vector<Sym>& syms) {
        std::string s;
        for (size_t i = 0; i < syms.size(); ++i) {
            if (i > 0) s += "\u0002";
            s += symbolId(syms[i]);
        }
        return s;
    };

    auto joined = paragraphSymbols(aO);
    auto nextSyms = paragraphSymbols(next);
    joined.insert(joined.end(), nextSyms.begin(), nextSyms.end());

    if (ids(joined) != ids(paragraphSymbols(aF)) || marksKey(next.newline) != marksKey(aF.newline)) {
        return std::nullopt;
    }
    if (marksKey(aO.newline) == marksKey(aF.newline)) return std::nullopt;

    return NewlineJoinResult{.next = next, .o = aO};
}

inline bool reBreakNeeded(const ParagraphBlock& o, const ParagraphBlock& f) {
    auto ids = [](const ParagraphBlock& p) {
        std::vector<std::string> keys;
        if (p.suggestionsRaw && p.suggestionsRaw->holds<slimr::Json::object_t>()) {
            const auto& obj = p.suggestionsRaw->get<slimr::Json::object_t>();
            if (obj.contains("suggestedParagraphStyleChanges") && obj.at("suggestedParagraphStyleChanges").holds<slimr::Json::object_t>()) {
                for (const auto& [k, _] : obj.at("suggestedParagraphStyleChanges").get<slimr::Json::object_t>()) keys.push_back(k);
            }
            if (obj.contains("suggestedBulletChanges") && obj.at("suggestedBulletChanges").holds<slimr::Json::object_t>()) {
                for (const auto& [k, _] : obj.at("suggestedBulletChanges").get<slimr::Json::object_t>()) keys.push_back(k);
            }
        }
        std::sort(keys.begin(), keys.end());
        std::string s;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) s += ",";
            s += keys[i];
        }
        return s;
    };
    return ids(o) != ids(f) || marksKey(o.newline) != marksKey(f.newline);
}

void containerReconcile(
    const std::vector<Block>& oBlocks,
    const std::vector<Block>& fBlocks,
    ReconcileContext& ctx
);

inline void keptReconcile(
    const KeptPair& pair,
    const std::optional<KeptOverride>& override,
    ReconcileContext& ctx,
    const ParagraphBlock* next = nullptr
) {
    const auto& f = pair.f;
    const auto& o = pair.o;

    if (o.is_paragraph() && f.is_paragraph()) {
        const auto& before = override ? override->o : o.as_paragraph();
        const auto& fP = f.as_paragraph();
        ctx.bulletsNow[fP.key] = before.bullet;

        std::vector<std::string> skipFields;
        if (!bulletSame(before.bullet, fP.bullet)) {
            skipFields.push_back("indentFirstLine");
            skipFields.push_back("indentStart");
        }
        bool reBreak = reBreakNeeded(before, fP);
        if (reBreak && !next) {
            throw CoreError(
                "suggestionConflict",
                "rejecting a paragraph's paragraph-style or paragraph-break suggestion re-breaks it, so keep the paragraph right after it in place and unchanged in kind"
            );
        }
        ParagraphReconcileOptions opts;
        opts.forceFullStyle = override ? override->forceFullStyle : false;
        if (reBreak && next) opts.reBreak = *next;
        opts.skipBullets = true;
        opts.skipFields = std::move(skipFields);
        paragraphReconcile(before, fP, ctx, std::move(opts));
    } else if (o.is_table() && f.is_table()) {
        tableReconcile(
            o.as_table(),
            f.as_table(),
            ctx,
            [&ctx](const CellModel& oc, const CellModel& fc) {
                inCellRun(ctx, [&]() {
                    containerReconcile(oc.blocks, fc.blocks, ctx);
                });
            },
            cellFillFor(ctx)
        );
    } else if (o.is_section_break() && f.is_section_break()) {
        Range range = originOf(o);
        sectionStyleEmit(o.as_section_break().sectionStyle, f.as_section_break(), range.start, ctx);
    }
}

inline std::optional<KeptOverride> gapReconcile(
    const KeptPair* a,
    const KeptPair* q,
    const std::vector<Block>& deleted,
    const std::vector<Block>& inserted,
    ReconcileContext& ctx
) {
    if (deleted.empty() && inserted.empty()) return std::nullopt;

    const ParagraphBlock* aO = (a && a->o.is_paragraph()) ? &a->o.as_paragraph() : nullptr;
    const ParagraphBlock* qO = (q && q->o.is_paragraph()) ? &q->o.as_paragraph() : nullptr;
    const ParagraphBlock* qF = (q && q->f.is_paragraph()) ? &q->f.as_paragraph() : nullptr;

    bool protectedNewline = !qO;
    RequestOrigin origin;
    if (!inserted.empty()) {
        origin.key = inserted[0].key;
        if (inserted[0].stamp) origin.tabIndex = inserted[0].stamp->tabIndex;
    } else if (!deleted.empty()) {
        origin.key = deleted[0].key;
    }

    std::optional<int> beforePos;
    if (qO) beforePos = originOf(*qO).start;

    auto joined = (deleted.size() == 1 && inserted.empty()) ? newlineJoin(a, deleted[0]) : std::nullopt;
    if (joined) {
        Range aOrigin = originOf(joined->o);
        rangeDelete(aOrigin.end - 1, aOrigin.end, ctx, origin);
        ParagraphBlock newO = joined->o;
        newO.inlines.insert(newO.inlines.end(), joined->next.inlines.begin(), joined->next.inlines.end());
        newO.newline = joined->next.newline;
        newO.origin = Range{.end = originOf(joined->next).end - 1, .start = aOrigin.start};
        return KeptOverride{.forceFullStyle = false, .o = std::move(newO)};
    }

    if (!deleted.empty()) {
        int first = originOf(deleted[0]).start;
        const auto& last = deleted.back();
        int lastEnd = originOf(last).end;

        if (!protectedNewline) {
            rangeDelete(first, lastEnd, ctx, origin);
            beforePos = first;
        } else if (!inserted.empty()) {
            if (!last.is_paragraph()) {
                throw CoreError("internal", "the block before a protected newline must be a paragraph (V1/V2)");
            }
            rangeDelete(first, lastEnd - 1, ctx, origin);
            streamInsert(
                inserted,
                InsertLocation{
                    .kind = "reuse",
                    .pos = first,
                    .inherit = last.as_paragraph(),
                    .reusedKey = last.key
                },
                ctx
            );
            return std::nullopt;
        } else {
            if (!aO || !a || !last.is_paragraph()) {
                throw CoreError("internal", "a merge needs a kept paragraph before the deleted blocks");
            }
            Range aOrigin = originOf(*aO);
            rangeDelete(aOrigin.end - 1, lastEnd - 1, ctx, origin);
            bool aEmpty = (aOrigin.end - aOrigin.start == 1);
            if (!aEmpty) {
                ParagraphBlock newO = *aO;
                newO.newline.style = last.as_paragraph().newline.style;
                return KeptOverride{.forceFullStyle = false, .o = std::move(newO)};
            }
            const auto& lastP = last.as_paragraph();
            if (lastP.headingId) {
                ctx.identityTransfers.push_back(IdentityTransfer{
                    .fromKey = lastP.key,
                    .headingId = lastP.headingId,
                    .tabId = ctx.tabId,
                    .toKey = a->f.key
                });
            }
            ParagraphBlock newO = *aO;
            newO.bullet = lastP.bullet;
            newO.headingId = lastP.headingId;
            newO.newline = lastP.newline;
            newO.style = lastP.style;
            return KeptOverride{.forceFullStyle = true, .o = std::move(newO)};
        }
    }

    if (inserted.empty()) return std::nullopt;

    int split = static_cast<int>(inserted.size());
    while (split > 0 && qF && qF->bullet && inserted[split - 1].is_paragraph() &&
           bulletSame(inserted[split - 1].as_paragraph().bullet, qF->bullet)) {
        split--;
    }

    std::vector<Block> head(inserted.begin(), inserted.begin() + split);
    std::vector<Block> tail(inserted.begin() + split, inserted.end());

    std::optional<ParagraphBlock> qNow;
    if (qF) {
        ParagraphBlock qCopy = *qF;
        auto it = ctx.bulletsNow.find(qF->key);
        if (it != ctx.bulletsNow.end()) qCopy.bullet = it->second;
        qNow = std::move(qCopy);
    }

    if (!tail.empty() && beforePos) {
        streamInsert(tail, InsertLocation{.kind = "before", .pos = *beforePos, .inherit = qNow, .reusedKey = std::nullopt}, ctx);
    }
    if (head.empty()) return std::nullopt;

    bool hasStructure = std::any_of(head.begin(), head.end(), [](const Block& b) { return !b.is_paragraph(); });
    const auto& first = head[0];
    bool joinsA = aO && aO->bullet && first.is_paragraph() && bulletSame(first.as_paragraph().bullet, aO->bullet);

    bool endsInPageBreak = head.back().is_paragraph() && pageBreakParagraphIs(head.back().as_paragraph());

    if (beforePos && (!aO || hasStructure || endsInPageBreak || !(protectedNewline || joinsA))) {
        streamInsert(head, InsertLocation{.kind = "before", .pos = *beforePos, .inherit = qNow, .reusedKey = std::nullopt}, ctx);
        return std::nullopt;
    }

    if (!aO) throw CoreError("internal", "no paragraph to insert next to");

    if (!hasStructure) {
        streamInsert(head, InsertLocation{.kind = "after", .pos = originOf(*aO).end - 1, .inherit = *aO, .reusedKey = std::nullopt}, ctx);
    } else {
        int pos = originOf(*aO).end - 1;
        requestPush(ctx, RequestBuilder::insertTextAt(pos, "\n", ctx.tabId), origin);
        streamInsert(head, InsertLocation{.kind = "reuse", .pos = pos + 1, .inherit = *aO, .reusedKey = std::nullopt}, ctx);
    }

    JsonObject newlineStyle = aO->newline.style;
    if (!aO->inlines.empty()) {
        const auto& lastInline = aO->inlines.back();
        if (lastInline.style.holds<slimr::Json::object_t>()) {
            newlineStyle = lastInline.style;
        }
    }
    ParagraphBlock newO = *aO;
    newO.newline.style = std::move(newlineStyle);
    return KeptOverride{.forceFullStyle = false, .o = std::move(newO)};
}

inline void containerReconcile(
    const std::vector<Block>& oBlocks,
    const std::vector<Block>& fBlocks,
    ReconcileContext& ctx
) {
    std::map<std::string, int> oIndex;
    for (size_t i = 0; i < oBlocks.size(); ++i) {
        oIndex[oBlocks[i].key] = static_cast<int>(i);
    }

    std::vector<KeptPair> kept;
    for (size_t fi = 0; fi < fBlocks.size(); ++fi) {
        const auto& f = fBlocks[fi];
        auto it = oIndex.find(f.key);
        if (it == oIndex.end()) continue;
        int oi = it->second;
        if (!kept.empty() && oi <= kept.back().oi) {
            throw CoreError("internal", std::format("block {} moved; a move must be a delete plus a create", f.key));
        }
        kept.push_back(KeptPair{.f = f, .o = oBlocks[oi], .fi = static_cast<int>(fi), .oi = oi});
    }

    for (int g = static_cast<int>(kept.size()); g >= 0; --g) {
        const KeptPair* a = (g > 0) ? &kept[g - 1] : nullptr;
        const KeptPair* q = (g < static_cast<int>(kept.size())) ? &kept[g] : nullptr;

        int delStart = a ? a->oi + 1 : 0;
        int delEnd = q ? q->oi : static_cast<int>(oBlocks.size());
        std::vector<Block> deleted(oBlocks.begin() + delStart, oBlocks.begin() + delEnd);

        int insStart = a ? a->fi + 1 : 0;
        int insEnd = q ? q->fi : static_cast<int>(fBlocks.size());
        std::vector<Block> inserted(fBlocks.begin() + insStart, fBlocks.begin() + insEnd);

        auto override = gapReconcile(a, q, deleted, inserted, ctx);
        const ParagraphBlock* next = (q && deleted.empty() && inserted.empty() && q->f.is_paragraph())
            ? &q->f.as_paragraph()
            : nullptr;

        if (a) {
            keptReconcile(*a, override, ctx, next);
        }
    }
}

} // namespace gdocsmith::reconcile
