#pragma once

#include "emulator/conformance.hpp"
#include "emulator/document_emulator.hpp"
#include "emulator/types.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace gdocsmith::emulator;

inline std::string find_fixture_dir() {
    std::vector<std::string> candidates = {
        "tests/fixtures/conformance",
        "../tests/fixtures/conformance",
        "../../tests/fixtures/conformance",
        "plugins/gdocsmith-mcp/tests/fixtures/conformance"};
    for (const auto &path : candidates) {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            return path;
        }
    }
    return "";
}

// Loading is part of the conformance obligation, not a reason to skip a fixture.
inline ConformanceFixture read_conformance_fixture(std::istream &input, std::string_view source) {
    const std::string label(source);
    if (!input.good())
        throw std::runtime_error(label + ": cannot read fixture");
    std::string content;
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        content.append(buffer, static_cast<size_t>(input.gcount()));
    }
    if (input.bad() || !input.eof())
        throw std::runtime_error(label + ": cannot read fixture");
    auto parsed = slimr::parse_json<slimr::Json>(content);
    if (!parsed)
        throw std::runtime_error(label + ": malformed fixture JSON");
    const auto &json = *parsed;
    // Reject invalid shapes before fixture_expand can silently default fields or
    // dereference the absent expansion of a null `before` document.
    if (!json.is_object() ||
        !json.contains("id") || !json["id"].is_string() ||
        !json.contains("name") || !json["name"].is_string() ||
        !json.contains("before") || !json["before"].is_object() ||
        !json.contains("after") || !(json["after"].is_object() || json["after"].is_null()) ||
        !json.contains("requests") || !json["requests"].is_array() ||
        (json.contains("replies") && !json["replies"].is_array()) ||
        (json.contains("error") && !json["error"].is_string())) {
        throw std::runtime_error(label + ": invalid fixture shape");
    }
    return fixture_expand(json);
}

inline ConformanceFixture load_conformance_fixture(const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input.is_open())
        throw std::runtime_error(path.string() + ": cannot open fixture");
    try {
        return read_conformance_fixture(input, path.string());
    } catch (const std::ios_base::failure &) {
        throw std::runtime_error(path.string() + ": cannot read fixture");
    }
}

TEST_SUITE("emulator::conformance") {
    TEST_CASE("fixtures exist and pass conformance") {
        std::string fixture_dir = find_fixture_dir();
        REQUIRE(!fixture_dir.empty());

        std::vector<std::string> json_files;
        for (const auto &entry : std::filesystem::directory_iterator(fixture_dir)) {
            if (entry.path().extension() == ".json") {
                json_files.push_back(entry.path().string());
            }
        }
        std::sort(json_files.begin(), json_files.end());
        CHECK(json_files.size() > 80);

        size_t checked = 0;
        for (const auto &file_path : json_files) {
            INFO("fixture: " << file_path);
            // Report failures per file and keep checking the remaining fixtures.
            try {
                const auto fx = load_conformance_fixture(file_path);
                INFO(fx.id << " (" << fx.name << ")");
                const auto diffs = conformance_compare(fx);
                for (const auto &diff : diffs) {
                    INFO(diff);
                    CHECK(diff.empty());
                }
                CHECK(diffs.empty());
                ++checked;
            } catch (const std::exception &error) {
                FAIL_CHECK(std::string(error.what()));
            }
        }
        CHECK(checked == json_files.size());
    }

    TEST_CASE("fixture loading rejects malformed JSON and invalid shapes") {
        for (const auto &text : {"{", "", "null", "[]", "{}",
                                 R"({"id":"bad","name":"bad","before":null,"after":null,"requests":[]})"}) {
            INFO("invalid input: " << text);
            std::istringstream input(text);
            CHECK_THROWS_AS(read_conformance_fixture(input, "invalid.json"), std::runtime_error);
        }
        std::istringstream malformed("{");
        CHECK_THROWS_WITH(read_conformance_fixture(malformed, "B1.json"),
                          "B1.json: malformed fixture JSON");
    }

    TEST_CASE("missing and unreadable fixtures are explicit errors") {
        slimr::testing::TempDirGuard dir("conformance_load_");
        const auto missing = dir.path / "missing.json";
        CHECK_THROWS_WITH(load_conformance_fixture(missing),
                          (missing.string() + ": cannot open fixture").c_str());
        std::istringstream unreadable("{}");
        unreadable.setstate(std::ios::badbit);
        CHECK_THROWS_WITH(read_conformance_fixture(unreadable, "unreadable.json"),
                          "unreadable.json: cannot read fixture");
        CHECK_THROWS_AS(load_conformance_fixture(dir.path), std::runtime_error);
    }

    TEST_CASE("valid fixture passes and wrong expected outcomes are detected") {
        const auto dir = find_fixture_dir();
        REQUIRE(!dir.empty());
        auto fx = load_conformance_fixture(std::filesystem::path(dir) / "B1.json");
        REQUIRE(conformance_compare(fx).empty());
        SUBCASE("wrong resulting document") {
            fx.after = fx.before;
            CHECK_FALSE(conformance_compare(fx).empty());
        }
        SUBCASE("wrong API rejection") {
            fx.after.reset();
            CHECK_FALSE(conformance_compare(fx).empty());
        }
        SUBCASE("wrong reply count") {
            fx.replies = std::vector<slimr::Json>{};
            CHECK_FALSE(conformance_compare(fx).empty());
        }
        SUBCASE("omitted replies still means an empty expected reply list") {
            fx.replies.reset();
            CHECK_FALSE(conformance_compare(fx).empty());
        }
    }
}
