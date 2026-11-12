#pragma once

#include "test.hpp"
#include "test_helpers.hpp"

using namespace gdocsmith::emulator;
using namespace gdocsmith::emulator::test_util;

TEST_SUITE("emulator::emulate_structure") {
    TEST_CASE("insertPageBreak occupies 2 indices (atom + newline split)") {
        slimr::Json doc = make_simple_doc({"abcdef"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertPageBreak", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(4)}}}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        auto content = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                           .get<slimr::Json::array_t>();
        // Section break + 2 paragraphs
        CHECK(content.size() == 3);
        const auto& p1_elems = content[1]["paragraph"]["elements"].get<slimr::Json::array_t>();
        CHECK(p1_elems.size() == 3);
        CHECK(p1_elems[0]["textRun"]["content"].get<std::string>() == "abc");
        CHECK(p1_elems[1].contains("pageBreak"));
        CHECK(p1_elems[2]["textRun"]["content"].get<std::string>() == "\n");

        const auto& p2_elems = content[2]["paragraph"]["elements"].get<slimr::Json::array_t>();
        CHECK(p2_elems[0]["textRun"]["content"].get<std::string>() == "def\n");
    }

    TEST_CASE("insertSectionBreak occupies 2 indices (newline split + break marker)") {
        slimr::Json doc = make_simple_doc({"abcdef"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertSectionBreak", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(4)}}},
                    {"sectionType", "NEXT_PAGE"}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        auto content = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                           .get<slimr::Json::array_t>();
        // Leading sectionBreak, p1, sectionBreak, p2
        CHECK(content.size() == 4);
        CHECK(content[0].contains("sectionBreak"));
        CHECK(content[1].contains("paragraph"));
        CHECK(content[2].contains("sectionBreak"));
        CHECK(content[3].contains("paragraph"));
        CHECK(content[2]["sectionBreak"]["sectionStyle"]["sectionType"].get<std::string>() == "NEXT_PAGE");
    }

    TEST_CASE("insertPerson, insertDate, insertRichLink insert single-index chips") {
        slimr::Json doc = make_simple_doc({"a"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertPerson", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(2)}}},
                    {"personProperties", slimr::Json::object_t{{"email", "a@example.com"}}}
                }}
            },
            slimr::Json::object_t{
                {"insertDate", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(3)}}},
                    {"dateElementProperties", slimr::Json::object_t{{"timestamp", "2026-01-15T00:00:00Z"}}}
                }}
            },
            slimr::Json::object_t{
                {"insertRichLink", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(4)}}},
                    {"richLinkProperties", slimr::Json::object_t{{"uri", "https://example.com"}}}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        auto p_elems = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                           .get<slimr::Json::array_t>()[1]["paragraph"]["elements"].get<slimr::Json::array_t>();
        CHECK(p_elems.size() == 5); // "a", person, date, richLink, "\n"
        CHECK(p_elems[1].contains("person"));
        CHECK(p_elems[2].contains("dateElement"));
        CHECK(p_elems[3].contains("richLink"));
    }

    TEST_CASE("addDocumentTab creates a tab; deleteTab removes it") {
        slimr::Json doc = make_simple_doc({"a"});
        std::vector<slimr::Json> add_reqs = {
            slimr::Json::object_t{
                {"addDocumentTab", slimr::Json::object_t{
                    {"tabProperties", slimr::Json::object_t{
                        {"parentTabId", "t.0"},
                        {"title", "New Tab"}
                    }}
                }}
            }
        };
        auto res = requests_emulate(doc, add_reqs);
        const auto& roots = res.json["tabs"].get<slimr::Json::array_t>();
        CHECK(roots.size() == 1);
        CHECK(roots[0].contains("childTabs"));
        const auto& children = roots[0]["childTabs"].get<slimr::Json::array_t>();
        CHECK(children.size() == 1);
        CHECK(children[0]["tabProperties"]["title"].get<std::string>() == "New Tab");
        std::string new_tab_id = children[0]["tabProperties"]["tabId"].get<std::string>();

        std::vector<slimr::Json> del_reqs = {
            slimr::Json::object_t{
                {"deleteTab", slimr::Json::object_t{{"tabId", new_tab_id}}}
            }
        };
        auto res_del = requests_emulate(res.json, del_reqs);
        CHECK(res_del.json["tabs"].get<slimr::Json::array_t>().size() == 1);
    }

    TEST_CASE("updateDocumentStyle applies dotted field paths") {
        slimr::Json doc = make_simple_doc({"a"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"updateDocumentStyle", slimr::Json::object_t{
                    {"documentStyle", slimr::Json::object_t{
                        {"pageSize", slimr::Json::object_t{
                            {"width", slimr::Json::object_t{{"magnitude", 612.0}, {"unit", "PT"}}}
                        }}
                    }},
                    {"fields", "pageSize.width"}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        const auto& ds = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["documentStyle"];
        CHECK(ds["pageSize"]["width"]["magnitude"].as<double>() == 612.0);
    }
}
