#pragma once

#include "test.hpp"
#include "test_helpers.hpp"

namespace gdocsmith::emulator::test_util {

inline slimr::Json dropdown_update_fixture() {
    auto doc = make_simple_doc({""});
    slimr::Json::object_t definitions;
    for (const auto &id : {"def.target", "def.unrelated"}) {
        definitions[id] = slimr::Json::object_t{
            {"dropdownDefinitionId", id},
            {"dropdownDefinitionProperties", slimr::Json::object_t{
                                                 {"title", id},
                                                 {"options", slimr::Json::array_t{
                                                                 slimr::Json::object_t{{"optionId", "opt.a"}, {"displayValue", "Alpha"}},
                                                                 slimr::Json::object_t{{"optionId", "opt.b"}, {"displayValue", "Beta"}}}}}}};
    }
    doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["dropdownDefinitions"] = definitions;
    std::vector<slimr::Json> requests;
    int index = 1;
    for (const auto &[definition, option] :
         std::vector<std::pair<std::string, std::string>>{
             {"def.target", "opt.a"}, {"def.target", "opt.b"}, {"def.unrelated", "opt.a"}}) {
        requests.push_back(slimr::Json::object_t{{"insertDropdown", slimr::Json::object_t{
                                                                        {"location", slimr::Json::object_t{{"index", index++}}},
                                                                        {"dropdownDefinitionId", definition},
                                                                        {"selectedOptionId", option}}}});
    }
    return requests_emulate(doc, requests).json;
}

inline const slimr::Json &dropdown_update_tab(const slimr::Json &doc) {
    return doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"];
}

inline std::vector<slimr::Json> dropdown_update_chips(const slimr::Json &doc) {
    std::vector<slimr::Json> chips;
    for (const auto &element : dropdown_update_tab(doc)["body"]["content"]
                                   .get<slimr::Json::array_t>()[1]["paragraph"]["elements"]
                                   .get<slimr::Json::array_t>()) {
        if (element.contains("dropdown"))
            chips.push_back(element["dropdown"]);
    }
    return chips;
}

inline slimr::Json dropdown_options_request(const slimr::Json &options) {
    return slimr::Json::object_t{{"updateDropdownDefinitionProperties", slimr::Json::object_t{
                                                                            {"tabId", "t.0"}, {"dropdownDefinitionId", "def.target"}, {"fields", "options"}, {"dropdownDefinitionProperties", slimr::Json::object_t{{"options", options}}}}}};
}

inline slimr::Json &dropdown_update_first_chip(slimr::Json &doc) {
    return doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"].get<slimr::Json::array_t>()[1]["paragraph"]["elements"].get<slimr::Json::array_t>()[0]["dropdown"];
}

} // namespace gdocsmith::emulator::test_util

