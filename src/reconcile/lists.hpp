#pragma once

#include "model/errors.hpp"
#include "model/layout.hpp"
#include "model/lists.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"
#include "reconcile/context.hpp"
#include "reconcile/requests.hpp"

#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::reconcile {

using namespace model;

inline const std::vector<std::string> INDENT_FIELDS = {"indentFirstLine", "indentStart"};

inline const ParagraphBlock* paragraphAt(const std::vector<Block>& blocks, int i) {
    if (i < 0 || static_cast<size_t>(i) >= blocks.size()) return nullptr;
    const auto& b = blocks[i];
    return b.is_paragraph() ? &b.as_paragraph() : nullptr;
}

inline Range rangeOf(const std::unordered_map<std::string, Range>& ranges, const Block& block) {
    auto it = ranges.find(block.key);
    if (it == ranges.end()) {
        throw CoreError("internal", std::format("no layout for block {}", block.key));
    }
    return it->second;
}

inline RequestOrigin paragraphRequestOrigin(const ParagraphBlock& p) {
    return RequestOrigin{.key = p.key, .tabIndex = p.stamp ? std::optional<int>(p.stamp->tabIndex) : std::nullopt};
}

inline JsonObject unbulletedIndents(int nesting) {
    if (nesting == 0) {
        return slimr::Json::object_t{
            {"indentStart", RequestBuilder::pt(0)}
        };
    }
    auto indent = RequestBuilder::pt(36 * nesting);
    return slimr::Json::object_t{
        {"indentFirstLine", indent},
        {"indentStart", indent}
    };
}

inline void indentsRestore(
    const ParagraphBlock& m,
    const JsonObject& after,
    const std::unordered_map<std::string, Range>& ranges,
    ReconcileContext& ctx
) {
    std::vector<std::string> fields;
    for (const auto& field : INDENT_FIELDS) {
        JsonObject afterVal = after.contains(field) ? after[field] : make_json_object();
        JsonObject mVal = m.style.contains(field) ? m.style[field] : make_json_object();
        if (!styleEqual(afterVal, mVal)) {
            fields.push_back(field);
        }
    }
    if (fields.empty()) return;

    JsonObject style = slimr::Json::object_t{};
    for (const auto& field : fields) {
        if (m.style.contains(field)) {
            style[field] = m.style[field];
        }
    }
    Block b(m);
    Range range = rangeOf(ranges, b);
    requestPush(ctx, RequestBuilder::paragraphStyleUpdate(range.start, range.end, style, fields, ctx.tabId), paragraphRequestOrigin(m));
}

inline void bulletsRemove(
    const std::vector<ParagraphBlock>& members,
    const std::unordered_map<std::string, Range>& ranges,
    ReconcileContext& ctx,
    const auto& now
) {
    Block firstB(members.front());
    Block lastB(members.back());
    Range first = rangeOf(ranges, firstB);
    Range last = rangeOf(ranges, lastB);

    requestPush(
        ctx,
        RequestBuilder::deleteParagraphBullets(first.start, last.end, "", ctx.tabId),
        paragraphRequestOrigin(members.front())
    );
    for (const auto& m : members) {
        auto nowM = now(m);
        indentsRestore(m, unbulletedIndents(nowM ? nowM->nestingLevel : 0), ranges, ctx);
    }
}

inline void bulletsCreate(
    const std::vector<ParagraphBlock>& members,
    std::string_view preset,
    const std::vector<JsonObject>& levels,
    const std::unordered_map<std::string, Range>& ranges,
    ReconcileContext& ctx,
    const auto& now
) {
    Block firstB(members.front());
    Block lastB(members.back());
    Range first = rangeOf(ranges, firstB);
    Range last = rangeOf(ranges, lastB);
    RequestOrigin origin = paragraphRequestOrigin(members.front());

    bool hasAnyNow = false;
    for (const auto& m : members) {
        if (now(m).has_value()) {
            hasAnyNow = true;
            break;
        }
    }

    if (hasAnyNow) {
        requestPush(ctx, RequestBuilder::deleteParagraphBullets(first.start, last.end, "", ctx.tabId), origin);
        for (const auto& m : members) {
            auto nowM = now(m);
            if (!nowM || nowM->nestingLevel == 0) continue;
            Block b(m);
            Range range = rangeOf(ranges, b);
            requestPush(
                ctx,
                RequestBuilder::paragraphStyleUpdate(range.start, range.end, make_json_object(), INDENT_FIELDS, ctx.tabId),
                paragraphRequestOrigin(m)
            );
        }
    }

    int tabs = 0;
    for (auto it = members.rbegin(); it != members.rend(); ++it) {
        const auto& m = *it;
        int nesting = m.bullet ? m.bullet->nestingLevel : 0;
        if (nesting > 0) {
            Block b(m);
            requestPush(
                ctx,
                RequestBuilder::insertTextAt(rangeOf(ranges, b).start, std::string(nesting, '\t'), ctx.tabId),
                paragraphRequestOrigin(m)
            );
            tabs += nesting;
        }
    }

    requestPush(
        ctx,
        RequestBuilder::createParagraphBullets(first.start, last.end + tabs, preset, "", ctx.tabId),
        origin
    );

    for (const auto& m : members) {
        int nesting = m.bullet ? m.bullet->nestingLevel : 0;
        JsonObject level = (nesting < static_cast<int>(levels.size())) ? levels[nesting] : make_json_object();
        JsonObject after = slimr::Json::object_t{};
        for (const auto& field : INDENT_FIELDS) {
            if (level.contains(field)) after[field] = level[field];
        }
        indentsRestore(m, after, ranges, ctx);

        std::string spacingMode;
        if (m.style.contains("spacingMode") && m.style["spacingMode"].holds<std::string>()) {
            spacingMode = m.style["spacingMode"].get<std::string>();
        }
        if (spacingMode != "COLLAPSE_LISTS") {
            Block b(m);
            Range range = rangeOf(ranges, b);
            JsonObject style = slimr::Json::object_t{};
            if (!spacingMode.empty()) style["spacingMode"] = spacingMode;
            requestPush(
                ctx,
                RequestBuilder::paragraphStyleUpdate(range.start, range.end, style, {"spacingMode"}, ctx.tabId),
                paragraphRequestOrigin(m)
            );
        }
    }
}

