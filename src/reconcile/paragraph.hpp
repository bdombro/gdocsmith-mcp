#pragma once

#include "diff/myers.hpp"
#include "model/errors.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/suggestions.hpp"
#include "model/symbols.hpp"
#include "model/types.hpp"
#include "reconcile/context.hpp"
#include "reconcile/requests.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::reconcile {

using namespace model;

struct ParagraphReconcileOptions {
    bool forceFullStyle = false;
    std::vector<std::string> skipFields;
    bool skipBullets = false;
    std::optional<ParagraphBlock> reBreak;
};

inline std::string namedOf(const JsonObject& style) {
    if (style.holds<slimr::Json::object_t>()) {
        const auto& obj = style.get<slimr::Json::object_t>();
        auto it = obj.find("namedStyleType");
        if (it != obj.end() && it->second.holds<std::string>()) {
            return it->second.get<std::string>();
        }
    }
    return "NORMAL_TEXT";
}

inline bool bulletsEqual(const ParagraphBlock& a, const ParagraphBlock& b) {
    std::string aId = a.bullet ? a.bullet->listId : "";
    std::string bId = b.bullet ? b.bullet->listId : "";
    int aLevel = a.bullet ? a.bullet->nestingLevel : 0;
    int bLevel = b.bullet ? b.bullet->nestingLevel : 0;
    return aId == bId && aLevel == bLevel;
}

inline JsonObject withoutLink(const JsonObject& style) {
    JsonObject out = style;
    if (out.holds<slimr::Json::object_t>()) {
        out.get<slimr::Json::object_t>().erase("link");
    }
    return out;
}

inline bool sameFields(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

inline std::vector<std::string> fieldsOf(const JsonObject& a, const JsonObject& b) {
    std::set<std::string> set;
    if (a.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : a.get<slimr::Json::object_t>()) set.insert(k);
    }
    if (b.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : b.get<slimr::Json::object_t>()) set.insert(k);
    }
    return std::vector<std::string>(set.begin(), set.end());
}

inline void textStyleRequest(
    ReconcileContext& ctx,
    int start,
    int end,
    const std::vector<std::string>& fields,
    const JsonObject& style,
    const RequestOrigin& origin
) {
    std::set<std::string> mask(fields.begin(), fields.end());
    if (mask.contains("link")) {
        mask.insert("underline");
        mask.insert("foregroundColor");
    }
    std::vector<std::string> sorted(mask.begin(), mask.end());
    requestPush(ctx, RequestBuilder::textStyleUpdate(start, end, pick(style, sorted), sorted, ctx.tabId), origin);
}

struct PendingLinkRun {
    int length = 0;
    JsonObject link;
    int offset = 0;
};

inline void pendingLinkRecord(
    ReconcileContext& ctx,
    std::string_view paragraphKey,
    const PendingLinkRun& run
) {
    if (!run.link.holds<slimr::Json::object_t>()) return;
    const auto& linkObj = run.link.get<slimr::Json::object_t>();
    auto it = linkObj.find("heading");
    if (it == linkObj.end() || !it->second.holds<slimr::Json::object_t>()) return;
    const auto& hObj = it->second.get<slimr::Json::object_t>();

    PendingLink pl;
    pl.paragraphKey = std::string(paragraphKey);
    pl.length = run.length;
    pl.offset = run.offset;
    if (auto itK = hObj.find("key"); itK != hObj.end() && itK->second.holds<std::string>()) {
        pl.headingKey = itK->second.get<std::string>();
    }
    if (auto itT = hObj.find("tabId"); itT != hObj.end() && itT->second.holds<std::string>()) {
        pl.tabId = itT->second.get<std::string>();
    }
    if (auto itD = hObj.find("docId"); itD != hObj.end() && itD->second.holds<std::string>()) {
        pl.docId = itD->second.get<std::string>();
    }
    ctx.pendingLinks.push_back(std::move(pl));
}

