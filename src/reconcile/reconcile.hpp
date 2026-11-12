#pragma once

#include "emulator/emulate.hpp"
#include "emulator/types.hpp"
#include "model/api_facts.hpp"
#include "model/equivalence.hpp"
#include "model/errors.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/layout.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/symbols.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"
#include "reconcile/container.hpp"
#include "reconcile/context.hpp"
#include "reconcile/lists.hpp"
#include "reconcile/requests.hpp"
#include "reconcile/tables.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gdocsmith::reconcile {

using namespace model;

/** A tab the create phase must add before its content can be sent. */
struct TabToAdd {
    int index = 0;
    std::optional<std::string> parentTabId;
    std::string tabId;
    std::string title;
};

struct DeletedRange {
    Range range;
    std::string tabId;
};

struct ListRebuildWithTab {
    std::vector<std::string> keys;
    std::string listId;
    bool lossy = false;
    std::string tabId;
};

struct SegmentToAdd {
    std::optional<int> breakIndex;
    std::string kind; // "footer" | "header"
    std::string tabId;
};

struct FootnoteBody {
    TabModel final;
    std::string id;
    std::vector<JsonObject> requests;
    std::string tabId;
};

struct SegmentCheck {
    TabModel final;
    TabModel original;
    std::vector<JsonObject> requests;
    std::string segmentId;
    std::string tabId;
};

struct DocPlanSummary {
    int contentRequests = 0;
    int tabOps = 0;
    int tabsToAdd = 0;
};

/** Everything the flush and guard need for one document. */
struct DocPlan {
    std::vector<JsonObject> contentRequests;
    std::vector<DeletedRange> deletedRanges;
    std::string docId;
    std::vector<IdentityTransfer> identityTransfers;
    std::vector<ListRebuildWithTab> listRebuilds;
    std::vector<RequestOrigin> origins;
    std::vector<PendingLink> pendingLinks;
    std::vector<std::string> protectedTouches;
    DocPlanSummary summary;
    std::vector<JsonObject> tabOps;
    std::vector<TabToAdd> tabsToAdd;
    std::vector<SegmentToAdd> segmentsToAdd;
    std::vector<FootnoteBody> footnoteBodies;
    std::vector<SegmentCheck> segmentChecks;
};

/** Inputs to a document's reconcile. */
struct DocReconcileInput {
    DocModel final;
    DocModel original;
    JsonObject originalJson;
    std::map<std::string, std::vector<Range>> carryRanges;
};

inline void indicesShift(slimr::Json& value, int by) {
    if (value.is_array()) {
        for (auto& entry : value.get<slimr::Json::array_t>()) {
            indicesShift(entry, by);
        }
    } else if (value.is_object()) {
        auto& record = value.get<slimr::Json::object_t>();
        if (record.contains("startIndex") && record["startIndex"].is_number()) {
            record["startIndex"] = record["startIndex"].as<int>() + by;
        }
        if (record.contains("endIndex") && record["endIndex"].is_number()) {
            record["endIndex"] = record["endIndex"].as<int>() + by;
        }
        for (auto& [_, entry] : record) {
            if (entry.is_array() || entry.is_object()) {
                indicesShift(entry, by);
            }
        }
    }
}

inline slimr::Json make_footnote_blank_content() {
    static const auto content = []() -> slimr::Json {
        const char* raw = R"json([
  {
    "endIndex": 2,
    "paragraph": {
      "elements": [
        {
          "endIndex": 2,
          "startIndex": 0,
          "textRun": { "content": " \n", "textStyle": { "fontSize": { "magnitude": 10, "unit": "PT" } } }
        }
      ],
      "paragraphStyle": {
        "direction": "LEFT_TO_RIGHT",
        "lineSpacing": 100,
        "namedStyleType": "NORMAL_TEXT",
        "spaceBelow": { "unit": "PT" }
      }
    },
    "startIndex": 0
  }
])json";
        auto parsed = slimr::parse_json<slimr::Json>(raw);
        return parsed ? *parsed : slimr::Json::array_t{};
    }();
    return content;
}

inline JsonObject footnoteBlankJson(const std::string& tabId, const std::string& title, const JsonObject& namedStyles) {
    slimr::Json content = make_footnote_blank_content();
    indicesShift(content, 1);

    slimr::Json secBreak = slimr::Json::object_t{
        {"startIndex", 0},
        {"endIndex", 1},
        {"sectionBreak", slimr::Json::object_t{
            {"sectionStyle", slimr::Json::object_t{}}
        }}
    };
    slimr::Json bodyContent = slimr::Json::array_t{secBreak};
    for (const auto& item : content.get<slimr::Json::array_t>()) {
        bodyContent.get<slimr::Json::array_t>().push_back(item);
    }

    slimr::Json docJson = slimr::Json::object_t{
        {"title", ""},
        {"tabs", slimr::Json::array_t{
            slimr::Json::object_t{
                {"tabProperties", slimr::Json::object_t{
                    {"tabId", tabId},
                    {"title", title}
                }},
                {"documentTab", slimr::Json::object_t{
                    {"body", slimr::Json::object_t{
                        {"content", std::move(bodyContent)}
                    }},
                    {"namedStyles", namedStyles}
                }}
            }
        }}
    };
    return docJson;
}

inline slimr::Json segmentRequest(const slimr::Json& req, std::string_view segmentId) {
    if (req.is_array()) {
        slimr::Json arr = slimr::Json::array_t{};
        for (const auto& item : req.get<slimr::Json::array_t>()) {
            arr.get<slimr::Json::array_t>().push_back(segmentRequest(item, segmentId));
        }
        return arr;
    }
    if (!req.is_object()) return req;
    slimr::Json out = slimr::Json::object_t{};
    for (const auto& [k, v] : req.get<slimr::Json::object_t>()) {
        out[k] = segmentRequest(v, segmentId);
    }
    if (out.contains("tabId") && (out.contains("index") || out.contains("startIndex") || out.contains("endIndex"))) {
        for (const auto& field : {"index", "startIndex", "endIndex"}) {
            if (out.contains(field) && out[field].is_number()) {
                out[field] = out[field].as<int>() - 1;
            }
        }
        out["segmentId"] = std::string(segmentId);
    }
    return out;
}

