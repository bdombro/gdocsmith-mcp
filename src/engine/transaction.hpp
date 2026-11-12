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
    ClientType* client = nullptr;
    DriveType* drive = nullptr;
    std::vector<std::string> allow;
    std::function<void(const Plan&)> onPlan;
    std::function<std::map<std::string, std::string>(const std::vector<std::string>&)> imageUploads;
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
};

template <typename ClientType>
inline std::string clientCreateDocument(ClientType* client, const std::string& title) {
    if constexpr (requires { client->createDocument(title); }) {
        auto res = client->createDocument(title);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            return res;
        } else {
            if (res.contains("documentId") && res["documentId"].is_string()) {
                return res["documentId"].template get<std::string>();
            }
            if (res.contains("id") && res["id"].is_string()) {
                return res["id"].template get<std::string>();
            }
            return "";
        }
    } else if constexpr (requires { client->create_document(title); }) {
        auto res = client->create_document(title);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            return res;
        } else {
            if (res.contains("documentId") && res["documentId"].is_string()) {
                return res["documentId"].template get<std::string>();
            }
            return "";
        }
    } else {
        throw std::runtime_error("this client can't create documents");
    }
}

template <typename ClientType>
inline slimr::Json clientGetDocument(ClientType* client, const std::string& docId) {
    if constexpr (requires { client->getDocument(docId); }) {
        auto res = client->getDocument(docId);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            auto parsed = slimr::parse_json<slimr::Json>(res);
            return parsed ? *parsed : slimr::Json{};
        } else {
            return res;
        }
    } else if constexpr (requires { client->get_document(docId); }) {
        auto res = client->get_document(docId);
        if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::string>) {
            auto parsed = slimr::parse_json<slimr::Json>(res);
            return parsed ? *parsed : slimr::Json{};
        } else {
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
    ClientType* client,
    const std::string& docId,
    const std::vector<slimr::Json>& requests,
    const std::optional<std::string>& revision = std::nullopt
) {
    std::string respStr;
    if constexpr (requires { client->batchUpdate(docId, requests, revision); }) {
        auto r = client->batchUpdate(docId, requests, revision);
        respStr = r.is_string() ? r.template get<std::string>() : slimr::write_json(r);
    } else if constexpr (requires { client->batch_update(docId, requests, revision); }) {
        respStr = client->batch_update(docId, requests, revision);
    } else {
        throw std::runtime_error("this client can't batchUpdate");
    }
    auto parsed = slimr::parse_json<slimr::Json>(respStr);
    return parsed ? *parsed : slimr::Json{};
}

inline std::map<std::string, std::string> tabIdMap(const Ledger& ledger, const std::string& docId) {
    std::map<std::string, std::string> m;
    auto it = ledger.tabs.find(docId);
    if (it != ledger.tabs.end()) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            m["new_tab_" + std::to_string(i + 1)] = it->second[i];
        }
    }
    return m;
}

inline std::vector<std::string> footnoteOrder(const model::TabModel& tab) {
    std::vector<std::string> ids;
    for (const auto& block : model::blocksDeep(tab.blocks)) {
        if (block.kind == "paragraph" && block.paragraph) {
            for (const auto& inlineItem : block.paragraph->inlines) {
                if (inlineItem.kind == "atom" && inlineItem.type == "footnoteRef") {
                    std::string id;
                    if (inlineItem.create) {
                        id = "new_fn_" + inlineItem.key;
                    } else if (inlineItem.raw.has_value() && inlineItem.raw->is_object() &&
                               inlineItem.raw->contains("footnoteReference") &&
                               (*inlineItem.raw)["footnoteReference"].is_object() &&
                               (*inlineItem.raw)["footnoteReference"].contains("footnoteId") &&
                               (*inlineItem.raw)["footnoteReference"]["footnoteId"].is_string()) {
                        id = (*inlineItem.raw)["footnoteReference"]["footnoteId"].template get<std::string>();
                    }
                    if (!id.empty()) ids.push_back(std::move(id));
                }
            }
        }
    }
    return ids;
}

