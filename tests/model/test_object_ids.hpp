#pragma once

#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/object_ids.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace gdocsmith::model::test {

TEST_CASE("objectIdsAssign - tails grow until unique across tabs; counters run across tabs") {
    using namespace gdocsmith::model;
    ParagraphSpec p1;
    p1.content.push_back(ParagraphAtomSpec{.type = "image"});
    p1.content.push_back(ParagraphAtomSpec{.type = "pageBreak"});

    ParagraphSpec p2;
    p2.content.push_back(ParagraphAtomSpec{.type = "person"});
    p2.content.push_back(ParagraphAtomSpec{.type = "horizontalRule"});
    p2.content.push_back(ParagraphAtomSpec{.type = "image"});

    DocSpec spec;
    spec.tabs.push_back(TabSpec{
        .tabId = "t.0",
        .title = "One",
        .blocks = {
            BlockSpec::make_paragraph(std::move(p1)),
            BlockSpec::make_section_break()
        }
    });
    spec.tabs.push_back(TabSpec{
        .tabId = "t.1",
        .title = "Two",
        .blocks = {
            BlockSpec::make_paragraph(std::move(p2)),
            BlockSpec::make_toc(2)
        }
    });

    auto json = docJsonBuild(spec);
    std::vector<std::string> ids = {"kix.aaa1234", "p.xtoc1", "kix.bbb1234"};
    size_t idIdx = 0;
    if (json.contains("tabs") && json["tabs"].holds<slimr::Json::array_t>()) {
        for (auto& tab : json["tabs"].get<slimr::Json::array_t>()) {
            if (tab.contains("documentTab") && tab["documentTab"].contains("body") &&
                tab["documentTab"]["body"].contains("content") &&
                tab["documentTab"]["body"]["content"].holds<slimr::Json::array_t>()) {
                for (auto& el : tab["documentTab"]["body"]["content"].get<slimr::Json::array_t>()) {
                    if (el.contains("paragraph") && el["paragraph"].contains("elements") &&
                        el["paragraph"]["elements"].holds<slimr::Json::array_t>()) {
                        for (auto& part : el["paragraph"]["elements"].get<slimr::Json::array_t>()) {
                            if (part.contains("inlineObjectElement") && idIdx < ids.size()) {
                                part["inlineObjectElement"]["inlineObjectId"] = ids[idIdx++];
                            }
                            if (part.contains("person") && idIdx < ids.size()) {
                                part["person"]["personId"] = ids[idIdx++];
                            }
                        }
                    }
                }
            }
        }
    }

    KeyAllocator keys;
    auto doc = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    REQUIRE(doc.tabs.size() == 2);

    auto sortedVals = [](const TabModel& tab) {
        std::vector<std::string> vals;
        if (tab.objectIds.has_value()) {
            for (const auto& [_, v] : *tab.objectIds) vals.push_back(v);
        }
        std::sort(vals.begin(), vals.end());
        return vals;
    };

    std::vector<std::string> expectedTab0 = {"a1234", "pb1", "s1", "s2"};
    CHECK(sortedVals(doc.tabs[0]) == expectedTab0);

    std::vector<std::string> expectedTab1 = {"b1234", "hr1", "s3", "toc1", "xtoc1"};
    CHECK(sortedVals(doc.tabs[1]) == expectedTab1);

    CHECK(doc.tabs[1].objectIds.has_value());
    CHECK((*doc.tabs[1].objectIds)[std::string(OPENING_SECTION_KEY)] == "s3");
}

} // namespace gdocsmith::model::test
