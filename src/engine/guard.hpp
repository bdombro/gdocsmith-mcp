#pragma once

#include "engine/load.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"
#include "reconcile/reconcile.hpp"

#include <algorithm>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

using reconcile::RequestOrigin;
using reconcile::PendingLink;
using reconcile::TabToAdd;
using reconcile::SegmentToAdd;
using reconcile::FootnoteBody;
using FootnoteBodyPlan = reconcile::FootnoteBody;
using RangeWithTab = reconcile::DeletedRange;
using PlanSummary = reconcile::DocPlanSummary;
using ListRebuild = reconcile::ListRebuildWithTab;
using reconcile::DocPlan;

inline const std::vector<std::string>& allowKinds() {
    static const std::vector<std::string> kinds = {
        "brokenLink",
        "commentChanges",
        "lossyImageMove",
        "lossyListRebuild",
        "namedRangeDamage",
    };
    return kinds;
}

struct GuardFinding {
    std::optional<std::string> anchor;
    std::string docId;
    std::string kind;
    std::string message;
    std::optional<std::string> allowKind;
    std::string severity = "block"; // "block" | "warn"
    std::optional<std::string> tabId;
    std::optional<int> tabIndex;
    bool waived = false;

    bool operator==(const GuardFinding&) const = default;
};

struct GuardDoc {
    std::vector<CommentAnchor> comments;
    model::DocModel final;
    model::DocModel original;
    DocPlan plan;
    std::vector<model::Tombstone> tombstones;
};

struct GuardInput {
    std::vector<GuardDoc> docs;
    std::vector<std::string> allow;
    std::optional<std::vector<model::DocModel>> linkSources;
};