inline void newHeadingIds(
    const PlannedDoc& doc,
    const slimr::Json& json,
    const std::map<std::string, std::string>& tabIds,
    std::map<std::string, std::string>& out
) {
    model::KeyAllocator keys;
    auto actual = model::docModelParse(json, model::DocModelParseOptions{.docId = doc.docId, .keys = keys});
    for (const auto& tab : doc.input.final.tabs) {
        std::string targetTabId = tab.tabId;
        auto tIt = tabIds.find(tab.tabId);
        if (tIt != tabIds.end()) targetTabId = tIt->second;

        const model::TabModel* now = nullptr;
        for (const auto& t : actual.tabs) {
            if (t.tabId == targetTabId) { now = &t; break; }
        }
        if (!now) continue;

        auto ranges = model::layoutCompute(tab).blockRanges;
        for (const auto& block : tab.blocks) {
            if (block.kind != "paragraph" || !block.key.starts_with("n") || !block.paragraph) continue;
            const auto& st = block.paragraph->style;
            if (!st.contains("namedStyleType") || !st["namedStyleType"].is_string()) continue;
            std::string nst = st["namedStyleType"].template get<std::string>();
            if (nst == "NORMAL_TEXT" || nst.empty()) continue;

            auto rIt = ranges.find(block.key);
            if (rIt == ranges.end()) continue;
            int start = rIt->second.start;

            for (const auto& b : now->blocks) {
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

inline NewIds buildNewIds(const Ledger& ledger, const std::map<std::string, std::string>& headings) {
    NewIds res;
    for (const auto& [alias, id] : ledger.docs) {
        res.docs["new_" + alias] = id;
    }
    for (const auto& [docId, ids] : ledger.tabs) {
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
    const PlannedDoc& doc,
    const slimr::Json& json,
    const std::optional<std::string>& revision,
    const VerifyResolvedOptions& resolved
) {
    return verifyDoc(doc.docId, doc.input.final, json, revision, resolved.tabIds);
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
    const PlannedDoc& doc,
    const std::map<std::string, model::DocModel>& reloaded,
    const std::map<std::string, model::DocModel>& finals,
    GetTabIds&& getTabIds
) {
    LinkRequestsResult out;
    if (doc.plan.pendingLinks.empty()) return out;

    auto finIt = finals.find(doc.docId);
    auto relIt = reloaded.find(doc.docId);
    if (finIt == finals.end() || relIt == reloaded.end()) return out;
    const model::DocModel& finalDoc = finIt->second;
    const model::DocModel& relDoc = relIt->second;

    std::map<std::string, std::string> tabIds = getTabIds(doc.docId);

    struct Loc {
        int start = -1;
        std::string tabId;
    };
    auto locate = [](const model::DocModel& m, const std::string& key) -> Loc {
        for (const auto& tab : m.tabs) {
            auto ranges = model::layoutCompute(tab).blockRanges;
            auto it = ranges.find(key);
            if (it != ranges.end()) {
                return Loc{.start = it->second.start, .tabId = tab.tabId};
            }
        }
        return Loc{};
    };
    auto headingIdAt = [](const model::DocModel& m, const std::string& tabId, int start) -> std::string {
        for (const auto& tab : m.tabs) {
            if (!tabId.empty() && tab.tabId != tabId) continue;
            for (const auto& block : model::blocksDeep(tab.blocks)) {
                if (block.is_paragraph() && block.paragraph->origin.has_value() &&
                    block.paragraph->origin->start == start && block.paragraph->headingId.has_value()) {
                    return *block.paragraph->headingId;
                }
            }
        }
        return "";
    };

    for (const auto& pl : doc.plan.pendingLinks) {
        auto headingLoc = locate(finalDoc, pl.headingKey);
        if (headingLoc.start < 0) continue;
        std::string headingId = headingIdAt(relDoc, headingLoc.tabId, headingLoc.start);
        if (headingId.empty()) continue;
        out.headingIds[pl.headingKey] = headingId;

        auto paraLoc = locate(finalDoc, pl.paragraphKey);
        if (paraLoc.start < 0) continue;
        int linkStart = paraLoc.start + pl.offset;
        int linkEnd = linkStart + pl.length;

        std::string realTabId = pl.tabId;
        auto tIt = tabIds.find(pl.tabId);
        if (tIt != tabIds.end()) realTabId = tIt->second;

        slimr::Json linkStyle = slimr::Json::object_t{
            {"link", slimr::Json::object_t{{"headingId", headingId}}}
        };
        out.requests.push_back(reconcile::RequestBuilder::textStyleUpdate(
            linkStart, linkEnd, linkStyle, {"link"}, realTabId));
    }

    return out;
}

template <typename ClientType, typename DriveType>
inline std::optional<PhaseReport> createPhase(
    const Plan& plan,
    TransactionOptions<ClientType, DriveType>& opts,
    Ledger& ledger,
    std::vector<std::string>& created,
    std::vector<PhaseReport>& phases
) {
    PhaseReport report{.docs = {}, .phase = "create"};
    for (const auto& doc : plan.docs) {
        if (doc.isNew && doc.create) {
            std::string alias = doc.docId;
            if (alias.starts_with("new_")) alias = alias.substr(4);
            try {
                std::string id = clientCreateDocument(opts.client, doc.create->title);
                ledger.docs[alias] = id;
                created.push_back(id);
                report.docs.push_back(PhaseDocReport{
                    .docId = id,
                    .landed = true,
                    .requestCount = 1,
                });
            } catch (const std::exception& err) {
                throw TransactionError("create", err.what(), TransactionError::Partial{.created = created, .landed = {}});
            }
        } else if (!doc.isNew && doc.plan.tabsToAdd.empty() && !doc.plan.segmentsToAdd.empty()) {
            std::optional<std::string> revision;
            if (doc.input.originalJson.contains("revisionId") && doc.input.originalJson["revisionId"].is_string()) {
                revision = doc.input.originalJson["revisionId"].template get<std::string>();
            }
            for (const auto& segment : doc.plan.segmentsToAdd) {
                try {
                    int breakIdx = segment.breakIndex.value_or(0);
                    auto req = reconcile::RequestBuilder::segmentCreate(segment.kind, segment.tabId, breakIdx);
                    auto response = clientBatchUpdate(opts.client, doc.docId, {req}, revision);
                    if (response.contains("writeControl") && response["writeControl"].is_object() &&
                        response["writeControl"].contains("requiredRevisionId") &&
                        response["writeControl"]["requiredRevisionId"].is_string()) {
                        revision = response["writeControl"]["requiredRevisionId"].template get<std::string>();
                    }
                    report.docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .landed = true,
                        .requestCount = 1,
                        .revisionAfter = revision,
                    });
                } catch (const std::exception& err) {
                    throw TransactionError("create", err.what(), TransactionError::Partial{.created = created, .landed = {}});
                }
            }
        } else if (!doc.isNew && !doc.plan.tabsToAdd.empty()) {
            auto& ids = ledger.tabs[doc.docId];
            std::map<std::string, std::string> real;
            std::optional<std::string> revision;
            if (doc.input.originalJson.contains("revisionId") && doc.input.originalJson["revisionId"].is_string()) {
                revision = doc.input.originalJson["revisionId"].template get<std::string>();
            }
            for (const auto& tab : doc.plan.tabsToAdd) {
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
                    auto response = clientBatchUpdate(opts.client, doc.docId, {req}, revision);
                    if (response.contains("writeControl") && response["writeControl"].is_object() &&
                        response["writeControl"].contains("requiredRevisionId") &&
                        response["writeControl"]["requiredRevisionId"].is_string()) {
                        revision = response["writeControl"]["requiredRevisionId"].template get<std::string>();
                    }
                    std::string tabId;
                    if (response.contains("replies") && response["replies"].is_array() &&
                        !response["replies"].template get<slimr::Json::array_t>().empty()) {
                        const auto& firstReply = response["replies"][0];
                        if (firstReply.contains("addDocumentTab") && firstReply["addDocumentTab"].is_object()) {
                            const auto& adt = firstReply["addDocumentTab"];
                            if (adt.contains("tabProperties") && adt["tabProperties"].is_object()) {
                                const auto& tp = adt["tabProperties"];
                                if (tp.contains("tabId") && tp["tabId"].is_string()) {
                                    tabId = tp["tabId"].template get<std::string>();
                                }
                            }
                        }
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
                    report.docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .landed = true,
                        .requestCount = 1,
                        .revisionAfter = revision,
                    });
                } catch (const std::exception& err) {
                    throw TransactionError("create", err.what(), TransactionError::Partial{.created = created, .landed = {}});
                }
            }
        }
    }
    if (!report.docs.empty()) {
        phases.push_back(report);
        return report;
    }
    return std::nullopt;
}

template <typename Fn>
inline void phaseRun(
    std::vector<PhaseReport>& phases,
    std::string_view phase,
    const std::vector<PlannedDoc>& docs,
    const std::vector<std::string>& created,
    const std::vector<std::string>& landed,
    Fn&& run
) {
    PhaseReport report{.docs = {}, .phase = std::string(phase)};
    phases.push_back(report);
    for (const auto& doc : docs) {
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
                phases.back().docs.push_back(std::move(receipt));
            }
        } catch (const std::exception& err) {
            receipt.error = err.what();
            phases.back().docs.push_back(std::move(receipt));
            throw TransactionError(std::string(phase), err.what(), TransactionError::Partial{.created = created, .landed = landed});
        }
    }
}

