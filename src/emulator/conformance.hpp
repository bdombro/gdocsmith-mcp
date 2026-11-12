#pragma once

#include "types.hpp"
#include "document_emulator.hpp"
#include "model/from_json.hpp"
#include "model/equivalence.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::emulator {

struct ConformanceFixture {
    std::string id;
    std::string name;
    slimr::Json before;
    std::optional<slimr::Json> after;
    std::vector<slimr::Json> requests;
    std::optional<std::vector<slimr::Json>> replies;
    std::optional<std::string> error;
};

inline const std::set<std::string> ID_FIELDS = {
    "dateId", "headingId", "inlineObjectId", "listId", "objectId",
    "parentTabId", "personId", "richLinkId", "tabId"
};

inline const std::set<std::string> ID_KEYED_FIELDS = {
    "inlineObjects", "lists", "positionedObjects"
};

inline const std::vector<std::string> RICH_LINK_DERIVED_FIELDS = {
    "mimeType", "title"
};

inline void tabs_walk(slimr::Json& doc, const std::function<void(slimr::Json&)>& visit) {
    if (!doc.is_object() || !doc.contains("tabs") || !doc["tabs"].is_array()) return;
    auto walk = [&](auto& self, slimr::Json& tabs_arr) -> void {
        if (!tabs_arr.is_array()) return;
        for (auto& tab : tabs_arr.get<slimr::Json::array_t>()) {
            if (tab.is_object() && tab.contains("documentTab")) {
                visit(tab["documentTab"]);
            }
            if (tab.is_object() && tab.contains("childTabs")) {
                self(self, tab["childTabs"]);
            }
        }
    };
    walk(walk, doc["tabs"]);
}

inline ConformanceFixture fixture_expand(const slimr::Json& json) {
    ConformanceFixture fx;
    if (json.contains("id") && json["id"].is_string()) fx.id = json["id"].get<std::string>();
    if (json.contains("name") && json["name"].is_string()) fx.name = json["name"].get<std::string>();
    if (json.contains("error") && json["error"].is_string()) fx.error = json["error"].get<std::string>();

    if (json.contains("requests") && json["requests"].is_array()) {
        for (const auto& r : json["requests"].get<slimr::Json::array_t>()) {
            fx.requests.push_back(r);
        }
    }
    if (json.contains("replies") && json["replies"].is_array()) {
        std::vector<slimr::Json> reps;
        for (const auto& r : json["replies"].get<slimr::Json::array_t>()) {
            reps.push_back(r);
        }
        fx.replies = std::move(reps);
    }

    auto expand_doc = [](const slimr::Json& doc) -> std::optional<slimr::Json> {
        if (doc.is_null()) return std::nullopt;
        slimr::Json out = doc;
        out["documentId"] = "conformance";
        tabs_walk(out, [](slimr::Json& dt) {
            auto tmpl = blank_tab_template();
            for (const auto& field : {"documentStyle", "namedStyles"}) {
                if (dt.contains(field) && dt[field].is_string() && dt[field].get<std::string>() == "$blank") {
                    dt[field] = tmpl[field];
                }
            }
        });
        return out;
    };

    if (json.contains("before")) fx.before = *expand_doc(json["before"]);
    if (json.contains("after") && !json["after"].is_null()) fx.after = expand_doc(json["after"]);

    return fx;
}

inline void ids_collect(const slimr::Json& node, std::vector<std::string>& out, std::set<std::string>& seen) {
    if (node.is_array()) {
        for (const auto& item : node.get<slimr::Json::array_t>()) {
            ids_collect(item, out, seen);
        }
        return;
    }
    if (!node.is_object()) return;
    for (const auto& [key, value] : node.get<slimr::Json::object_t>()) {
        if (value.is_string() && ID_FIELDS.contains(std::string(key))) {
            std::string s = value.get<std::string>();
            if (seen.insert(s).second) out.push_back(s);
        }
        if (key == "heading" && value.is_object() && value.contains("id") && value["id"].is_string()) {
            std::string s = value["id"].get<std::string>();
            if (seen.insert(s).second) out.push_back(s);
        }
        if (ID_KEYED_FIELDS.contains(std::string(key)) && value.is_object()) {
            for (const auto& [k, v] : value.get<slimr::Json::object_t>()) {
                std::string s = std::string(k);
                if (seen.insert(s).second) out.push_back(s);
            }
        }
        ids_collect(value, out, seen);
    }
}