inline bool segmentRequestIs(const slimr::Json& req) {
    if (req.is_object()) {
        if (req.contains("segmentId")) return true;
        for (const auto& [_, v] : req.get<slimr::Json::object_t>()) {
            if (segmentRequestIs(v)) return true;
        }
    } else if (req.is_array()) {
        for (const auto& item : req.get<slimr::Json::array_t>()) {
            if (segmentRequestIs(item)) return true;
        }
    }
    return false;
}

inline slimr::Json tabIdsRewrite(const slimr::Json& req, const std::map<std::string, std::string>& map) {
    if (map.empty()) return req;
    if (req.is_array()) {
        slimr::Json arr = slimr::Json::array_t{};
        for (const auto& item : req.get<slimr::Json::array_t>()) {
            arr.get<slimr::Json::array_t>().push_back(tabIdsRewrite(item, map));
        }
        return arr;
    }
    if (!req.is_object()) return req;
    slimr::Json out = slimr::Json::object_t{};
    for (const auto& [k, v] : req.get<slimr::Json::object_t>()) {
        if ((k == "tabId" || k == "parentTabId") && v.is_string()) {
            auto it = map.find(v.get<std::string>());
            out[k] = (it != map.end()) ? it->second : v.get<std::string>();
        } else {
            out[k] = tabIdsRewrite(v, map);
        }
    }
    return out;
}

inline slimr::Json withParent(const slimr::Json& add, const std::string& parentTabId) {
    slimr::Json copy = add;
    if (copy.contains("addDocumentTab") && copy["addDocumentTab"].is_object()) {
        auto& addDocTab = copy["addDocumentTab"].get<slimr::Json::object_t>();
        if (addDocTab.contains("tabProperties") && addDocTab["tabProperties"].is_object()) {
            addDocTab["tabProperties"].get<slimr::Json::object_t>()["parentTabId"] = parentTabId;
        }
    }
    return copy;
}

inline slimr::Json segmentJson(const slimr::Json& json, const std::string& tabId, const std::string& segmentId, const std::string& title) {
    std::function<const slimr::Json*(const slimr::Json*)> findTab = [&](const slimr::Json* tabs) -> const slimr::Json* {
        if (!tabs || !tabs->is_array()) return nullptr;
        for (const auto& t : tabs->get<slimr::Json::array_t>()) {
            if (t.contains("tabProperties") && t["tabProperties"].contains("tabId") &&
                t["tabProperties"]["tabId"].is_string() && t["tabProperties"]["tabId"].get<std::string>() == tabId) {
                return &t;
            }
            if (t.contains("childTabs")) {
                auto* child = findTab(&t["childTabs"]);
                if (child) return child;
            }
        }
        return nullptr;
    };
    const slimr::Json* tabNode = json.contains("tabs") ? findTab(&json["tabs"]) : nullptr;
    const slimr::Json& dt = (tabNode && tabNode->contains("documentTab")) ? (*tabNode)["documentTab"] : json;

    const slimr::Json* segment = nullptr;
    for (const char* poolName : {"headers", "footers", "footnotes"}) {
        if (dt.contains(poolName) && dt[poolName].is_object() && dt[poolName].contains(segmentId)) {
            segment = &dt[poolName][segmentId];
            break;
        }
    }

    slimr::Json contentArr = slimr::Json::array_t{};
    if (segment && segment->contains("content") && (*segment)["content"].is_array()) {
        contentArr = (*segment)["content"];
    }
    indicesShift(contentArr, 1);

    slimr::Json secBreak = slimr::Json::object_t{
        {"startIndex", 0},
        {"endIndex", 1},
        {"sectionBreak", slimr::Json::object_t{
            {"sectionStyle", slimr::Json::object_t{}}
        }}
    };
    slimr::Json bodyContent = slimr::Json::array_t{secBreak};
    for (const auto& item : contentArr.get<slimr::Json::array_t>()) {
        bodyContent.get<slimr::Json::array_t>().push_back(item);
    }

    slimr::Json docTab = slimr::Json::object_t{
        {"body", slimr::Json::object_t{
            {"content", bodyContent}
        }}
    };
    if (dt.contains("inlineObjects")) docTab["inlineObjects"] = dt["inlineObjects"];
    if (dt.contains("lists")) docTab["lists"] = dt["lists"];
    if (dt.contains("namedStyles")) docTab["namedStyles"] = dt["namedStyles"];

    slimr::Json res = slimr::Json::object_t{
        {"title", ""},
        {"tabs", slimr::Json::array_t{
            slimr::Json::object_t{
                {"tabProperties", slimr::Json::object_t{
                    {"tabId", tabId},
                    {"title", title}
                }},
                {"documentTab", std::move(docTab)}
            }
        }}
    };
    if (json.contains("revisionId")) res["revisionId"] = json["revisionId"];
    return res;
}

inline void stripDocLinks(std::vector<Block>& blocks) {
    for (auto& b : blocks) {
        if (b.is_paragraph()) {
            auto& p = b.as_paragraph();
            for (auto& inlineSpan : p.inlines) {
                if (inlineSpan.style.contains("link") && inlineSpan.style["link"].is_object()) {
                    const auto& link = inlineSpan.style["link"];
                    if (link.contains("heading") && link["heading"].is_object() && link["heading"].contains("key")) {
                        if (inlineSpan.style.holds<slimr::Json::object_t>()) {
                            inlineSpan.style.get<slimr::Json::object_t>().erase("link");
                        }
                    }
                }
            }
        } else if (b.is_table()) {
            auto& tbl = b.as_table();
            for (auto& row : tbl.rows) {
                for (auto& cell : row.cells) {
                    stripDocLinks(cell.blocks);
                }
            }
        }
    }
}

