#pragma once

#include "lens/links.hpp"
#include "test.hpp"

#include <optional>
#include <string>

namespace gdocsmith::lens::test {

TEST_CASE("urlEncode keeps unreserved characters and encodes the rest") {
    CHECK(urlEncode("abc-._~") == "abc-.%5F~");
    CHECK(urlEncode("AZaz09-.") == "AZaz09-.");
    CHECK(urlEncode("a b") == "a%20b");
    CHECK(urlEncode("a_b") == "a%5Fb");
    CHECK(urlEncode("hello!") == "hello%21");
    CHECK(urlEncode("a+b") == "a%2Bb");
    CHECK(urlEncode("") == "");
}

TEST_CASE("headingSlug lowercases, collapses whitespace and url-encodes") {
    CHECK(headingSlug("Hello World") == "hello-world");
    CHECK(headingSlug("A B  C") == "a-b-c");
    CHECK(headingSlug("Hello & Goodbye") == "hello-%26-goodbye");
    CHECK(headingSlug("  padded  ") == "padded");
    CHECK(headingSlug("") == "");
    CHECK(headingSlug("   ") == "");
    CHECK(headingSlug(" Launch! (Now) ") == "launch!-(now)");
#ifdef __APPLE__
    CHECK(headingSlug(" CAFE\u0301\u00A0Launch ") == "caf%C3%A9-launch");
#endif
}

TEST_CASE("documentLinkParse recognizes gdoc: and # hrefs as pending links") {
    auto gdoc = documentLinkParse("gdoc:abc123", "t.0");
    REQUIRE(gdoc.has_value());
    CHECK(gdoc->kind == std::string("pending"));
    CHECK(gdoc->tabId == std::optional<std::string>("t.0"));
    CHECK(gdoc->key == std::optional<std::string>("file-link:gdoc:abc123"));
    CHECK(!gdoc->url.has_value());
    CHECK(!gdoc->headingId.has_value());

    auto hash = documentLinkParse("#heading-1", "t.1");
    REQUIRE(hash.has_value());
    CHECK(hash->kind == std::string("pending"));
    CHECK(hash->tabId == std::optional<std::string>("t.1"));
    CHECK(hash->key == std::optional<std::string>("file-link:#heading-1"));

    auto bare = documentLinkParse("#", "t.0");
    REQUIRE(bare.has_value());
    CHECK(bare->key == std::optional<std::string>("file-link:#"));
}

TEST_CASE("documentLinkParse rejects bookmarks and external hrefs") {
    CHECK(!documentLinkParse("#bookmark=x", "t.0").has_value());
    CHECK(!documentLinkParse("https://example.com", "t.0").has_value());
    CHECK(!documentLinkParse("plain", "t.0").has_value());
    CHECK(!documentLinkParse("", "t.0").has_value());
}

TEST_CASE("documentLinkParse returns an equal LinkTarget") {
    LinkTarget expected{
        .kind = "pending",
        .tabId = "t.2",
        .key = "file-link:gdoc:x",
    };
    CHECK(documentLinkParse("gdoc:x", "t.2") == std::optional<LinkTarget>(expected));
}

TEST_CASE("source document links resolve native IDs new heading keys tabs and foreign document URLs") {
    model::DocModel doc;
    doc.docId = "local";
    model::TabModel first;
    first.tabId = "tab-main";
    first.title = "Main";
    model::ParagraphBlock oldHeading;
    oldHeading.key = "old";
    oldHeading.headingId = "provider-heading";
    oldHeading.style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    oldHeading.inlines.push_back(model::Inline::from_text("Goals"));
    first.blocks.emplace_back(oldHeading);
    model::ParagraphBlock newHeading = oldHeading;
    newHeading.key = "new";
    newHeading.headingId.reset();
    newHeading.inlines = {model::Inline::from_text("New heading")};
    first.blocks.emplace_back(newHeading);
    model::ParagraphBlock links;
    links.key = "links";
    for (const auto* href : {"#goals", "#new-heading", "#other__", "#other__remote", "gdoc:foreign#remote"}) {
        links.inlines.push_back(model::Inline::from_text("x", slimr::Json::object_t{
            {"link", documentLinkStyle(href, first.tabId)}
        }));
    }
    first.blocks.emplace_back(links);
    model::TabModel other;
    other.tabId = "tab-other";
    other.title = "Other";
    oldHeading.key = "remote";
    oldHeading.headingId = "provider-remote";
    oldHeading.inlines = {model::Inline::from_text("Remote")};
    other.blocks.emplace_back(oldHeading);
    doc.tabs = {first, other};
    model::DocModel foreign;
    foreign.docId = "foreign";
    foreign.tabs = {other};
    documentLinksResolve(doc, {foreign});
    const auto& resolved = doc.tabs[0].blocks.back().as_paragraph().inlines;
    CHECK(resolved[0].style["link"]["heading"]["id"] == slimr::Json("provider-heading"));
    CHECK(resolved[1].style["link"]["heading"]["key"] == slimr::Json("new"));
    CHECK(resolved[2].style["link"]["tabId"] == slimr::Json("tab-other"));
    CHECK(resolved[3].style["link"]["heading"]["id"] == slimr::Json("provider-remote"));
    CHECK(resolved[4].style["link"]["url"] == slimr::Json("https://docs.google.com/document/d/foreign/edit?tab=tab-other#heading=provider-remote"));
    CHECK(documentLinkHref(LinkTarget{.kind = "heading", .headingId = "provider-remote", .tabId = "tab-other"}, doc, doc.tabs[0]) == "#other__remote");
    CHECK(documentLinkHref(LinkTarget{.kind = "url", .url = "https://docs.google.com/document/d/local/edit?tab=tab-main#heading=provider-heading"}, doc, doc.tabs[0]) == "#goals");
    CHECK(documentLinkHref(LinkTarget{.kind = "url", .url = "https://docs.google.com/document/d/local/edit?tab=tab-other#heading=provider-remote"}, doc, doc.tabs[0]) == "#other__remote");
    CHECK(documentLinkHref(LinkTarget{.kind = "url", .url = "https://docs.google.com/document/d/local/edit?tab=tab-other"}, doc, doc.tabs[0]) == "#other__");
    CHECK_FALSE(documentLinkHref(LinkTarget{.kind = "url", .url = "https://example.invalid/path"}, doc, doc.tabs[0]));
    CHECK_FALSE(documentLinkHref(LinkTarget{.kind = "url", .url = "https://docs.google.com/document/d/foreign/edit#heading=provider-heading"}, doc, doc.tabs[0]));
    CHECK(documentLinkParse("https://docs.google.com/document/d/local/edit#goals", "tab-main").has_value());
}

TEST_CASE("source document links reject missing ambiguous invalid and new slug URL targets") {
    model::DocModel doc;
    model::TabModel tab;
    tab.tabId = "main";
    model::ParagraphBlock heading;
    heading.key = "heading";
    heading.style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    heading.inlines = {model::Inline::from_text("Same")};
    tab.blocks.emplace_back(heading);
    model::ParagraphBlock link;
    link.inlines = {model::Inline::from_text("link")};
    tab.blocks.emplace_back(link);
    doc.tabs = {tab};
    auto check = [&](const std::string& href, const std::string& code) {
        auto candidate = doc;
        candidate.tabs[0].blocks.back().as_paragraph().inlines[0].style["link"] = documentLinkStyle(href, "main");
        try { documentLinksResolve(candidate); FAIL("expected refusal"); }
        catch (const model::CoreError& error) { CHECK(error.code == code); }
    };
    check("#absent", "linkTargetNotFound");
    check("#bad%ZZ", "unsupportedSyntax");
    check("gdoc:unreadable#same", "linkTargetNotFound");
    check("https://docs.google.com/document/d/local/edit#same", "unsupportedSyntax");
    doc.tabs[0].blocks.insert(doc.tabs[0].blocks.begin(), model::Block(heading));
    check("#same", "anchorAmbiguous");
}

TEST_CASE("source document links match encoded Unicode normalized fragments") {
#ifdef __APPLE__
    model::DocModel doc;
    model::TabModel tab;
    tab.tabId = "main";
    model::ParagraphBlock heading;
    heading.key = "heading";
    heading.headingId = "provider-heading";
    heading.style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    heading.inlines = {model::Inline::from_text("Café")};
    tab.blocks.emplace_back(heading);
    model::ParagraphBlock link;
    link.inlines = {model::Inline::from_text("link", slimr::Json::object_t{
        {"link", documentLinkStyle("#CAFE%CC%81", "main")}
    })};
    tab.blocks.emplace_back(link);
    doc.tabs = {tab};
    documentLinksResolve(doc);
    CHECK(doc.tabs[0].blocks.back().as_paragraph().inlines[0].style["link"]["heading"]["id"] == slimr::Json("provider-heading"));
#endif
}

} // namespace gdocsmith::lens::test
