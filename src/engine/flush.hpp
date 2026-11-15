#pragma once

#include "engine/guard.hpp"
#include "lens/named_ranges.hpp"
#include "model/equivalence.hpp"
#include "model/errors.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/layout.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

constexpr size_t MAX_REQUESTS_PER_BATCH = 2000;

struct PhaseDocReport {
    std::string docId;
    std::optional<std::string> error;
    bool landed = false;
    int requestCount = 0;
    std::optional<std::string> revisionAfter;
    std::string outcome = "notSent"; // "confirmed" | "unknown" | "notSent"
    int unknownRequestCount = 0;

    bool operator==(const PhaseDocReport&) const = default;
};

struct PhaseReport {
    std::vector<PhaseDocReport> docs;
    std::string phase; // "create" | "content" | "tabs" | "footnotes" | "links"

    bool operator==(const PhaseReport&) const = default;
};

class TransactionError : public std::runtime_error {
public:
    std::string phase;
    std::string causeMessage;
    struct Partial {
        std::vector<std::string> created;
        std::vector<std::string> landed;
        std::vector<std::string> unknown;
    } partial;
    std::vector<PhaseReport> phases;
    std::optional<int> tabIndex;
    int landedRequestCount = 0;
    int unknownRequestCount = 0;
    std::optional<std::string> revisionAfter;

    TransactionError(
        std::string ph,
        std::string cause,
        Partial part = {},
        std::optional<int> tIndex = std::nullopt,
        int landedReqs = 0
    )
        : std::runtime_error("send failed in phase \"" + ph + "\": " + cause),
          phase(std::move(ph)),
          causeMessage(std::move(cause)),
          partial(std::move(part)),
          tabIndex(tIndex),
          landedRequestCount(landedReqs) {}
};

class RevisionConflict : public std::runtime_error {
public:
    std::string docId;

    explicit RevisionConflict(std::string id)
        : std::runtime_error("document " + id + " changed since it was loaded"),
          docId(std::move(id)) {}
};

inline bool revisionConflictIs(std::string_view msg) {
    std::string lower;
    lower.reserve(msg.size());
    for (char c : msg) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower.find("does not match the latest revision") != std::string::npos ||
           lower.find("required revision") != std::string::npos;
}

inline std::optional<int> requestIndexOfError(std::string_view msg) {
    static const std::regex reqIdxRegex(R"(requests\[(\d+)\])");
    std::string s(msg);
    std::smatch match;
    if (std::regex_search(s, match, reqIdxRegex)) {
        const auto digits = match[1].str();
        int index = 0;
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), index);
        if (parsed.ec == std::errc{}) return index;
    }
    return std::nullopt;
}

struct ContentSendResult {
    int requestCount = 0;
    std::optional<std::string> revisionAfter;
};

inline std::string revisionRequired(const std::optional<std::string>& revision) {
    if (!revision || revision->empty()) {
        throw model::CoreError("staleBase", "A nonempty expected revision is required before sending document edits.");
    }
    return *revision;
}

inline std::string responseRevision(const slimr::Json& response) {
    if (response.is_object() && response.contains("writeControl") &&
        response["writeControl"].is_object()) {
        const auto& control = response["writeControl"];
        if (control.contains("requiredRevisionId") && control["requiredRevisionId"].is_string()) {
            auto revision = control["requiredRevisionId"].get<std::string>();
            if (!revision.empty()) return revision;
        }
    }
    throw std::runtime_error("batchUpdate returned no authoritative revision; the write outcome is unknown.");
}

template <typename ClientType>
inline ContentSendResult contentSend(
    ClientType* client,
    const std::string& docId,
    const std::vector<slimr::Json>& requests,
    const std::vector<RequestOrigin>& origins,
    const std::optional<std::string>& initialRevision = std::nullopt
) {
    if (requests.empty()) return {.requestCount = 0, .revisionAfter = initialRevision};
    revisionRequired(initialRevision);
    if (!client) throw model::CoreError("operationFailed", "A Docs client is required before sending edits.");
    if constexpr (!(requires { client->batchUpdate(docId, requests, initialRevision); }) &&
                  !(requires { client->batch_update(docId, requests, initialRevision); })) {
        throw model::CoreError("operationFailed", "This client cannot batchUpdate.");
    }
    if (requests.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw model::CoreError("operationFailed", "The request count exceeds the supported transaction size.");
    }
    std::optional<std::string> revision = initialRevision;
    for (size_t offset = 0; offset < requests.size(); offset += MAX_REQUESTS_PER_BATCH) {
        size_t end = std::min(offset + MAX_REQUESTS_PER_BATCH, requests.size());
        std::vector<slimr::Json> chunk(requests.begin() + offset, requests.begin() + end);
        try {
            std::string respStr;
            if constexpr (requires { client->batchUpdate(docId, chunk, revision); }) {
                auto r = client->batchUpdate(docId, chunk, revision);
                respStr = r.is_string() ? r.template get<std::string>() : slimr::write_json(r);
            } else if constexpr (requires { client->batch_update(docId, chunk, revision); }) {
                respStr = client->batch_update(docId, chunk, revision);
            } else {
                throw std::runtime_error("this client can't batchUpdate");
            }

            auto parsed = slimr::parse_json<slimr::Json>(respStr);
            if (!parsed || !parsed->is_object()) {
                throw std::runtime_error("Invalid batchUpdate response; the write outcome is unknown.");
            }
            revision = responseRevision(*parsed);
        } catch (const std::exception& err) {
            std::string msg = err.what();
            if (offset == 0 && revisionConflictIs(msg)) {
                throw RevisionConflict(docId);
            }
            auto reqIdx = requestIndexOfError(msg);
            std::optional<int> tabIndex;
            if (reqIdx.has_value() && (offset + *reqIdx) < origins.size()) {
                tabIndex = origins[offset + *reqIdx].tabIndex;
            }
            TransactionError::Partial partial;
            if (offset > 0) partial.landed.push_back(docId);
            const bool unknown = !revisionConflictIs(msg) && !reqIdx.has_value();
            if (unknown) partial.unknown.push_back(docId);
            TransactionError failure("content", msg, std::move(partial), tabIndex, static_cast<int>(offset));
            failure.unknownRequestCount = unknown ? static_cast<int>(chunk.size()) : 0;
            failure.revisionAfter = revision;
            throw failure;
        }
    }
    return ContentSendResult{
        .requestCount = static_cast<int>(requests.size()),
        .revisionAfter = revision,
    };
}