inline DocModel pendingLinksStrip(const DocModel& doc) {
    DocModel copy = doc;
    for (auto& tab : copy.tabs) {
        stripDocLinks(tab.blocks);
    }
    return copy;
}

inline DocModel docOf(const TabModel& tab, const std::string& tabId) {
    TabModel copy = tab;
    copy.segments.clear();
    copy.tabId = tabId;
    return DocModel{
        .docId = "segment",
        .title = "",
        .isNew = false,
        .revisionId = std::nullopt,
        .suggestionsViewMode = std::nullopt,
        .tabs = {std::move(copy)}
    };
}

inline CompareResult docPlanSelfCheck(
    const DocPlan& plan,
    const DocReconcileInput& input
) {
    std::vector<slimr::Json> adds;
    for (const auto& t : plan.tabsToAdd) {
        adds.push_back(RequestBuilder::documentTabAdd(t.title, RequestBuilder::DocumentTabAddOpts{
            .index = t.index,
            .parentTabId = t.parentTabId
        }));
    }
    slimr::Json json = input.originalJson;
    std::map<std::string, std::string> tabIdMap;
    for (size_t i = 0; i < adds.size(); ++i) {
        const auto& parent = plan.tabsToAdd[i].parentTabId;
        slimr::Json resolved = (parent && tabIdMap.contains(*parent)) ? withParent(adds[i], tabIdMap[*parent]) : adds[i];
        auto res = emulator::requestsEmulate(json, {resolved});
        std::string minted = res.replies[0]["addDocumentTab"]["tabProperties"]["tabId"].get<std::string>();
        tabIdMap[plan.tabsToAdd[i].tabId] = minted;
        json = std::move(res.json);
    }

    std::vector<slimr::Json> content;
    for (const auto& req : plan.contentRequests) {
        if (!segmentRequestIs(req)) {
            content.push_back(tabIdsRewrite(req, tabIdMap));
        }
    }
    for (const auto& req : plan.tabOps) {
        content.push_back(tabIdsRewrite(req, tabIdMap));
    }
    json = emulator::requestsEmulate(json, content).json;

    DocModel actual = docModelParse(json, DocModelParseOptions{.docId = plan.docId, .keys = KeyAllocator{}});

    std::set<std::string> listsRebuilt;
    for (const auto& r : plan.listRebuilds) listsRebuilt.insert(r.listId);

    DocModel finalStripped = pendingLinksStrip(input.final);
    auto result = docModelsCompare(finalStripped, actual, CompareOptions{
        .identityTransfers = plan.identityTransfers,
        .listsRebuilt = listsRebuilt,
        .tabIdMap = tabIdMap,
        .skipSegments = true
    });

    for (const auto& check : plan.segmentChecks) {
        auto emulated = emulator::requestsEmulate(
            segmentJson(input.originalJson, check.tabId, check.segmentId, check.final.title),
            check.requests
        ).json;
        DocModel after = docModelParse(emulated, DocModelParseOptions{.docId = plan.docId, .keys = KeyAllocator{}});
        auto compared = docModelsCompare(docOf(check.final, check.tabId), after, CompareOptions{.skipSegments = true});
        for (const auto& diff : compared.diffs) {
            result.diffs.push_back(std::format("header/footer {}: {}", check.segmentId, diff));
        }
    }

    for (const auto& body : plan.footnoteBodies) {
        auto emulated = emulator::requestsEmulate(
            footnoteBlankJson(body.tabId, body.final.title, body.final.namedStyles),
            body.requests
        ).json;
        DocModel after = docModelParse(emulated, DocModelParseOptions{.docId = plan.docId, .keys = KeyAllocator{}});
        auto compared = docModelsCompare(docOf(body.final, body.tabId), after, CompareOptions{.skipSegments = true});
        for (const auto& diff : compared.diffs) {
            result.diffs.push_back(std::format("footnote {}: {}", body.id, diff));
        }
    }

    return CompareResult{.diffs = result.diffs, .equal = result.diffs.empty()};
}

inline void segmentsReconcile(const TabModel& o, const TabModel& f, DocPlan& plan) {
    for (const auto& segment : f.segments) {
        if (segment.kind == "footnote" && segment.id.starts_with("new:")) {
            ReconcileContext ctx = reconcileContextCreate(f.tabId);
            if (segment.original && segment.tab) {
                const TabModel& blank = *segment.original;
                inCellRun(ctx, [&]() {
                    containerReconcile(blank.blocks, segment.tab->blocks, ctx);
                });
                plan.footnoteBodies.push_back(FootnoteBody{
                    .final = *segment.tab,
                    .id = segment.id,
                    .requests = std::move(ctx.requests),
                    .tabId = f.tabId
                });
            }
            continue;
        }
        if (segment.id.starts_with("new:") && segment.kind != "footnote") {
            std::optional<int> breakIndex;
            if (segment.section) {
                int secCount = 0;
                for (const auto& block : f.blocks) {
                    if (block.is_section_break()) {
                        secCount++;
                        if (secCount == *segment.section - 1) { // segment.section - 2 (0-indexed break)
                            if (block.origin) {
                                breakIndex = block.origin->start;
                            }
                            break;
                        }
                    }
                }
            }
            if (segment.section && !breakIndex.has_value()) {
                throw CoreError(
                    "unsupportedSyntax",
                    std::format("section {} is new, so its {} can be added only after this import creates it; import, then add the {}",
                        *segment.section, segment.kind, segment.kind)
                );
            }
            plan.segmentsToAdd.push_back(SegmentToAdd{
                .breakIndex = breakIndex,
                .kind = segment.kind,
                .tabId = f.tabId
            });
            continue;
        }
        auto wasIt = std::find_if(o.segments.begin(), o.segments.end(), [&](const auto& s) { return s.id == segment.id; });
        if (wasIt == o.segments.end()) continue;
        const auto& was = *wasIt;
        if (!was.tab || !segment.tab) continue;
        ReconcileContext ctx = reconcileContextCreate(f.tabId);
        inCellRun(ctx, [&]() {
            containerReconcile(was.tab->blocks, segment.tab->blocks, ctx);
        });
        if (ctx.requests.empty()) continue;
        plan.segmentChecks.push_back(SegmentCheck{
            .final = *segment.tab,
            .original = *was.tab,
            .requests = ctx.requests,
            .segmentId = segment.id,
            .tabId = f.tabId
        });
        for (const auto& req : ctx.requests) {
            plan.contentRequests.push_back(segmentRequest(req, segment.id));
        }
        for (const auto& orig : ctx.origins) {
            plan.origins.push_back(orig);
        }
    }
    for (const auto& segment : o.segments) {
        if (segment.kind == "footnote") continue;
        bool inFinal = std::any_of(f.segments.begin(), f.segments.end(), [&](const auto& s) { return s.id == segment.id; });
        if (inFinal) continue;
        plan.contentRequests.push_back(RequestBuilder::segmentDelete(segment.kind, segment.id, f.tabId));
        plan.origins.push_back(RequestOrigin{});
    }
}

