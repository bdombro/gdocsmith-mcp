#pragma once

#include "test.hpp"
#include "test_helpers.hpp"

using namespace gdocsmith::emulator;
using namespace gdocsmith::emulator::test_util;

TEST_SUITE("emulator::fake_google") {
    TEST_CASE("batchUpdate throws on a revision mismatch") {
        FakeGoogle fake;
        slimr::Json doc = make_simple_doc({"a"});
        fake.seed_document("d1", doc, 1);

        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(1)}}},
                    {"text", "x"}
                }}
            }
        };
        CHECK_THROWS(fake.batch_update("d1", reqs, "rev-99"));
    }

    TEST_CASE("batchUpdate response carries writeControl with new revision") {
        FakeGoogle fake;
        slimr::Json doc = make_simple_doc({"a"});
        fake.seed_document("d1", doc, 1);

        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(1)}}},
                    {"text", "x"}
                }}
            }
        };
        std::string res_str = fake.batch_update("d1", reqs, "rev-1");
        auto parsed = slimr::parse_json<slimr::Json>(res_str);
        REQUIRE(parsed.has_value());
        CHECK((*parsed)["documentId"].get<std::string>() == "d1");
        CHECK((*parsed)["writeControl"]["requiredRevisionId"].get<std::string>() == "rev-2");
        CHECK(fake.get_doc("d1").revision == 2);
    }

    TEST_CASE("externalEdit bumps revision") {
        FakeGoogle fake;
        slimr::Json doc = make_simple_doc({"a"});
        fake.seed_document("d1", doc, 5);
        CHECK(fake.revision_id(fake.get_doc("d1")) == "rev-5");

        fake.external_edit("d1", [](slimr::Json& d) {
            d["title"] = "New Title";
        });
        CHECK(fake.revision_id(fake.get_doc("d1")) == "rev-6");
        CHECK(fake.get_doc("d1").json["title"].get<std::string>() == "New Title");
    }

    TEST_CASE("createDocument and copyFile") {
        FakeGoogle fake;
        std::string doc_id = fake.create_document("My Doc");
        CHECK(!doc_id.empty());
        CHECK(fake.get_doc(doc_id).json["title"].get<std::string>() == "My Doc");

        std::string copy_id = fake.copy_file(doc_id, "Copy Doc");
        CHECK(copy_id != doc_id);
        CHECK(fake.get_doc(copy_id).json["title"].get<std::string>() == "Copy Doc");
    }

    TEST_CASE("beforeBatchUpdate hook fires before requests apply") {
        FakeGoogle fake;
        slimr::Json doc = make_simple_doc({"a"});
        fake.seed_document("d1", doc, 1);

        std::string seen_text;
        fake.before_batch_update = [&](const std::string& id) {
            seen_text = doc_plain_text(fake.get_doc(id).json);
        };

        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertText", slimr::Json::object_t{
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(1)}}},
                    {"text", "x"}
                }}
            }
        };
        fake.batch_update("d1", reqs, "rev-1");
        CHECK(seen_text == "a\n");
        CHECK(doc_plain_text(fake.get_doc("d1").json) == "xa\n");
    }
}
