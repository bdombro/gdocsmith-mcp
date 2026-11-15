#pragma once

#include "lens/dropdowns.hpp"
#include "model/keys.hpp"
#include "model/walk.hpp"

#include <map>
#include <set>

namespace gdocsmith::engine {

inline std::vector<slimr::Json> dropdownOptionsOf(const slimr::Json& definition) {
    if (!definition.contains("dropdownDefinitionProperties")) return {};
    const auto& properties = definition["dropdownDefinitionProperties"];
    if (!properties.contains("options") || !properties["options"].is_array()) return {};
    return properties["options"].get<slimr::Json::array_t>();
}

inline slimr::Json dropdownListsExport(const model::TabModel& tab) {
    auto definitions = model::make_json_object();
    for (const auto& [id, definition] : tab.dropdownDefinitions) definitions[id] = definition;
    auto out = model::make_json_object();
    const auto lists = lens::dropdownListsOf(definitions);
    const std::map<std::string, lens::DropdownList> ordered(lists.begin(), lists.end());
    for (const auto& [id, list] : ordered) {
        slimr::Json::array_t options;
        for (const auto& option : list.options) options.push_back(lens::dropdownOptionToJson(option));
        auto value = slimr::Json(slimr::Json::object_t{{"options", std::move(options)}});
        if (list.title && !list.title->empty()) value["title"] = *list.title;
        out[id] = std::move(value);
    }
    return out;
}

inline void dropdownDefinitionsApply(model::TabModel& tab, const slimr::Json& lists,
                                     model::KeyAllocator& keys, std::map<std::string, std::string>& aliases) {
    std::unordered_map<std::string, model::JsonObject> definitions;
    std::set<std::string> reserved;
    for (const auto& [id, definition] : tab.dropdownDefinitions) {
        reserved.insert(id);
        for (const auto& option : dropdownOptionsOf(definition)) {
            if (option.contains("optionId") && option["optionId"].is_string()) {
                reserved.insert(option["optionId"].get<std::string>());
            }
        }
    }
    for (const auto& [_, id] : aliases) reserved.insert(id);
    auto mint = [&](std::string_view prefix) {
        std::string id;
        do { id = std::string(prefix) + keys.next("n"); } while (reserved.contains(id));
        reserved.insert(id);
        return id;
    };
    for (const auto& [key, list] : lists.get<slimr::Json::object_t>()) {
        std::vector<lens::DropdownOption> options;
        for (const auto& option : list["options"].get<slimr::Json::array_t>()) options.push_back(lens::dropdownOptionOf(option));
        if (options.size() < 2 || options.size() > 50) throw model::CoreError("unsupportedSyntax",
            "dropdown list \"" + key + "\" needs 2 to 50 options, not " + std::to_string(options.size()));
        std::set<std::string> names;
        for (const auto& option : options) if (!names.insert(option.name).second) {
            throw model::CoreError("unsupportedSyntax", "dropdown list \"" + key + "\" has two options named \"" + option.name + "\"");
        }
        const bool existing = tab.dropdownDefinitions.contains(key);
        std::string id = key;
        if (!existing) {
            if (!aliases.contains(key)) aliases[key] = mint("kix.");
            id = aliases.at(key);
        }
        const auto stored = tab.dropdownDefinitions.find(id);
        const auto old = stored == tab.dropdownDefinitions.end() ? std::vector<slimr::Json>{} : dropdownOptionsOf(stored->second);
        std::set<std::string> taken;
        for (const auto& option : old) {
            if (names.contains(option["displayValue"].get<std::string>())) taken.insert(option["optionId"].get<std::string>());
        }
        slimr::Json::array_t built;
        for (size_t i = 0; i < options.size(); ++i) {
            const auto& option = options[i];
            std::optional<slimr::Json> previous;
            for (const auto& candidate : old) if (candidate["displayValue"] == slimr::Json(option.name)) previous = candidate;
            if (!previous && i < old.size() && !names.contains(old[i]["displayValue"].get<std::string>()) &&
                !taken.contains(old[i]["optionId"].get<std::string>())) previous = old[i];
            const auto optionId = previous ? (*previous)["optionId"].get<std::string>() : mint("dropdownItem.");
            taken.insert(optionId);
            auto value = slimr::Json(slimr::Json::object_t{{"displayValue", option.name}, {"optionId", optionId}});
            bool same = false;
            if (previous) {
                const auto style = previous->contains("textStyle") ? (*previous)["textStyle"] : model::make_json_object();
                const auto color = style.contains("foregroundColor") ? model::colorHexFromOptional(style["foregroundColor"]) : std::nullopt;
                const auto background = style.contains("backgroundColor") ? model::colorHexFromOptional(style["backgroundColor"]) : std::nullopt;
                same = color == option.color && background == option.background;
                if (same && previous->contains("textStyle")) value["textStyle"] = (*previous)["textStyle"];
            }
            if (!same) value["textStyle"] = slimr::Json::object_t{
                {"backgroundColor", option.background ? model::colorOptionalFromHex(*option.background) : model::make_json_object()},
                {"foregroundColor", option.color ? model::colorOptionalFromHex(*option.color) : model::make_json_object()}};
            built.push_back(std::move(value));
        }
        auto definition = stored == tab.dropdownDefinitions.end() ? model::make_json_object() : stored->second;
        definition["dropdownDefinitionId"] = id;
        definition["dropdownDefinitionProperties"] = slimr::Json::object_t{
            {"options", std::move(built)}, {"title", list.contains("title") ? list["title"] : slimr::Json(existing ? "" : key)}};
        definitions[id] = std::move(definition);
    }
    tab.dropdownDefinitions = std::move(definitions);
    for (auto& block : tab.blocks) {
        auto relabel = [&](auto& self, model::Block& item) -> void {
            if (item.is_table()) {
                for (auto& row : item.as_table().rows) for (auto& cell : row.cells) for (auto& child : cell.blocks) self(self, child);
            } else if (item.is_paragraph()) for (auto& atom : item.as_paragraph().inlines) {
                if (atom.type != "dropdown" || !atom.raw || !atom.raw->contains("dropdown")) continue;
                auto& props = (*atom.raw)["dropdown"]["dropdownProperties"];
                if (!props.contains("dropdownDefinitionId") || !props.contains("selectedOptionId")) continue;
                const auto definition = tab.dropdownDefinitions.find(props["dropdownDefinitionId"].get<std::string>());
                if (definition == tab.dropdownDefinitions.end()) continue;
                for (const auto& option : dropdownOptionsOf(definition->second)) {
                    if (option["optionId"] == props["selectedOptionId"]) props["displayValue"] = option["displayValue"];
                }
            }
        };
        relabel(relabel, block);
    }
}

inline void dropdownChipsCheck(const model::TabModel& tab) {
    for (const auto& atom : model::atomsDeep(tab.blocks)) {
        if (atom.type != "dropdown") continue;
        auto props = model::make_json_object();
        if (atom.raw && atom.raw->contains("dropdown") && (*atom.raw)["dropdown"].contains("dropdownProperties")) {
            props = (*atom.raw)["dropdown"]["dropdownProperties"];
        }
        const auto id = atom.create && atom.create->definitionId ? *atom.create->definitionId :
            props.contains("dropdownDefinitionId") ? props["dropdownDefinitionId"].get<std::string>() : "";
        const auto selected = atom.create && atom.create->optionId ? *atom.create->optionId :
            props.contains("selectedOptionId") ? props["selectedOptionId"].get<std::string>() : "";
        const auto name = props.contains("displayValue") ? props["displayValue"].get<std::string>() : "a dropdown";
        const auto definition = tab.dropdownDefinitions.find(id);
        if (definition == tab.dropdownDefinitions.end()) throw model::CoreError("unsupportedSyntax",
            "dropdown \"" + name + "\" uses a list the dropdowns frontmatter removed; delete the chip or keep its list");
        const auto options = dropdownOptionsOf(definition->second);
        if (std::none_of(options.begin(), options.end(), [&](const auto& option) { return option["optionId"] == slimr::Json(selected); })) {
            throw model::CoreError("unsupportedSyntax", "dropdown \"" + name +
                "\" selects an option the dropdowns frontmatter removed; select another option first (option=…)");
        }
    }
}

} // namespace gdocsmith::engine
