#pragma once

#include "lens/frontmatter.hpp"
#include "lens/document.hpp"
#include "lens/text_styles.hpp"
#include "model/types.hpp"

#include <set>
#include <string>

namespace gdocsmith::engine {

inline model::JsonObject namedStyleSetOf(const model::TabModel& tab) {
    auto result = model::make_json_object();
    if (!tab.namedStyles.contains("styles") || !tab.namedStyles["styles"].is_array()) return result;
    for (const auto& entry : tab.namedStyles["styles"].get<slimr::Json::array_t>()) {
        if (!entry.contains("namedStyleType") || !entry["namedStyleType"].is_string()) continue;
        auto declaration = entry.contains("paragraphStyle")
            ? lens::declarationJson(lens::paragraphDeclarationExport(entry["paragraphStyle"])) : model::make_json_object();
        if (declaration.is_object()) declaration.get<slimr::Json::object_t>().erase("namedStyle");
        if (entry.contains("textStyle")) {
            const auto& text = entry["textStyle"];
            for (const auto& [field, value] : lens::textStyleExport(text).values) {
                if (field == "fontSize" || field == "fontWeight") declaration[field] = std::stod(value);
                else declaration[field] = value;
            }
            for (const auto* field : {"bold", "italic", "underline", "strikethrough", "smallCaps"}) {
                if (text.contains(field)) declaration[field] = text[field];
            }
        }
        result[entry["namedStyleType"].get<std::string>()] = std::move(declaration);
    }
    return result;
}

inline bool namedStyleSetApply(model::TabModel& tab, const model::JsonObject& before,
                               const model::JsonObject& after, const model::TabModel& generic) {
    const auto fallback = namedStyleSetOf(generic);
    auto styles = tab.namedStyles.contains("styles") ? tab.namedStyles["styles"] : slimr::Json(slimr::Json::array_t{});
    if (!styles.is_array() || !before.is_object() || !after.is_object()) {
        throw model::CoreError("unsupportedSyntax", "Named styles must be mappings of named style types to declarations.");
    }
    const std::set<std::string> textFields{
        "bold", "italic", "underline", "strikethrough", "smallCaps", "fontFamily", "fontWeight",
        "fontSize", "foregroundColor", "backgroundColor", "baselineOffset"};
    std::set<std::string> types;
    for (const auto* set : {&before, &after}) for (const auto& [type, value] : set->get<slimr::Json::object_t>()) {
        if (std::find(lens::NAMED_STYLE_TYPES.begin(), lens::NAMED_STYLE_TYPES.end(), type) == lens::NAMED_STYLE_TYPES.end() ||
            !value.is_object()) throw model::CoreError("unsupportedSyntax", "Invalid named style declaration.");
        types.insert(type);
    }
    bool changed = false;
    for (const auto& type : types) {
        const auto was = before.contains(type) ? before[type] : model::make_json_object();
        const auto now = after.contains(type) ? after[type] : model::make_json_object();
        std::set<std::string> fields;
        for (const auto* declaration : {&was, &now}) for (const auto& [field, value] : declaration->get<slimr::Json::object_t>()) {
            (void)value;
            fields.insert(field);
        }
        auto textPatch = model::make_json_object(), paragraphDeclaration = model::make_json_object();
        for (const auto& field : fields) {
            if (was.contains(field) == now.contains(field) &&
                (!was.contains(field) || was[field] == now[field])) continue;
            const auto value = now.contains(field) ? now[field] :
                fallback.contains(type) && fallback[type].contains(field) ? fallback[type][field] : slimr::Json(nullptr);
            if (was.contains(field) && was[field] == value) continue;
            if (!textFields.contains(field)) {
                paragraphDeclaration[field] = value;
                continue;
            }
            if (field == "fontFamily" || field == "fontWeight") {
                if (value.is_null()) textPatch["weightedFontFamily"] = nullptr;
                else {
                    if (textPatch.contains("weightedFontFamily") && textPatch["weightedFontFamily"].is_null()) {
                        textPatch["weightedFontFamily"] = model::make_json_object();
                    }
                    lens::DirectiveAttrs attrs;
                    attrs.values.emplace_back(field, value.is_string() ? value.get<std::string>() : slimr::write_json(value));
                    lens::textStyleApply(textPatch, attrs);
                }
            } else if (field == "baselineOffset" && value.is_string() && value.get<std::string>() == "NONE") {
                textPatch[field] = value;
            } else if (value.is_null()) textPatch[field] = nullptr;
            else if (field == "bold" || field == "italic" || field == "underline" || field == "strikethrough" || field == "smallCaps") {
                if (!value.is_boolean()) throw model::CoreError("unsupportedSyntax", "Named style flags must be booleans.");
                textPatch[field] = value;
            } else {
                lens::DirectiveAttrs attrs;
                attrs.values.emplace_back(field, value.is_string() ? value.get<std::string>() : slimr::write_json(value));
                lens::textStyleApply(textPatch, attrs);
            }
        }
        const auto validated = lens::frontmatterValidate(slimr::Json::object_t{
            {"paragraphStyles", slimr::Json::object_t{{"named", paragraphDeclaration}}}}, "tab");
        const auto paragraphPatch = lens::paragraphDeclarationPatch(validated.paragraphStyles.at("named"));
        if (textPatch.get<slimr::Json::object_t>().empty() && paragraphPatch.get<slimr::Json::object_t>().empty()) continue;
        if (tab.suggestedNamedStylesChanges) throw model::CoreError("unsupportedSyntax",
            "Pending named-style suggestions must be handled in Google Docs before changing named styles.");
        auto& entries = styles.get<slimr::Json::array_t>();
        auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.contains("namedStyleType") && entry["namedStyleType"] == slimr::Json(type);
        });
        if (found == entries.end()) {
            entries.push_back(slimr::Json::object_t{{"namedStyleType", type}});
            found = entries.end() - 1;
        }
        auto apply = [&](const char* field, const model::JsonObject& patch) {
            auto value = found->contains(field) ? (*found)[field] : model::make_json_object();
            if (!value.is_object()) value = model::make_json_object();
            for (const auto& [name, entry] : patch.get<slimr::Json::object_t>()) {
                if (entry.is_null()) value.get<slimr::Json::object_t>().erase(name);
                else if (name == "weightedFontFamily" && value.contains(name) && value[name].is_object()) {
                    for (const auto& [fontField, fontValue] : entry.get<slimr::Json::object_t>()) value[name][fontField] = fontValue;
                } else value[name] = entry;
            }
            (*found)[field] = std::move(value);
        };
        apply("textStyle", textPatch);
        apply("paragraphStyle", paragraphPatch);
        changed = true;
    }
    if (changed) tab.namedStyles["styles"] = std::move(styles);
    return changed;
}

} // namespace gdocsmith::engine
