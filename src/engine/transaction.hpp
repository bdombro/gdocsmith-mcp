#pragma once

#include "emulator/fake_google.hpp"
#include "engine/flush.hpp"
#include "engine/guard.hpp"
#include "engine/plan.hpp"
#include "engine/session.hpp"
#include "model/edit.hpp"
#include "model/errors.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/layout.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"
#include "reconcile/reconcile.hpp"
#include "reconcile/requests.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

/** How to run a transaction. */
template <typename ClientType, typename DriveType = ClientType>
struct TransactionOptions {
    ClientType *client = nullptr;
    DriveType *drive = nullptr;
    std::vector<std::string> allow;
    std::function<void(const Plan &)> onPlan;
    std::function<std::map<std::string, std::string>(const std::vector<std::string> &)> imageUploads;
};

struct NewIds {
    std::map<std::string, std::string> docs;
    std::map<std::string, std::string> headings;
    std::map<std::string, std::string> tabs;
};

using DocVerification = VerificationResult;

/** What a transaction did. */
template <typename T>
struct TransactionResult {
    int attempts = 0;
    std::string diff;
    std::vector<TabDiff> diffs;
    std::vector<GuardFinding> findings;
    NewIds newIds;
    T output{};
    std::vector<PhaseReport> phases;
    bool refused = false;
    std::vector<DocVerification> verification;
    std::map<std::string, slimr::Json> documents;
};

template <typename ClientType>
inline std::string clientCreateDocument(ClientType *client, const std::string &title) {
    if (!client)
        throw model::CoreError("operationFailed", "A Docs client is required before creating a document.");
    if constexpr (requires { client->createDocument(title); }) {
        auto res = client->createDocument(title);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            if (res.empty())
                throw model::CoreError("operationFailed", "Document creation returned an empty ID; its outcome is unknown.");
            return res;
        } else {
            if (res.contains("documentId") && res["documentId"].is_string() &&
                !res["documentId"].template get<std::string>().empty()) {
                return res["documentId"].template get<std::string>();
            }
            if (res.contains("id") && res["id"].is_string() && !res["id"].template get<std::string>().empty()) {
                return res["id"].template get<std::string>();
            }
            throw model::CoreError("operationFailed", "Document creation returned no document ID; its outcome is unknown.");
        }
    } else if constexpr (requires { client->create_document(title); }) {
        auto res = client->create_document(title);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            if (res.empty())
                throw model::CoreError("operationFailed", "Document creation returned an empty ID; its outcome is unknown.");
            return res;
        } else {
            if (res.contains("documentId") && res["documentId"].is_string() &&
                !res["documentId"].template get<std::string>().empty()) {
                return res["documentId"].template get<std::string>();
            }
            throw model::CoreError("operationFailed", "Document creation returned no document ID; its outcome is unknown.");
        }
    } else {
        throw std::runtime_error("this client can't create documents");
    }
}

template <typename ClientType>
inline slimr::Json clientGetDocument(ClientType *client, const std::string &docId) {
    if constexpr (requires { client->getDocument(docId); }) {
        auto res = client->getDocument(docId);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            auto parsed = slimr::parse_json<slimr::Json>(res);
            if (!parsed || !parsed->is_object()) {
                throw model::CoreError("operationFailed", "Invalid document read response.");
            }
            return *parsed;
        } else {
            if (!res.is_object())
                throw model::CoreError("operationFailed", "Invalid document read response.");
            return res;
        }
    } else if constexpr (requires { client->get_document(docId); }) {
        auto res = client->get_document(docId);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            auto parsed = slimr::parse_json<slimr::Json>(res);
            if (!parsed || !parsed->is_object()) {
                throw model::CoreError("operationFailed", "Invalid document read response.");
            }
            return *parsed;
        } else {
            if (!res.is_object())
                throw model::CoreError("operationFailed", "Invalid document read response.");
            return res;
        }
    } else if constexpr (requires { client->get_doc(docId); }) {
        auto doc = client->get_doc(docId);
        auto json = doc.json;
        if (!json.contains("revisionId")) {
            json["revisionId"] = client->revision_id(doc);
        }
        return json;
    } else {
        throw std::runtime_error("this client can't get documents");
    }
}

template <typename ClientType>
inline slimr::Json clientBatchUpdate(
    ClientType *client,
    const std::string &docId,
    const std::vector<slimr::Json> &requests,
    const std::optional<std::string> &revision = std::nullopt) {
    revisionRequired(revision);
    if (!client)
        throw model::CoreError("operationFailed", "A Docs client is required before sending edits.");
    if constexpr (!(requires { client->batchUpdate(docId, requests, revision); }) &&
                  !(requires { client->batch_update(docId, requests, revision); })) {
        throw model::CoreError("operationFailed", "This client cannot batchUpdate.");
    }
    std::string respStr;
    try {
        if constexpr (requires { client->batchUpdate(docId, requests, revision); }) {
            auto r = client->batchUpdate(docId, requests, revision);
            respStr = r.is_string() ? r.template get<std::string>() : slimr::write_json(r);
        } else if constexpr (requires { client->batch_update(docId, requests, revision); }) {
            respStr = client->batch_update(docId, requests, revision);
        } else {
            throw std::runtime_error("this client can't batchUpdate");
        }
        auto parsed = slimr::parse_json<slimr::Json>(respStr);
        if (!parsed || !parsed->is_object()) {
            throw model::CoreError("operationFailed", "Invalid batchUpdate response; the write outcome is unknown.");
        }
        responseRevision(*parsed);
        return *parsed;
    } catch (const std::exception &error) {
        TransactionError failure("create", error.what());
        if (!revisionConflictIs(error.what()) && !requestIndexOfError(error.what())) {
            failure.partial.unknown.push_back(docId);
            failure.unknownRequestCount = static_cast<int>(requests.size());
        }
        throw failure;
    }
}