inline void ids_collect(const slimr::Json& node, std::set<std::string>& out) {
    std::vector<std::string> dummy;
    ids_collect(node, dummy, out);
}

inline slimr::Json ids_normalize(const slimr::Json& doc, const std::set<std::string>& baseline) {
    std::vector<std::string> found;
    std::set<std::string> seen;
    ids_collect(doc, found, seen);

    std::map<std::string, std::string> rename_map;
    for (const auto& id : found) {
        if (!baseline.contains(id)) {
            if (!rename_map.contains(id)) {
                rename_map[id] = "NEW#" + std::to_string(rename_map.size() + 1);
            }
        }
    }

    auto walk = [&](auto& self, const slimr::Json& node, std::string_view parent_key) -> slimr::Json {
        if (node.is_array()) {
            slimr::Json::array_t arr;
            for (const auto& item : node.get<slimr::Json::array_t>()) {
                arr.push_back(self(self, item, parent_key));
            }
            return arr;
        }
        if (!node.is_object()) return node;

        slimr::Json out = slimr::Json::object_t{};
        for (const auto& [key, value] : node.get<slimr::Json::object_t>()) {
            std::string out_key = std::string(key);
            if (!parent_key.empty() && ID_KEYED_FIELDS.contains(std::string(parent_key))) {
                auto it = rename_map.find(std::string(key));
                if (it != rename_map.end()) out_key = it->second;
            }
            bool is_id = ID_FIELDS.contains(std::string(key)) || (parent_key == "heading" && key == "id");
            if (value.is_string() && is_id) {
                auto it = rename_map.find(value.get<std::string>());
                out[out_key] = (it != rename_map.end()) ? it->second : value;
            } else {
                out[out_key] = self(self, value, key);
            }
        }
        return out;
    };

    return walk(walk, doc, "");
}

inline slimr::Json server_derived_strip(slimr::Json doc) {
    auto walk = [](auto& self, slimr::Json& node) -> void {
        if (node.is_array()) {
            for (auto& item : node.get<slimr::Json::array_t>()) {
                self(self, item);
            }
            return;
        }
        if (!node.is_object()) return;
        if (node.contains("richLinkProperties") && node["richLinkProperties"].is_object()) {
            for (const auto& field : RICH_LINK_DERIVED_FIELDS) {
                if (node["richLinkProperties"].contains(field)) {
                    node["richLinkProperties"].get<slimr::Json::object_t>().erase(field);
                }
            }
        }
        for (auto& [k, v] : node.get<slimr::Json::object_t>()) {
            self(self, v);
        }
    };
    walk(walk, doc);
    return doc;
}

inline slimr::Json reply_shape(const slimr::Json& node) {
    if (node.is_array()) {
        slimr::Json::array_t arr;
        for (const auto& item : node.get<slimr::Json::array_t>()) {
            arr.push_back(reply_shape(item));
        }
        return arr;
    }
    if (!node.is_object()) return node;

    std::map<std::string, slimr::Json> sorted;
    for (const auto& [key, value] : node.get<slimr::Json::object_t>()) {
        if (value.is_string() && std::string(key).ends_with("Id")) {
            sorted[std::string(key)] = "ID";
        } else {
            sorted[std::string(key)] = reply_shape(value);
        }
    }
    slimr::Json::object_t out;
    for (auto& [k, v] : sorted) {
        out.insert({k, std::move(v)});
    }
    return out;
}