template <typename ClientType, typename DocType>
inline ContentSendResult contentSend(
    ClientType* client,
    const DocType& doc
) {
    std::optional<std::string> initialRevision;
    if (doc.input.originalJson.contains("revisionId") && doc.input.originalJson["revisionId"].is_string()) {
        initialRevision = doc.input.originalJson["revisionId"].template get<std::string>();
    }
    return contentSend(client, doc.docId, doc.plan.contentRequests, doc.plan.origins, initialRevision);
}

template <typename ClientType, typename DocType>
inline ContentSendResult contentSend(
    ClientType& client,
    const DocType& doc
) {
    return contentSend(&client, doc);
}

template <typename ClientType>
inline std::optional<std::string> batchSend(
    ClientType* client,
    const std::string& docId,
    const std::vector<slimr::Json>& requests,
    const std::optional<std::string>& revision
) {
    return contentSend(client, docId, requests, {}, revision).revisionAfter;
}

struct VerificationResult {
    std::vector<std::string> diffs;
    std::string docId;
    std::string status; // "emulated" | "inconclusive" | "match" | "mismatch" | "skipped"

    bool operator==(const VerificationResult&) const = default;
};

inline VerificationResult verifyDoc(
    const std::string& docId,
    const model::DocModel& expected,
    const slimr::Json& actualJson,
    const std::optional<std::string>& lastRevision,
    const std::map<std::string, std::string>& tabIdMap = {},
    const std::vector<model::IdentityTransfer>& identityTransfers = {},
    const std::set<std::string>& listsRebuilt = {}
) {
    if (lastRevision.has_value()) {
        if (!actualJson.is_object() || !actualJson.contains("revisionId") ||
            !actualJson["revisionId"].is_string() ||
            actualJson["revisionId"].get<std::string>() != *lastRevision) {
            return VerificationResult{.diffs = {}, .docId = docId, .status = "skipped"};
        }
    }

    model::KeyAllocator keys;
    auto actual = model::docModelParse(actualJson, model::DocModelParseOptions{.docId = docId, .keys = keys});

    auto compare = model::docModelsCompare(expected, actual, model::CompareOptions{
        .identityTransfers = identityTransfers,
        .listsRebuilt = listsRebuilt,
        .tabIdMap = tabIdMap,
    });

    if (!compare.equal) {
        return VerificationResult{
            .diffs = compare.diffs,
            .docId = docId,
            .status = "mismatch",
        };
    }

    return VerificationResult{
        .diffs = {},
        .docId = docId,
        .status = "match",
    };
}