inline std::map<std::string, std::string> tabIdMap(const Ledger &ledger, const std::string &docId) {
    std::map<std::string, std::string> m;
    auto it = ledger.tabs.find(docId);
    if (it != ledger.tabs.end()) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            m["new_tab_" + std::to_string(i + 1)] = it->second[i];
        }
    }
    return m;
}

inline std::vector<std::string> footnoteOrder(const model::TabModel &tab) {
    std::vector<std::string> ids;
    for (const auto &block : model::blocksDeep(tab.blocks)) {
        if (block.kind == "paragraph" && block.paragraph) {
            for (const auto &inlineItem : block.paragraph->inlines) {
                if (inlineItem.kind == "atom" && inlineItem.type == "footnoteRef") {
                    std::string id;
                    if (inlineItem.create) {
                        id = "new:fn:" + inlineItem.key;
                    } else if (inlineItem.raw.has_value() && inlineItem.raw->is_object() &&
                               inlineItem.raw->contains("footnoteReference") &&
                               (*inlineItem.raw)["footnoteReference"].is_object() &&
                               (*inlineItem.raw)["footnoteReference"].contains("footnoteId") &&
                               (*inlineItem.raw)["footnoteReference"]["footnoteId"].is_string()) {
                        id = (*inlineItem.raw)["footnoteReference"]["footnoteId"].template get<std::string>();
                    }
                    if (!id.empty())
                        ids.push_back(std::move(id));
                }
            }
        }
    }
    return ids;
}

inline void newHeadingIds(
    const PlannedDoc &doc,
    const slimr::Json &json,
    const std::map<std::string, std::string> &tabIds,
    std::map<std::string, std::string> &out) {
    model::KeyAllocator keys;
    auto actual = model::docModelParse(json, model::DocModelParseOptions{.docId = doc.docId, .keys = keys});
    for (const auto &tab : doc.input.final.tabs) {
        std::string targetTabId = tab.tabId;
        auto tIt = tabIds.find(tab.tabId);
        if (tIt != tabIds.end())
            targetTabId = tIt->second;

        const model::TabModel *now = nullptr;
        for (const auto &t : actual.tabs) {
            if (t.tabId == targetTabId) {
                now = &t;
                break;
            }
        }
        if (!now)
            continue;

        auto ranges = model::layoutCompute(tab).blockRanges;
        for (const auto &block : tab.blocks) {
            if (block.kind != "paragraph" || !block.key.starts_with("n") || !block.paragraph)
                continue;
            const auto &st = block.paragraph->style;
            if (!st.contains("namedStyleType") || !st["namedStyleType"].is_string())
                continue;
            std::string nst = st["namedStyleType"].template get<std::string>();
            if (nst == "NORMAL_TEXT" || nst.empty())
                continue;

            auto rIt = ranges.find(block.key);
            if (rIt == ranges.end())
                continue;
            int start = rIt->second.start;

            for (const auto &b : now->blocks) {
                if (b.kind == "paragraph" && b.paragraph && b.paragraph->origin &&
                    b.paragraph->origin->start == start) {
                    if (b.paragraph->headingId) {
                        out[block.key] = *b.paragraph->headingId;
                    }
                    break;
                }
            }
        }
    }
}

inline NewIds buildNewIds(const Ledger &ledger, const std::map<std::string, std::string> &headings) {
    NewIds res;
    for (const auto &[alias, id] : ledger.docs) {
        res.docs["new_" + alias] = id;
    }
    for (const auto &[docId, ids] : ledger.tabs) {
        for (size_t i = 0; i < ids.size(); ++i) {
            res.tabs[docId + "/new_tab_" + std::to_string(i + 1)] = ids[i];
        }
    }
    res.headings = headings;
    return res;
}

/** Resolved provisional ids passed into the verification phase. */
struct VerifyResolvedOptions {
    std::map<std::string, std::string> footnoteIds;
    std::map<std::string, std::string> headingIds;
    std::map<std::string, std::string> tabIds;
};