inline CoreError listRefusal(
    const TabModel& tab,
    const ParagraphBlock& paragraph,
    std::string_view message,
    const ParagraphBlock* predecessor = nullptr
) {
    auto label = [](const ParagraphBlock& block) {
        std::string s;
        for (const auto& inlineItem : block.inlines) {
            if (inlineItem.is_text()) s += inlineItem.text;
        }
        if (s.size() > 120) s = s.substr(0, 120);
        return glz::write_json(s).value_or(std::format("\"{}\"", s));
    };
    std::string location = std::format("tab {} ({}), paragraph {}", glz::write_json(tab.title).value_or(tab.title), tab.tabId, label(paragraph));
    std::string previous = predecessor ? std::format("; after {}", label(*predecessor)) : "";

    slimr::Json::object_t details{
        {"key", paragraph.key},
        {"tabId", tab.tabId}
    };
    if (paragraph.bullet) details["listId"] = paragraph.bullet->listId;
    if (predecessor) details["predecessorKey"] = predecessor->key;
    if (paragraph.stamp) details["tabIndex"] = static_cast<int64_t>(paragraph.stamp->tabIndex);

    return CoreError("unrealizableList", std::format("{}\n{}{}", message, location, previous), std::move(details));
}

inline std::optional<int> nestedSpanEnd(
    const TabModel& tab,
    const std::vector<Block>& blocks,
    std::string_view listId,
    int start
) {
    int last = start;
    for (int i = start; i < static_cast<int>(blocks.size()); ++i) {
        const auto* p = paragraphAt(blocks, i);
        if (p && p->bullet && p->bullet->listId == listId) last = i;
    }
    int depth = 0;
    for (int i = start; i <= last; ++i) {
        const auto* p = paragraphAt(blocks, i);
        if (!p || !p->bullet) return std::nullopt;
        if (p->bullet->listId == listId) {
            depth = p->bullet->nestingLevel;
        } else if (p->bullet->nestingLevel <= depth || !tab.lists.contains(p->bullet->listId)) {
            return std::nullopt;
        }
    }
    return last;
}

inline bool listSeenElsewhere(
    const std::vector<Block>& blocks,
    std::string_view listId,
    int start,
    int end
) {
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if ((i < start || i > end) && blocks[i].is_paragraph()) {
            const auto& p = blocks[i].as_paragraph();
            if (p.bullet && p.bullet->listId == listId) return true;
        }
    }
    return false;
}