namespace detail {

struct InboundLinksResult {
    std::set<std::string> headings;
    std::set<std::string> tabs;
};

inline void visitLink(const std::string& docId, const slimr::Json& link, InboundLinksResult& res) {
    if (!link.is_object()) return;
    if (link.contains("heading") && link["heading"].is_object()) {
        const auto& h = link["heading"];
        if (h.contains("id") && h["id"].is_string()) {
            res.headings.insert(docId + '\0' + h["id"].get<std::string>());
        }
    }
    if (link.contains("headingId") && link["headingId"].is_string()) {
        res.headings.insert(docId + '\0' + link["headingId"].get<std::string>());
    }
    if (link.contains("tabId") && link["tabId"].is_string()) {
        res.tabs.insert(docId + '\0' + link["tabId"].get<std::string>());
    }
    if (link.contains("url") && link["url"].is_string()) {
        std::string url = link["url"].get<std::string>();
        static const std::regex urlRegex(R"(^https:\/\/docs\.google\.com\/document\/d\/([^/?#]+)[^?#]*(?:\?([^#]*))?(?:#(.*))?$)");
        std::smatch match;
        if (std::regex_match(url, match, urlRegex)) {
            std::string targetDocId = match[1];
            std::string query = match[2];
            std::string fragment = match[3];
            if (!query.empty()) {
                static const std::regex tabQueryRegex(R"((?:^|&)tab=([^&]*))");
                std::smatch tabMatch;
                if (std::regex_search(query, tabMatch, tabQueryRegex)) {
                    res.tabs.insert(targetDocId + '\0' + tabMatch[1].str());
                }
            }
            if (fragment.starts_with("heading=")) {
                res.headings.insert(targetDocId + '\0' + fragment.substr(8));
            }
        }
    }
}

inline void walkRawLinks(const std::string& docId, const slimr::Json& node, InboundLinksResult& res) {
    if (node.is_array()) {
        for (const auto& item : node.get<slimr::Json::array_t>()) {
            walkRawLinks(docId, item, res);
        }
    } else if (node.is_object()) {
        for (const auto& [key, value] : node.get<slimr::Json::object_t>()) {
            if (key == "link" && value.is_object()) {
                visitLink(docId, value, res);
            } else {
                walkRawLinks(docId, value, res);
            }
        }
    }
}

inline InboundLinksResult inboundLinks(const std::vector<model::DocModel>& docs) {
    InboundLinksResult res;
    for (const auto& doc : docs) {
        for (const auto& tab : doc.tabs) {
            for (const auto& block : model::blocksDeep(tab.blocks)) {
                if (block.kind == "paragraph" && block.paragraph) {
                    for (const auto& inlineItem : block.paragraph->inlines) {
                        if (inlineItem.style.contains("link") && inlineItem.style["link"].is_object()) {
                            visitLink(doc.docId, inlineItem.style["link"], res);
                        }
                    }
                } else if (block.kind == "toc" && block.toc) {
                    if (block.toc->raw.is_object() || block.toc->raw.is_array()) {
                        walkRawLinks(doc.docId, block.toc->raw, res);
                    }
                }
            }
        }
    }
    return res;
}

struct DeletionWithTab {
    model::Range range;
    std::optional<int> tabIndex;
    std::string tabId;
};

inline std::vector<DeletionWithTab> deletionsWithTabs(const GuardDoc& doc) {
    std::vector<DeletionWithTab> res;
    const auto& plan = doc.plan;
    for (const auto& del : plan.deletedRanges) {
        std::optional<int> tabIndex;
        for (size_t i = 0; i < plan.contentRequests.size(); ++i) {
            const auto& req = plan.contentRequests[i];
            if (req.contains("deleteContentRange") && req["deleteContentRange"].is_object()) {
                const auto& dcr = req["deleteContentRange"];
                if (dcr.contains("range") && dcr["range"].is_object()) {
                    const auto& r = dcr["range"];
                    int start = r.contains("startIndex") && r["startIndex"].is_number() ? static_cast<int>(r["startIndex"].get<int64_t>()) : 0;
                    int end = r.contains("endIndex") && r["endIndex"].is_number() ? static_cast<int>(r["endIndex"].get<int64_t>()) : 0;
                    std::string rTabId = r.contains("tabId") && r["tabId"].is_string() ? r["tabId"].get<std::string>() : del.tabId;
                    if (start == del.range.start && end == del.range.end && rTabId == del.tabId) {
                        if (i < plan.origins.size()) {
                            tabIndex = plan.origins[i].tabIndex;
                        }
                        break;
                    }
                }
            }
        }

        if (!tabIndex.has_value()) {
            const model::TabModel* originalTab = nullptr;
            for (const auto& t : doc.original.tabs) {
                if (t.tabId == del.tabId) {
                    originalTab = &t;
                    break;
                }
            }
            if (originalTab) {
                std::vector<std::string> coveredKeys;
                for (const auto& b : model::blocksDeep(originalTab->blocks)) {
                    if (b.origin.has_value() && b.origin->start < del.range.end && b.origin->end > del.range.start) {
                        coveredKeys.push_back(b.key);
                    }
                }
                for (const auto& t : doc.tombstones) {
                    if (t.tabId == del.tabId) {
                        if (std::find(coveredKeys.begin(), coveredKeys.end(), t.key) != coveredKeys.end()) {
                            tabIndex = t.stamp.tabIndex;
                            break;
                        }
                    }
                }
            }
        }
        res.push_back(DeletionWithTab{.range = del.range, .tabIndex = tabIndex, .tabId = del.tabId});
    }
    return res;
}

struct InsertionEntry {
    int index = 0;
    std::string tabId;
};

inline std::vector<InsertionEntry> insertionsOf(const GuardDoc& doc) {
    std::vector<InsertionEntry> res;
    for (const auto& req : doc.plan.contentRequests) {
        if (req.contains("insertText") && req["insertText"].is_object()) {
            const auto& it = req["insertText"];
            if (it.contains("location") && it["location"].is_object()) {
                const auto& loc = it["location"];
                if (loc.contains("index") && loc["index"].is_number()) {
                    std::string tabId = loc.contains("tabId") && loc["tabId"].is_string() ? loc["tabId"].get<std::string>() : "";
                    res.push_back(InsertionEntry{.index = static_cast<int>(loc["index"].get<int64_t>()), .tabId = tabId});
                }
            }
        }
    }
    return res;
}

inline std::string snippet(std::string_view text) {
    std::string res;
    bool inWs = false;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!inWs && !res.empty()) {
                res += ' ';
                inWs = true;
            }
        } else {
            res += c;
            inWs = false;
        }
    }
    while (!res.empty() && res.back() == ' ') res.pop_back();
    if (res.size() > 40) {
        return res.substr(0, 37) + "...";
    }
    return res;
}

inline const model::Block* blockDeep(const model::TabModel& tab, const std::string& key) {
    for (const auto& b : model::blocksDeep(tab.blocks)) {
        if (b.key == key) return &b;
    }
    return nullptr;
}

