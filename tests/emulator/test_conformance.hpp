#pragma once

#include "test.hpp"
#include "emulator/types.hpp"
#include "emulator/document_emulator.hpp"
#include "emulator/conformance.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace gdocsmith::emulator;

inline std::string find_fixture_dir() {
    std::vector<std::string> candidates = {
        "tests/fixtures/conformance",
        "../tests/fixtures/conformance",
        "../../tests/fixtures/conformance",
        "plugins/gdocsmith-mcp/tests/fixtures/conformance"
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            return path;
        }
    }
    return "";
}

TEST_SUITE("emulator::conformance") {
    TEST_CASE("fixtures exist and pass conformance") {
        std::string fixture_dir = find_fixture_dir();
        REQUIRE(!fixture_dir.empty());

        std::vector<std::string> json_files;
        for (const auto& entry : std::filesystem::directory_iterator(fixture_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                json_files.push_back(entry.path().string());
            }
        }
        std::sort(json_files.begin(), json_files.end());
        CHECK(json_files.size() > 80);

        int passed = 0;
        int failed = 0;
        std::vector<std::string> failure_messages;

        for (const auto& file_path : json_files) {
            std::ifstream file(file_path);
            if (!file.is_open()) continue;
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            auto parsed = slimr::parse_json<slimr::Json>(content);
            if (!parsed) continue;

            ConformanceFixture fx = fixture_expand(*parsed);
            auto diffs = conformance_compare(fx);
            if (diffs.empty()) {
                passed++;
            } else {
                failed++;
                std::string err = fx.id + " (" + fx.name + "):";
                for (const auto& d : diffs) err += "\n  " + d;
                failure_messages.push_back(err);
            }
        }

        // We require high conformance rate on ported fixtures
        CHECK(passed > 0);
        // Output failure report if any
        if (failed > 0) {
            for (const auto& msg : failure_messages) {
                MESSAGE(msg);
            }
        }
        CHECK(failed == 0);
    }
}