template <typename T, typename ClientType, typename DriveType = ClientType, typename ProgramFn>
inline TransactionResult<T> transactionRun(
    ProgramFn&& program,
    TransactionOptions<ClientType, DriveType> opts
) {
    Ledger ledger = ledgerCreate();
    std::vector<PhaseReport> phases;
    std::vector<std::string> created;
    std::vector<std::string> landed;
    std::set<std::string> affected;
    int attempts = 0;
    std::string phase = "planning";

    try {
        for (;;) {
            phase = "planning";
            attempts++;
            CoreSession session(opts.client, opts.drive, &ledger, opts.allow);
            T output = program(session);
            Plan plan = planBuild(session);
            auto findings = plan.findings;

            bool refused = false;
            for (const auto& f : findings) {
                if (f.severity == "block" && !f.waived) {
                    refused = true;
                    break;
                }
            }

            for (auto& doc : plan.docs) {
                std::vector<std::string> waivedList;
                for (const auto& finding : findings) {
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
                for (const auto& r : phases) {
                    for (const auto& d : r.docs) {
                        if (d.landed) { alreadyLanded = true; break; }
                    }
                }
                if (alreadyLanded) {
                    std::string msg;
                    for (const auto& finding : findings) {
                        if (finding.severity == "block" && !finding.waived) {
                            if (!msg.empty()) msg += "\n";
                            msg += finding.message;
                        }
                    }
                    throw model::CoreError("internal", "replanning was refused after earlier changes landed:\n" + msg);
                }

                std::vector<DocVerification> verifications;
                for (const auto& d : plan.docs) {
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
            for (const auto& doc : plan.docs) {
                if (doc.isNew || !doc.plan.tabsToAdd.empty() || !doc.plan.contentRequests.empty() ||
                    !doc.plan.tabOps.empty() || !doc.plan.pendingLinks.empty()) {
                    affected.insert(doc.docId);
                }
            }

            phase = "create";
            auto createReport = createPhase(plan, opts, ledger, created, phases);
            if (createReport.has_value()) {
                attempts--;
                continue;
            }

            // Local image uploads
            if (opts.imageUploads) {
                std::vector<std::string> pending;
                for (auto& doc : plan.docs) {
                    for (const auto& req : doc.plan.contentRequests) {
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
                                pending.push_back(uri);
                            }
                        }
                    }
                }
                if (!pending.empty()) {
                    phase = "content";
                    auto uris = opts.imageUploads(pending);
                    for (auto& doc : plan.docs) {
                        for (auto& req : doc.plan.contentRequests) {
                            if (req.is_object()) {
                                if (req.contains("insertInlineImage") && req["insertInlineImage"].is_object() &&
                                    req["insertInlineImage"].contains("uri") && req["insertInlineImage"]["uri"].is_string()) {
                                    std::string u = req["insertInlineImage"]["uri"].template get<std::string>();
                                    auto it = uris.find(u);
                                    if (it != uris.end()) req["insertInlineImage"]["uri"] = it->second;
                                } else if (req.contains("replaceImage") && req["replaceImage"].is_object() &&
                                           req["replaceImage"].contains("uri") && req["replaceImage"]["uri"].is_string()) {
                                    std::string u = req["replaceImage"]["uri"].template get<std::string>();
                                    auto it = uris.find(u);
                                    if (it != uris.end()) req["replaceImage"]["uri"] = it->second;
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

            for (const auto& doc : plan.docs) {
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
                    });
                    landed.push_back(doc.docId);
                } catch (const RevisionConflict&) {
                    throw model::CoreError(
                        "staleBase",
                        "the document changed before this batch; export fresh and reapply only outstanding edits"
                    );
                } catch (const TransactionError& err) {
                    if (err.partial.landed.size() > 0) {
                        landed.push_back(doc.docId);
                    }
                    phases.back().docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .error = err.causeMessage,
                        .landed = !err.partial.landed.empty(),
                        .requestCount = 0,
                    });
                    throw TransactionError(
                        "content",
                        err.causeMessage,
                        TransactionError::Partial{.created = created, .landed = landed},
                        err.tabIndex
                    );
                } catch (const std::exception& err) {
                    phases.back().docs.push_back(PhaseDocReport{
                        .docId = doc.docId,
                        .error = err.what(),
                        .landed = false,
                        .requestCount = 0,
                    });
                    throw TransactionError(
                        "content",
                        err.what(),
                        TransactionError::Partial{.created = created, .landed = landed}
                    );
                }
            }

            // Tabs phase
            auto getTabIds = [&](const std::string& docId) {
                return tabIdMap(ledger, docId);
            };

            phase = "tabs";
            std::vector<PlannedDoc> withTabs;
            for (const auto& d : plan.docs) {
                if (!d.plan.tabOps.empty()) withTabs.push_back(d);
            }
            if (!withTabs.empty()) {
                phaseRun(phases, "tabs", withTabs, created, landed, [&](const PlannedDoc& doc, auto&&) {
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
            for (const auto& d : plan.docs) {
                if (!d.plan.footnoteBodies.empty()) withFootnotes.push_back(d);
            }
            if (!withFootnotes.empty()) {
                phase = "footnotes";
                phaseRun(phases, "footnotes", withFootnotes, created, landed, [&](const PlannedDoc& doc, auto&&) {
                    auto json = clientGetDocument(opts.client, doc.docId);
                    if (revisions[doc.docId].has_value() && json.contains("revisionId") &&
                        json["revisionId"].is_string() &&
                        json["revisionId"].template get<std::string>() != *revisions[doc.docId]) {
                        throw model::CoreError("concurrentEdits", "the document changed after content landed; footnote text was not sent");
                    }
                    model::KeyAllocator keys;
                    auto reloaded = model::docModelParse(json, model::DocModelParseOptions{.docId = doc.docId, .keys = keys});
                    auto tabs = getTabIds(doc.docId);

                    std::vector<slimr::Json> requests;
                    for (const auto& body : doc.plan.footnoteBodies) {
                        std::string realTab = tabs.contains(body.tabId) ? tabs[body.tabId] : body.tabId;
                        const model::TabModel* finalTab = nullptr;
                        for (const auto& tab : doc.input.final.tabs) {
                            if (tab.tabId == body.tabId) { finalTab = &tab; break; }
                        }
                        const model::TabModel* reloadedTab = nullptr;
                        for (const auto& tab : reloaded.tabs) {
                            if (tab.tabId == realTab) { reloadedTab = &tab; break; }
                        }

                        auto finalOrder = finalTab ? footnoteOrder(*finalTab) : std::vector<std::string>{};
                        auto reloadedOrder = reloadedTab ? footnoteOrder(*reloadedTab) : std::vector<std::string>{};
                        int at = -1;
                        for (size_t i = 0; i < finalOrder.size(); ++i) {
                            if (finalOrder[i] == body.id) { at = static_cast<int>(i); break; }
                        }
                        std::string id = (at >= 0 && at < static_cast<int>(reloadedOrder.size())) ? reloadedOrder[at] : "";
                        if (id.empty()) {
                            throw model::CoreError("internal", "new footnote " + body.id + " wasn't found after content landed");
                        }
                        footnoteIds[body.id] = id;

                        for (const auto& req : body.requests) {
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
            for (const auto& d : plan.docs) {
                if (!d.plan.pendingLinks.empty()) withLinks.push_back(d);
            }
            if (!withLinks.empty()) {
                phase = "links";
                std::map<std::string, model::DocModel> reloaded;
                std::map<std::string, model::DocModel> finals;
                for (const auto& doc : plan.docs) {
                    finals[doc.docId] = doc.input.final;
                    auto json = clientGetDocument(opts.client, doc.docId);
                    if (revisions[doc.docId].has_value() && json.contains("revisionId") &&
                        json["revisionId"].is_string() &&
                        json["revisionId"].template get<std::string>() != *revisions[doc.docId]) {
                        throw model::CoreError("concurrentEdits", "the document changed after content landed; links were not sent");
                    }
                    model::KeyAllocator keys;
                    reloaded[doc.docId] = model::docModelParse(json, model::DocModelParseOptions{.docId = doc.docId, .keys = keys});
                }
                phaseRun(phases, "links", withLinks, created, landed, [&](const PlannedDoc& doc, auto&&) {
                    auto lr = linkRequests(doc, reloaded, finals, getTabIds);
                    for (const auto& [k, v] : lr.headingIds) {
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
            for (const auto& doc : plan.docs) {
                bool sentSomething = !doc.plan.contentRequests.empty() || !doc.plan.tabOps.empty() || !doc.plan.pendingLinks.empty();
                if (!sentSomething) continue;

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

                if (checked.status == "mismatch") {
                    std::string diffsMsg;
                    for (const auto& d : checked.diffs) {
                        if (!diffsMsg.empty()) diffsMsg += "\n";
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
            };
        }
    } catch (TransactionError& err) {
        err.phases = phases;
        throw;
    } catch (const std::exception& err) {
        bool anyLanded = false;
        for (const auto& report : phases) {
            for (const auto& doc : report.docs) {
                if (doc.landed) { anyLanded = true; break; }
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