inline std::vector<std::optional<Sym>> keptOriginals(
    const std::vector<Sym>& oSyms,
    size_t fLength,
    const std::vector<diff::Hunk>& hunks
) {
    std::vector<std::optional<Sym>> out(fLength, std::nullopt);
    size_t ai = 0;
    size_t bi = 0;
    auto allHunks = hunks;
    allHunks.push_back(diff::Hunk{
        .aStart = static_cast<int>(oSyms.size()),
        .aEnd = static_cast<int>(oSyms.size()),
        .bStart = static_cast<int>(fLength),
        .bEnd = static_cast<int>(fLength)
    });
    for (const auto& h : allHunks) {
        while (ai < static_cast<size_t>(h.aStart)) {
            out[bi++] = oSyms[ai++];
        }
        ai = static_cast<size_t>(h.aEnd);
        bi = static_cast<size_t>(h.bEnd);
    }
    return out;
}

inline JsonObject textStylesEmit(
    int base,
    const std::vector<Sym>& fSyms,
    const std::vector<std::optional<Sym>>& kept,
    const JsonObject& newlineStyle,
    const ParagraphBlock& f,
    ReconcileContext& ctx,
    const RequestOrigin& origin
) {
    int textLength = symbolsUtf16Length(fSyms);
    JsonObject newline = newlineStyle;

    struct StyleGroup {
        int end = 0;
        std::vector<std::string> fields;
        int start = 0;
        JsonObject style;
    };
    std::optional<StyleGroup> group;

    auto flush = [&]() {
        if (!group) return;
        textStyleRequest(ctx, group->start, group->end, group->fields, group->style, origin);
        if (group->end == base + textLength) {
            std::vector<std::string> fields;
            for (const auto& fld : group->fields) {
                if (fld != "link") fields.push_back(fld);
            }
            if (!newline.holds<slimr::Json::object_t>()) newline = slimr::Json::object_t{};
            auto& nlObj = newline.get<slimr::Json::object_t>();
            const auto& stObj = group->style.holds<slimr::Json::object_t>() ? group->style.get<slimr::Json::object_t>() : slimr::Json::object_t{};
            for (const auto& fld : fields) {
                auto it = stObj.find(fld);
                if (it == stObj.end()) nlObj.erase(fld);
                else nlObj[fld] = it->second;
            }
        }
        group = std::nullopt;
    };

    int pos = base;
    std::optional<PendingLinkRun> pending;

    for (size_t i = 0; i < fSyms.size(); ++i) {
        const auto& sym = fSyms[i];
        int length = sym.is_atom() && sym.atom ? sym.atom->length : codePointUtf16Units(sym.ch);
        JsonObject style = (sym.is_atom() && sym.atom && sym.atom->style) ? *sym.atom->style : sym.style;

        std::optional<JsonObject> link;
        if (style.holds<slimr::Json::object_t>()) {
            const auto& stObj = style.get<slimr::Json::object_t>();
            auto it = stObj.find("link");
            if (it != stObj.end()) link = it->second;
        }

        std::optional<std::string> pendingKey;
        if (link && link->holds<slimr::Json::object_t>()) {
            const auto& lObj = link->get<slimr::Json::object_t>();
            auto itH = lObj.find("heading");
            if (itH != lObj.end() && itH->second.holds<slimr::Json::object_t>()) {
                const auto& hObj = itH->second.get<slimr::Json::object_t>();
                auto itK = hObj.find("key");
                if (itK != hObj.end() && itK->second.holds<std::string>()) {
                    pendingKey = itK->second.get<std::string>();
                }
            }
        }

        if (pending && (!pendingKey || !styleEqual(pending->link, *link))) {
            pendingLinkRecord(ctx, f.key, *pending);
            pending = std::nullopt;
        }
        if (pendingKey) {
            if (pending) pending->length += length;
            else if (link) pending = PendingLinkRun{.length = length, .link = *link, .offset = pos - base};
        }

        const auto& before = kept[i];
        bool isNewAtom = sym.is_atom() && !before;
        std::vector<std::string> fields;
        if (!isNewAtom) {
            if (!before) {
                fields = std::vector<std::string>(TEXT_STYLE_FIELDS.begin(), TEXT_STYLE_FIELDS.end());
            } else {
                JsonObject oldStyle = (before->is_atom() && before->atom && before->atom->style) ? *before->atom->style : before->style;
                fields = styleFieldsChanged(oldStyle, style, fieldsOf(oldStyle, style));
            }
        }

        JsonObject requestStyle = !pendingKey ? style : withoutLink(style);
        if (group && !fields.empty() && group->end == pos && sameFields(group->fields, fields) &&
            styleEqual(pick(group->style, fields), pick(requestStyle, fields))) {
            group->end += length;
        } else {
            flush();
            if (!fields.empty()) {
                group = StyleGroup{.end = pos + length, .fields = fields, .start = pos, .style = requestStyle};
            }
        }
        pos += length;
    }
    flush();
    if (pending) pendingLinkRecord(ctx, f.key, *pending);
    return newline;
}

