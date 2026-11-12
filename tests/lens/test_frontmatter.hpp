#pragma once

#include "lens/frontmatter.hpp"
#include "test.hpp"
#include <functional>

namespace gdocsmith::lens::test {

TEST_CASE("frontmatterValidate refuses invalid fields with exact messages") {
    auto make_base = []() {
        slimr::Json j = slimr::json_obj();
        j["doc"] = "d";
        j["gdocsmith"] = GDOCSMITH_VERSION;
        j["revision"] = "r";
        j["title"] = "T";
        return j;
    };

    struct Case {
        std::function<slimr::Json()> build;
        std::string expected_msg;
    };

    std::vector<Case> cases = {
        {
            [&] { auto j = make_base(); j["extra"] = 1; return j; },
            "unknown frontmatter field \"extra\""
        },
        {
            [&] { auto j = make_base(); j["title"] = " "; return j; },
            "title must be a nonempty string"
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ts = slimr::json_obj();
                ts["-bad"] = slimr::json_obj();
                j["textStyles"] = std::move(ts);
                return j;
            },
            "style name \"-bad\" must match"
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ts = slimr::json_obj();
                slimr::Json a = slimr::json_obj();
                a["glow"] = true;
                ts["a"] = std::move(a);
                j["textStyles"] = std::move(ts);
                return j;
            },
            "style \"a\" has unknown attribute \"glow\""
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ts = slimr::json_obj();
                slimr::Json a = slimr::json_obj();
                a["foregroundColor"] = "red";
                ts["a"] = std::move(a);
                j["textStyles"] = std::move(ts);
                return j;
            },
            "style \"a\" attribute \"foregroundColor\" must be #RRGGBB"
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ts = slimr::json_obj();
                slimr::Json a = slimr::json_obj();
                a["fontSize"] = 0;
                ts["a"] = std::move(a);
                j["textStyles"] = std::move(ts);
                return j;
            },
            "style \"a\" attribute \"fontSize\" must be a number"
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ps = slimr::json_obj();
                slimr::Json p = slimr::json_obj();
                p["alignment"] = "LEFT";
                ps["p"] = std::move(p);
                j["paragraphStyles"] = std::move(ps);
                return j;
            },
            "invalid alignment: LEFT"
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ps = slimr::json_obj();
                slimr::Json p = slimr::json_obj();
                p["namedStyle"] = slimr::Json(nullptr);
                ps["p"] = std::move(p);
                j["paragraphStyles"] = std::move(ps);
                return j;
            },
            "namedStyle cannot be null; use NORMAL_TEXT"
        },
        {
            [&] {
                auto j = make_base();
                slimr::Json ps = slimr::json_obj();
                slimr::Json p = slimr::json_obj();
                p["spaceAbove"] = "1";
                ps["p"] = std::move(p);
                j["paragraphStyles"] = std::move(ps);
                return j;
            },
            "spaceAbove must be a finite valid point value"
        }
    };

    for (const auto& c : cases) {
        bool threw = false;
        try {
            frontmatterValidate(c.build(), "document");
        } catch (const CoreError& err) {
            threw = true;
            CHECK(std::string(err.what()).find(c.expected_msg) != std::string::npos);
        }
        CHECK(threw);
    }
}

TEST_CASE("frontmatterValidate __proto__ refusal") {
    slimr::Json j = slimr::json_obj();
    j["doc"] = "d";
    j["gdocsmith"] = GDOCSMITH_VERSION;
    j["revision"] = "r";
    j["title"] = "T";
    slimr::Json ts = slimr::json_obj();
    slimr::Json p = slimr::json_obj();
    p["bold"] = true;
    ts["__proto__"] = std::move(p);
    j["textStyles"] = std::move(ts);

    bool threw = false;
    try {
        frontmatterValidate(j, "document");
    } catch (const CoreError& err) {
        threw = true;
        CHECK(std::string(err.what()).find("style name \"__proto__\" must match") != std::string::npos);
    }
    CHECK(threw);
}

TEST_CASE("frontmatterValidate uppercases colors and keeps resets") {
    slimr::Json j = slimr::json_obj();
    j["doc"] = "d";
    j["gdocsmith"] = GDOCSMITH_VERSION;
    j["revision"] = "r";
    j["title"] = "T";
    slimr::Json ts = slimr::json_obj();
    slimr::Json a = slimr::json_obj();
    a["bold"] = slimr::Json(nullptr);
    a["foregroundColor"] = "#c0392b";
    ts["a"] = std::move(a);
    j["textStyles"] = std::move(ts);

    auto df = frontmatterValidate(j, "document");
    CHECK(df.textStyles.contains("a"));
    CHECK(df.textStyles["a"].foregroundColor == std::optional<std::string>("#C0392B"));
}

} // namespace gdocsmith::lens::test
