#pragma once

#include "emulator/conformance.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/layout.hpp"
#include "test.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::model::test {

enum class CorpusState {
    unavailable,
    empty,
    nonempty
};

struct CorpusRunResult {
    CorpusState state = CorpusState::unavailable;
    size_t docs_executed = 0;
    size_t tabs_executed = 0;
    std::vector<std::string> errors;
};

inline bool corpusRequireNonEmpty(const CorpusRunResult& res) {
    return res.state == CorpusState::nonempty &&
           res.docs_executed > 0 &&
           res.tabs_executed > 0 &&
           res.errors.empty();
}

inline std::vector<Range> tabRawRangesCollect(const JsonObject& dt) {
    std::vector<Range> ranges;
    auto walk = [&](auto& self, const slimr::Json::array_t& content) -> void {
        for (const auto& raw : content) {
            if (raw.contains("startIndex") && raw.contains("endIndex")) {
                int s = raw["startIndex"].holds<int64_t>()
                            ? static_cast<int>(raw["startIndex"].get<int64_t>())
                            : static_cast<int>(raw["startIndex"].get<double>());
                int e = raw["endIndex"].holds<int64_t>()
                            ? static_cast<int>(raw["endIndex"].get<int64_t>())
                            : static_cast<int>(raw["endIndex"].get<double>());
                ranges.push_back(Range{.end = e, .start = s});
            }
            if (raw.contains("table") && raw["table"].contains("tableRows") &&
                raw["table"]["tableRows"].holds<slimr::Json::array_t>()) {
                for (const auto& row : raw["table"]["tableRows"].get<slimr::Json::array_t>()) {
                    if (row.contains("startIndex") && row.contains("endIndex")) {
                        int s = row["startIndex"].holds<int64_t>()
                                    ? static_cast<int>(row["startIndex"].get<int64_t>())
                                    : static_cast<int>(row["startIndex"].get<double>());
                        int e = row["endIndex"].holds<int64_t>()
                                    ? static_cast<int>(row["endIndex"].get<int64_t>())
                                    : static_cast<int>(row["endIndex"].get<double>());
                        ranges.push_back(Range{.end = e, .start = s});
                    }
                    if (row.contains("tableCells") && row["tableCells"].holds<slimr::Json::array_t>()) {
                        for (const auto& cell : row["tableCells"].get<slimr::Json::array_t>()) {
                            if (cell.contains("startIndex") && cell.contains("endIndex")) {
                                int s = cell["startIndex"].holds<int64_t>()
                                            ? static_cast<int>(cell["startIndex"].get<int64_t>())
                                            : static_cast<int>(cell["startIndex"].get<double>());
                                int e = cell["endIndex"].holds<int64_t>()
                                            ? static_cast<int>(cell["endIndex"].get<int64_t>())
                                            : static_cast<int>(cell["endIndex"].get<double>());
                                ranges.push_back(Range{.end = e, .start = s});
                            }
                            if (cell.contains("content") && cell["content"].holds<slimr::Json::array_t>()) {
                                self(self, cell["content"].get<slimr::Json::array_t>());
                            }
                        }
                    }
                }
            }
        }
    };
    if (dt.contains("body") && dt["body"].contains("content") &&
        dt["body"]["content"].holds<slimr::Json::array_t>()) {
        walk(walk, dt["body"]["content"].get<slimr::Json::array_t>());
    }
    return ranges;
}

inline void collectRawTabBodiesDfs(const JsonObject& tabJson, std::vector<const JsonObject*>& out) {
    if (tabJson.contains("documentTab") && tabJson["documentTab"].is_object()) {
        out.push_back(&tabJson["documentTab"]);
    } else {
        out.push_back(&tabJson);
    }
    if (tabJson.contains("childTabs") && tabJson["childTabs"].holds<slimr::Json::array_t>()) {
        for (const auto& child : tabJson["childTabs"].get<slimr::Json::array_t>()) {
            collectRawTabBodiesDfs(child, out);
        }
    }
}

inline std::vector<const JsonObject*> docRawTabBodiesDfs(const JsonObject& docJson) {
    const auto& raw = rawDocumentOf(docJson);
    std::vector<const JsonObject*> out;
    if (raw.contains("tabs") && raw["tabs"].holds<slimr::Json::array_t>() &&
        !raw["tabs"].get<slimr::Json::array_t>().empty()) {
        for (const auto& tabJson : raw["tabs"].get<slimr::Json::array_t>()) {
            collectRawTabBodiesDfs(tabJson, out);
        }
    } else {
        out.push_back(&raw);
    }
    return out;
}

