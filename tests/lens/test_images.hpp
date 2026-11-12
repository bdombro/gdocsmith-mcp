#pragma once

#include "lens/images.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("imageUriResolve for remote, upload and local paths") {
    CHECK(imageUriResolve("https://example.com/pic.png", std::nullopt) == "https://example.com/pic.png");
    CHECK(imageUriResolve("http://example.com/pic.png", std::nullopt) == "upload:http://example.com/pic.png");

    std::string rel_res = imageUriResolve("images/pic.png", "/tmp");
    CHECK(rel_res == "upload:/tmp/images/pic.png");

    bool threw = false;
    try {
        imageUriResolve("images/pic.png", std::nullopt);
    } catch (const CoreError& err) {
        threw = true;
        CHECK(err.code == "unsupportedSyntax");
    }
    CHECK(threw);

    bool scheme_threw = false;
    try {
        imageUriResolve("ftp://example.com/pic.png", std::nullopt);
    } catch (const CoreError& err) {
        scheme_threw = true;
        CHECK(err.code == "unsupportedSyntax");
    }
    CHECK(scheme_threw);
}

} // namespace gdocsmith::lens::test
