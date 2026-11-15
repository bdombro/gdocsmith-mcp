#pragma once

#include "json.hpp"
#include "lens/cell_styles.hpp"
#include "lens/directives.hpp"
#include "lens/json_helper.hpp"
#include "model/style_values.hpp"
#include "model/types.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gdocsmith::lens {

struct DropdownOption {
    std::optional<std::string> background;
    std::optional<std::string> color;
    std::string name;

    bool operator==(const DropdownOption &other) const = default;
};

struct DropdownList {
    std::vector<DropdownOption> options;
    std::optional<std::string> title;

    bool operator==(const DropdownList &other) const = default;
};

using DropdownLists = std::unordered_map<std::string, DropdownList>;

inline DropdownOption dropdownOptionOf(const slimr::Json &entry) {
    DropdownOption opt;
    if (entry.is_string()) {
        opt.name = entry.get<std::string>();
        return opt;
    }
    if (entry.is_object() && !entry.get<slimr::Json::object_t>().empty()) {
        const auto &[name, colors] = *entry.get<slimr::Json::object_t>().begin();
        opt.name = name;
        if (colors.is_object()) {
            if (auto *bg = json::get(colors, "background")) {
                if (bg->is_string())
                    opt.background = bg->get<std::string>();
            }
            if (auto *col = json::get(colors, "color")) {
                if (col->is_string())
                    opt.color = col->get<std::string>();
            }
        }
    }
    return opt;
}

inline slimr::Json dropdownOptionToJson(const DropdownOption &opt) {
    if (!opt.background && !opt.color) {
        return slimr::Json(opt.name);
    }
    slimr::Json colors = slimr::json_obj();
    if (opt.background)
        colors["background"] = *opt.background;
    if (opt.color)
        colors["color"] = *opt.color;
    slimr::Json j = slimr::json_obj();
    j[opt.name] = std::move(colors);
    return j;
}

inline DropdownLists dropdownListsOf(const slimr::Json &dropdownDefinitions) {
    DropdownLists lists;
    if (!dropdownDefinitions.is_object())
        return lists;

    for (const auto &[id, def] : dropdownDefinitions.get<slimr::Json::object_t>()) {
        auto *props = json::get(def, "dropdownDefinitionProperties");
        if (!props || !props->is_object())
            continue;
        DropdownList dl;
        if (auto *title = json::get(*props, "title")) {
            if (title->is_string())
                dl.title = title->get<std::string>();
        }

        if (auto *options = json::get(*props, "options")) {
            if (options->is_array()) {
                for (const auto &option : options->get<slimr::Json::array_t>()) {
                    DropdownOption opt;
                    if (auto *dv = json::get(option, "displayValue")) {
                        opt.name = dv->is_string() ? dv->get<std::string>() : "";
                    }
                    if (auto *textStyle = json::get(option, "textStyle")) {
                        if (textStyle->is_object()) {
                            if (auto *fg = json::get(*textStyle, "foregroundColor")) {
                                auto hex = colorHexFromOptional(*fg);
                                if (hex)
                                    opt.color = *hex;
                            }
                            if (auto *bg = json::get(*textStyle, "backgroundColor")) {
                                auto hex = colorHexFromOptional(*bg);
                                if (hex)
                                    opt.background = *hex;
                            }
                        }
                    }
                    dl.options.push_back(std::move(opt));
                }
            }
        }
        lists[id] = std::move(dl);
    }
    return lists;
}

inline std::optional<std::string> optionIdOf(const slimr::Json &definition, std::string_view name) {
    auto *props = json::get(definition, "dropdownDefinitionProperties");
    if (!props || !props->is_object())
        return std::nullopt;
    auto *options = json::get(*props, "options");
    if (!options || !options->is_array())
        return std::nullopt;
    for (const auto &option : options->get<slimr::Json::array_t>()) {
        auto *dv = json::get(option, "displayValue");
        if (dv && dv->is_string() && dv->get<std::string>() == name) {
            auto *oid = json::get(option, "optionId");
            if (oid && oid->is_string())
                return oid->get<std::string>();
        }
    }
    return std::nullopt;
}

struct DropdownSelection {
    std::string definitionId;
    std::string optionId;
};

inline DropdownSelection dropdownResolve(const model::TabModel &tab, const std::string &list,
                                         const std::string &option,
                                         const std::map<std::string, std::string> &createdIds = {}) {
    auto definitionId = list;
    auto definition = tab.dropdownDefinitions.find(definitionId);
    if (definition == tab.dropdownDefinitions.end()) {
        const auto created = createdIds.find(list);
        if (created != createdIds.end()) {
            definitionId = created->second;
            definition = tab.dropdownDefinitions.find(definitionId);
        }
    }
    if (definition == tab.dropdownDefinitions.end()) {
        throw CoreError("unsupportedSyntax", "there's no dropdown list \"" + list +
                                                 "\" in this tab's dropdowns; name a list by its key there");
    }
    const auto optionId = optionIdOf(definition->second, option);
    if (!optionId)
        throw CoreError("unsupportedSyntax", "dropdown list \"" + list + "\" has no option \"" + option + "\"");
    return {.definitionId = definitionId, .optionId = *optionId};
}

} // namespace gdocsmith::lens