inline void containerBulletsReconcile(
    const TabModel& tab,
    const std::vector<Block>& blocks,
    const std::unordered_map<std::string, Range>& ranges,
    ReconcileContext& ctx
) {
    auto now = [&](const ParagraphBlock& p) -> std::optional<BulletRef> {
        auto it = ctx.bulletsNow.find(p.key);
        if (it != ctx.bulletsNow.end()) return it->second;
        return p.bullet;
    };

    int i = 0;
    int n = static_cast<int>(blocks.size());
    while (i < n) {
        const auto* p = paragraphAt(blocks, i);
        if (!p || bulletSame(now(*p), p->bullet)) {
            i++;
            continue;
        }
        if (!p->bullet) {
            int end = i;
            while (end + 1 < n) {
                const auto* nextP = paragraphAt(blocks, end + 1);
                if (nextP && !nextP->bullet && now(*nextP).has_value()) {
                    end++;
                } else {
                    break;
                }
            }
            std::vector<ParagraphBlock> members;
            for (int k = i; k <= end; ++k) members.push_back(blocks[k].as_paragraph());
            bulletsRemove(members, ranges, ctx, now);
            i = end + 1;
            continue;
        }

        std::string listId = p->bullet->listId;
        bool isNew = listId.rfind("new:", 0) == 0;
        int groupEnd = i;
        while (groupEnd + 1 < n) {
            const auto* next = paragraphAt(blocks, groupEnd + 1);
            if (!next || !next->bullet || next->bullet->listId != listId || bulletSame(now(*next), next->bullet)) {
                break;
            }
            groupEnd++;
        }

        std::vector<ParagraphBlock> group;
        for (int k = i; k <= groupEnd; ++k) group.push_back(blocks[k].as_paragraph());

        const ListDef* def = tab.lists.contains(listId) ? &tab.lists.at(listId) : nullptr;
        std::string preset;
        if (def && def->preset) {
            preset = *def->preset;
        } else {
            std::string kind = def ? listKind(*def, 0) : "bullet";
            preset = defaultPresetForKind(kind);
        }

        const auto* pred = paragraphAt(blocks, i - 1);
        std::optional<std::string> predPreset;
        if (pred && pred->bullet && tab.lists.contains(pred->bullet->listId)) {
            predPreset = tab.lists.at(pred->bullet->listId).preset;
        }
        bool predSwallows = pred && pred->bullet && predPreset == preset;

        bool groupAllNesting0 = std::all_of(group.begin(), group.end(), [](const ParagraphBlock& m) {
            return m.bullet && m.bullet->nestingLevel == 0;
        });

        if (!isNew && pred && pred->bullet && pred->bullet->listId == listId &&
            pred->bullet->nestingLevel == 0 && predSwallows && groupAllNesting0) {
            bulletsCreate(group, preset, def ? def->nestingLevels : std::vector<JsonObject>{}, ranges, ctx, now);
            i = groupEnd + 1;
            continue;
        }

        // Rebuild the list's whole contiguous run.
        int runStart = i;
        while (!isNew && runStart > 0) {
            const auto* prev = paragraphAt(blocks, runStart - 1);
            if (prev && prev->bullet && prev->bullet->listId == listId) runStart--;
            else break;
        }
        int runEnd = groupEnd;
        while (runEnd + 1 < n) {
            const auto* nxt = paragraphAt(blocks, runEnd + 1);
            if (nxt && nxt->bullet && nxt->bullet->listId == listId) runEnd++;
            else break;
        }

        const auto* runPred = paragraphAt(blocks, runStart - 1);
        std::optional<std::string> runPredPreset;
        if (runPred && runPred->bullet && tab.lists.contains(runPred->bullet->listId)) {
            runPredPreset = tab.lists.at(runPred->bullet->listId).preset;
        }
        if (runPred && runPred->bullet && runPredPreset == preset) {
            throw listRefusal(
                tab,
                blocks[runStart].as_paragraph(),
                "this list directly follows another list of the same style, so Google Docs would merge them; put a paragraph between them or change one list's style",
                runPred
            );
        }

        std::vector<ParagraphBlock> run;
        for (int k = runStart; k <= runEnd; ++k) run.push_back(blocks[k].as_paragraph());

        if (runPred && runPred->bullet && (run[0].bullet ? run[0].bullet->nestingLevel : 0) > 0) {
            throw listRefusal(
                tab,
                run[0],
                "Google Docs can't start a new list at a nested level right after another list; put a paragraph between them",
                runPred
            );
        }

        if (!isNew) {
            std::vector<std::string> keys;
            for (const auto& m : run) keys.push_back(m.key);
            ctx.listRebuilds.push_back(ListRebuild{
                .keys = std::move(keys),
                .listId = listId,
                .lossy = !def || !def->preset
            });
        } else if (listSeenElsewhere(blocks, listId, runStart, runEnd)) {
            std::string msg = !nestedSpanEnd(tab, blocks, listId, runStart).has_value()
                ? "a new list's items must be consecutive"
                : "Google Docs can't nest a different kind of list inside a new list; nest items of the same kind";
            throw listRefusal(tab, run[0], msg);
        }

        std::vector<JsonObject> presetLevels;
        const auto& pTable = listPresetTable();
        auto itPreset = pTable.find(preset);
        if (itPreset != pTable.end()) {
            for (const auto& lvl : itPreset->second) {
                slimr::Json j;
                if (!glz::read_json(j, glz::write_json(lvl).value_or("{}"))) {
                    presetLevels.push_back(j);
                }
            }
        }

        bulletsCreate(run, preset, (def && !def->nestingLevels.empty()) ? def->nestingLevels : presetLevels, ranges, ctx, now);
        i = runEnd + 1;
    }
}

inline void bulletsReconcile(
    const TabModel& finalTab,
    ReconcileContext& ctx
) {
    auto layout = layoutCompute(finalTab);
    for (const auto& blocks : containersDeep(finalTab.blocks)) {
        containerBulletsReconcile(finalTab, blocks, layout.blockRanges, ctx);
    }
}

} // namespace gdocsmith::reconcile