template <typename AddFn>
inline void headingLinkCheck(
    AddFn&& add,
    const GuardDoc& doc,
    const model::TabModel& tab,
    const model::TabModel* finalTab,
    const model::ParagraphBlock& heading,
    std::optional<int> deletedBy
) {
    std::string docId = doc.original.docId;
    std::string textContent;
    for (const auto& in : heading.inlines) {
        if (in.kind == "text") textContent += in.text;
    }
    std::string text = snippet(textContent);

    const model::Block* now = finalTab ? blockDeep(*finalTab, heading.key) : nullptr;
    const model::IdentityTransfer* moved = nullptr;
    for (const auto& t : doc.plan.identityTransfers) {
        if (t.fromKey == heading.key && heading.headingId.has_value() && t.headingId == *heading.headingId && t.toKey != heading.key) {
            moved = &t;
            break;
        }
    }

    std::string headingIdStr = heading.headingId.value_or("");
    if (!now) {
        if (moved) {
            add(GuardFinding{
                .anchor = headingIdStr,
                .docId = docId,
                .kind = "identityLoss",
                .message = "links to heading \"" + text + "\" now land on the content that replaced it",
                .severity = "warn",
                .tabId = tab.tabId,
                .tabIndex = deletedBy,
            });
        } else {
            add(GuardFinding{
                .anchor = headingIdStr,
                .docId = docId,
                .kind = "headingLink",
                .message = "deleting heading \"" + text + "\" breaks links to it",
                .allowKind = "brokenLink",
                .severity = "block",
                .tabId = tab.tabId,
                .tabIndex = deletedBy,
            });
        }
        return;
    }

    std::optional<int> tabIndex;
    if (now->kind == "paragraph" && now->stamp.has_value()) {
        tabIndex = now->stamp->tabIndex;
    }

    bool stillHeading = (now->kind == "paragraph" &&
                         now->paragraph &&
                         now->paragraph->style.contains("namedStyleType") &&
                         now->paragraph->style["namedStyleType"].is_string() &&
                         now->paragraph->style["namedStyleType"].get<std::string>() != "NORMAL_TEXT");

    if (!stillHeading) {
        add(GuardFinding{
            .anchor = headingIdStr,
            .docId = docId,
            .kind = "headingLink",
            .message = "turning heading \"" + text + "\" into text breaks links to it",
            .allowKind = "brokenLink",
            .severity = "block",
            .tabId = tab.tabId,
            .tabIndex = tabIndex,
        });
    } else if (moved) {
        add(GuardFinding{
            .anchor = headingIdStr,
            .docId = docId,
            .kind = "headingLink",
            .message = "links to heading \"" + text + "\" would move to other content",
            .allowKind = "brokenLink",
            .severity = "block",
            .tabId = tab.tabId,
            .tabIndex = tabIndex,
        });
    }
}

} // namespace detail

