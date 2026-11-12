#pragma once

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

} // namespace gdocsmith::lens::test
