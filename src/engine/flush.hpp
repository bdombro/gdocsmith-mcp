#pragma once

#include "engine/guard.hpp"
#include "model/equivalence.hpp"
#include "model/errors.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/layout.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"

#include <algorithm>
#include <map>
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
    } partial;
    std::vector<PhaseReport> phases;
    std::optional<int> tabIndex;
    int landedRequestCount = 0;

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
        return std::stoi(match[1].str());
    }
    return std::nullopt;
}

struct ContentSendResult {
    int requestCount = 0;
    std::optional<std::string> revisionAfter;
};

template <typename ClientType>
inline ContentSendResult contentSend(
    ClientType* client,
    const std::string& docId,
    const std::vector<slimr::Json>& requests,
    const std::vector<RequestOrigin>& origins,
    const std::optional<std::string>& initialRevision = std::nullopt
) {
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
            }

            auto parsed = slimr::parse_json<slimr::Json>(respStr);
            if (parsed && parsed->contains("writeControl") && (*parsed)["writeControl"].is_object()) {
                const auto& wc = (*parsed)["writeControl"];
                if (wc.contains("requiredRevisionId") && wc["requiredRevisionId"].is_string()) {
                    revision = wc["requiredRevisionId"].template get<std::string>();
                }
            }
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
            throw TransactionError("content", msg, {}, tabIndex, static_cast<int>(offset));
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
    if (requests.empty()) return revision;
    std::string respStr;
    if constexpr (requires { client->batchUpdate(docId, requests, revision); }) {
        auto r = client->batchUpdate(docId, requests, revision);
        respStr = r.is_string() ? r.template get<std::string>() : slimr::write_json(r);
    } else if constexpr (requires { client->batch_update(docId, requests, revision); }) {
        respStr = client->batch_update(docId, requests, revision);
    }

    auto parsed = slimr::parse_json<slimr::Json>(respStr);
    if (parsed && parsed->contains("writeControl") && (*parsed)["writeControl"].is_object()) {
        const auto& wc = (*parsed)["writeControl"];
        if (wc.contains("requiredRevisionId") && wc["requiredRevisionId"].is_string()) {
            return wc["requiredRevisionId"].template get<std::string>();
        }
    }
    return revision;
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
    const std::map<std::string, std::string>& tabIdMap = {}
) {
    if (lastRevision.has_value() && actualJson.contains("revisionId") && actualJson["revisionId"].is_string()) {
        if (actualJson["revisionId"].get<std::string>() != *lastRevision) {
            return VerificationResult{.diffs = {}, .docId = docId, .status = "skipped"};
        }
    }

    model::KeyAllocator keys;
    auto actual = model::docModelParse(actualJson, model::DocModelParseOptions{.docId = docId, .keys = keys});

    auto compare = model::docModelsCompare(expected, actual, model::CompareOptions{
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

} // namespace gdocsmith::engine