inline std::vector<std::string> changedFields(const JsonObject& a, const JsonObject& b) {
    std::set<std::string> keys;
    if (a.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : a.get<slimr::Json::object_t>()) keys.insert(k);
    }
    if (b.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : b.get<slimr::Json::object_t>()) keys.insert(k);
    }
    std::vector<std::string> diffs;
    for (const auto& k : keys) {
        JsonObject aVal = a.contains(k) ? a[k] : make_json_object();
        JsonObject bVal = b.contains(k) ? b[k] : make_json_object();
        if (!styleEqual(aVal, bVal)) {
            diffs.push_back(k);
        }
    }
    std::sort(diffs.begin(), diffs.end());
    return diffs;
}

struct NamedStyleEntry {
    std::string namedStyleType;
    JsonObject paragraphStyle;
    JsonObject textStyle;
};

inline std::vector<NamedStyleEntry> namedStyleEntries(const JsonObject& namedStyles) {
    std::vector<NamedStyleEntry> res;
    if (namedStyles.contains("styles") && namedStyles["styles"].holds<slimr::Json::array_t>()) {
        for (const auto& item : namedStyles["styles"].get<slimr::Json::array_t>()) {
            NamedStyleEntry entry;
            if (item.contains("namedStyleType") && item["namedStyleType"].holds<std::string>()) {
                entry.namedStyleType = item["namedStyleType"].get<std::string>();
            }
            if (item.contains("paragraphStyle") && item["paragraphStyle"].holds<slimr::Json::object_t>()) {
                entry.paragraphStyle = item["paragraphStyle"];
            } else {
                entry.paragraphStyle = make_json_object();
            }
            if (item.contains("textStyle") && item["textStyle"].holds<slimr::Json::object_t>()) {
                entry.textStyle = item["textStyle"];
            } else {
                entry.textStyle = make_json_object();
            }
            res.push_back(std::move(entry));
        }
    }
    return res;
}

struct NamedStyleFieldsChangedResult {
    std::vector<std::string> paragraph;
    std::vector<std::string> text;
};

inline NamedStyleFieldsChangedResult namedStyleFieldsChanged(
    const NamedStyleEntry* before,
    const NamedStyleEntry& after
) {
    auto diff = [](const JsonObject& a, const JsonObject& b) {
        std::set<std::string> keys;
        if (a.holds<slimr::Json::object_t>()) {
            for (const auto& [k, _] : a.get<slimr::Json::object_t>()) keys.insert(k);
        }
        if (b.holds<slimr::Json::object_t>()) {
            for (const auto& [k, _] : b.get<slimr::Json::object_t>()) keys.insert(k);
        }
        std::vector<std::string> diffs;
        for (const auto& k : keys) {
            JsonObject aVal = a.contains(k) ? a[k] : make_json_object();
            JsonObject bVal = b.contains(k) ? b[k] : make_json_object();
            if (!styleEqual(aVal, bVal)) {
                diffs.push_back(k);
            }
        }
        std::sort(diffs.begin(), diffs.end());
        return diffs;
    };
    JsonObject beforePara = before ? before->paragraphStyle : make_json_object();
    JsonObject beforeText = before ? before->textStyle : make_json_object();
    auto paraDiff = diff(beforePara, after.paragraphStyle);
    std::vector<std::string> paraFiltered;
    for (const auto& f : paraDiff) {
        if (f != "namedStyleType") paraFiltered.push_back(f);
    }
    return NamedStyleFieldsChangedResult{
        .paragraph = std::move(paraFiltered),
        .text = diff(beforeText, after.textStyle)
    };
}

inline std::vector<JsonObject> dropdownOptions(const JsonObject& definition) {
    if (definition.contains("dropdownDefinitionProperties")) {
        const auto& props = definition["dropdownDefinitionProperties"];
        if (props.contains("options") && props["options"].holds<slimr::Json::array_t>()) {
            std::vector<JsonObject> res;
            for (const auto& item : props["options"].get<slimr::Json::array_t>()) {
                res.push_back(item);
            }
            return res;
        }
    }
    return {};
}

