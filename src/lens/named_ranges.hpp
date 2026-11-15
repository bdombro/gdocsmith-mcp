#pragma once

#include <string>
#include <vector>
#include <optional>
#include "directives.hpp"
#include "tokens.hpp"
#include "model/errors.hpp"
#include "model/layout.hpp"
#include "model/walk.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace gdocsmith::lens {

struct RangeMark {
    std::string edge; // "start" or "end"
    std::optional<std::string> key;
    std::string name;
    int offset{0};
    bool same{false};

    bool operator==(const RangeMark& other) const = default;
};

struct RangeCreate {
    struct Position {
        std::string key;
        int offset{0};
        bool operator==(const Position& other) const = default;
    };
    Position start;
    Position end;
    std::string name;

    bool operator==(const RangeCreate& other) const = default;
};

/** Marker tokens use kind=namedrange, name=... and exactly one start/end flag.
 * The parser owns token construction; marker offsets exclude the markers themselves.
 */
inline std::vector<RangeMark> rangeMarks(const std::vector<ProjSpan>& spans) {
    std::vector<RangeMark> out;
    int offset = 0;
    for (const auto& span : spans) {
        if (span.kind == "token" && span.token && span.token->kind == "namedrange") {
            const auto& attrs = span.token->attrs;
            const auto name = attrs.get_value("name");
            const bool start = attrs.has_flag("start"), end = attrs.has_flag("end");
            if (!name || start == end)
                throw model::CoreError("unsupportedSyntax", "named range marker needs a name and one edge");
            out.push_back({.edge = start ? "start" : "end", .name = *name, .offset = offset});
        } else {
            const size_t count = span.kind == "token" ? 1 : model::utf8CodePoints(span.text).size();
            if (count > static_cast<size_t>(std::numeric_limits<int>::max() - offset))
                throw model::CoreError("unsupportedSyntax", "named range marker offset is too large");
            offset += static_cast<int>(count);
        }
    }
    return out;
}

struct ShownRange {
    model::NamedRangeDef range;
    size_t segment{0};
};

/** Owning entries: callers may mutate the tab after taking this snapshot. */
inline std::vector<ShownRange> namedRangesShown(const model::TabModel& tab) {
    const auto paragraphs = model::paragraphsDeep(tab.blocks);
    std::vector<ShownRange> out;
    for (const auto& range : tab.namedRanges) {
        for (size_t i = 0; i < range.ranges.size(); ++i) {
            const auto& r = range.ranges[i];
            if (r.segmentId) continue;
            bool start = false, end = false;
            for (const auto& p : paragraphs) {
                if (!p.origin) continue;
                start |= r.start >= p.origin->start && r.start < p.origin->end;
                end |= r.end > p.origin->start && r.end <= p.origin->end;
            }
            if (start && end) out.push_back({range, i});
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        const auto& x = a.range.ranges[a.segment];
        const auto& y = b.range.ranges[b.segment];
        return x.start < y.start || (x.start == y.start && x.end < y.end);
    });
    return out;
}

