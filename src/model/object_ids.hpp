#pragma once

#include "model/raw_json.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gdocsmith::model {

inline constexpr std::string_view OPENING_SECTION_KEY = "§opening";

inline std::optional<std::string> googleIdOf(const Atom& atom) {
    if (!atom.raw.has_value() || !atom.raw->holds<slimr::Json::object_t>()) {
        return std::nullopt;
    }
    const auto& map = atom.raw->get<slimr::Json::object_t>();
    auto findId = [&](std::string_view objKey, std::string_view idKey) -> std::optional<std::string> {
        auto it = map.find(objKey);
        if (it != map.end() && it->second.holds<slimr::Json::object_t>()) {
            const auto& inner = it->second.get<slimr::Json::object_t>();
            auto idIt = inner.find(idKey);
            if (idIt != inner.end() && idIt->second.holds<std::string>()) {
                const auto& s = idIt->second.get<std::string>();
                if (!s.empty()) return s;
            }
        }
        return std::nullopt;
    };
    if (auto id = findId("inlineObjectElement", "inlineObjectId")) return id;
    if (auto id = findId("person", "personId")) return id;
    if (auto id = findId("dateElement", "dateId")) return id;
    if (auto id = findId("richLink", "richLinkId")) return id;
    if (auto id = findId("dropdown", "dropdownId")) return id;
    return std::nullopt;
}

inline void objectIdsAssign(DocModel& doc) {
    struct GoogleEntry {
        std::string id;
        std::string key;
        TabModel* tab = nullptr;
    };
    std::vector<GoogleEntry> google;
    std::map<std::string, int> counters;

    auto counted = [&](TabModel& tab, const std::string& key, const std::string& prefix) {
        int n = counters[prefix] + 1;
        counters[prefix] = n;
        if (!tab.objectIds.has_value()) tab.objectIds.emplace();
        (*tab.objectIds)[key] = prefix + std::to_string(n);
    };

    static const std::unordered_map<std::string, std::string> COUNTED_PREFIXES = {
        {"autoText", "at"},
        {"columnBreak", "cb"},
        {"equation", "eq"},
        {"horizontalRule", "hr"},
        {"pageBreak", "pb"}
    };

    for (auto& top : doc.tabs) {
        std::vector<TabModel*> allTabs;
        allTabs.push_back(&top);
        for (auto& seg : top.segments) {
            if (seg.tab) allTabs.push_back(seg.tab.get());
        }
        for (auto* tabPtr : allTabs) {
            auto& tab = *tabPtr;
            tab.objectIds = std::unordered_map<std::string, std::string>{};
            if (&tab == &top) {
                counted(tab, std::string(OPENING_SECTION_KEY), "s");
            }
            for (const auto& block : blocksDeep(tab.blocks)) {
                if (block.is_section_break()) {
                    counted(tab, block.key, "s");
                } else if (block.is_toc()) {
                    counted(tab, block.key, "toc");
                } else if (block.is_paragraph()) {
                    const auto& p = block.as_paragraph();
                    if (p.positionedObjectIds.has_value()) {
                        for (const auto& id : *p.positionedObjectIds) {
                            google.push_back(GoogleEntry{.id = id, .key = id, .tab = &tab});
                        }
                    }
                    for (const auto& inlineItem : p.inlines) {
                        if (!inlineItem.is_atom()) continue;
                        auto atom = inlineItem.to_atom();
                        auto id = googleIdOf(atom);
                        if (id.has_value()) {
                            google.push_back(GoogleEntry{.id = *id, .key = atom.key, .tab = &tab});
                        } else {
                            auto it = COUNTED_PREFIXES.find(atom.type);
                            if (it != COUNTED_PREFIXES.end()) {
                                counted(tab, atom.key, it->second);
                            }
                        }
                    }
                }
            }
        }
    }

    std::set<std::string> taken;
    for (const auto& top : doc.tabs) {
        std::vector<const TabModel*> allTabs;
        allTabs.push_back(&top);
        for (const auto& seg : top.segments) {
            if (seg.tab) allTabs.push_back(seg.tab.get());
        }
        for (const auto* tabPtr : allTabs) {
            if (tabPtr->objectIds.has_value()) {
                for (const auto& [_, val] : *tabPtr->objectIds) {
                    taken.insert(val);
                }
            }
        }
    }

    for (const auto& entry : google) {
        int length = std::min(4, static_cast<int>(entry.id.size()));
        auto getSuffix = [&](int len) {
            return entry.id.substr(entry.id.size() - len);
        };
        auto collides = [&](int len) {
            std::string suff = getSuffix(len);
            if (taken.contains(suff)) return true;
            for (const auto& other : google) {
                if (&other != &entry && other.id != entry.id && other.id.size() >= static_cast<size_t>(len)) {
                    if (other.id.substr(other.id.size() - len) == suff) return true;
                }
            }
            return false;
        };

        while (length < static_cast<int>(entry.id.size()) && collides(length)) {
            length++;
        }
        std::string finalId = getSuffix(length);
        taken.insert(finalId);
        if (!entry.tab->objectIds.has_value()) entry.tab->objectIds.emplace();
        (*entry.tab->objectIds)[entry.key] = finalId;
    }
}

} // namespace gdocsmith::model
