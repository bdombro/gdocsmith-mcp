#pragma once

#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <set>
#include <string>

namespace gdocsmith::model::test {

inline DocModel parseDoc(const JsonObject& json) {
    KeyAllocator keys;
    return docModelParse(json, DocModelParseOptions{.docId = "d1", .keys = keys});
}

TEST_CASE("docModelParse - merges equal-style runs") {
    using namespace gdocsmith::model;
    JsonObject boldStyle = slimr::Json::object_t{{"bold", true}};
    ParagraphSpec p;
    p.content.push_back(ParagraphTextSpec{.text = "Hello, ", .style = boldStyle});
    p.content.push_back(ParagraphTextSpec{.text = "world", .style = boldStyle});

    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_paragraph(std::move(p))}});
    auto doc = parseDoc(docJsonBuild(spec));
    const auto& pBlock = doc.tabs[0].blocks[0].as_paragraph();
    CHECK(pBlock.inlines.size() == 1);
    CHECK(pBlock.inlines[0].text == "Hello, world");
    CHECK(pBlock.inlines[0].is_text());
}

TEST_CASE("docModelParse - atoms keep JSON lengths (equation len 3)") {
    using namespace gdocsmith::model;
    ParagraphSpec p;
    p.content.push_back(ParagraphAtomSpec{.type = "equation", .atomLen = 3});
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_paragraph(std::move(p))}});
    auto doc = parseDoc(docJsonBuild(spec));
    const auto& pBlock = doc.tabs[0].blocks[0].as_paragraph();
    REQUIRE(pBlock.inlines.size() == 1);
    CHECK(pBlock.inlines[0].is_atom());
    CHECK(pBlock.inlines[0].length == 3);
    CHECK(pBlock.inlines[0].type == "equation");
}

TEST_CASE("docModelParse - richLink title adds no text") {
    using namespace gdocsmith::model;
    ParagraphSpec p;
    p.content.push_back(ParagraphAtomSpec{.type = "richLink"});
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_paragraph(std::move(p))}});
    auto doc = parseDoc(docJsonBuild(spec));
    const auto& pBlock = doc.tabs[0].blocks[0].as_paragraph();
    REQUIRE(pBlock.inlines.size() == 1);
    CHECK(pBlock.inlines[0].is_atom());
    CHECK(pBlock.inlines[0].length == 1);
    CHECK(pBlock.inlines[0].type == "richLink");
}

TEST_CASE("docModelParse - nestingLevel defaults 0") {
    using namespace gdocsmith::model;
    ParagraphSpec p;
    p.content.push_back("item");
    p.bullet = ParagraphSpec::Bullet{.listId = "kix.list1", .nestingLevel = 0};
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_paragraph(std::move(p))}});
    auto doc = parseDoc(docJsonBuild(spec));
    const auto& pBlock = doc.tabs[0].blocks[0].as_paragraph();
    REQUIRE(pBlock.bullet.has_value());
    CHECK(pBlock.bullet->listId == "kix.list1");
    CHECK(pBlock.bullet->nestingLevel == 0);
}

TEST_CASE("docModelParse - headingId lifted out of style") {
    using namespace gdocsmith::model;
    ParagraphSpec p;
    p.content.push_back("Title");
    p.headingId = "h.abc";
    p.style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_paragraph(std::move(p))}});
    auto doc = parseDoc(docJsonBuild(spec));
    const auto& pBlock = doc.tabs[0].blocks[0].as_paragraph();
    CHECK(pBlock.headingId == "h.abc");
    CHECK(!pBlock.style.contains("headingId"));
    CHECK(pBlock.style.contains("namedStyleType"));
    CHECK(pBlock.style["namedStyleType"].get<std::string>() == "HEADING_1");
}

TEST_CASE("docModelParse - suggestions mark protected (paragraph, table)") {
    using namespace gdocsmith::model;
    ParagraphSpec p;
    p.content.push_back(ParagraphTextSpec{.text = "new text", .sugIns = std::vector<std::string>{"s1"}});

    TableBlockSpec tbs;
    ParagraphSpec cellP;
    cellP.content.push_back(ParagraphTextSpec{.text = "old", .sugDel = std::vector<std::string>{"s2"}});
    tbs.cells = {{{BlockSpec::make_paragraph(std::move(cellP))}}};

    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {
        BlockSpec::make_paragraph(std::move(p)),
        BlockSpec::make_table(std::move(tbs))
    }});
    auto doc = parseDoc(docJsonBuild(spec));
    CHECK(doc.tabs[0].blocks[0].as_paragraph().is_protected);
    CHECK(doc.tabs[0].blocks[1].as_table().is_protected);
}

TEST_CASE("docModelParse - table keys and origins") {
    using namespace gdocsmith::model;
    TableBlockSpec tbs;
    std::vector<std::vector<BlockSpec>> row;
    row.push_back({BlockSpec::make_paragraph(ParagraphSpec{.content = {"a"}})});
    row.push_back({BlockSpec::make_paragraph(ParagraphSpec{.content = {"b"}})});
    tbs.cells.push_back(std::move(row));

    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_table(std::move(tbs))}});
    auto doc = parseDoc(docJsonBuild(spec));
    const auto& table = doc.tabs[0].blocks[0].as_table();
    CHECK(table.origin.has_value());
    std::set<std::string> keys;
    keys.insert(table.key);
    for (const auto& c : table.columns) keys.insert(c.key);
    for (const auto& r : table.rows) {
        keys.insert(r.key);
        for (const auto& cell : r.cells) keys.insert(cell.key);
    }
    CHECK(keys.size() == 1 + table.columns.size() + table.rows.size() + table.rows[0].cells.size());
    for (const auto& r : table.rows) {
        CHECK(r.origin.has_value());
        for (const auto& cell : r.cells) {
            CHECK(cell.origin.has_value());
        }
    }
}