inline bool isGoogleDocSnapshot(const JsonObject& json) {
    const JsonObject* raw = &json;
    if (json.contains("document") && json["document"].is_object()) {
        raw = &json["document"];
    }
    if (raw->contains("tabs") && (*raw)["tabs"].holds<slimr::Json::array_t>() &&
        !(*raw)["tabs"].get<slimr::Json::array_t>().empty()) {
        const auto& first = (*raw)["tabs"].get<slimr::Json::array_t>()[0];
        return first.is_object() && first.contains("tabProperties") && first.contains("documentTab");
    }
    return raw->contains("body") && (*raw)["body"].is_object();
}

inline void verifyCorpusDocument(const JsonObject& docJson, std::string_view docId, CorpusRunResult& res) {
    try {
        KeyAllocator keys;
        auto doc = docModelParse(docJson, DocModelParseOptions{.docId = std::string(docId), .keys = keys});
        ++res.docs_executed;

        auto rawTabs = docRawTabBodiesDfs(docJson);
        if (rawTabs.size() != doc.tabs.size()) {
            res.errors.push_back(std::format("{}: raw tab count {} != parsed tab count {}",
                                             docId, rawTabs.size(), doc.tabs.size()));
            return;
        }

        for (size_t i = 0; i < doc.tabs.size(); ++i) {
            const auto& tab = doc.tabs[i];
            auto layout = layoutCompute(tab);
            ++res.tabs_executed;

            std::vector<std::string> gotRanges;
            for (const auto& [_, r] : layout.blockRanges) gotRanges.push_back(std::format("{}-{}", r.start, r.end));
            for (const auto& [_, r] : layout.rowRanges) gotRanges.push_back(std::format("{}-{}", r.start, r.end));
            for (const auto& [_, r] : layout.cellRanges) gotRanges.push_back(std::format("{}-{}", r.start, r.end));
            std::sort(gotRanges.begin(), gotRanges.end());

            std::vector<std::string> wantRanges;
            for (const auto& r : tabRawRangesCollect(*rawTabs[i])) {
                if (r.start != 0) {
                    wantRanges.push_back(std::format("{}-{}", r.start, r.end));
                }
            }
            std::sort(wantRanges.begin(), wantRanges.end());

            if (gotRanges != wantRanges) {
                res.errors.push_back(std::format("{}[tab {}]: layout ranges ({}) != raw JSON ranges ({})",
                                                 docId, tab.tabId, gotRanges.size(), wantRanges.size()));
            }
        }
    } catch (const std::exception& ex) {
        res.errors.push_back(std::format("{}: {}", docId, ex.what()));
    }
}

inline CorpusRunResult runSyntheticCorpusDir(const std::filesystem::path& dir) {
    CorpusRunResult res;
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        res.state = CorpusState::unavailable;
        return res;
    }

    std::vector<std::filesystem::path> jsonFiles;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            jsonFiles.push_back(entry.path());
        }
    }
    std::sort(jsonFiles.begin(), jsonFiles.end());

    if (jsonFiles.empty()) {
        res.state = CorpusState::empty;
        return res;
    }

    res.state = CorpusState::nonempty;
    for (const auto& path : jsonFiles) {
        std::ifstream input(path);
        if (!input.is_open()) {
            res.errors.push_back(std::format("{}: cannot open snapshot", path.string()));
            continue;
        }
        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        auto parsed = slimr::parse_json<slimr::Json>(content);
        if (!parsed) {
            res.errors.push_back(std::format("{}: malformed JSON", path.string()));
            continue;
        }
        const auto& json = *parsed;
        if (json.is_object() && json.contains("before") && json["before"].is_object() &&
            json.contains("requests") && json["requests"].is_array()) {
            try {
                auto fx = gdocsmith::emulator::fixture_expand(json);
                verifyCorpusDocument(fx.before, fx.id + "#before", res);
                if (fx.after.has_value() && fx.after->is_object()) {
                    verifyCorpusDocument(*fx.after, fx.id + "#after", res);
                }
            } catch (const std::exception& ex) {
                res.errors.push_back(std::format("{}: {}", path.string(), ex.what()));
            }
        } else if (json.is_object() && isGoogleDocSnapshot(json)) {
            const auto& docObj = (json.contains("document") && json["document"].is_object())
                                     ? json["document"]
                                     : json;
            verifyCorpusDocument(docObj, path.stem().string(), res);
        }
    }
    return res;
}

inline std::filesystem::path findCorpusSubdir(std::string_view rel) {
    std::vector<std::filesystem::path> prefixes = {
        ".",
        "..",
        "../..",
        "plugins/gdocsmith-mcp"
    };
    for (const auto& prefix : prefixes) {
        auto candidate = prefix / rel;
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }
    return {};
}

