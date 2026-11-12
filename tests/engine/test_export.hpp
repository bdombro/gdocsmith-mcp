#pragma once

#include "engine/export.hpp"
#include "test.hpp"

#include <filesystem>
#include <string>

namespace gdocsmith::engine::test {

TEST_CASE("export - documentId parsing") {
    CHECK_EQ(documentId("1234567890abcdef_-"), "1234567890abcdef_-");
    CHECK_EQ(documentId("https://docs.google.com/document/d/1234567890abcdef_-/edit"), "1234567890abcdef_-");
    CHECK_EQ(documentId("https://docs.google.com/document/d/doc_abc123"), "doc_abc123");

    CHECK_THROWS_AS(documentId("http://docs.google.com/document/d/doc"), model::CoreError);
    CHECK_THROWS_AS(documentId("https://google.com/document/d/doc"), model::CoreError);
    CHECK_THROWS_AS(documentId("https://docs.google.com/spreadsheets/d/sheet"), model::CoreError);
    CHECK_THROWS_AS(documentId("bad id with spaces"), model::CoreError);
}

TEST_CASE("export - commentReplies filtering") {
    DriveComment comment;
    comment.id = "c1";
    comment.content = "Top comment";

    DriveCommentReply r1;
    r1.id = "r1";
    r1.content = "First reply";
    r1.deleted = false;

    DriveCommentReply r2;
    r2.id = "r2";
    r2.content = "Deleted reply";
    r2.deleted = true;

    DriveCommentReply r3;
    r3.id = "r3";
    r3.content = "   \t\n  ";
    r3.deleted = false;

    comment.replies = {r1, r2, r3};

    auto filtered = commentReplies(comment);
    CHECK_EQ(filtered.size(), 1u);
    CHECK_EQ(filtered[0].id, "r1");
    CHECK_EQ(filtered[0].content, "First reply");
}

TEST_CASE("export - commentBlocksBuild ordering and anchors") {
    model::DocModel doc;
    doc.docId = "doc1";
    doc.revisionId = "rev1";
    doc.title = "Test Doc";

    model::TabModel tab1;
    tab1.tabId = "t.0";
    tab1.title = "Tab 1";
    model::ParagraphBlock p1;
    p1.inlines.push_back(model::Inline::from_text("First paragraph content\n"));
    tab1.blocks.push_back(model::Block(std::move(p1)));

    model::TabModel tab2;
    tab2.tabId = "t.1";
    tab2.title = "Tab 2";
    model::ParagraphBlock p2;
    p2.inlines.push_back(model::Inline::from_text("Second tab content\n"));
    tab2.blocks.push_back(model::Block(std::move(p2)));

    doc.tabs = {std::move(tab1), std::move(tab2)};

    DriveComment c_unresolved;
    c_unresolved.id = "c1";
    c_unresolved.content = "Unresolved comment";
    c_unresolved.resolved = false;
    c_unresolved.author = DriveCommentAuthor{.displayName = "Alice", .me = true};

    DriveComment c_resolved;
    c_resolved.id = "c2";
    c_resolved.content = "Resolved comment";
    c_resolved.resolved = true;
    c_resolved.author = DriveCommentAuthor{.displayName = "Bob", .me = false};

    std::vector<DriveComment> comments = {c_resolved, c_unresolved};

    // Without includeResolved
    auto blocks1 = commentBlocksBuild(doc, comments, false);
    CHECK_EQ(blocks1.size(), 1u);
    CHECK_EQ(blocks1[0].id, "c1");
    CHECK_EQ(blocks1[0].text, "Unresolved comment");
    CHECK_EQ(blocks1[0].author, std::optional<std::string>("Alice"));
    CHECK(blocks1[0].mine);

    // With includeResolved
    auto blocks2 = commentBlocksBuild(doc, comments, true);
    CHECK_EQ(blocks2.size(), 2u);
}

TEST_CASE("export - exportModel creates artifacts and writes markdown") {
    auto tmpDir = std::filesystem::temp_directory_path() / ("gdocsmith-export-test-" + detail::unique_tmp_suffix());
    std::filesystem::create_directories(tmpDir);

    model::DocModel doc;
    doc.docId = "doc_export_123";
    doc.revisionId = "r1";
    doc.title = "Export Test";

    model::TabModel tab;
    tab.tabId = "t.0";
    tab.title = "Overview";
    model::ParagraphBlock p;
    p.inlines.push_back(model::Inline::from_text("Hello export world\n"));
    tab.blocks.push_back(model::Block(std::move(p)));
    doc.tabs = {std::move(tab)};

    DriveComment c;
    c.id = "c1";
    c.content = "Looks good!";
    c.resolved = false;

    FileServices services;
    services.apiUser = "agent@example.com";
    services.root = tmpDir.string();

    auto written = exportModel(doc, {c}, services);

    CHECK_EQ(written.warnings.size(), 0u);
    CHECK(std::filesystem::exists(written.exportPath));
    CHECK(written.markdown.find("Hello export world") != std::string::npos);
    CHECK(written.markdown.find(":::comment{id=c1") != std::string::npos);
    CHECK(written.commentsLine.has_value());
    CHECK(*written.commentsLine > 1);

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

} // namespace gdocsmith::engine::test