TEST_SUITE("emulator::dropdown_definition_update") {
    TEST_CASE("no-request dropdown roundtrip preserves absent explicit empty and populated styles") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        for (const int shape : {0, 1, 2}) {
            auto doc = dropdown_update_fixture();
            auto &chip = dropdown_update_first_chip(doc);
            chip.get<slimr::Json::object_t>().erase("textStyle");
            if (shape == 1)
                chip["textStyle"] = slimr::Json::object_t{};
            if (shape == 2)
                chip["textStyle"] = slimr::Json::object_t{{"bold", true}, {"italic", true}};
            const auto before = dropdown_update_chips(doc);
            const auto result = requests_emulate(doc, {});
            CHECK(dropdown_update_chips(result.json) == before);
            CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"] ==
                  dropdown_update_tab(doc)["dropdownDefinitions"]);
        }
    }

    TEST_CASE("definition rename preserves absent explicit empty and populated chip styles") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        for (const int shape : {0, 1, 2}) {
            auto doc = dropdown_update_fixture();
            auto &chip = dropdown_update_first_chip(doc);
            chip.get<slimr::Json::object_t>().erase("textStyle");
            if (shape == 1)
                chip["textStyle"] = slimr::Json::object_t{};
            if (shape == 2)
                chip["textStyle"] = slimr::Json::object_t{{"bold", true}, {"italic", true}};
            auto expected = dropdown_update_chips(doc);
            expected[0]["dropdownProperties"]["displayValue"] = "Renamed";
            auto options = dropdown_update_tab(doc)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"];
            options.get<slimr::Json::array_t>()[0]["displayValue"] = "Renamed";
            const auto result = requests_emulate(doc, {dropdown_options_request(options)});
            CHECK(dropdown_update_chips(result.json) == expected);
        }
    }

    TEST_CASE("explicit atom style clearing materializes empty style without blanket empty stripping") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        for (const bool initially_absent : {true, false}) {
            auto doc = dropdown_update_fixture();
            auto &chip = dropdown_update_first_chip(doc);
            chip.get<slimr::Json::object_t>().erase("textStyle");
            if (!initially_absent)
                chip["textStyle"] = slimr::Json::object_t{{"bold", true}};
            auto expected = dropdown_update_chips(doc);
            expected[0]["textStyle"] = slimr::Json::object_t{};
            const auto result = requests_emulate(doc, {slimr::Json::object_t{{"updateTextStyle", slimr::Json::object_t{
                                                                                                     {"range", slimr::Json::object_t{{"tabId", "t.0"}, {"startIndex", 1}, {"endIndex", 2}}},
                                                                                                     {"fields", "bold"},
                                                                                                     {"textStyle", slimr::Json::object_t{}}}}}});
            CHECK(dropdown_update_chips(result.json) == expected);
            CHECK(dropdown_update_chips(requests_emulate(result.json, {}).json) == expected);
        }
    }

    TEST_CASE("renamed selected option relabels only matching chips and preserves provider IDs") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        const auto doc = dropdown_update_fixture();
        const auto before = dropdown_update_chips(doc);
        auto options = dropdown_update_tab(doc)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"];
        options.get<slimr::Json::array_t>()[0]["displayValue"] = "Renamed";
        const auto result = requests_emulate(doc, {dropdown_options_request(options)});
        const auto chips = dropdown_update_chips(result.json);
        REQUIRE(chips.size() == 3);
        auto expected = before[0];
        expected["dropdownProperties"]["displayValue"] = "Renamed";
        CHECK(chips[0] == expected);
        CHECK(chips[1] == before[1]);
        CHECK(chips[2] == before[2]);
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.target"]["dropdownDefinitionId"] ==
              slimr::Json("def.target"));
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"] == options);
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.unrelated"] ==
              dropdown_update_tab(doc)["dropdownDefinitions"]["def.unrelated"]);
    }

    TEST_CASE("recolor updates options without rewriting any chip or unrelated definition") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        const auto doc = dropdown_update_fixture();
        auto options = dropdown_update_tab(doc)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"];
        options.get<slimr::Json::array_t>()[0]["textStyle"] = slimr::Json::object_t{
            {"foregroundColor", slimr::Json::object_t{
                                    {"color", slimr::Json::object_t{{"rgbColor", slimr::Json::object_t{{"green", 1.0}}}}}}}};
        const auto result = requests_emulate(doc, {dropdown_options_request(options)});
        CHECK(dropdown_update_chips(result.json) == dropdown_update_chips(doc));
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"] == options);
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.unrelated"] ==
              dropdown_update_tab(doc)["dropdownDefinitions"]["def.unrelated"]);
    }

    TEST_CASE("title-only mask does not relabel chips from an unselected options patch") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        const auto doc = dropdown_update_fixture();
        auto options = dropdown_update_tab(doc)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"];
        options.get<slimr::Json::array_t>()[0]["displayValue"] = "Ignored";
        auto request = dropdown_options_request(options);
        request["updateDropdownDefinitionProperties"]["fields"] = "title";
        request["updateDropdownDefinitionProperties"]["dropdownDefinitionProperties"]["title"] = "New title";
        const auto result = requests_emulate(doc, {request});
        CHECK(dropdown_update_chips(result.json) == dropdown_update_chips(doc));
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["title"] ==
              slimr::Json("New title"));
        CHECK(dropdown_update_tab(result.json)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"] ==
              dropdown_update_tab(doc)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"]);
    }

    TEST_CASE("replacement cannot remove a selected option even when option count is valid") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        const auto doc = dropdown_update_fixture();
        auto options = dropdown_update_tab(doc)["dropdownDefinitions"]["def.target"]["dropdownDefinitionProperties"]["options"];
        options.get<slimr::Json::array_t>()[0]["optionId"] = "opt.replacement";
        CHECK_THROWS_AS(requests_emulate(doc, {dropdown_options_request(options)}), EmulatorError);
    }

    TEST_CASE("replacement option count must remain between two and fifty") {
        using namespace gdocsmith::emulator;
        using namespace gdocsmith::emulator::test_util;
        const auto doc = dropdown_update_fixture();
        for (const size_t count : {size_t{0}, size_t{1}, size_t{51}}) {
            slimr::Json::array_t options(count, slimr::Json::object_t{
                                                    {"optionId", "opt.a"}, {"displayValue", "Alpha"}});
            CHECK_THROWS_AS(requests_emulate(doc, {dropdown_options_request(options)}), EmulatorError);
        }
    }
}