inline std::vector<std::string> dropdownsWiden(
    const TabModel& o,
    const TabModel& f,
    ReconcileContext& ctx
) {
    std::vector<std::string> narrowed;
    for (const auto& [id, definition] : f.dropdownDefinitions) {
        auto it = o.dropdownDefinitions.find(id);
        JsonObject properties = definition.contains("dropdownDefinitionProperties") ? definition["dropdownDefinitionProperties"] : make_json_object();
        if (it == o.dropdownDefinitions.end()) {
            JsonObject defObj = slimr::Json::object_t{
                {"dropdownDefinitionId", id},
                {"dropdownDefinitionProperties", properties}
            };
            ctx.requests.push_back(RequestBuilder::dropdownDefinitionCreate(defObj, ctx.tabId));
            ctx.origins.push_back(RequestOrigin{});
            continue;
        }
        const auto& was = it->second;
        JsonObject wasProps = was.contains("dropdownDefinitionProperties") ? was["dropdownDefinitionProperties"] : make_json_object();
        if (styleEqual(wasProps, properties)) continue;

        auto finalOptions = dropdownOptions(definition);
        auto wasOptions = dropdownOptions(was);
        std::set<std::string> kept;
        for (const auto& opt : finalOptions) {
            if (opt.contains("optionId") && opt["optionId"].holds<std::string>()) {
                kept.insert(opt["optionId"].get<std::string>());
            }
        }
        std::vector<JsonObject> dropped;
        for (const auto& opt : wasOptions) {
            if (opt.contains("optionId") && opt["optionId"].holds<std::string>()) {
                if (!kept.contains(opt["optionId"].get<std::string>())) {
                    dropped.push_back(opt);
                }
            }
        }
        if (!dropped.empty()) {
            narrowed.push_back(id);
        }
        auto combinedOptions = finalOptions;
        for (const auto& d : dropped) combinedOptions.push_back(d);

        JsonObject mergedProps = properties;
        slimr::Json::array_t optArr;
        for (const auto& opt : combinedOptions) optArr.push_back(opt);
        mergedProps["options"] = std::move(optArr);

        ctx.requests.push_back(RequestBuilder::dropdownDefinitionUpdate(id, mergedProps, ctx.tabId));
        ctx.origins.push_back(RequestOrigin{});
    }
    return narrowed;
}

inline void dropdownsNarrow(
    const TabModel& o,
    const TabModel& f,
    const std::vector<std::string>& narrowed,
    ReconcileContext& ctx
) {
    auto selected = [](const TabModel& tab) {
        std::map<std::string, JsonObject> m;
        for (const auto& atom : atomsDeep(tab.blocks)) {
            if (atom.type == "dropdown" && atom.raw && atom.raw->contains("dropdown")) {
                m[atom.key] = (*atom.raw)["dropdown"];
            }
        }
        return m;
    };
    auto before = selected(o);
    for (const auto& [key, dropdown] : selected(f)) {
        auto it = before.find(key);
        if (it == before.end()) continue;
        const auto& was = it->second;
        std::string option = dropdown.contains("dropdownProperties") && dropdown["dropdownProperties"].contains("selectedOptionId") && dropdown["dropdownProperties"]["selectedOptionId"].holds<std::string>()
            ? dropdown["dropdownProperties"]["selectedOptionId"].get<std::string>() : "";
        std::string wasOption = was.contains("dropdownProperties") && was["dropdownProperties"].contains("selectedOptionId") && was["dropdownProperties"]["selectedOptionId"].holds<std::string>()
            ? was["dropdownProperties"]["selectedOptionId"].get<std::string>() : "";
        if (wasOption == option) continue;

        std::string dropdownId = dropdown.contains("dropdownId") && dropdown["dropdownId"].holds<std::string>()
            ? dropdown["dropdownId"].get<std::string>() : "";
        ctx.requests.push_back(RequestBuilder::dropdownSelect(dropdownId, option, ctx.tabId));
        ctx.origins.push_back(RequestOrigin{.key = key});
    }
    for (const auto& id : narrowed) {
        auto it = f.dropdownDefinitions.find(id);
        if (it != f.dropdownDefinitions.end()) {
            JsonObject props = it->second.contains("dropdownDefinitionProperties") ? it->second["dropdownDefinitionProperties"] : make_json_object();
            ctx.requests.push_back(RequestBuilder::dropdownDefinitionUpdate(id, props, ctx.tabId));
            ctx.origins.push_back(RequestOrigin{});
        }
    }
    for (const auto& [id, _] : o.dropdownDefinitions) {
        if (!f.dropdownDefinitions.contains(id)) {
            ctx.requests.push_back(RequestBuilder::dropdownDefinitionDelete(id, ctx.tabId));
            ctx.origins.push_back(RequestOrigin{});
        }
    }
}

inline void imagesReplace(const TabModel& f, ReconcileContext& ctx) {
    for (const auto& atom : atomsDeep(f.blocks)) {
        if (atom.replaceUri) {
            std::string id;
            if (atom.raw && atom.raw->contains("inlineObjectElement")) {
                const auto& ioe = (*atom.raw)["inlineObjectElement"];
                if (ioe.contains("inlineObjectId") && ioe["inlineObjectId"].is_string()) {
                    id = ioe["inlineObjectId"].get<std::string>();
                }
            }
            ctx.requests.push_back(RequestBuilder::imageReplace(id, *atom.replaceUri, ctx.tabId));
            ctx.origins.push_back(RequestOrigin{.key = atom.key});
        }
    }
}

inline void positionedDeletes(const TabModel& o, const TabModel& f, ReconcileContext& ctx) {
    for (const auto& [id, _] : o.positionedObjects) {
        if (!f.positionedObjects.contains(id)) {
            ctx.requests.push_back(RequestBuilder::positionedObjectDelete(id, ctx.tabId));
            ctx.origins.push_back(RequestOrigin{});
        }
    }
}

inline const Block* blockFindConst(const TabModel& tab, std::string_view key) {
    for (const auto& b : tab.blocks) {
        if (b.key == key) return &b;
    }
    for (const auto& table : tablesDeep(tab.blocks)) {
        for (const auto& row : table.rows) {
            for (const auto& cell : row.cells) {
                for (const auto& b : cell.blocks) {
                    if (b.key == key) return &b;
                }
            }
        }
    }
    return nullptr;
}

inline int rangeEdgeIndex(const TabModel& tab, const Layout& layout, std::string_view key, int offset) {
    const Block* found = blockFindConst(tab, key);
    auto it = layout.blockRanges.find(std::string(key));
    if (!found || !found->is_paragraph() || it == layout.blockRanges.end()) {
        throw CoreError("internal", std::format("named range edge in a missing paragraph {}", key));
    }
    int start = it->second.start;
    auto syms = paragraphSymbols(found->as_paragraph());
    std::vector<Sym> visible;
    for (const auto& s : syms) {
        if (!symHidden(s)) visible.push_back(s);
    }
    int sum = 0;
    int limit = std::min(offset, static_cast<int>(visible.size()));
    for (int i = 0; i < limit; ++i) {
        if (visible[i].is_char()) {
            sum += codePointUtf16Units(visible[i].ch);
        } else if (visible[i].is_atom()) {
            sum += visible[i].atom->length;
        }
    }
    return start + sum;
}