struct TempCorpusDir {
    std::filesystem::path path;
    explicit TempCorpusDir(std::filesystem::path p) : path(std::move(p)) {
        std::filesystem::create_directories(path);
    }
    ~TempCorpusDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    TempCorpusDir(const TempCorpusDir&) = delete;
    TempCorpusDir& operator=(const TempCorpusDir&) = delete;
};

TEST_CASE("corpus - unavailable directory classifies unavailable and fails closed") {
    auto missing = std::filesystem::temp_directory_path() / "gdocsmith_missing_corpus_dir_18a";
    std::error_code ec;
    std::filesystem::remove_all(missing, ec);

    auto res = runSyntheticCorpusDir(missing);
    CHECK(res.state == CorpusState::unavailable);
    CHECK(res.docs_executed == 0);
    CHECK(res.tabs_executed == 0);
    CHECK(!corpusRequireNonEmpty(res));
}

TEST_CASE("corpus - empty directory classifies empty and fails closed") {
    TempCorpusDir emptyDir(std::filesystem::temp_directory_path() / "gdocsmith_empty_corpus_dir_18a");
    auto res = runSyntheticCorpusDir(emptyDir.path);
    CHECK(res.state == CorpusState::empty);
    CHECK(res.docs_executed == 0);
    CHECK(res.tabs_executed == 0);
    CHECK(!corpusRequireNonEmpty(res));
}

TEST_CASE("corpus - fail-closed zero-execution guard rejects zero docs or tabs") {
    CorpusRunResult zeroBoth{.state = CorpusState::nonempty, .docs_executed = 0, .tabs_executed = 0};
    CorpusRunResult zeroTabs{.state = CorpusState::nonempty, .docs_executed = 1, .tabs_executed = 0};
    CorpusRunResult zeroDocs{.state = CorpusState::nonempty, .docs_executed = 0, .tabs_executed = 1};
    CHECK(!corpusRequireNonEmpty(zeroBoth));
    CHECK(!corpusRequireNonEmpty(zeroTabs));
    CHECK(!corpusRequireNonEmpty(zeroDocs));
}

TEST_CASE("corpus - wrong-layout fault is detected and fails") {
    auto parsed = slimr::parse_json<slimr::Json>(R"({
        "documentId": "fault-doc",
        "revisionId": "rev-1",
        "title": "Fault",
        "tabs": [{
            "tabProperties": {"tabId": "t.0", "title": "Main"},
            "documentTab": {
                "body": {"content": [
                    {"startIndex": 0, "endIndex": 1, "sectionBreak": {"sectionStyle": {}}},
                    {"startIndex": 1, "endIndex": 99, "paragraph": {
                        "elements": [
                            {"startIndex": 1, "endIndex": 3, "textRun": {"content": "Hi", "textStyle": {}}},
                            {"startIndex": 3, "endIndex": 4, "textRun": {"content": "\n", "textStyle": {}}}
                        ],
                        "paragraphStyle": {}
                    }}
                ]}
            }
        }]
    })");
    REQUIRE(parsed.has_value());

    CorpusRunResult res{.state = CorpusState::nonempty};
    verifyCorpusDocument(*parsed, "fault-doc", res);
    CHECK(res.docs_executed == 1);
    CHECK(res.tabs_executed == 1);
    CHECK(!res.errors.empty());
    CHECK(!corpusRequireNonEmpty(res));
}

TEST_CASE("corpus - synthetic conformance and parity seed snapshots execute non-empty and match layout") {
    auto conformanceDir = findCorpusSubdir("tests/fixtures/conformance");
    REQUIRE(!conformanceDir.empty());
    auto confRes = runSyntheticCorpusDir(conformanceDir);
    INFO("conformance docs_executed=" << confRes.docs_executed << " tabs_executed=" << confRes.tabs_executed);
    for (const auto& err : confRes.errors) {
        FAIL_CHECK(err);
    }
    CHECK(corpusRequireNonEmpty(confRes));
    CHECK(confRes.docs_executed > 0);
    CHECK(confRes.tabs_executed >= confRes.docs_executed);

    auto seedsDir = findCorpusSubdir("tests/parity/seeds");
    REQUIRE(!seedsDir.empty());
    auto seedRes = runSyntheticCorpusDir(seedsDir);
    INFO("seeds docs_executed=" << seedRes.docs_executed << " tabs_executed=" << seedRes.tabs_executed);
    for (const auto& err : seedRes.errors) {
        FAIL_CHECK(err);
    }
    CHECK(corpusRequireNonEmpty(seedRes));
    CHECK(seedRes.docs_executed > 0);
    CHECK(seedRes.tabs_executed > seedRes.docs_executed);
}

} // namespace gdocsmith::model::test
