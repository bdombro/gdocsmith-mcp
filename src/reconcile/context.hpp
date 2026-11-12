#pragma once

#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gdocsmith::reconcile {

using JsonObject = model::JsonObject;
using Range = model::Range;
using BulletRef = model::BulletRef;
using IdentityTransfer = model::IdentityTransfer;

struct RequestOrigin {
    std::optional<std::string> key;
    std::optional<int> tabIndex;

    bool operator==(const RequestOrigin&) const = default;
};

struct PendingLink {
    std::optional<std::string> docId;
    std::string headingKey;
    int length = 0;
    int offset = 0;
    std::string paragraphKey;
    std::string tabId;

    bool operator==(const PendingLink&) const = default;
};

struct ListRebuild {
    std::vector<std::string> keys;
    std::string listId;
    bool lossy = false;

    bool operator==(const ListRebuild&) const = default;
};

struct ReconcileContext {
    std::map<std::string, std::optional<BulletRef>> bulletsNow;
    std::vector<Range> carryRanges;
    std::vector<Range> deletedRanges;
    std::vector<IdentityTransfer> identityTransfers;
    int inCell = 0;
    std::vector<RequestOrigin> origins;
    std::vector<ListRebuild> listRebuilds;
    std::vector<PendingLink> pendingLinks;
    std::vector<std::string> protectedTouches;
    std::vector<JsonObject> requests;
    std::string tabId;
};

inline ReconcileContext reconcileContextCreate(
    std::string_view tabId,
    const std::vector<Range>& carryRanges = {}
) {
    ReconcileContext ctx;
    ctx.tabId = std::string(tabId);
    ctx.carryRanges = carryRanges;
    ctx.inCell = 0;
    return ctx;
}

inline void requestPush(
    ReconcileContext& ctx,
    JsonObject req,
    RequestOrigin origin = {}
) {
    ctx.requests.push_back(std::move(req));
    ctx.origins.push_back(std::move(origin));
}

inline JsonObject pick(const JsonObject& style, const std::vector<std::string>& fields) {
    JsonObject out = slimr::Json::object_t{};
    if (!style.holds<slimr::Json::object_t>()) return out;
    const auto& src = style.get<slimr::Json::object_t>();
    auto& dst = out.get<slimr::Json::object_t>();
    for (const auto& f : fields) {
        auto it = src.find(f);
        if (it != src.end()) {
            dst[f] = it->second;
        }
    }
    return out;
}

inline bool bulletSame(const std::optional<BulletRef>& a, const std::optional<BulletRef>& b) {
    if (!a.has_value() && !b.has_value()) return true;
    if (!a.has_value() || !b.has_value()) return false;
    return a->listId == b->listId && a->nestingLevel == b->nestingLevel;
}

template <typename Fn>
auto inCellRun(ReconcileContext& ctx, Fn&& fn) -> decltype(fn()) {
    ctx.inCell++;
    struct Guard {
        ReconcileContext& c;
        ~Guard() { c.inCell--; }
    } guard{ctx};
    return fn();
}

} // namespace gdocsmith::reconcile