inline void namedRangesReconcile(const TabModel& o, const TabModel& f, ReconcileContext& ctx) {
    std::set<std::string> kept;
    for (const auto& range : f.namedRanges) kept.insert(range.namedRangeId);
    for (const auto& range : o.namedRanges) {
        if (!kept.contains(range.namedRangeId)) {
            ctx.requests.push_back(RequestBuilder::namedRangeDelete(range.namedRangeId, ctx.tabId));
            ctx.origins.push_back(RequestOrigin{});
        }
    }
    bool hasCreates = std::any_of(f.namedRanges.begin(), f.namedRanges.end(), [](const auto& r) { return r.create.has_value(); });
    if (!hasCreates) return;

    Layout layout = layoutCompute(f);
    for (const auto& range : f.namedRanges) {
        if (!range.create) continue;
        const auto& create = *range.create;
        int startIndex = rangeEdgeIndex(f, layout, create.start.key, create.start.offset);
        int endIndex = rangeEdgeIndex(f, layout, create.end.key, create.end.offset);
        ctx.requests.push_back(
            RequestBuilder::namedRangeCreate(range.name, startIndex, endIndex, ctx.tabId)
        );
        ctx.origins.push_back(RequestOrigin{});
    }
}

inline void tabReconcile(const TabModel& o, const TabModel& f, ReconcileContext& ctx) {
    auto before = namedStyleEntries(o.namedStyles);
    for (const auto& entry : namedStyleEntries(f.namedStyles)) {
        const NamedStyleEntry* was = nullptr;
        for (const auto& e : before) {
            if (e.namedStyleType == entry.namedStyleType) {
                was = &e;
                break;
            }
        }
        auto [paragraphDiff, textDiff] = namedStyleFieldsChanged(was, entry);
        if (paragraphDiff.empty() && textDiff.empty()) continue;

        std::vector<std::string> fields;
        for (const auto& field : textDiff) fields.push_back(std::format("textStyle.{}", field));
        for (const auto& field : paragraphDiff) fields.push_back(std::format("paragraphStyle.{}", field));

        JsonObject namedStyle = slimr::Json::object_t{
            {"namedStyleType", entry.namedStyleType},
            {"paragraphStyle", pick(entry.paragraphStyle, paragraphDiff)},
            {"textStyle", pick(entry.textStyle, textDiff)}
        };

        ctx.requests.push_back(RequestBuilder::updateNamedStyle(RequestBuilder::UpdateNamedStyleOpts{
            .fields = fields,
            .namedStyle = namedStyle,
            .tabId = ctx.tabId
        }));
        ctx.origins.push_back(RequestOrigin{});
    }

    auto narrowed = dropdownsWiden(o, f, ctx);
    containerReconcile(o.blocks, f.blocks, ctx);
    bulletsReconcile(f, ctx);
    dropdownsNarrow(o, f, narrowed, ctx);
    imagesReplace(f, ctx);
    namedRangesReconcile(o, f, ctx);
    positionedDeletes(o, f, ctx);

    auto sectionFields = changedFields(o.leadingSectionStyle, f.leadingSectionStyle);
    if (!sectionFields.empty()) {
        ctx.requests.push_back(RequestBuilder::sectionStyleUpdate(
            0,
            1,
            pick(f.leadingSectionStyle, sectionFields),
            sectionFields,
            ctx.tabId
        ));
        ctx.origins.push_back(RequestOrigin{});
    }

    JsonObject documentBase = o.documentStyle;
    if (!sectionFields.empty() && f.documentStyle.contains("documentFormat")) {
        JsonObject docFormat = o.documentStyle.contains("documentFormat") ? o.documentStyle["documentFormat"] : make_json_object();
        docFormat["documentMode"] = "PAGES";
        documentBase["documentFormat"] = std::move(docFormat);
    }

    auto docFieldsAll = changedFields(documentBase, f.documentStyle);
    std::vector<std::string> documentFields;
    for (const auto& field : docFieldsAll) {
        if (!field.ends_with("HeaderId") && !field.ends_with("FooterId")) {
            documentFields.push_back(field);
        }
    }
    if (!documentFields.empty()) {
        std::string fieldsJoined;
        for (size_t i = 0; i < documentFields.size(); ++i) {
            if (i > 0) fieldsJoined += ",";
            fieldsJoined += documentFields[i];
        }
        ctx.requests.push_back(RequestBuilder::updateDocumentStyle(RequestBuilder::UpdateDocumentStyleOpts{
            .documentStyle = pick(f.documentStyle, documentFields),
            .fields = fieldsJoined,
            .tabId = ctx.tabId
        }));
        ctx.origins.push_back(RequestOrigin{});
    }
}