inline std::vector<GuardFinding> guardEvaluate(const GuardInput& input) {
    std::vector<model::DocModel> linkSources;
    if (input.linkSources.has_value()) {
        linkSources = *input.linkSources;
    } else {
        for (const auto& d : input.docs) {
            linkSources.push_back(d.final);
        }
    }
    auto links = detail::inboundLinks(linkSources);

    std::vector<GuardFinding> findings;
    auto add = [&](GuardFinding f) {
        f.waived = (f.severity == "block" && f.allowKind.has_value() &&
                    std::find(input.allow.begin(), input.allow.end(), *f.allowKind) != input.allow.end());
        findings.push_back(std::move(f));
    };

    for (const auto& doc : input.docs) {
        std::string docId = doc.original.docId;
        auto deletions = detail::deletionsWithTabs(doc);

        if (doc.original.suggestionsViewMode.has_value() &&
            *doc.original.suggestionsViewMode != "SUGGESTIONS_INLINE" &&
            !doc.plan.contentRequests.empty()) {
            add(GuardFinding{
                .docId = docId,
                .kind = "viewMode",
                .message = "the document was loaded in " + *doc.original.suggestionsViewMode + " mode; edits need SUGGESTIONS_INLINE",
                .severity = "block",
            });
        }

        auto inserts = detail::insertionsOf(doc);
        for (const auto& comment : doc.comments) {
            for (const auto& occurrence : comment.occurrences) {
                std::vector<detail::DeletionWithTab> overlaps;
                for (const auto& del : deletions) {
                    if (del.tabId == occurrence.tabId &&
                        del.range.start < occurrence.range.end &&
                        del.range.end > occurrence.range.start) {
                        overlaps.push_back(del);
                    }
                }
                std::sort(overlaps.begin(), overlaps.end(), [](const auto& a, const auto& b) {
                    return a.range.start < b.range.start;
                });

                int coveredTo = occurrence.range.start;
                for (const auto& del : overlaps) {
                    if (del.range.start > coveredTo) break;
                    coveredTo = std::max(coveredTo, del.range.end);
                }
                bool removes = !overlaps.empty() && coveredTo >= occurrence.range.end;
                bool inserted = false;
                for (const auto& at : inserts) {
                    if (at.tabId == occurrence.tabId && at.index > occurrence.range.start && at.index < occurrence.range.end) {
                        inserted = true;
                        break;
                    }
                }

                if (!removes && overlaps.empty() && !inserted) continue;

                bool free = !removes && comment.resolved;
                GuardFinding finding{
                    .anchor = comment.commentId,
                    .docId = docId,
                    .kind = removes ? "comment" : "commentPartial",
                    .message = removes
                        ? "removes the anchor of comment " + comment.commentId
                        : (free
                            ? "Edited the quoted text of resolved comment " + comment.commentId + "; Google may keep it on part of the new text."
                            : "edits the quoted text of comment " + comment.commentId),
                    .allowKind = free ? std::nullopt : std::make_optional("commentChanges"),
                    .severity = free ? "warn" : "block",
                    .tabId = occurrence.tabId,
                    .tabIndex = overlaps.empty() ? std::nullopt : overlaps[0].tabIndex,
                };
                add(std::move(finding));
            }
        }

        for (const auto& tab : doc.original.tabs) {
            const model::TabModel* finalTab = nullptr;
            for (const auto& t : doc.final.tabs) {
                if (t.tabId == tab.tabId) {
                    finalTab = &t;
                    break;
                }
            }

            auto tombstoneOf = [&](const std::string& key) -> const model::Tombstone* {
                for (const auto& t : doc.tombstones) {
                    if (t.key == key && t.tabId == tab.tabId) return &t;
                }
                return nullptr;
            };

            for (const auto& block : model::blocksDeep(tab.blocks)) {
                if (block.kind == "paragraph" && block.paragraph && block.paragraph->headingId.has_value()) {
                    if (links.headings.contains(docId + '\0' + *block.paragraph->headingId)) {
                        const auto* ts = tombstoneOf(block.key);
                        std::optional<int> deletedBy = ts ? std::make_optional(ts->stamp.tabIndex) : std::nullopt;
                        detail::headingLinkCheck(add, doc, tab, finalTab, *block.paragraph, deletedBy);
                    }
                }
            }

            if (!finalTab && links.tabs.contains(docId + '\0' + tab.tabId)) {
                add(GuardFinding{
                    .docId = docId,
                    .kind = "tabLink",
                    .message = "deleting tab \"" + tab.title + "\" breaks links to it",
                    .allowKind = "brokenLink",
                    .severity = "block",
                    .tabId = tab.tabId,
                });
            }

            std::set<std::string> keptNamedRanges;
            if (finalTab) {
                for (const auto& range : finalTab->namedRanges) {
                    keptNamedRanges.insert(range.namedRangeId);
                }
            }

            for (const auto& range : tab.namedRanges) {
                if (!keptNamedRanges.contains(range.namedRangeId)) continue;
                for (const auto& r : range.ranges) {
                    if (r.segmentId.has_value()) continue;
                    for (const auto& d : deletions) {
                        if (d.tabId == tab.tabId && d.range.start <= r.start && d.range.end >= r.end) {
                            add(GuardFinding{
                                .anchor = range.namedRangeId,
                                .docId = docId,
                                .kind = "namedRange",
                                .message = "deleting all of named range \"" + range.name + "\"'s text deletes the range; remove its markers too, or keep some text",
                                .allowKind = "namedRangeDamage",
                                .severity = "block",
                                .tabId = tab.tabId,
                                .tabIndex = d.tabIndex,
                            });
                        }
                    }
                }
            }
        }

        for (const auto& rebuild : doc.plan.listRebuilds) {
            if (rebuild.lossy) {
                add(GuardFinding{
                    .anchor = rebuild.keys.empty() ? "" : rebuild.keys[0],
                    .docId = docId,
                    .kind = "listRebuild",
                    .message = "changing this list's nesting rebuilds it, and its custom look can't be kept",
                    .allowKind = "lossyListRebuild",
                    .severity = "block",
                    .tabId = rebuild.tabId,
                });
            }
        }
    }
    return findings;
}

} // namespace gdocsmith::engine