/** Compares the reloaded document against the intended model. */
inline VerificationResult verify(
    const PlannedDoc &doc,
    const slimr::Json &json,
    const std::optional<std::string> &revision,
    const VerifyResolvedOptions &resolved) {
    auto expected = doc.input.final;
    auto resolveTab = [&](auto &self, model::TabModel &tab) -> void {
        auto blocks = [&](auto &visit, std::vector<model::Block> &container) -> void {
            for (auto &block : container) {
                if (block.is_table()) {
                    for (auto &row : block.table->rows)
                        for (auto &cell : row.cells)
                            visit(visit, cell.blocks);
                } else if (block.is_paragraph()) {
                    for (auto &item : block.paragraph->inlines) {
                        if (item.style.contains("link") && item.style["link"].is_object()) {
                            auto &link = item.style["link"];
                            if (link.contains("heading") && link["heading"].is_object() &&
                                link["heading"].contains("key") && link["heading"]["key"].is_string()) {
                                const auto key = link["heading"]["key"].template get<std::string>();
                                auto found = resolved.headingIds.find(key);
                                if (found == resolved.headingIds.end()) {
                                    throw model::CoreError("internal", "A pending heading link was not resolved before verification.");
                                }
                                const auto heading = link["heading"];
                                const auto targetDoc = heading.contains("docId") && heading["docId"].is_string()
                                                           ? heading["docId"].template get<std::string>()
                                                           : doc.docId;
                                const auto targetTab = heading.contains("tabId") && heading["tabId"].is_string()
                                                           ? heading["tabId"].template get<std::string>()
                                                           : tab.tabId;
                                const auto realTab = resolved.tabIds.contains(targetTab)
                                                         ? resolved.tabIds.at(targetTab)
                                                         : targetTab;
                                if (targetDoc == doc.docId) {
                                    link = slimr::Json::object_t{{"heading", slimr::Json::object_t{
                                                                                 {"id", found->second}, {"tabId", realTab}}}};
                                } else {
                                    link = slimr::Json::object_t{{"url", "https://docs.google.com/document/d/" +
                                                                             targetDoc + "/edit?tab=" + realTab + "#heading=" + found->second}};
                                }
                            }
                        }
                        if (item.raw && item.raw->contains("footnoteReference") &&
                            (*item.raw)["footnoteReference"].is_object()) {
                            auto &ref = (*item.raw)["footnoteReference"];
                            if (ref.contains("footnoteId") && ref["footnoteId"].is_string()) {
                                auto found = resolved.footnoteIds.find(ref["footnoteId"].template get<std::string>());
                                if (found != resolved.footnoteIds.end())
                                    ref["footnoteId"] = found->second;
                            }
                        }
                    }
                }
            }
        };
        blocks(blocks, tab.blocks);
        for (auto &segment : tab.segments) {
            if (resolved.footnoteIds.contains(segment.id))
                segment.id = resolved.footnoteIds.at(segment.id);
            if (segment.tab) {
                segment.tab = std::make_shared<model::TabModel>(*segment.tab);
                self(self, *segment.tab);
            }
        }
    };
    for (auto &tab : expected.tabs)
        resolveTab(resolveTab, tab);
    std::set<std::string> rebuilt;
    for (const auto &list : doc.plan.listRebuilds)
        rebuilt.insert(list.listId);
    auto result = verifyDoc(doc.docId, expected, json, revision, resolved.tabIds, doc.plan.identityTransfers, rebuilt);
    if (result.status == "skipped")
        return result;
    const auto preservation = verifyNamedRanges(doc.docId, expected, json, doc.plan.contentRequests,
                                                resolved.tabIds, doc.approvedNamedRangeLosses.value_or(std::vector<std::string>{}));
    result.diffs.insert(result.diffs.end(), preservation.diffs.begin(), preservation.diffs.end());
    if (preservation.status == "mismatch")
        result.status = "mismatch";
    else if (result.status == "match" && preservation.status == "inconclusive")
        result.status = "inconclusive";
    return result;
}

/** Requests generated by the links phase. */
struct LinkRequestsResult {
    std::map<std::string, std::string> headingIds;
    std::vector<slimr::Json> requests;
};

/**
 * Turns pending heading links into `updateTextStyle` requests, resolving each
 * heading key to the real heading id minted by the content phase.
 */
