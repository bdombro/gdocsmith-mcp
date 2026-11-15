#pragma once

#include "lens/atom_put.hpp"
#include "lens/dropdowns.hpp"
#include "lens/json_helper.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("dropdownOptionOf parses string and colored options") {
    slimr::Json str_opt("In Progress");
    auto opt1 = dropdownOptionOf(str_opt);
    CHECK(opt1.name == "In Progress");
    CHECK(!opt1.color.has_value());
    CHECK(!opt1.background.has_value());

    slimr::Json obj_opt = slimr::json_obj();
    slimr::Json col = slimr::json_obj();
    col["color"] = "#FFFFFF";
    col["background"] = "#1967D2";
    obj_opt["Done"] = std::move(col);

    auto opt2 = dropdownOptionOf(obj_opt);
    CHECK(opt2.name == "Done");
    CHECK(opt2.color == std::optional<std::string>("#FFFFFF"));
    CHECK(opt2.background == std::optional<std::string>("#1967D2"));
}

TEST_CASE("dropdownListsOf extracts lists and option IDs") {
    slimr::Json defs = slimr::json_obj();
    slimr::Json list1 = slimr::json_obj();
    slimr::Json props = slimr::json_obj();
    props["title"] = "Status";

    slimr::Json options = slimr::json_arr();
    slimr::Json o1 = slimr::json_obj();
    o1["optionId"] = "opt.1";
    o1["displayValue"] = "Todo";
    json::push_back(options, std::move(o1));

    slimr::Json o2 = slimr::json_obj();
    o2["optionId"] = "opt.2";
    o2["displayValue"] = "Done";
    json::push_back(options, std::move(o2));

    props["options"] = std::move(options);
    list1["dropdownDefinitionProperties"] = std::move(props);
    defs["kix.status"] = std::move(list1);

    auto lists = dropdownListsOf(defs);
    CHECK(lists.contains("kix.status"));
    CHECK(lists["kix.status"].title == std::optional<std::string>("Status"));
    CHECK(lists["kix.status"].options.size() == 2u);
    CHECK(lists["kix.status"].options[0].name == "Todo");
    CHECK(lists["kix.status"].options[1].name == "Done");

    CHECK(optionIdOf(defs["kix.status"], "Todo") == std::optional<std::string>("opt.1"));
    CHECK(optionIdOf(defs["kix.status"], "Done") == std::optional<std::string>("opt.2"));
    CHECK(!optionIdOf(defs["kix.status"], "Missing").has_value());
}

TEST_CASE("source dropdown create select and explicit keys preserve provider option IDs") {
    model::TabModel tab;
    tab.dropdownDefinitions["list-1"] = slimr::Json::object_t{
        {"dropdownDefinitionProperties", slimr::Json::object_t{
                                             {"title", "Status"}, {"options", slimr::Json::array_t{slimr::Json::object_t{{"displayValue", "Todo"}, {"optionId", "provider-todo"}}, slimr::Json::object_t{{"displayValue", "Done"}, {"optionId", "provider-done"}}}}}}};
    model::EditContext context;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    const auto created = atomTokenCreate(target, TokenRef{.kind = "dropdown", .attrs = directiveAttrsParse("list=list-1 option=Done")}, {}, report);
    REQUIRE(created.create);
    CHECK(created.create->definitionId == "list-1");
    CHECK(created.create->optionId == "provider-done");
    model::Atom existing{.key = "chip", .type = "dropdown", .raw = slimr::Json::object_t{{"dropdown", slimr::Json::object_t{{"dropdownId", "provider-chip"}, {"dropdownProperties", slimr::Json::object_t{{"dropdownDefinitionId", "list-1"}, {"selectedOptionId", "provider-todo"}, {"displayValue", "Todo"}}}}}}};
    TokenRef before{.kind = "dropdown", .attrs = directiveAttrsParse("id=chip list=list-1"), .label = "Todo"};
    auto after = before;
    after.label = "Done";
    after.attrs.values.emplace_back("option", "Done");
    CHECK(atomTokenIdentity(before) == atomTokenIdentity(after));
    atomTokenKeep(existing, before, after, tab, {});
    CHECK(existing.key == "chip");
    REQUIRE(existing.raw);
    CHECK((*existing.raw)["dropdown"]["dropdownId"] == slimr::Json("provider-chip"));
    CHECK((*existing.raw)["dropdown"]["dropdownProperties"]["selectedOptionId"] == slimr::Json("provider-done"));
    CHECK_THROWS_AS(atomTokenCreate(target, TokenRef{.kind = "dropdown", .attrs = directiveAttrsParse("list=Status option=Done")}, {}, report), model::CoreError);
    tab.dropdownDefinitions["list-2"] = tab.dropdownDefinitions["list-1"];
    tab.dropdownDefinitions["list-2"]["dropdownDefinitionProperties"]["title"] = "list-1";
    tab.dropdownDefinitions["list-2"]["dropdownDefinitionProperties"]["options"][1]["optionId"] = "other-done";
    CHECK_THROWS_AS(atomTokenCreate(target, TokenRef{.kind = "dropdown", .attrs = directiveAttrsParse("list=Status option=Done")}, {}, report), model::CoreError);
    const auto collision = atomTokenCreate(target, TokenRef{.kind = "dropdown", .attrs = directiveAttrsParse("list=list-1 option=Done")}, {}, report);
    REQUIRE(collision.create);
    CHECK(collision.create->definitionId == "list-1");
    CHECK(collision.create->optionId == "provider-done");
    AtomPutOptions options;
    options.createdDropdownIds = {{"new-list", "list-2"}, {"list-1", "list-2"}};
    const auto alias = atomTokenCreate(target, TokenRef{.kind = "dropdown", .attrs = directiveAttrsParse("list=new-list option=Done")}, options, report);
    REQUIRE(alias.create);
    CHECK(alias.create->definitionId == "list-2");
    CHECK(alias.create->optionId == "other-done");
    const auto precedence = atomTokenCreate(target, TokenRef{.kind = "dropdown", .attrs = directiveAttrsParse("list=list-1 option=Done")}, options, report);
    REQUIRE(precedence.create);
    CHECK(precedence.create->definitionId == "list-1");
    after.attrs.values.back().second = "Missing";
    CHECK_THROWS_AS(atomTokenKeep(existing, before, after, tab, {}), model::CoreError);
}

} // namespace gdocsmith::lens::test
