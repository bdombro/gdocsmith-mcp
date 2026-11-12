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

} // namespace gdocsmith::lens::test