template <typename GetTabIds>
inline LinkRequestsResult linkRequests(
    const PlannedDoc &doc,
    const std::map<std::string, model::DocModel> &reloaded,
    const std::map<std::string, model::DocModel> &finals,
    GetTabIds &&getTabIds) {
    LinkRequestsResult out;
    if (doc.plan.pendingLinks.empty())
        return out;

    auto finIt = finals.find(doc.docId);
    auto relIt = reloaded.find(doc.docId);
    if (finIt == finals.end() || relIt == reloaded.end()) {
        throw model::CoreError("linkTargetNotFound", "A linked document has no authoritative readback.");
    }
    const model::DocModel &finalDoc = finIt->second;
    const model::DocModel &relDoc = relIt->second;

    std::map<std::string, std::string> tabIds = getTabIds(doc.docId);

    struct Loc {
        int start = -1;
        std::string tabId;
    };
    auto locate = [](const model::DocModel &m, const std::string &key) -> Loc {
        for (const auto &tab : m.tabs) {
            auto ranges = model::layoutCompute(tab).blockRanges;
            auto it = ranges.find(key);
            if (it != ranges.end()) {
                return Loc{.start = it->second.start, .tabId = tab.tabId};
            }
        }
        return Loc{};
    };
    auto paragraphAt = [](const model::DocModel &m, const std::string &tabId, int start)
        -> std::optional<model::ParagraphBlock> {
        for (const auto &tab : m.tabs) {
            if (!tabId.empty() && tab.tabId != tabId)
                continue;
            for (const auto &block : model::blocksDeep(tab.blocks)) {
                if (block.is_paragraph() && block.paragraph->origin.has_value() &&
                    block.paragraph->origin->start == start) {
                    return *block.paragraph;
                }
            }
        }
        return std::nullopt;
    };
    auto plain = [](const model::ParagraphBlock &paragraph) {
        std::string text;
        for (const auto &item : paragraph.inlines)
            if (item.is_text())
                text += item.text;
        return text;
    };

    for (const auto &pl : doc.plan.pendingLinks) {
        const auto targetDocId = pl.docId.value_or(doc.docId);
        auto targetFinal = finals.find(targetDocId);
        auto targetNow = reloaded.find(targetDocId);
        if (targetFinal == finals.end() || targetNow == reloaded.end()) {
            throw model::CoreError("linkTargetNotFound", "A linked heading's document is missing.");
        }
        auto headingLoc = locate(targetFinal->second, pl.headingKey);
        auto targetTabIds = getTabIds(targetDocId);
        auto realHeadingTab = targetTabIds.contains(headingLoc.tabId)
                                  ? targetTabIds.at(headingLoc.tabId)
                                  : headingLoc.tabId;
        auto heading = paragraphAt(targetNow->second, realHeadingTab, headingLoc.start);
        std::optional<model::ParagraphBlock> finalHeading;
        for (const auto &tab : targetFinal->second.tabs)
            for (const auto &paragraph : model::paragraphsDeep(tab.blocks))
                if (paragraph.key == pl.headingKey)
                    finalHeading = paragraph;
        if (!heading || !heading->headingId || !finalHeading || plain(*heading) != plain(*finalHeading)) {
            throw model::CoreError("linkTargetNotFound", "The linked heading did not match the authoritative readback.");
        }
        std::string headingId = *heading->headingId;
        out.headingIds[pl.headingKey] = headingId;

        auto paraLoc = locate(finalDoc, pl.paragraphKey);
        if (paraLoc.start < 0) {
            throw model::CoreError("linkTargetNotFound", "The source paragraph for a heading link is missing.");
        }
        int linkStart = paraLoc.start + pl.offset;
        int linkEnd = linkStart + pl.length;

        std::string realTabId = paraLoc.tabId;
        auto tIt = tabIds.find(paraLoc.tabId);
        if (tIt != tabIds.end())
            realTabId = tIt->second;

        auto source = paragraphAt(relDoc, realTabId, paraLoc.start);
        std::optional<model::ParagraphBlock> plannedSource;
        for (const auto &tab : finalDoc.tabs)
            for (const auto &paragraph : model::paragraphsDeep(tab.blocks))
                if (paragraph.key == pl.paragraphKey)
                    plannedSource = paragraph;
        if (!source || !plannedSource || plain(*source) != plain(*plannedSource)) {
            throw model::CoreError("linkTargetNotFound", "The heading link's source did not match the authoritative readback.");
        }
        slimr::Json target = targetDocId == doc.docId
                                 ? slimr::Json(slimr::Json::object_t{{"heading", slimr::Json::object_t{
                                                                                     {"id", headingId}, {"tabId", realHeadingTab}}}})
                                 : slimr::Json(slimr::Json::object_t{{"url", "https://docs.google.com/document/d/" + targetDocId + "/edit?tab=" + realHeadingTab + "#heading=" + headingId}});
        slimr::Json linkStyle = slimr::Json::object_t{{"link", target}};
        int units = 0;
        for (const auto &item : plannedSource->inlines) {
            const int length = item.is_text() ? model::utf16CodeUnitLength(item.text) : item.length;
            if (pl.offset >= units && pl.offset < units + length) {
                for (const auto &name : {"foregroundColor", "underline"})
                    if (item.style.contains(name))
                        linkStyle[name] = item.style[name];
            }
            units += length;
        }
        out.requests.push_back(reconcile::RequestBuilder::textStyleUpdate(
            linkStart, linkEnd, linkStyle, {"foregroundColor", "link", "underline"}, realTabId));
        if (pl.offset + pl.length == units) {
            slimr::Json newline = slimr::Json::object_t{};
            for (const auto &name : {"foregroundColor", "underline"})
                if (plannedSource->newline.style.contains(name))
                    newline[name] = plannedSource->newline.style[name];
            out.requests.push_back(reconcile::RequestBuilder::textStyleUpdate(
                linkEnd, linkEnd + 1, newline, {"foregroundColor", "underline"}, realTabId));
        }
    }

    return out;
}

