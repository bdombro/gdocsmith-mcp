#pragma once

#include "test.hpp"
#include "test_helpers.hpp"

using namespace gdocsmith::emulator;
using namespace gdocsmith::emulator::test_util;

TEST_SUITE("emulator::requests_emulate") {
    TEST_CASE("insertText inherits style from the previous character in the same paragraph") {
        slimr::Json doc = make_simple_doc({"ab"});
        // Give 'ab' bold style
        auto& tr = doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                       .get<slimr::Json::array_t>()[1]["paragraph"]["elements"].get<slimr::Json::array_t>()[0]["textRun"];
        tr["textStyle"] = slimr::Json::object_t{{"bold", true}};

        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(2)}}},
                    {"text", "X"}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        CHECK(doc_plain_text(res.json) == "aXb\n");

        auto p_elems = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                           .get<slimr::Json::array_t>()[1]["paragraph"]["elements"].get<slimr::Json::array_t>();
        CHECK(p_elems.size() == 1);
        CHECK(json_bool(p_elems[0]["textRun"]["textStyle"]["bold"]) == true);
    }

    TEST_CASE("insertText with an embedded newline splits paragraph") {
        slimr::Json doc = make_simple_doc({"Hello World"});
        auto& p = doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                      .get<slimr::Json::array_t>()[1]["paragraph"];
        p["paragraphStyle"] = slimr::Json::object_t{
            {"headingId", "h.orig"},
            {"namedStyleType", "HEADING_1"}
        };

        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(7)}}},
                    {"text", "\n"}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        CHECK(doc_plain_text(res.json) == "Hello \nWorld\n");

        auto content = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                           .get<slimr::Json::array_t>();
        // Section break + 2 paragraphs
        CHECK(content.size() == 3);
        CHECK(content[1]["paragraph"]["paragraphStyle"]["headingId"].get<std::string>() == "h.orig");
        CHECK(content[2]["paragraph"]["paragraphStyle"].contains("headingId"));
        CHECK(content[2]["paragraph"]["paragraphStyle"]["headingId"].get<std::string>() != "h.orig");
    }

    TEST_CASE("deleteContentRange merges paragraphs carrying original state") {
        slimr::Json doc = make_simple_doc({"AAA", "BBB"});
        auto& content = doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"].get<slimr::Json::array_t>();
        content[1]["paragraph"]["paragraphStyle"]["namedStyleType"] = "HEADING_1";
        content[2]["paragraph"]["paragraphStyle"]["namedStyleType"] = "HEADING_2";

        // "AAA\n" is 1..5, "BBB\n" is 5..9. Delete across boundary: 3..6
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"deleteContentRange", slimr::Json::object_t{
                    {"range", slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(3)},
                        {"endIndex", static_cast<int64_t>(6)}
                    }}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        CHECK(doc_plain_text(res.json) == "AABB\n");

        auto new_content = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"].get<slimr::Json::array_t>();
        CHECK(new_content.size() == 2); // Section break + 1 merged paragraph
        CHECK(new_content[1]["paragraph"]["paragraphStyle"]["namedStyleType"].get<std::string>() == "HEADING_1");
    }

    TEST_CASE("updateTextStyle applies fields") {
        slimr::Json doc = make_simple_doc({"ab"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"updateTextStyle", slimr::Json::object_t{
                    {"fields", "bold,underline"},
                    {"range", slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(1)},
                        {"endIndex", static_cast<int64_t>(3)}
                    }},
                    {"textStyle", slimr::Json::object_t{{"underline", true}}}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        auto p_elems = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                           .get<slimr::Json::array_t>()[1]["paragraph"]["elements"].get<slimr::Json::array_t>();
        CHECK(json_bool(p_elems[0]["textRun"]["textStyle"]["underline"]) == true);
    }

    TEST_CASE("createParagraphBullets and deleteParagraphBullets") {
        slimr::Json doc = make_simple_doc({"\t\titem", "z"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"createParagraphBullets", slimr::Json::object_t{
                    {"bulletPreset", "BULLET_DISC_CIRCLE_SQUARE"},
                    {"range", slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(1)},
                        {"endIndex", static_cast<int64_t>(8)}
                    }}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        auto p = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                     .get<slimr::Json::array_t>()[1]["paragraph"];
        CHECK(p.contains("bullet"));
        CHECK(p["bullet"]["nestingLevel"].as<int>() == 2);

        // Delete bullet
        std::vector<slimr::Json> del_reqs = {
            slimr::Json::object_t{
                {"deleteParagraphBullets", slimr::Json::object_t{
                    {"range", slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(1)},
                        {"endIndex", static_cast<int64_t>(6)}
                    }}
                }}
            }
        };
        auto res_del = requests_emulate(res.json, del_reqs);
        auto p_del = res_del.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                         .get<slimr::Json::array_t>()[1]["paragraph"];
        CHECK(!p_del.contains("bullet"));
        CHECK(p_del["paragraphStyle"]["indentStart"]["magnitude"].as<double>() == 72.0);
    }

    TEST_CASE("replaceAllText accurately replaces text occurrences") {
        slimr::Json doc = make_simple_doc({"hello world hello"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"replaceAllText", slimr::Json::object_t{
                    {"containsText", slimr::Json::object_t{
                        {"text", "hello"},
                        {"matchCase", true}
                    }},
                    {"replaceText", "hi"}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        CHECK(doc_plain_text(res.json) == "hi world hi\n");
        CHECK(res.replies.size() == 1);
        CHECK(res.replies[0]["replaceAllText"]["occurrencesChanged"].as<int>() == 2);
    }

    TEST_CASE("error code: DELETE_LAST_NEWLINE") {
        slimr::Json doc = make_simple_doc({"a"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"deleteContentRange", slimr::Json::object_t{
                    {"range", slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(2)},
                        {"endIndex", static_cast<int64_t>(3)}
                    }}
                }}
            }
        };
        CHECK_THROWS_AS(requests_emulate(doc, reqs), EmulatorError);
        try {
            requests_emulate(doc, reqs);
        } catch (const EmulatorError& e) {
            CHECK(e.code == EmulatorErrorCode::DELETE_LAST_NEWLINE);
        }
    }

    TEST_CASE("error code: RANGE_AT_SEGMENT_END") {
        slimr::Json doc = make_simple_doc({"a"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(3)}}},
                    {"text", "x"}
                }}
            }
        };
        try {
            requests_emulate(doc, reqs);
            CHECK(false);
        } catch (const EmulatorError& e) {
            CHECK(e.code == EmulatorErrorCode::RANGE_AT_SEGMENT_END);
        }
    }

    TEST_CASE("error code: TAB_REQUIRED") {
        slimr::Json doc = make_simple_doc({"a"});
        // add second tab
        doc["tabs"].get<slimr::Json::array_t>().push_back(doc["tabs"].get<slimr::Json::array_t>()[0]);
        doc["tabs"].get<slimr::Json::array_t>()[1]["tabProperties"]["tabId"] = "t.1";

        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(1)}}},
                    {"text", "x"}
                }}
            }
        };
        try {
            requests_emulate(doc, reqs);
            CHECK(false);
        } catch (const EmulatorError& e) {
            CHECK(e.code == EmulatorErrorCode::TAB_REQUIRED);
        }
    }

    TEST_CASE("error code: UNKNOWN_REQUEST on empty replaceAllText") {
        slimr::Json doc = make_simple_doc({"a"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"replaceAllText", slimr::Json::object_t{}}
            }
        };
        try {
            requests_emulate(doc, reqs);
            CHECK(false);
        } catch (const EmulatorError& e) {
            CHECK(e.code == EmulatorErrorCode::UNKNOWN_REQUEST);
        }
    }
}