TEST_CASE("docModelParse - TOC is opaque") {
    using namespace gdocsmith::model;
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_toc(5)}});
    auto doc = parseDoc(docJsonBuild(spec));
    CHECK(doc.tabs[0].blocks[0].is_toc());
    CHECK(doc.tabs[0].blocks[0].as_toc().length == 5);
}

TEST_CASE("docModelParse - leading section break is not a block; a mid-body one is") {
    using namespace gdocsmith::model;
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {"a"}}),
        BlockSpec::make_section_break()
    }});
    auto doc = parseDoc(docJsonBuild(spec));
    CHECK(doc.tabs[0].blocks.size() == 2);
    CHECK(doc.tabs[0].blocks[1].is_section_break());
    CHECK(doc.tabs[0].leadingSectionStyle.data.index() != 0); // valid object
}

TEST_CASE("docModelParse - tabs DFS with parentTabId") {
    using namespace gdocsmith::model;
    JsonObject json = slimr::Json::object_t{
        {"documentId", "d1"},
        {"revisionId", "rev-1"},
        {"title", "Doc"},
        {"tabs", slimr::Json::array_t{
            slimr::Json::object_t{
                {"documentTab", slimr::Json::object_t{
                    {"body", slimr::Json::object_t{
                        {"content", slimr::Json::array_t{
                            slimr::Json::object_t{{"startIndex", static_cast<int64_t>(0)}, {"endIndex", static_cast<int64_t>(1)}, {"sectionBreak", slimr::Json::object_t{{"sectionStyle", slimr::Json::object_t{}}}}}
                        }}
                    }}
                }},
                {"tabProperties", slimr::Json::object_t{{"tabId", "t.0"}, {"title", "Root"}}}
            },
            slimr::Json::object_t{
                {"childTabs", slimr::Json::array_t{
                    slimr::Json::object_t{
                        {"documentTab", slimr::Json::object_t{
                            {"body", slimr::Json::object_t{
                                {"content", slimr::Json::array_t{
                                    slimr::Json::object_t{{"startIndex", static_cast<int64_t>(0)}, {"endIndex", static_cast<int64_t>(1)}, {"sectionBreak", slimr::Json::object_t{{"sectionStyle", slimr::Json::object_t{}}}}}
                                }}
                            }}
                        }},
                        {"tabProperties", slimr::Json::object_t{{"parentTabId", "t.1"}, {"tabId", "t.2"}, {"title", "Child"}}}
                    }
                }},
                {"documentTab", slimr::Json::object_t{
                    {"body", slimr::Json::object_t{
                        {"content", slimr::Json::array_t{
                            slimr::Json::object_t{{"startIndex", static_cast<int64_t>(0)}, {"endIndex", static_cast<int64_t>(1)}, {"sectionBreak", slimr::Json::object_t{{"sectionStyle", slimr::Json::object_t{}}}}}
                        }}
                    }}
                }},
                {"tabProperties", slimr::Json::object_t{{"tabId", "t.1"}, {"title", "Parent"}}}
            }
        }}
    };
    auto doc = parseDoc(json);
    REQUIRE(doc.tabs.size() == 3);
    CHECK(doc.tabs[0].tabId == "t.0");
    CHECK(doc.tabs[0].parentTabId == std::nullopt);
    CHECK(doc.tabs[1].tabId == "t.1");
    CHECK(doc.tabs[1].parentTabId == std::nullopt);
    CHECK(doc.tabs[2].tabId == "t.2");
    CHECK(doc.tabs[2].parentTabId == "t.1");
}

TEST_CASE("docModelParse - legacy doc synthesizes t.0") {
    using namespace gdocsmith::model;
    JsonObject json = slimr::Json::object_t{
        {"documentId", "d1"},
        {"title", "Legacy"},
        {"body", slimr::Json::object_t{
            {"content", slimr::Json::array_t{
                slimr::Json::object_t{{"startIndex", static_cast<int64_t>(0)}, {"endIndex", static_cast<int64_t>(1)}, {"sectionBreak", slimr::Json::object_t{{"sectionStyle", slimr::Json::object_t{}}}}}
            }}
        }}
    };
    auto doc = parseDoc(json);
    REQUIRE(doc.tabs.size() == 1);
    CHECK(doc.tabs[0].tabId == "t.0");
}

TEST_CASE("docModelParse - throws on non-newline tail") {
    using namespace gdocsmith::model;
    JsonObject json = slimr::Json::object_t{
        {"documentId", "d1"},
        {"title", "Bad"},
        {"body", slimr::Json::object_t{
            {"content", slimr::Json::array_t{
                slimr::Json::object_t{{"startIndex", static_cast<int64_t>(0)}, {"endIndex", static_cast<int64_t>(1)}, {"sectionBreak", slimr::Json::object_t{{"sectionStyle", slimr::Json::object_t{}}}}},
                slimr::Json::object_t{
                    {"startIndex", static_cast<int64_t>(1)},
                    {"endIndex", static_cast<int64_t>(6)},
                    {"paragraph", slimr::Json::object_t{
                        {"elements", slimr::Json::array_t{
                            slimr::Json::object_t{
                                {"startIndex", static_cast<int64_t>(1)},
                                {"endIndex", static_cast<int64_t>(6)},
                                {"textRun", slimr::Json::object_t{{"content", "hello"}, {"textStyle", slimr::Json::object_t{}}}}
                            }
                        }}
                    }}
                }
            }}
        }}
    };
    CHECK_THROWS_AS(parseDoc(json), ParseError);
}

} // namespace gdocsmith::model::test
