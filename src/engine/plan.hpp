#pragma once

#include "diff/unified.hpp"
#include "engine/guard.hpp"
#include "engine/load.hpp"
#include "engine/session.hpp"
#include "model/equivalence.hpp"
#include "model/errors.hpp"
#include "model/types.hpp"
#include "reconcile/reconcile.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

/** One document's part of a plan. */
struct PlannedDoc {
    std::optional<std::vector<std::string>> approvedNamedRangeLosses;
    model::CompareResult check;
    struct CreateInfo {
        std::string title;
    };
    std::optional<CreateInfo> create;
    std::string docId;
    reconcile::DocReconcileInput input;
    bool isNew = false;
    reconcile::DocPlan plan;
};

/** One changed tab's unified diff. */
struct TabDiff {
    std::string docId;
    std::string tabId;
    std::string tabTitle;
    std::string text;

    bool operator==(const TabDiff&) const = default;
};

/** An operation's plan. */
struct Plan {
    std::string diff;
    std::vector<TabDiff> diffs;
    std::vector<PlannedDoc> docs;
    std::vector<GuardFinding> findings;
};

/** Renders a tab's body into clean markdown for unified diffing. */
inline std::string tabBodyMarkdown(const model::TabModel& tab) {
    std::string md;
    for (const auto& block : tab.blocks) {
        if (block.is_paragraph() && block.paragraph) {
            std::string para_text;
            std::string prefix;
            if (block.paragraph->style.contains("namedStyleType") &&
                block.paragraph->style["namedStyleType"].is_string()) {
                std::string nst = block.paragraph->style["namedStyleType"].get<std::string>();
                if (nst == "HEADING_1") prefix = "# ";
                else if (nst == "HEADING_2") prefix = "## ";
                else if (nst == "HEADING_3") prefix = "### ";
                else if (nst == "HEADING_4") prefix = "#### ";
                else if (nst == "HEADING_5") prefix = "##### ";
                else if (nst == "HEADING_6") prefix = "###### ";
            }
            for (const auto& in : block.paragraph->inlines) {
                if (in.is_text()) {
                    para_text += in.text;
                }
            }
            if (!para_text.empty() || !prefix.empty()) {
                if (!md.empty()) {
                    if (!md.ends_with("\n\n")) {
                        if (!md.ends_with('\n')) md += "\n\n";
                        else md += "\n";
                    }
                }
                md += prefix + para_text;
            }
        }
    }
    std::string trimmed = md;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
        trimmed.pop_back();
    }
    return trimmed.empty() ? "" : (trimmed + "\n");
}

/** Document-file diffs of every tab that changed (added and deleted tabs included). */
inline std::vector<TabDiff> tabDiffs(const model::DocModel& original, const model::DocModel& finalDoc) {
    std::map<std::string, std::string> originalBodies;
    for (const auto& tab : original.tabs) {
        originalBodies[tab.tabId] = tabBodyMarkdown(tab);
    }
    std::map<std::string, std::string> finalBodies;
    for (const auto& tab : finalDoc.tabs) {
        finalBodies[tab.tabId] = tabBodyMarkdown(tab);
    }

    std::set<std::string> idSet;
    std::vector<std::string> ids;
    for (const auto& t : original.tabs) {
        if (!idSet.contains(t.tabId)) {
            idSet.insert(t.tabId);
            ids.push_back(t.tabId);
        }
    }
    for (const auto& t : finalDoc.tabs) {
        if (!idSet.contains(t.tabId)) {
            idSet.insert(t.tabId);
            ids.push_back(t.tabId);
        }
    }

    std::vector<TabDiff> out;
    for (const auto& tabId : ids) {
        const model::TabModel* before = nullptr;
        for (const auto& t : original.tabs) {
            if (t.tabId == tabId) { before = &t; break; }
        }
        const model::TabModel* after = nullptr;
        for (const auto& t : finalDoc.tabs) {
            if (t.tabId == tabId) { after = &t; break; }
        }

        std::string title = after ? after->title : (before ? before->title : tabId);
        std::string beforeTitle = before ? before->title : title;
        std::string afterTitle = after ? after->title : title;

        std::string origBody = originalBodies.contains(tabId) ? originalBodies[tabId] : "";
        std::string finBody = finalBodies.contains(tabId) ? finalBodies[tabId] : "";

        diff::UnifiedDiffOptions opts{
            .newPath = "b/" + finalDoc.title + "/" + afterTitle,
            .oldPath = "a/" + original.title + "/" + beforeTitle,
            .context = 3,
        };
        std::string text = diff::unifiedDiffFormat(origBody, finBody, opts);
        if (!text.empty() || (before && after && before->title != after->title)) {
            out.push_back(TabDiff{
                .docId = finalDoc.docId,
                .tabId = tabId,
                .tabTitle = title,
                .text = std::move(text),
            });
        }
    }
    return out;
}