inline void symbolsInsertEmit(
    int pos,
    const std::vector<Sym>& syms,
    ReconcileContext& ctx,
    const RequestOrigin& origin
) {
    struct Piece {
        bool isText = true;
        std::string text;
        Sym sym;
    };
    std::vector<Piece> pieces;
    for (const auto& sym : syms) {
        if (sym.is_char()) {
            if (!pieces.empty() && pieces.back().isText) {
                pieces.back().text += sym.ch;
            } else {
                pieces.push_back(Piece{.isText = true, .text = sym.ch, .sym = sym});
            }
        } else {
            pieces.push_back(Piece{.isText = false, .text = "", .sym = sym});
        }
    }

    for (auto it = pieces.rbegin(); it != pieces.rend(); ++it) {
        if (it->isText) {
            requestPush(ctx, RequestBuilder::insertTextAt(pos, it->text, ctx.tabId), origin);
            continue;
        }
        const auto& create = it->sym.atom ? it->sym.atom->create : std::nullopt;
        if (!create) {
            throw CoreError("atomMove", "an existing atom can't be inserted elsewhere (the API can't move content)");
        }
        if (create->type == "person") {
            requestPush(ctx, RequestBuilder::insertPerson(RequestBuilder::InsertPersonOpts{
                .email = create->email.value_or(""),
                .index = pos,
                .tabId = ctx.tabId
            }), origin);
        } else if (create->type == "date") {
            requestPush(ctx, RequestBuilder::insertDate(RequestBuilder::InsertDateOpts{
                .dateFormat = create->dateFormat,
                .index = pos,
                .locale = create->locale,
                .tabId = ctx.tabId,
                .timeFormat = create->timeFormat,
                .timeZoneId = create->timeZoneId,
                .timestamp = create->timestamp
            }), origin);
        } else if (create->type == "footnoteRef") {
            requestPush(ctx, RequestBuilder::createFootnote(pos, ctx.tabId), origin);
        } else if (create->type == "dropdown") {
            requestPush(ctx, RequestBuilder::insertDropdown(RequestBuilder::InsertDropdownOpts{
                .definitionId = create->definitionId.value_or(""),
                .index = pos,
                .optionId = create->optionId.value_or(""),
                .tabId = ctx.tabId
            }), origin);
        } else if (create->type == "richLink") {
            requestPush(ctx, RequestBuilder::insertRichLink(RequestBuilder::InsertRichLinkOpts{
                .index = pos,
                .tabId = ctx.tabId,
                .uri = create->uri.value_or("")
            }), origin);
        } else if (create->type == "image") {
            requestPush(ctx, RequestBuilder::insertInlineImage(RequestBuilder::InsertInlineImageOpts{
                .heightPt = create->heightPt,
                .index = pos,
                .tabId = ctx.tabId,
                .uri = create->uri.value_or(""),
                .widthPt = create->widthPt
            }), origin);
        } else {
            throw CoreError("internal", "page breaks are inserted by the container reconciler");
        }
    }
}