template <typename ClientType, typename DriveType>
inline std::optional<PhaseReport> createPhase(
    const Plan &plan,
    TransactionOptions<ClientType, DriveType> &opts,
    Ledger &ledger,
    std::vector<std::string> &created,
    std::vector<PhaseReport> &phases,
    std::vector<std::string> &landed) {
    if (!opts.client)
        throw model::CoreError("operationFailed", "A Docs client is required before creating documents.");
    phases.push_back(PhaseReport{.docs = {}, .phase = "create"});
    auto &report = phases.back();
    auto recordLanded = [&](const std::string &id) {
        if (std::find(landed.begin(), landed.end(), id) == landed.end())
            landed.push_back(id);
    };
    auto confirmedCount = [&](const std::string &id) {
        int count = 0;
        for (const auto &receipt : report.docs) {
            if (receipt.docId == id)
                count += receipt.requestCount;
        }
        return count;
    };
    auto send = [&](const PlannedDoc &doc, const slimr::Json &request,
                    std::optional<std::string> &revision) {
        try {
            auto response = clientBatchUpdate(opts.client, doc.docId, {request}, revision);
            revision = responseRevision(response);
            ledger.revisions[doc.docId] = *revision;
            recordLanded(doc.docId);
            report.docs.push_back(PhaseDocReport{
                .docId = doc.docId,
                .landed = true,
                .requestCount = 1,
                .revisionAfter = revision,
                .outcome = "confirmed",
            });
            return response;
        } catch (const TransactionError &error) {
            report.docs.push_back(PhaseDocReport{
                .docId = doc.docId,
                .error = error.causeMessage,
                .revisionAfter = revision,
                .outcome = error.partial.unknown.empty() ? "notSent" : "unknown",
                .unknownRequestCount = error.unknownRequestCount,
            });
            TransactionError failure("create", error.causeMessage,
                                     {.created = created, .landed = landed, .unknown = error.partial.unknown},
                                     std::nullopt, confirmedCount(doc.docId));
            failure.unknownRequestCount = error.unknownRequestCount;
            failure.revisionAfter = revision;
            throw failure;
        }
    };
    for (const auto &doc : plan.docs) {
        if (doc.isNew && doc.create) {
            if constexpr (!(requires { opts.client->createDocument(doc.create->title); }) &&
                          !(requires { opts.client->create_document(doc.create->title); })) {
                throw model::CoreError("operationFailed", "This client cannot create documents; nothing was sent.");
            }
            std::string alias = doc.docId;
            if (alias.starts_with("new_"))
                alias = alias.substr(4);
            try {
                std::string id = clientCreateDocument(opts.client, doc.create->title);
                ledger.docs[alias] = id;
                created.push_back(id);
                recordLanded(id);
                report.docs.push_back(PhaseDocReport{
                    .docId = id,
                    .landed = true,
                    .requestCount = 1,
                    .outcome = "confirmed",
                });
            } catch (const std::exception &err) {
                report.docs.push_back(PhaseDocReport{
                    .docId = doc.docId,
                    .error = err.what(),
                    .outcome = "unknown",
                    .unknownRequestCount = 1,
                });
                TransactionError failure("create", err.what(),
                                         {.created = created, .landed = landed, .unknown = {doc.docId}});
                failure.unknownRequestCount = 1;
                throw failure;
            }
        } else if (!doc.isNew && doc.plan.tabsToAdd.empty() && !doc.plan.segmentsToAdd.empty()) {
            std::optional<std::string> revision;
            if (doc.input.originalJson.contains("revisionId") && doc.input.originalJson["revisionId"].is_string()) {
                revision = doc.input.originalJson["revisionId"].template get<std::string>();
            }
            for (const auto &segment : doc.plan.segmentsToAdd) {
                try {
                    int breakIdx = segment.breakIndex.value_or(0);
                    auto req = reconcile::RequestBuilder::segmentCreate(segment.kind, segment.tabId, breakIdx);
                    send(doc, req, revision);
                } catch (const TransactionError &) {
                    throw;
                } catch (const std::exception &err) {
                    throw TransactionError("create", err.what(), {.created = created, .landed = landed});
                }
            }
        } else if (!doc.isNew && !doc.plan.tabsToAdd.empty()) {
            auto &ids = ledger.tabs[doc.docId];
            std::map<std::string, std::string> real;
            std::optional<std::string> revision;
            if (doc.input.originalJson.contains("revisionId") && doc.input.originalJson["revisionId"].is_string()) {
                revision = doc.input.originalJson["revisionId"].template get<std::string>();
            }
            for (const auto &tab : doc.plan.tabsToAdd) {
                std::optional<std::string> parentTabId;
                if (tab.parentTabId) {
                    auto pIt = real.find(*tab.parentTabId);
                    parentTabId = (pIt != real.end()) ? pIt->second : *tab.parentTabId;
                }
                try {
                    reconcile::RequestBuilder::DocumentTabAddOpts addOpts{
                        .index = tab.index,
                        .parentTabId = parentTabId,
                    };
                    auto req = reconcile::RequestBuilder::documentTabAdd(tab.title, addOpts);
                    auto response = send(doc, req, revision);
                    std::string tabId;
                    if (response.contains("replies") && response["replies"].is_array() &&
                        !response["replies"].template get<slimr::Json::array_t>().empty()) {
                        const auto &firstReply = response["replies"][0];
                        if (firstReply.contains("addDocumentTab") && firstReply["addDocumentTab"].is_object()) {
                            const auto &adt = firstReply["addDocumentTab"];
                            if (adt.contains("tabProperties") && adt["tabProperties"].is_object()) {
                                const auto &tp = adt["tabProperties"];
                                if (tp.contains("tabId") && tp["tabId"].is_string()) {
                                    tabId = tp["tabId"].template get<std::string>();
                                }
                            }
                        }
                    }
                    if (tabId.empty()) {
                        throw model::CoreError("operationFailed", "A tab was created but its ID was not returned; do not replay this creation.");
                    }
                    real[tab.tabId] = tabId;
                    std::string prefix = "new_tab_";
                    if (tab.tabId.starts_with(prefix)) {
                        int ordinal = std::stoi(tab.tabId.substr(prefix.size()));
                        if (ordinal > 0) {
                            if (ids.size() < static_cast<size_t>(ordinal)) {
                                ids.resize(ordinal);
                            }
                            ids[ordinal - 1] = tabId;
                        }
                    }
                } catch (const TransactionError &) {
                    throw;
                } catch (const std::exception &err) {
                    throw TransactionError("create", err.what(), {.created = created, .landed = landed});
                }
            }
        }
    }
    if (!report.docs.empty()) {
        return report;
    }
    phases.pop_back();
    return std::nullopt;
}

template <typename Fn>
inline void phaseRun(
    std::vector<PhaseReport> &phases,
    std::string_view phase,
    const std::vector<PlannedDoc> &docs,
    const std::vector<std::string> &created,
    std::vector<std::string> &landed,
    Fn &&run) {
    PhaseReport report{.docs = {}, .phase = std::string(phase)};
    phases.push_back(report);
    for (const auto &doc : docs) {
        PhaseDocReport receipt{.docId = doc.docId, .landed = false, .requestCount = 0};
        try {
            auto result = run(doc, [&](int count) {
                receipt.landed = true;
                receipt.requestCount += count;
            });
            if (result.requestCount > 0) {
                receipt.landed = true;
                receipt.requestCount = result.requestCount;
                receipt.revisionAfter = result.revisionAfter;
                receipt.outcome = "confirmed";
                if (std::find(landed.begin(), landed.end(), doc.docId) == landed.end())
                    landed.push_back(doc.docId);
                phases.back().docs.push_back(std::move(receipt));
            }
        } catch (const TransactionError &err) {
            receipt.error = err.causeMessage;
            receipt.requestCount += err.landedRequestCount;
            receipt.landed = receipt.requestCount > 0;
            receipt.revisionAfter = err.revisionAfter;
            receipt.outcome = err.partial.unknown.empty()
                                  ? (receipt.landed ? "confirmed" : "notSent")
                                  : "unknown";
            receipt.unknownRequestCount = err.unknownRequestCount;
            if (receipt.landed && std::find(landed.begin(), landed.end(), doc.docId) == landed.end()) {
                landed.push_back(doc.docId);
            }
            const int confirmed = receipt.requestCount;
            phases.back().docs.push_back(std::move(receipt));
            TransactionError failure(std::string(phase), err.causeMessage,
                                     {.created = created, .landed = landed, .unknown = err.partial.unknown},
                                     err.tabIndex, confirmed);
            failure.unknownRequestCount = err.unknownRequestCount;
            failure.revisionAfter = err.revisionAfter;
            throw failure;
        } catch (const std::exception &err) {
            receipt.error = err.what();
            if (receipt.landed) {
                receipt.outcome = "confirmed";
                if (std::find(landed.begin(), landed.end(), doc.docId) == landed.end())
                    landed.push_back(doc.docId);
            }
            phases.back().docs.push_back(std::move(receipt));
            throw TransactionError(std::string(phase), err.what(), TransactionError::Partial{.created = created, .landed = landed});
        }
    }
}