inline std::string titleKey(std::string_view title) {
    auto start = title.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return "";
    auto end = title.find_last_not_of(" \t\n\r");
    std::string s(title.substr(start, end - start + 1));
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<JsonObject> tabOpsPlan(
    const DocModel& original,
    const DocModel& final,
    std::vector<TabToAdd>& adds
) {
    std::vector<JsonObject> ops;
    std::set<std::string> finalIds;
    for (const auto& t : final.tabs) finalIds.insert(t.tabId);

    std::set<std::string> used;
    for (const auto& t : original.tabs) used.insert(titleKey(t.title));
    for (const auto& t : final.tabs) used.insert(titleKey(t.title));

    auto temporary = [&](const std::string& title) -> std::string {
        std::string candidate = std::format("{} (renaming)", title);
        for (int n = 2; used.contains(titleKey(candidate)); ++n) {
            candidate = std::format("{} (renaming {})", title, n);
        }
        used.insert(titleKey(candidate));
        return candidate;
    };

    std::set<std::string> held;
    for (const auto& t : original.tabs) held.insert(titleKey(t.title));
    for (auto& add : adds) {
        if (held.contains(titleKey(add.title))) {
            add.title = temporary(add.title);
        }
    }

    std::map<std::string, std::optional<std::string>> parentOf;
    std::map<std::string, std::string> titleOf;
    std::map<std::string, std::optional<std::string>> emojiOf;
    std::map<std::optional<std::string>, std::vector<std::string>> children;

    auto siblingsOf = [&](const std::optional<std::string>& parent) -> std::vector<std::string>& {
        return children[parent];
    };

    for (const auto& tab : original.tabs) {
        siblingsOf(tab.parentTabId).push_back(tab.tabId);
        parentOf[tab.tabId] = tab.parentTabId;
        titleOf[tab.tabId] = tab.title;
        emojiOf[tab.tabId] = tab.iconEmoji;
    }

    for (const auto& add : adds) {
        auto& siblings = siblingsOf(add.parentTabId);
        int idx = std::min(add.index, static_cast<int>(siblings.size()));
        siblings.insert(siblings.begin() + idx, add.tabId);
        parentOf[add.tabId] = add.parentTabId;
        titleOf[add.tabId] = add.title;
    }

    auto update = [&](
        const std::string& tabId,
        const RequestBuilder::DocumentTabPropertiesUpdateOpts& properties,
        const std::vector<std::string>& fields
    ) {
        ops.push_back(RequestBuilder::documentTabPropertiesUpdate(tabId, properties, fields));
    };

    auto within = [&](const std::string& tabId, const std::string& ancestor) -> bool {
        std::optional<std::string> at = tabId;
        while (at.has_value()) {
            if (*at == ancestor) return true;
            auto it = parentOf.find(*at);
            at = (it != parentOf.end()) ? it->second : std::nullopt;
        }
        return false;
    };

    for (const auto& tab : final.tabs) {
        auto it = parentOf.find(tab.tabId);
        if (it == parentOf.end() || !it->second.has_value() || finalIds.contains(*it->second)) continue;
        auto parent = it->second;
        std::optional<std::string> target = (tab.parentTabId && !within(*tab.parentTabId, tab.tabId)) ? tab.parentTabId : std::nullopt;

        update(tab.tabId, RequestBuilder::DocumentTabPropertiesUpdateOpts{.parentTabId = target}, {"parentTabId"});
        auto& from = siblingsOf(parent);
        auto pos = std::find(from.begin(), from.end(), tab.tabId);
        if (pos != from.end()) from.erase(pos);
        siblingsOf(target).push_back(tab.tabId);
        parentOf[tab.tabId] = target;
    }

    std::optional<JsonObject> originalTabDelete;
    const std::string ORIGINAL_TAB_ID = "t.0";

    for (const auto& tab : original.tabs) {
        auto it = parentOf.find(tab.tabId);
        std::optional<std::string> parent = (it != parentOf.end()) ? it->second : std::nullopt;
        if (finalIds.contains(tab.tabId) || (parent.has_value() && !finalIds.contains(*parent))) continue;
        auto& siblings = siblingsOf(parent);
        if (tab.tabId == ORIGINAL_TAB_ID) {
            originalTabDelete = RequestBuilder::deleteTab(tab.tabId);
            auto pos = std::find(siblings.begin(), siblings.end(), tab.tabId);
            if (pos != siblings.end()) {
                int curIdx = static_cast<int>(std::distance(siblings.begin(), pos));
                int lastIdx = static_cast<int>(siblings.size()) - 1;
                if (curIdx != lastIdx) {
                    update(tab.tabId, RequestBuilder::DocumentTabPropertiesUpdateOpts{.index = lastIdx}, {"index"});
                    siblings.erase(pos);
                    siblings.push_back(tab.tabId);
                }
            }
            continue;
        }
        ops.push_back(RequestBuilder::deleteTab(tab.tabId));
        auto pos = std::find(siblings.begin(), siblings.end(), tab.tabId);
        if (pos != siblings.end()) siblings.erase(pos);
    }

    std::map<std::string, int> takers;
    for (size_t i = 0; i < final.tabs.size(); ++i) {
        const auto& t = final.tabs[i];
        auto it = titleOf.find(t.tabId);
        if (it != titleOf.end() && it->second != t.title) {
            takers[titleKey(t.title)] = static_cast<int>(i);
        }
    }

    for (size_t index = 0; index < final.tabs.size(); ++index) {
        const auto& tab = final.tabs[index];
        auto it = titleOf.find(tab.tabId);
        if (it == titleOf.end()) continue;
        auto takerIt = takers.find(titleKey(it->second));
        if (takerIt == takers.end() || takerIt->second >= static_cast<int>(index)) continue;
        std::string title = temporary(tab.title);
        update(tab.tabId, RequestBuilder::DocumentTabPropertiesUpdateOpts{.title = title}, {"title"});
        titleOf[tab.tabId] = title;
    }

    auto order = [&](const std::optional<std::string>& parent) -> std::vector<std::string> {
        std::vector<std::string> ord;
        for (const auto& t : final.tabs) {
            if (t.parentTabId == parent) ord.push_back(t.tabId);
        }
        return ord;
    };

    for (const auto& tab : final.tabs) {
        std::vector<std::string> fields;
        RequestBuilder::DocumentTabPropertiesUpdateOpts properties;

        auto emojiIt = emojiOf.find(tab.tabId);
        std::optional<std::string> emojiWas = (emojiIt != emojiOf.end()) ? emojiIt->second : std::nullopt;
        if (emojiWas != tab.iconEmoji) {
            fields.push_back("iconEmoji");
            properties.iconEmoji = tab.iconEmoji;
        }

        if (titleOf[tab.tabId] != tab.title) {
            fields.push_back("title");
            properties.title = tab.title;
        }

        auto before = parentOf[tab.tabId];
        auto& from = siblingsOf(before);
        auto pos = std::find(from.begin(), from.end(), tab.tabId);
        int have = (pos != from.end()) ? static_cast<int>(std::distance(from.begin(), pos)) : -1;
        if (pos != from.end()) from.erase(pos);

        auto& to = siblingsOf(tab.parentTabId);
        auto ord = order(tab.parentTabId);
        auto ordPos = std::find(ord.begin(), ord.end(), tab.tabId);
        int want = (ordPos != ord.end()) ? static_cast<int>(std::distance(ord.begin(), ordPos)) : static_cast<int>(to.size());

        if (before != tab.parentTabId) {
            fields.push_back("parentTabId");
            properties.parentTabId = tab.parentTabId;
        }

        if (before != tab.parentTabId ? (want != static_cast<int>(to.size())) : (have != want)) {
            fields.push_back("index");
            properties.index = want;
        }

        to.insert(to.begin() + want, tab.tabId);
        if (!fields.empty()) {
            update(tab.tabId, properties, fields);
        }
    }

    if (originalTabDelete) {
        ops.push_back(std::move(*originalTabDelete));
    }

    return ops;
}

inline TabModel blankTabModel(const std::string& tabId, const std::optional<TabModel>& first) {
    slimr::Json raw = emulator::blank_tab_template();
    std::optional<slimr::Json> firstDocStyle;
    if (first) {
        firstDocStyle = first->documentStyle;
    }
    raw["documentStyle"] = emulator::tab_document_style_new(firstDocStyle);
    if (first) {
        raw["namedStyles"] = first->namedStyles;
    }
    slimr::Json docJson = slimr::Json::object_t{
        {"title", ""},
        {"tabs", slimr::Json::array_t{
            slimr::Json::object_t{
                {"tabProperties", slimr::Json::object_t{
                    {"tabId", tabId},
                    {"title", ""}
                }},
                {"documentTab", std::move(raw)}
            }
        }}
    };
    auto parsed = docModelParse(docJson, DocModelParseOptions{.docId = "blank", .keys = KeyAllocator{}});
    TabModel tab = std::move(parsed.tabs[0]);
    tab.isNew = true;
    return tab;
}

inline int siblingIndex(const DocModel& doc, const TabModel& tab) {
    int count = 0;
    for (const auto& t : doc.tabs) {
        if (t.parentTabId == tab.parentTabId) {
            if (t.tabId == tab.tabId) return count;
            count++;
        }
    }
    return count;
}

inline DocPlan docReconcile(const DocReconcileInput& input) {
    const auto& [final, original, originalJson, carryRanges] = input;
    std::map<std::string, TabModel> originalTabs;
    for (const auto& t : original.tabs) {
        originalTabs.emplace(t.tabId, t);
    }

    DocPlan plan;
    plan.docId = final.docId;

    const auto blankOriginal = [&](const TabModel& tab) {
        return blankTabModel(tab.tabId, original.tabs.empty() ? std::nullopt : std::optional<TabModel>(original.tabs[0]));
    };

    for (const auto& tab : final.tabs) {
        auto beforeIt = originalTabs.find(tab.tabId);
        const TabModel* before = (beforeIt != originalTabs.end()) ? &beforeIt->second : nullptr;
        if (!before) {
            plan.tabsToAdd.push_back(TabToAdd{
                .index = siblingIndex(final, tab),
                .parentTabId = tab.parentTabId,
                .tabId = tab.tabId,
                .title = tab.title
            });
        }
        auto carryIt = carryRanges.find(tab.tabId);
        static const std::vector<Range> emptyRanges;
        const std::vector<Range>& carries = (carryIt != carryRanges.end()) ? carryIt->second : emptyRanges;
        ReconcileContext ctx = reconcileContextCreate(tab.tabId, carries);

        TabModel beforeModel = before ? *before : blankOriginal(tab);
        tabReconcile(beforeModel, tab, ctx);

        for (auto& req : ctx.requests) plan.contentRequests.push_back(std::move(req));
        for (auto& orig : ctx.origins) plan.origins.push_back(std::move(orig));
        for (const auto& range : ctx.deletedRanges) {
            plan.deletedRanges.push_back(DeletedRange{.range = range, .tabId = tab.tabId});
        }
        for (auto& transfer : ctx.identityTransfers) plan.identityTransfers.push_back(std::move(transfer));
        for (auto& rebuild : ctx.listRebuilds) {
            plan.listRebuilds.push_back(ListRebuildWithTab{
                .keys = rebuild.keys,
                .listId = rebuild.listId,
                .lossy = rebuild.lossy,
                .tabId = tab.tabId
            });
        }
        for (auto& link : ctx.pendingLinks) plan.pendingLinks.push_back(std::move(link));
        for (auto& touch : ctx.protectedTouches) plan.protectedTouches.push_back(std::move(touch));

        if (before) {
            segmentsReconcile(*before, tab, plan);
        }
    }

    plan.tabOps = tabOpsPlan(original, final, plan.tabsToAdd);

    std::set<std::string> finalIds;
    for (const auto& t : final.tabs) finalIds.insert(t.tabId);
    for (const auto& tab : original.tabs) {
        if (finalIds.contains(tab.tabId)) continue;
        std::vector<Range> origins;
        for (const auto& b : tab.blocks) {
            if (b.origin) origins.push_back(*b.origin);
        }
        if (origins.empty()) continue;
        int minStart = origins[0].start;
        int maxEnd = origins[0].end;
        for (const auto& r : origins) {
            minStart = std::min(minStart, r.start);
            maxEnd = std::max(maxEnd, r.end);
        }
        plan.deletedRanges.push_back(DeletedRange{.range = Range{.end = maxEnd, .start = minStart}, .tabId = tab.tabId});
    }

    plan.summary = DocPlanSummary{
        .contentRequests = static_cast<int>(plan.contentRequests.size()),
        .tabOps = static_cast<int>(plan.tabOps.size()),
        .tabsToAdd = static_cast<int>(plan.tabsToAdd.size())
    };

    return plan;
}

} // namespace gdocsmith::reconcile