/** Pair same-name markers FIFO, then manage only the exported range identities. */
inline std::set<std::string> namedRangesApply(
    model::TabModel& tab, const std::vector<RangeMark>& before,
    const std::vector<RangeMark>& after, const std::vector<ShownRange>& shown,
    const std::function<std::string()>& mint
) {
    struct Pair { RangeMark start; RangeMark end; size_t order; };
    auto pairs = [](const std::vector<RangeMark>& marks) {
        std::map<std::string, std::vector<size_t>> open;
        std::vector<Pair> out;
        for (size_t i = 0; i < marks.size(); ++i) {
            const auto& mark = marks[i];
            if (mark.offset < 0 || (mark.edge != "start" && mark.edge != "end"))
                throw model::CoreError("unsupportedSyntax", "invalid named range marker");
            auto& queue = open[mark.name];
            if (mark.edge == "start") queue.push_back(i);
            else {
                if (queue.empty())
                    throw model::CoreError("unsupportedSyntax", "named range end has no start before it");
                const auto at = queue.front();
                queue.erase(queue.begin());
                out.push_back({marks[at], mark, at});
            }
        }
        for (const auto& [name, queue] : open) {
            (void)name;
            if (!queue.empty())
                throw model::CoreError("unsupportedSyntax", "named range start has no end after it");
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
        return out;
    };
    const auto old = pairs(before);
    const auto written = pairs(after);
    if (old.size() != shown.size())
        throw model::CoreError("unsupportedSyntax", "named range marker snapshot does not match shown ranges");
    for (size_t i = 0; i < shown.size(); ++i) {
        const auto& entry = shown[i];
        if (entry.segment >= entry.range.ranges.size() || entry.range.ranges[entry.segment].segmentId ||
            old[i].start.name != entry.range.name)
            throw model::CoreError("unsupportedSyntax", "named range marker snapshot is not a shown body range");
    }
    std::set<std::string> managed, keep;
    std::map<std::string, model::NamedRangeDef::EdgePair> expected;
    std::vector<model::NamedRangeDef> creates;
    std::map<std::string, size_t> occurrence;
    for (const auto& entry : shown) managed.insert(entry.range.namedRangeId);
    std::vector<std::string> names;
    for (const auto& list : {old, written}) {
        for (const auto& pair : list) {
            if (std::find(names.begin(), names.end(), pair.start.name) == names.end())
                names.push_back(pair.start.name);
        }
    }
    std::vector<Pair> grouped;
    for (const auto& name : names)
        for (const auto& pair : written)
            if (pair.start.name == name) grouped.push_back(pair);
    for (const auto& pair : grouped) {
        if (pair.start.key && pair.start.key == pair.end.key && pair.start.offset == pair.end.offset)
            throw model::CoreError("unsupportedSyntax", "named range would be empty; remove its markers to delete it");
        const size_t wanted = occurrence[pair.start.name]++;
        size_t count = 0;
        const ShownRange* exported = nullptr;
        for (size_t i = 0; i < old.size(); ++i) {
            if (old[i].start.name == pair.start.name && count++ == wanted) {
                exported = &shown[i];
                break;
            }
        }
        if (exported && pair.start.same && pair.end.same) {
            keep.insert(exported->range.namedRangeId);
            if (pair.start.key && pair.end.key && exported->range.ranges.size() == 1) {
                expected[exported->range.namedRangeId] = {
                    .end = {*pair.end.key, pair.end.offset},
                    .start = {*pair.start.key, pair.start.offset}};
            }
            continue;
        }
        if (!pair.start.key || !pair.end.key)
            throw model::CoreError("unsupportedSyntax", "named ranges can only change in paragraphs and list items");
        model::NamedRangeDef created;
        created.name = pair.start.name;
        created.namedRangeId = mint();
        created.create = model::NamedRangeDef::EdgePair{
            .end = {*pair.end.key, pair.end.offset}, .start = {*pair.start.key, pair.start.offset}};
        creates.push_back(std::move(created));
    }
    for (const auto& range : tab.namedRanges) {
        if (managed.contains(range.namedRangeId) && range.ranges.size() > 1 && !keep.contains(range.namedRangeId))
            throw model::CoreError("unsupportedSyntax", "named range covers several separate spans and cannot be changed");
    }
    std::vector<model::NamedRangeDef> result;
    for (auto range : tab.namedRanges) {
        if (managed.contains(range.namedRangeId) && !keep.contains(range.namedRangeId)) continue;
        if (expected.contains(range.namedRangeId)) range.expect = expected.at(range.namedRangeId);
        result.push_back(std::move(range));
    }
    result.insert(result.end(), creates.begin(), creates.end());
    tab.namedRanges = std::move(result);
    return managed;
}

/** Symbol offsets are Unicode scalars/atoms; API indices are actual UTF-16 units.
 * Suggestions have a separate coordinate space: refuse rather than guess across hidden text.
 */
inline int rangeEdgeIndex(const model::TabModel& tab, const model::Layout& layout,
                          const model::NamedRangeDef::Edge& edge) {
    const auto paragraphs = model::paragraphsDeep(tab.blocks);
    const auto p = std::find_if(paragraphs.begin(), paragraphs.end(),
        [&](const auto& item) { return item.key == edge.key; });
    const auto at = layout.blockRanges.find(edge.key);
    if (p == paragraphs.end() || at == layout.blockRanges.end())
        throw model::CoreError("internal", "named range edge in a missing paragraph");
    const auto syms = model::paragraphSymbols(*p);
    if (edge.offset < 0 || static_cast<size_t>(edge.offset) > syms.size())
        throw model::CoreError("unsupportedSyntax", "named range offset is outside its paragraph");
    int index = at->second.start;
    for (const auto& sym : syms) {
        if (model::symHidden(sym))
            throw model::CoreError("unsupportedSyntax", "named range edges in suggestion text cannot be established");
    }
    for (int i = 0; i < edge.offset; ++i)
        index += syms[i].is_char() ? model::codePointUtf16Units(syms[i].ch) : syms[i].atom->length;
    return index;
}

/** Final indices for explicit marker intent. Absence means inconclusive, never success. */
inline std::optional<model::Range> namedRangeExpected(
    const model::TabModel& tab, const model::NamedRangeDef& range
) {
    const auto& edges = range.create ? range.create : range.expect;
    if (!edges) return std::nullopt;
    if (!range.create && (range.ranges.size() != 1 || range.ranges[0].segmentId))
        throw model::CoreError("unsupportedSyntax", "named range edge tracking requires one body span");
    const auto layout = model::layoutCompute(tab);
    const int start = rangeEdgeIndex(tab, layout, edges->start);
    const int end = rangeEdgeIndex(tab, layout, edges->end);
    if (start >= end)
        throw model::CoreError("unsupportedSyntax", "named range would be empty or reversed");
    return model::Range{.end = end, .start = start};
}

/** Exact single-span readback check for preserved IDs (creation IDs need reply resolution).
 * No expectation/multi-span/non-body => inconclusive. A false result is a mismatch.
 */
inline std::optional<bool> namedRangeVerify(
    const model::TabModel& final, const model::NamedRangeDef& intended,
    const model::TabModel& actual
) {
    if (intended.create || intended.ranges.size() != 1 || intended.ranges[0].segmentId)
        return std::nullopt;
    const auto expected = namedRangeExpected(final, intended);
    if (!expected) return std::nullopt;
    const auto found = std::find_if(actual.namedRanges.begin(), actual.namedRanges.end(),
        [&](const auto& r) { return r.namedRangeId == intended.namedRangeId; });
    if (found == actual.namedRanges.end() || found->name != intended.name || found->ranges.size() != 1)
        return false;
    const auto& r = found->ranges[0];
    return !r.segmentId && r.start == expected->start && r.end == expected->end;
}

/** Predict a retained body's edges through sequential text requests. Unknown structural
 * requests, non-body coordinates and a fully consumed range are inconclusive.
 * Insertions at either edge are outside the original range (Node/Docs contract).
 */
inline std::optional<model::Range> namedRangePredict(
    model::Range current, std::string_view tabId, const std::vector<model::JsonObject>& requests
) {
    for (const auto& request : requests) {
        const bool insert = request.contains("insertText");
        const bool erase = request.contains("deleteContentRange");
        if (!insert && !erase) {
            if (request.contains("updateTextStyle") || request.contains("updateParagraphStyle") ||
                request.contains("createNamedRange") || request.contains("deleteNamedRange")) continue;
            return std::nullopt;
        }
        const auto& op = request[insert ? "insertText" : "deleteContentRange"];
        const char* field = insert ? "location" : "range";
        if (!op.contains(field) || !op[field].is_object()) return std::nullopt;
        const auto& at = op[field];
        if (at.contains("tabId")) {
            if (!at["tabId"].is_string()) return std::nullopt;
            if (at["tabId"].get<std::string>() != tabId) continue;
        } else return std::nullopt; // no implicit first-tab assumption
        if (at.contains("segmentId")) return std::nullopt;
        auto index = [&](const char* key) -> std::optional<int> {
            if (!at.contains(key) || !at[key].is_number()) return std::nullopt;
            const auto value = at[key].get<int64_t>();
            if (value < 0 || value > std::numeric_limits<int>::max()) return std::nullopt;
            return static_cast<int>(value);
        };
        if (insert) {
            const auto position = index("index");
            if (!position || !op.contains("text") || !op["text"].is_string()) return std::nullopt;
            const int length = model::utf16CodeUnitLength(op["text"].get<std::string>());
            if (current.end > std::numeric_limits<int>::max() - length) return std::nullopt;
            if (*position <= current.start) {
                current.start += length;
                current.end += length;
            } else if (*position < current.end) current.end += length;
        } else {
            const auto start = index("startIndex"), end = index("endIndex");
            if (!start || !end || *start > *end) return std::nullopt;
            if (*start <= current.start && *end >= current.end) return std::nullopt;
            auto move = [&](int edge) { return edge <= *start ? edge : edge - (std::min(edge, *end) - *start); };
            current.start = move(current.start);
            current.end = move(current.end);
        }
    }
    return current;
}

} // namespace gdocsmith::lens