/**
 * Plans every document in the session: reconciles it, proves the plan against the emulator,
 * carries existing documents' comment anchors through edits inside them, evaluates the guard
 * across all of them, and diffs each changed tab.
 */
inline Plan planBuild(CoreSession& session) {
    std::vector<PlannedDoc> docs;
    std::map<std::string, std::vector<CommentAnchor>> anchors;

    for (auto& [docId, state] : session.docs) {
        std::vector<CommentAnchor> comments = session.commentAnchorsFor(state);
        anchors[state.docId] = comments;

        std::map<std::string, std::vector<model::Range>> carryRanges;
        for (const auto& comment : comments) {
            for (const auto& occ : comment.occurrences) {
                carryRanges[occ.tabId].push_back(occ.range);
            }
        }

        reconcile::DocReconcileInput input{
            .final = state.current,
            .original = state.original,
            .originalJson = state.json,
            .carryRanges = std::move(carryRanges),
        };

        reconcile::DocPlan plan = reconcile::docReconcile(input);
        model::CompareResult check = reconcile::docPlanSelfCheck(plan, input);
        if (!check.equal) {
            std::string diffMsg;
            size_t limit = std::min(size_t{20}, check.diffs.size());
            for (size_t i = 0; i < limit; ++i) {
                if (!diffMsg.empty()) diffMsg += "\n";
                diffMsg += check.diffs[i];
            }
            throw model::CoreError(
                "internal",
                "the plan for " + state.docId + " wouldn't produce the intended document; this plan was not sent\n" + diffMsg
            );
        }

        PlannedDoc pdoc{
            .approvedNamedRangeLosses = std::nullopt,
            .check = std::move(check),
            .create = state.create ? std::make_optional(PlannedDoc::CreateInfo{.title = state.create->title}) : std::nullopt,
            .docId = state.docId,
            .input = std::move(input),
            .isNew = state.isNew,
            .plan = std::move(plan),
        };
        docs.push_back(std::move(pdoc));
    }

    std::vector<GuardDoc> guardDocs;
    for (const auto& doc : docs) {
        auto it = session.docs.find(doc.docId);
        if (it == session.docs.end()) continue;
        const auto& state = it->second;
        auto commIt = anchors.find(doc.docId);
        guardDocs.push_back(GuardDoc{
            .comments = commIt != anchors.end() ? commIt->second : std::vector<CommentAnchor>{},
            .final = state.current,
            .original = state.original,
            .plan = doc.plan,
            .tombstones = state.tombstones,
        });
    }

    std::vector<model::DocModel> linkSources;
    for (const auto& [_, s] : session.docs) {
        linkSources.push_back(s.current);
    }

    std::vector<GuardFinding> findings = guardEvaluate(GuardInput{
        .docs = guardDocs,
        .allow = session.allow,
        .linkSources = linkSources,
    });
    for (const auto& f : session.findings) {
        findings.push_back(f);
    }

    std::vector<TabDiff> diffs;
    for (const auto& doc : docs) {
        auto tds = tabDiffs(doc.input.original, doc.input.final);
        diffs.insert(diffs.end(), tds.begin(), tds.end());
    }

    std::string combinedDiff;
    for (const auto& d : diffs) {
        if (!combinedDiff.empty()) combinedDiff += "\n";
        combinedDiff += d.text;
    }

    return Plan{
        .diff = std::move(combinedDiff),
        .diffs = std::move(diffs),
        .docs = std::move(docs),
        .findings = std::move(findings),
    };
}

} // namespace gdocsmith::engine
