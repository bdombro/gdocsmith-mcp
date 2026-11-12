#pragma once

#include "engine/load.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

namespace gdocsmith::engine::test {

struct FakeClient {
    std::map<std::string, slimr::Json> docs;
    slimr::Json getDocument(const std::string& id) {
        return docs[id];
    }
};

TEST_CASE("load - docLoad returns a private copy and its model") {
    using namespace gdocsmith::engine;
    using namespace gdocsmith::model;

    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"hello world"}}),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"again world"}}),
                },
            },
            TabSpec{
                .tabId = "t.1",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"other tab"}}),
                },
            },
        },
    });

    FakeClient client;
    client.docs["d1"] = json;

    KeyAllocator keys;
    auto loaded = docLoad("d1", client, keys);

    REQUIRE(loaded.model.tabs.size() == 2);
    CHECK_EQ(loaded.model.tabs[0].tabId, "t.0");
    CHECK_EQ(loaded.model.tabs[1].tabId, "t.1");

    // Mutating loaded.json does not affect client
    loaded.json["title"] = "changed";
    CHECK_NE(client.docs["d1"]["title"].get<std::string>(), "changed");
}

TEST_CASE("load - quotes map to document ranges, across paragraphs and tabs; duplicates are ambiguous") {
    using namespace gdocsmith::engine;
    using namespace gdocsmith::model;

    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"hello world"}}),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"again world"}}),
                },
            },
            TabSpec{
                .tabId = "t.1",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"other tab"}}),
                },
            },
        },
    });

    FakeClient client;
    client.docs["d1"] = json;

    KeyAllocator keys;
    auto loaded = docLoad("d1", client, keys);

    std::vector<DriveComment> comments = {
        DriveComment{.id = "c1", .quotedFileContent = DriveCommentQuoted{.value = "world"}},
        DriveComment{.id = "c2", .quotedFileContent = DriveCommentQuoted{.value = "world\nagain"}},
        DriveComment{.id = "c3", .quotedFileContent = DriveCommentQuoted{.value = "other"}},
    };

    auto anchors = commentAnchorsMatch(loaded.model, comments);
    REQUIRE(anchors.size() == 3);

    // c1: world appears twice
    CHECK(anchors[0].ambiguous);
    REQUIRE(anchors[0].occurrences.size() == 2);
    CHECK_EQ(anchors[0].occurrences[0].tabId, "t.0");
    CHECK_EQ(anchors[0].occurrences[0].range.start, 7);
    CHECK_EQ(anchors[0].occurrences[0].range.end, 12);
    CHECK_EQ(anchors[0].occurrences[1].tabId, "t.0");
    CHECK_EQ(anchors[0].occurrences[1].range.start, 19);
    CHECK_EQ(anchors[0].occurrences[1].range.end, 24);

    // c2: cross paragraph "world\nagain"
    CHECK(!anchors[1].ambiguous);
    REQUIRE(anchors[1].occurrences.size() == 1);
    CHECK_EQ(anchors[1].occurrences[0].tabId, "t.0");
    CHECK_EQ(anchors[1].occurrences[0].range.start, 7);
    CHECK_EQ(anchors[1].occurrences[0].range.end, 18);

    // c3: "other" in tab t.1
    CHECK(!anchors[2].ambiguous);
    REQUIRE(anchors[2].occurrences.size() == 1);
    CHECK_EQ(anchors[2].occurrences[0].tabId, "t.1");
    CHECK_EQ(anchors[2].occurrences[0].range.start, 1);
    CHECK_EQ(anchors[2].occurrences[0].range.end, 6);
}

TEST_CASE("load - exportTextParse reads a list with two-letter labels and trailing text") {
    using namespace gdocsmith::engine;

    std::string text = "* Ship it.[a]\n* Later[aa]\n* [a]@ada to check\n[aa]Second\nnote line two\n\n\nhttps://example.com/after";
    auto parts = exportTextParse(text);

    CHECK_EQ(parts.entries.size(), 2);
    CHECK_EQ(parts.entries["a"], "@ada to check");
    CHECK_EQ(parts.entries["aa"], "Second");

    REQUIRE(parts.markers.size() == 2);
    CHECK_EQ(parts.markers[0].at, 10);
    CHECK_EQ(parts.markers[0].label, "a");
    CHECK_EQ(parts.markers[1].at, 18);
    CHECK_EQ(parts.markers[1].label, "aa");

    CHECK_EQ(parts.body, "* Ship it.\n* Later");
}

TEST_CASE("load - document with no list keeps its text and has no markers") {
    using namespace gdocsmith::engine;

    std::string text = "Plain [a] text\n";
    auto parts = exportTextParse(text);

    CHECK_EQ(parts.markers.size(), 0);
    CHECK_EQ(parts.body, "Plain [a] text\n");
}

} // namespace gdocsmith::engine::test