inline VerificationResult verifyNamedRanges(
    const std::string& docId,
    const model::DocModel& expected,
    const slimr::Json& actualJson,
    const std::vector<slimr::Json>& requests,
    const std::map<std::string, std::string>& tabIds,
    const std::vector<std::string>& approvedLosses
) {
    model::KeyAllocator keys;
    const auto actual = model::docModelParse(actualJson, {.docId = docId, .keys = keys});
    VerificationResult result{.docId = docId, .status = "match"};
    bool inconclusive = false;
    if (expected.title != actual.title) result.diffs.push_back("document title differs");
    for (const auto& tab : expected.tabs) {
        const auto id = tabIds.contains(tab.tabId) ? tabIds.at(tab.tabId) : tab.tabId;
        const auto now = std::find_if(actual.tabs.begin(), actual.tabs.end(),
            [&](const auto& candidate) { return candidate.tabId == id; });
        if (now == actual.tabs.end()) {
            result.diffs.push_back("tab identity missing: " + tab.title);
            continue;
        }
        auto unexported = [](const model::TabModel& value) {
            auto raw = value.headersFooters;
            for (const auto& segment : value.segments) {
                for (const auto& field : {"headers", "footers"}) {
                    if (raw.contains(field) && raw[field].is_object()) {
                        raw[field].get<slimr::Json::object_t>().erase(segment.id);
                    }
                }
            }
            return raw;
        };
        if (!model::styleEqual(unexported(tab), unexported(*now))) {
            result.diffs.push_back("unexported headers/footers changed: " + tab.title);
        }
        std::set<std::string> references;
        auto collect = [&](auto& self, const slimr::Json& value, std::string_view field) -> void {
            if (value.is_string() && (field == "inlineObjectId" || field == "footnoteId" || field == "positionedObjectIds")) {
                references.insert(value.get<std::string>());
            } else if (value.is_array()) {
                for (const auto& item : value.get<slimr::Json::array_t>()) self(self, item, field);
            } else if (value.is_object()) {
                for (const auto& [name, item] : value.get<slimr::Json::object_t>()) self(self, item, name);
            }
        };
        std::set<std::string> editedObjects;
        for (const auto& paragraph : model::paragraphsDeep(tab.blocks)) {
            if (paragraph.positionedObjectIds) {
                references.insert(paragraph.positionedObjectIds->begin(), paragraph.positionedObjectIds->end());
            }
            for (const auto& item : paragraph.inlines) {
                if (!item.raw) continue;
                collect(collect, *item.raw, "");
                if (item.replaceUri && item.raw->contains("inlineObjectElement") &&
                    (*item.raw)["inlineObjectElement"].contains("inlineObjectId") &&
                    (*item.raw)["inlineObjectElement"]["inlineObjectId"].is_string()) {
                    editedObjects.insert((*item.raw)["inlineObjectElement"]["inlineObjectId"].get<std::string>());
                }
            }
        }
        collect(collect, tab.headersFooters, "");
        for (const auto& segment : tab.segments) editedObjects.insert(segment.id);
        auto stable = [&](auto& self, slimr::Json value) -> slimr::Json {
            if (value.is_object()) {
                auto& fields = value.get<slimr::Json::object_t>();
                fields.erase("contentUri");
                for (auto& [_, entry] : fields) entry = self(self, std::move(entry));
            } else if (value.is_array()) {
                for (auto& entry : value.get<slimr::Json::array_t>()) entry = self(self, std::move(entry));
            }
            return value;
        };
        auto objectsCheck = [&](const auto& wanted, const auto& got, std::string_view field) {
            for (const auto& [objectId, value] : wanted) {
                if (!references.contains(objectId) || editedObjects.contains(objectId)) continue;
                const auto found = got.find(objectId);
                if (found == got.end() || !model::styleEqual(stable(stable, value), stable(stable, found->second))) {
                    result.diffs.push_back(std::string(field) + " object changed: " + objectId);
                }
            }
        };
        objectsCheck(tab.inlineObjects, now->inlineObjects, "inline");
        objectsCheck(tab.positionedObjects, now->positionedObjects, "positioned");
        objectsCheck(tab.footnotes, now->footnotes, "footnote");
        for (const auto& range : tab.namedRanges) {
            if (std::find(approvedLosses.begin(), approvedLosses.end(), range.namedRangeId) != approvedLosses.end()) continue;
            if (range.create) {
                const auto edges = lens::namedRangeExpected(tab, range);
                bool found = false;
                for (const auto& candidate : now->namedRanges) {
                    if (candidate.name == range.name && candidate.ranges.size() == 1 &&
                        !candidate.ranges[0].segmentId && edges &&
                        candidate.ranges[0].start == edges->start && candidate.ranges[0].end == edges->end) {
                        found = true;
                    }
                }
                if (!found) result.diffs.push_back("created named range missing or misplaced: " + range.name);
                else inconclusive = true; // Exact edges do not prove a newly minted identity without its reply.
                continue;
            }
            const auto preserved = std::find_if(now->namedRanges.begin(), now->namedRanges.end(),
                [&](const auto& candidate) { return candidate.namedRangeId == range.namedRangeId; });
            if (preserved == now->namedRanges.end() || preserved->name != range.name) {
                result.diffs.push_back("named range identity lost: " + range.name);
                continue;
            }
            auto exact = lens::namedRangeVerify(tab, range, *now);
            if (!exact && range.ranges.size() == 1 && !range.ranges[0].segmentId) {
                const auto predicted = lens::namedRangePredict(range.ranges[0], tab.tabId, requests);
                if (predicted) {
                    exact = preserved->ranges.size() == 1 && !preserved->ranges[0].segmentId &&
                        preserved->ranges[0].start == predicted->start && preserved->ranges[0].end == predicted->end;
                }
            }
            if (!exact) inconclusive = true;
            else if (!*exact) result.diffs.push_back("named range edges differ: " + range.name);
        }
    }
    if (!result.diffs.empty()) result.status = "mismatch";
    else if (inconclusive) {
        result.status = "inconclusive";
        result.diffs.push_back("Named-range identity or offsets could not be fully verified.");
    }
    return result;
}

} // namespace gdocsmith::engine