inline void newParagraphStylesEmit(
    int start,
    const ParagraphBlock& f,
    ReconcileContext& ctx
) {
    RequestOrigin origin{.key = f.key, .tabIndex = f.stamp ? std::optional<int>(f.stamp->tabIndex) : std::nullopt};
    auto fSyms = paragraphSymbols(f);
    int newlinePos = start + symbolsUtf16Length(fSyms);

    std::vector<std::string> fields;
    for (const auto& fld : PARAGRAPH_STYLE_FIELDS) {
        if (ctx.inCell && fld == "pageBreakBefore") continue;
        fields.push_back(std::string(fld));
    }
    JsonObject paragraphStyle = pick(f.style, fields);
    if (!paragraphStyle.contains("namedStyleType")) {
        paragraphStyle["namedStyleType"] = "NORMAL_TEXT";
    }
    if (f.bullet && (!ctx.bulletsNow.contains(f.key) || !ctx.bulletsNow[f.key].has_value())) {
        if (paragraphStyle.holds<slimr::Json::object_t>()) {
            paragraphStyle.get<slimr::Json::object_t>().erase("indentStart");
            paragraphStyle.get<slimr::Json::object_t>().erase("indentFirstLine");
        }
    }
    requestPush(ctx, RequestBuilder::paragraphStyleUpdate(start, newlinePos + 1, paragraphStyle, fields, ctx.tabId), origin);
    textStylesEmit(start, fSyms, std::vector<std::optional<Sym>>(fSyms.size(), std::nullopt), make_json_object(), f, ctx, origin);
    std::vector<std::string> txtFields(TEXT_STYLE_FIELDS.begin(), TEXT_STYLE_FIELDS.end());
    textStyleRequest(ctx, newlinePos, newlinePos + 1, txtFields, f.newline.style, origin);
}

inline std::optional<Range> suggestionRegion(
    const std::vector<Sym>& oSyms,
    int aStart,
    int aEnd,
    const std::vector<Sym>& inserted,
    int base
) {
    std::string key = marksKey(symMarks(inserted[0]));
    if (key.empty()) return std::nullopt;
    for (const auto& sym : inserted) {
        if (marksKey(symMarks(sym)) != key) return std::nullopt;
    }
    for (int i = aStart; i < aEnd; ++i) {
        if (marksKey(symMarks(oSyms[i])) != key) return std::nullopt;
    }
    int from = aStart;
    while (from > 0 && marksKey(symMarks(oSyms[from - 1])) == key) from--;
    int to = aEnd;
    while (to < static_cast<int>(oSyms.size()) && marksKey(symMarks(oSyms[to])) == key) to++;

    int start = base + symbolsUtf16Length(std::vector<Sym>(oSyms.begin(), oSyms.begin() + from));
    int len = symbolsUtf16Length(std::vector<Sym>(oSyms.begin() + from, oSyms.begin() + to));
    return Range{.end = start + len, .start = start};
}

inline std::optional<int> carryPosition(
    const std::vector<Range>& ranges,
    int start,
    int end,
    const std::optional<Sym>& first
) {
    if (end <= start || !first) return std::nullopt;
    auto it = std::find_if(ranges.begin(), ranges.end(), [&](const Range& r) {
        return r.start <= start && end <= r.end;
    });
    if (it == ranges.end()) return std::nullopt;
    if (start > it->start) return start;
    if (end < it->end) return end;
    int after = start + symbolsUtf16Length({*first});
    return after < end ? std::optional<int>(after) : std::nullopt;
}