// Structural comparison of normalized document JSON
inline void doc_json_compare_rec(std::string_view path, const slimr::Json& expected, const slimr::Json& actual,
                                 std::vector<std::string>& diffs) {
    if (expected.is_null() && actual.is_null()) return;
    if (expected.is_null() != actual.is_null()) {
        diffs.push_back(std::string(path) + ": one is null, one is not");
        return;
    }
    if (expected.is_string() && actual.is_string()) {
        if (expected.get<std::string>() != actual.get<std::string>()) {
            diffs.push_back(std::string(path) + ": expected \"" + expected.get<std::string>() + "\", got \"" + actual.get<std::string>() + "\"");
        }
        return;
    }
    if (expected.is_number() && actual.is_number()) {
        double e = expected.as<double>();
        double a = actual.as<double>();
        if (std::abs(e - a) > 1e-4) {
            diffs.push_back(std::string(path) + ": expected " + std::to_string(e) + ", got " + std::to_string(a));
        }
        return;
    }
    if (expected.is_boolean() && actual.is_boolean()) {
        if (expected.get<bool>() != actual.get<bool>()) {
            diffs.push_back(std::string(path) + ": boolean mismatch");
        }
        return;
    }
    if (expected.is_array() && actual.is_array()) {
        const auto& ea = expected.get<slimr::Json::array_t>();
        const auto& aa = actual.get<slimr::Json::array_t>();
        if (ea.size() != aa.size()) {
            diffs.push_back(std::string(path) + ": array size expected " + std::to_string(ea.size()) + ", got " + std::to_string(aa.size()));
            return;
        }
        for (size_t i = 0; i < ea.size(); ++i) {
            doc_json_compare_rec(std::string(path) + "[" + std::to_string(i) + "]", ea[i], aa[i], diffs);
        }
        return;
    }
    if (expected.is_object() && actual.is_object()) {
        const auto& eo = expected.get<slimr::Json::object_t>();
        const auto& ao = actual.get<slimr::Json::object_t>();
        for (const auto& [k, v] : eo) {
            // Ignore server revision id or document id if different
            if (k == "revisionId" || k == "documentId") continue;
            if (!ao.contains(k)) {
                diffs.push_back(std::string(path) + ": missing key \"" + std::string(k) + "\"");
            } else {
                doc_json_compare_rec(std::string(path) + "." + std::string(k), v, ao.find(k)->second, diffs);
            }
        }
        for (const auto& [k, v] : ao) {
            if (k == "revisionId" || k == "documentId") continue;
            if (!eo.contains(k)) {
                diffs.push_back(std::string(path) + ": unexpected extra key \"" + std::string(k) + "\"");
            }
        }
        return;
    }
    diffs.push_back(std::string(path) + ": type mismatch");
}

inline std::vector<std::string> conformance_compare(const ConformanceFixture& fx) {
    EmulateResult emulated;
    try {
        emulated = requests_emulate(fx.before, fx.requests);
    } catch (const EmulatorError& err) {
        if (err.code == EmulatorErrorCode::UNMODELED) return {};
        if (fx.after.has_value()) {
            return {"emulator rejected the batch the API accepted: " + std::string(err.what())};
        }
        return {};
    } catch (const std::exception& err) {
        if (fx.after.has_value()) {
            return {"emulator rejected the batch the API accepted: " + std::string(err.what())};
        }
        return {};
    }

    if (!fx.after.has_value()) {
        std::string err_msg = fx.error.value_or("(no error text)");
        return {"emulator accepted the batch the API rejected: " + err_msg};
    }

    std::set<std::string> baseline;
    ids_collect(fx.before, baseline);

    slimr::Json expected_norm = server_derived_strip(ids_normalize(*fx.after, baseline));
    slimr::Json actual_norm = server_derived_strip(ids_normalize(emulated.json, baseline));

    auto parse = [](const slimr::Json& doc) {
        gdocsmith::model::KeyAllocator keys;
        return gdocsmith::model::docModelParse(doc, gdocsmith::model::DocModelParseOptions{
            .docId = "conformance",
            .keys = keys
        });
    };
    std::vector<std::string> diffs;
    try {
        diffs = gdocsmith::model::docModelsCompare(parse(expected_norm), parse(actual_norm)).diffs;
    } catch (const std::exception& e) {
        diffs.push_back(std::string("parse error: ") + e.what());
    }

    if (fx.replies.has_value()) {
        slimr::Json::array_t exp_reps;
        for (const auto& r : *fx.replies) exp_reps.push_back(r);
        slimr::Json live_shape = reply_shape(exp_reps);

        slimr::Json::array_t act_reps;
        for (const auto& r : emulated.replies) act_reps.push_back(r);
        slimr::Json act_shape = reply_shape(act_reps);

        std::string s_live = slimr::write_json(live_shape);
        std::string s_act = slimr::write_json(act_shape);
        if (s_live != s_act) {
            diffs.push_back("replies: expected " + s_live + ", got " + s_act);
        }
    }

    return diffs;
}

} // namespace gdocsmith::emulator