template <typename T, typename ClientType, typename DriveType = ClientType, typename ProgramFn>
inline TransactionResult<T> transactionRun(
    ProgramFn &&program,
    TransactionOptions<ClientType, DriveType> opts) {
    Ledger ledger = ledgerCreate();
    std::vector<PhaseReport> phases;
    std::vector<std::string> created;
    std::vector<std::string> landed;
    std::set<std::string> affected;
    int attempts = 0;
    std::string phase = "planning";
    if (!opts.client)
        throw model::CoreError("operationFailed", "A transaction requires a configured Docs client.");

    try {
        for (;;) {
            phase = "planning";
            attempts++;
            CoreSession session(opts.client, opts.drive, &ledger, opts.allow);
            T output = program(session);
            Plan plan = planBuild(session);
            auto findings = plan.findings;

            bool refused = false;
            for (const auto &f : findings) {
                if (f.severity == "block" && !f.waived) {
                    refused = true;
                    break;
                }
            }

            for (auto &doc : plan.docs) {
                std::vector<std::string> waivedList;
                for (const auto &finding : findings) {
                    if (finding.docId == doc.docId && finding.kind == "namedRange" &&
                        finding.waived && finding.anchor.has_value()) {
                        waivedList.push_back(*finding.anchor);
                    }
                }
                if (!waivedList.empty()) {
                    doc.approvedNamedRangeLosses = std::move(waivedList);
                }
            }

            if (opts.onPlan) {
                opts.onPlan(plan);
            }

            if (refused) {
                bool alreadyLanded = false;
                for (const auto &r : phases) {
                    for (const auto &d : r.docs) {
                        if (d.landed) {
                            alreadyLanded = true;
                            break;
                        }
                    }
                }
                if (alreadyLanded) {
                    std::string msg;
                    for (const auto &finding : findings) {
                        if (finding.severity == "block" && !finding.waived) {
                            if (!msg.empty())
                                msg += "\n";
                            msg += finding.message;
                        }
                    }
                    throw model::CoreError("internal", "replanning was refused after earlier changes landed:\n" + msg);
                }

                std::vector<DocVerification> verifications;
                for (const auto &d : plan.docs) {
                    verifications.push_back(DocVerification{
                        .diffs = {},
                        .docId = d.docId,
                        .status = "emulated",
                    });
                }

                return TransactionResult<T>{
                    .attempts = attempts,
                    .diff = std::move(plan.diff),
                    .diffs = std::move(plan.diffs),
                    .findings = std::move(findings),
                    .newIds = buildNewIds(ledger, {}),
                    .output = std::move(output),
                    .phases = std::move(phases),
                    .refused = true,
                    .verification = std::move(verifications),
                };
            }

            // Create phase
            for (const auto &doc : plan.docs) {
                if (doc.isNew || !doc.plan.tabsToAdd.empty() || !doc.plan.contentRequests.empty() ||
                    !doc.plan.tabOps.empty() || !doc.plan.pendingLinks.empty()) {
                    affected.insert(doc.docId);
                }
            }

            phase = "create";
            auto createReport = createPhase(plan, opts, ledger, created, phases, landed);
            if (createReport.has_value()) {
                attempts--;
                continue;
            }

            // Local image uploads
            if (opts.imageUploads) {
                std::vector<std::string> pending;
                for (auto &doc : plan.docs) {
                    for (const auto &req : doc.plan.contentRequests) {
                        if (req.is_object()) {
                            std::string uri;
                            if (req.contains("insertInlineImage") && req["insertInlineImage"].is_object() &&
                                req["insertInlineImage"].contains("uri") && req["insertInlineImage"]["uri"].is_string()) {
                                uri = req["insertInlineImage"]["uri"].template get<std::string>();
                            } else if (req.contains("replaceImage") && req["replaceImage"].is_object() &&
                                       req["replaceImage"].contains("uri") && req["replaceImage"]["uri"].is_string()) {
                                uri = req["replaceImage"]["uri"].template get<std::string>();
                            }
                            if (uri.starts_with(model::IMAGE_UPLOAD_SCHEME)) {
                                if (std::find(pending.begin(), pending.end(), uri) == pending.end())
                                    pending.push_back(uri);
                            }
                        }
                    }
                }
                if (!pending.empty()) {
                    phase = "content";
                    auto uris = opts.imageUploads(pending);
                    for (auto &doc : plan.docs) {
                        for (auto &req : doc.plan.contentRequests) {
                            if (req.is_object()) {
                                if (req.contains("insertInlineImage") && req["insertInlineImage"].is_object() &&
                                    req["insertInlineImage"].contains("uri") && req["insertInlineImage"]["uri"].is_string()) {
                                    std::string u = req["insertInlineImage"]["uri"].template get<std::string>();
                                    auto it = uris.find(u);
                                    if (it != uris.end())
                                        req["insertInlineImage"]["uri"] = it->second;
                                } else if (req.contains("replaceImage") && req["replaceImage"].is_object() &&
                                           req["replaceImage"].contains("uri") && req["replaceImage"]["uri"].is_string()) {
                                    std::string u = req["replaceImage"]["uri"].template get<std::string>();
                                    auto it = uris.find(u);
                                    if (it != uris.end())
                                        req["replaceImage"]["uri"] = it->second;
                                }
                            }
                        }
                    }
                }
            }

            // Content phase
            phase = "content";
            PhaseReport contentReport{.docs = {}, .phase = "content"};
            phases.push_back(contentReport);
            std::map<std::string, std::optional<std::string>> revisions;

            for (const auto &doc : plan.docs) {
                if (doc.plan.contentRequests.empty()) {
                    if (doc.input.originalJson.contains("revisionId") && doc.input.originalJson["revisionId"].is_string()) {
                        revisions[doc.docId] = doc.input.originalJson["revisionId"].template get<std::string>();
                    }
                    continue;
                }
                try {
                    auto sent = contentSend(opts.client, doc);
                    revisions[doc.docId] = sent.revisionAfter;
                    phases.back().docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .landed = true,
                        .requestCount = sent.requestCount,
                        .revisionAfter = sent.revisionAfter,
                        .outcome = "confirmed",
                    });
                    landed.push_back(doc.docId);
                } catch (const RevisionConflict &error) {
                    phases.back().docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .error = error.what(),
                        .outcome = "notSent",
                    });
                    throw model::CoreError(
                        "staleBase",
                        "the document changed before this batch; export fresh and reapply only outstanding edits");
                } catch (const TransactionError &err) {
                    if (err.partial.landed.size() > 0) {
                        landed.push_back(doc.docId);
                    }
                    phases.back().docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .error = err.causeMessage,
                        .landed = !err.partial.landed.empty(),
                        .requestCount = err.landedRequestCount,
                        .revisionAfter = err.revisionAfter,
                        .outcome = err.partial.unknown.empty()
                                       ? (err.landedRequestCount > 0 ? "confirmed" : "notSent")
                                       : "unknown",
                        .unknownRequestCount = err.unknownRequestCount,
                    });
                    TransactionError failure(
                        "content",
                        err.causeMessage,
                        TransactionError::Partial{.created = created, .landed = landed, .unknown = err.partial.unknown},
                        err.tabIndex,
                        err.landedRequestCount);
                    failure.unknownRequestCount = err.unknownRequestCount;
                    failure.revisionAfter = err.revisionAfter;
                    throw failure;
                } catch (const std::exception &err) {
                    phases.back().docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .error = err.what(),
                        .landed = false,
                        .requestCount = 0,
                    });
                    throw TransactionError(
                        "content",
                        err.what(),
                        TransactionError::Partial{.created = created, .landed = landed});
                }
            }

            // Tabs phase
            auto getTabIds = [&](const std::string &docId) {
                return tabIdMap(ledger, docId);
            };

            phase = "tabs";
            std::vector<PlannedDoc> withTabs;
            for (const auto &d : plan.docs) {
                if (!d.plan.tabOps.empty())
                    withTabs.push_back(d);
            }
            if (!withTabs.empty()) {
                phaseRun(phases, "tabs", withTabs, created, landed, [&](const PlannedDoc &doc, auto &&) {
                    auto revision = batchSend(opts.client, doc.docId, doc.plan.tabOps, revisions[doc.docId]);
                    revisions[doc.docId] = revision;
                    return ContentSendResult{
                        .requestCount = static_cast<int>(doc.plan.tabOps.size()),
                        .revisionAfter = revision,
                    };
                });
            }

            // Footnotes phase
            std::map<std::string, std::string> footnoteIds;
            std::vector<PlannedDoc> withFootnotes;
            for (const auto &d : plan.docs) {
                if (!d.plan.footnoteBodies.empty())
                    withFootnotes.push_back(d);
            }
            if (!withFootnotes.empty()) {
                phase = "footnotes";
                phaseRun(phases, "footnotes", withFootnotes, created, landed, [&](const PlannedDoc &doc, auto &&) {
                    auto json = clientGetDocument(opts.client, doc.docId);
                    if (!revisions[doc.docId] || !json.contains("revisionId") ||
                        !json["revisionId"].is_string() ||
                        json["revisionId"].template get<std::string>() != *revisions[doc.docId]) {
                        throw model::CoreError("concurrentEdits", "the document changed after content landed; footnote text was not sent");
                    }
                    model::KeyAllocator keys;
                    auto reloaded = model::docModelParse(json, model::DocModelParseOptions{.docId = doc.docId, .keys = keys});
                    auto tabs = getTabIds(doc.docId);

                    std::vector<slimr::Json> requests;
                    for (const auto &body : doc.plan.footnoteBodies) {
                        std::string realTab = tabs.contains(body.tabId) ? tabs[body.tabId] : body.tabId;
                        const model::TabModel *finalTab = nullptr;
                        for (const auto &tab : doc.input.final.tabs) {
                            if (tab.tabId == body.tabId) {
                                finalTab = &tab;
                                break;
                            }
                        }
                        const model::TabModel *reloadedTab = nullptr;
                        for (const auto &tab : reloaded.tabs) {
                            if (tab.tabId == realTab) {
                                reloadedTab = &tab;
                                break;
                            }
                        }

                        auto finalOrder = finalTab ? footnoteOrder(*finalTab) : std::vector<std::string>{};
                        auto reloadedOrder = reloadedTab ? footnoteOrder(*reloadedTab) : std::vector<std::string>{};
                        int at = -1;
                        for (size_t i = 0; i < finalOrder.size(); ++i) {
                            if (finalOrder[i] == body.id) {
                                at = static_cast<int>(i);
                                break;
                            }
                        }
                        std::string id = (at >= 0 && at < static_cast<int>(reloadedOrder.size())) ? reloadedOrder[at] : "";
                        if (id.empty()) {
                            throw model::CoreError("internal", "new footnote " + body.id + " wasn't found after content landed");
                        }
                        footnoteIds[body.id] = id;

                        for (const auto &req : body.requests) {
                            std::string s = slimr::write_json(req);
                            // replace provisional tab id with real tab id
                            std::string target = "\"" + body.tabId + "\"";
                            std::string replacement = "\"" + realTab + "\"";
                            size_t pos = 0;
                            while ((pos = s.find(target, pos)) != std::string::npos) {
                                s.replace(pos, target.length(), replacement);
                                pos += replacement.length();
                            }
                            auto parsed = slimr::parse_json<slimr::Json>(s);
                            requests.push_back(reconcile::segmentRequest(parsed ? *parsed : req, id));
                        }
                    }
                    std::optional<std::string> curRev;
                    if (json.contains("revisionId") && json["revisionId"].is_string()) {
                        curRev = json["revisionId"].template get<std::string>();
                    }
                    auto revision = batchSend(opts.client, doc.docId, requests, curRev);
                    revisions[doc.docId] = revision;
                    return ContentSendResult{
                        .requestCount = static_cast<int>(requests.size()),
                        .revisionAfter = revision,
                    };
                });
            }

            // Links phase
            std::map<std::string, std::string> headingIds;
            std::vector<PlannedDoc> withLinks;
            for (const auto &d : plan.docs) {
                if (!d.plan.pendingLinks.empty())
                    withLinks.push_back(d);
            }
            if (!withLinks.empty()) {
                phase = "links";
                std::map<std::string, model::DocModel> reloaded;
                std::map<std::string, model::DocModel> finals;
                for (const auto &doc : plan.docs) {
                    finals[doc.docId] = doc.input.final;
                    auto json = clientGetDocument(opts.client, doc.docId);
                    if (!revisions[doc.docId] || !json.contains("revisionId") ||
                        !json["revisionId"].is_string() ||
                        json["revisionId"].template get<std::string>() != *revisions[doc.docId]) {
                        throw model::CoreError("concurrentEdits", "the document changed after content landed; links were not sent");
                    }
                    model::KeyAllocator keys;
                    reloaded[doc.docId] = model::docModelParse(json, model::DocModelParseOptions{.docId = doc.docId, .keys = keys});
                }
                phaseRun(phases, "links", withLinks, created, landed, [&](const PlannedDoc &doc, auto &&) {
                    auto lr = linkRequests(doc, reloaded, finals, getTabIds);
                    for (const auto &[k, v] : lr.headingIds) {
                        headingIds[k] = v;
                    }
                    auto revision = batchSend(opts.client, doc.docId, lr.requests, revisions[doc.docId]);
                    revisions[doc.docId] = revision;
                    return ContentSendResult{
                        .requestCount = static_cast<int>(lr.requests.size()),
                        .revisionAfter = revision,
                    };
                });
            }

            // Verification phase
            phase = "verification";
            std::vector<DocVerification> verifications;
            std::map<std::string, slimr::Json> documents;
            for (const auto &doc : plan.docs) {
                auto json = clientGetDocument(opts.client, doc.docId);
                auto tIds = getTabIds(doc.docId);
                newHeadingIds(doc, json, tIds, headingIds);

                VerifyResolvedOptions resolved{
                    .footnoteIds = footnoteIds,
                    .headingIds = headingIds,
                    .tabIds = tIds,
                };
                auto checked = verify(doc, json, revisions[doc.docId], resolved);
                verifications.push_back(checked);
                documents.emplace(doc.docId, std::move(json));

                if (checked.status == "mismatch") {
                    std::string diffsMsg;
                    for (const auto &d : checked.diffs) {
                        if (!diffsMsg.empty())
                            diffsMsg += "\n";
                        diffsMsg += d;
                    }
                    throw model::CoreError("internal", "post-write verification for " + doc.docId + " did not match the intended document:\n" + diffsMsg);
                }
            }

            return TransactionResult<T>{
                .attempts = attempts,
                .diff = std::move(plan.diff),
                .diffs = std::move(plan.diffs),
                .findings = std::move(findings),
                .newIds = buildNewIds(ledger, headingIds),
                .output = std::move(output),
                .phases = std::move(phases),
                .refused = false,
                .verification = std::move(verifications),
                .documents = std::move(documents),
            };
        }
    } catch (TransactionError &err) {
        err.phases = phases;
        throw;
    } catch (const std::exception &err) {
        bool anyLanded = false;
        for (const auto &report : phases) {
            for (const auto &doc : report.docs) {
                if (doc.landed) {
                    anyLanded = true;
                    break;
                }
            }
        }
        if (anyLanded) {
            TransactionError failure(phase, err.what(), TransactionError::Partial{.created = created, .landed = landed});
            failure.phases = phases;
            throw failure;
        }
        throw;
    }
}

} // namespace gdocsmith::engine