inline void paragraphReconcile(
    const ParagraphBlock& o,
    const ParagraphBlock& f,
    ReconcileContext& ctx,
    ParagraphReconcileOptions opts = {}
) {
    if (!o.origin) {
        throw CoreError("internal", std::format("paragraph {} has no original range", o.key));
    }
    RequestOrigin origin{.key = f.key, .tabIndex = f.stamp ? std::optional<int>(f.stamp->tabIndex) : std::nullopt};
    size_t requestCount = ctx.requests.size();
    int base = o.origin->start;
    auto oSyms = paragraphSymbols(o);
    auto fSyms = paragraphSymbols(f);

    auto hidden = [](const Sym& sym) {
        return sym.is_char() && sym.ch == "\uE907";
    };

    bool hasHidden = std::any_of(oSyms.begin(), oSyms.end(), hidden);
    std::vector<std::string> oIds, fIds;
    for (const auto& s : oSyms) oIds.push_back(symbolId(s));
    for (const auto& s : fSyms) fIds.push_back(symbolId(s));

    std::vector<diff::Hunk> hunks;
    if (hasHidden) {
        hunks = diff::diffHunksValued(oIds, fIds, [&](size_t i) {
            return hidden(oSyms[i]) ? static_cast<double>(oSyms.size() + 1) : 1.0;
        });
    } else {
        hunks = diff::diffHunks(oIds, fIds);
    }

    for (auto it = hunks.rbegin(); it != hunks.rend(); ++it) {
        const auto& h = *it;
        int pos = base + symbolsUtf16Length(std::vector<Sym>(oSyms.begin(), oSyms.begin() + h.aStart));
        int end = pos + symbolsUtf16Length(std::vector<Sym>(oSyms.begin() + h.aStart, oSyms.begin() + h.aEnd));
        std::vector<Sym> inserted(fSyms.begin() + h.bStart, fSyms.begin() + h.bEnd);

        std::optional<Range> region;
        if (!inserted.empty() && h.aEnd > h.aStart) {
            region = suggestionRegion(oSyms, h.aStart, h.aEnd, inserted, base);
        }
        std::optional<int> at;
        if (!inserted.empty()) {
            std::vector<Range> checkRanges = region ? std::vector<Range>{*region} : ctx.carryRanges;
            std::optional<Sym> first = h.aStart < static_cast<int>(oSyms.size())
                ? std::optional<Sym>(oSyms[h.aStart])
                : std::nullopt;
            at = carryPosition(checkRanges, pos, end, first);
        }
        if (region && !at) {
            throw CoreError("internal", "a suggestion can't be carried through a one-character region");
        }
        if (at) {
            int length = symbolsUtf16Length(inserted);
            symbolsInsertEmit(*at, inserted, ctx, origin);
            if (*at < end) {
                requestPush(ctx, RequestBuilder::contentRangeDelete(*at + length, end + length, ctx.tabId), origin);
            }
            if (pos < *at) {
                requestPush(ctx, RequestBuilder::contentRangeDelete(pos, *at, ctx.tabId), origin);
            }
            ctx.deletedRanges.push_back(Range{.end = end, .start = pos});
            continue;
        }
        if (h.aEnd > h.aStart) {
            requestPush(ctx, RequestBuilder::contentRangeDelete(pos, end, ctx.tabId), origin);
            ctx.deletedRanges.push_back(Range{.end = end, .start = pos});
        }
        if (!inserted.empty()) {
            symbolsInsertEmit(pos, inserted, ctx, origin);
        }
    }

    int newlinePos = base + symbolsUtf16Length(fSyms);
    if (opts.reBreak) {
        requestPush(ctx, RequestBuilder::contentRangeDelete(newlinePos, newlinePos + 1, ctx.tabId), origin);
        ctx.deletedRanges.push_back(Range{.end = o.origin->end, .start = o.origin->end - 1});
        requestPush(ctx, RequestBuilder::insertTextAt(newlinePos, "\n", ctx.tabId), origin);
        opts.forceFullStyle = true;
    }

    JsonObject styleBefore = o.style;
    if (!opts.skipBullets) {
        if (o.bullet && !f.bullet) {
            requestPush(ctx, RequestBuilder::deleteParagraphBullets(base, newlinePos + 1, "", ctx.tabId), origin);
            int nesting = o.bullet->nestingLevel;
            styleBefore["indentStart"] = RequestBuilder::pt(36 * nesting);
            if (nesting > 0) {
                styleBefore["indentFirstLine"] = RequestBuilder::pt(36 * nesting);
            } else if (styleBefore.holds<slimr::Json::object_t>()) {
                styleBefore.get<slimr::Json::object_t>().erase("indentFirstLine");
            }
        } else if (f.bullet && !bulletsEqual(o, f)) {
            throw CoreError("internal", "adding or changing bullets belongs to the list reconciler (M10)");
        }
    }

    std::vector<std::string> rawFields;
    if (opts.forceFullStyle) {
        rawFields = std::vector<std::string>(PARAGRAPH_STYLE_FIELDS.begin(), PARAGRAPH_STYLE_FIELDS.end());
    } else {
        rawFields = styleFieldsChanged(styleBefore, f.style, PARAGRAPH_STYLE_FIELDS);
    }

    std::vector<std::string> fields;
    for (const auto& field : rawFields) {
        if (std::find(opts.skipFields.begin(), opts.skipFields.end(), field) != opts.skipFields.end()) continue;
        if (ctx.inCell && field == "pageBreakBefore") continue;
        if (field == "namedStyleType" && !opts.forceFullStyle && namedOf(styleBefore) == namedOf(f.style)) continue;
        fields.push_back(field);
    }

    bool hasNamedStyleType = std::find(fields.begin(), fields.end(), "namedStyleType") != fields.end();
    bool restyleAll = opts.forceFullStyle && hasNamedStyleType;
    if (!fields.empty()) {
        JsonObject paragraphStyle = pick(f.style, fields);
        if (hasNamedStyleType && !paragraphStyle.contains("namedStyleType")) {
            paragraphStyle["namedStyleType"] = "NORMAL_TEXT";
        }
        requestPush(ctx, RequestBuilder::paragraphStyleUpdate(base, newlinePos + 1, paragraphStyle, fields, ctx.tabId), origin);
    }

    std::vector<std::optional<Sym>> kept = restyleAll
        ? std::vector<std::optional<Sym>>(fSyms.size(), std::nullopt)
        : keptOriginals(oSyms, fSyms.size(), hunks);

    JsonObject newlineAfterText = textStylesEmit(
        base,
        fSyms,
        kept,
        restyleAll ? make_json_object() : o.newline.style,
        f,
        ctx,
        origin
    );

    std::vector<std::string> newlineFields = restyleAll
        ? std::vector<std::string>(TEXT_STYLE_FIELDS.begin(), TEXT_STYLE_FIELDS.end())
        : styleFieldsChanged(newlineAfterText, f.newline.style, fieldsOf(newlineAfterText, f.newline.style));

    if (!newlineFields.empty()) {
        textStyleRequest(ctx, newlinePos, newlinePos + 1, newlineFields, f.newline.style, origin);
    }

    if (opts.reBreak) {
        const auto& next = *opts.reBreak;
        int start = newlinePos + 1;
        int end = start + symbolsUtf16Length(paragraphSymbols(next)) + 1;
        std::vector<std::string> nextFields;
        for (const auto& field : PARAGRAPH_STYLE_FIELDS) {
            if (ctx.inCell && field == "pageBreakBefore") continue;
            nextFields.push_back(std::string(field));
        }
        JsonObject nextStyle = pick(next.style, nextFields);
        if (!nextStyle.contains("namedStyleType")) nextStyle["namedStyleType"] = "NORMAL_TEXT";
        requestPush(ctx, RequestBuilder::paragraphStyleUpdate(start, end, nextStyle, nextFields, ctx.tabId), RequestOrigin{
            .key = next.key,
            .tabIndex = next.stamp ? std::optional<int>(next.stamp->tabIndex) : std::nullopt
        });
    }

    if (o.is_protected && ctx.requests.size() > requestCount) {
        ctx.protectedTouches.push_back(o.key);
    }
}

} // namespace gdocsmith::reconcile
