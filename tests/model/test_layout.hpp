#pragma once

#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/layout.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace gdocsmith::model::test {

inline std::vector<Range> rawRangesCollect(const JsonObject& json) {
    std::vector<Range> ranges;
    auto walk = [&](auto& self, const slimr::Json::array_t& content) -> void {
        for (const auto& raw : content) {
            if (raw.contains("startIndex") && raw.contains("endIndex")) {
                int s = raw["startIndex"].holds<int64_t>() ? static_cast<int>(raw["startIndex"].get<int64_t>()) : static_cast<int>(raw["startIndex"].get<double>());
                int e = raw["endIndex"].holds<int64_t>() ? static_cast<int>(raw["endIndex"].get<int64_t>()) : static_cast<int>(raw["endIndex"].get<double>());
                ranges.push_back(Range{.end = e, .start = s});
            }
            if (raw.contains("table") && raw["table"].contains("tableRows") && raw["table"]["tableRows"].holds<slimr::Json::array_t>()) {
                for (const auto& row : raw["table"]["tableRows"].get<slimr::Json::array_t>()) {
                    if (row.contains("startIndex") && row.contains("endIndex")) {
                        int s = row["startIndex"].holds<int64_t>() ? static_cast<int>(row["startIndex"].get<int64_t>()) : static_cast<int>(row["startIndex"].get<double>());
                        int e = row["endIndex"].holds<int64_t>() ? static_cast<int>(row["endIndex"].get<int64_t>()) : static_cast<int>(row["endIndex"].get<double>());
                        ranges.push_back(Range{.end = e, .start = s});
                    }
                    if (row.contains("tableCells") && row["tableCells"].holds<slimr::Json::array_t>()) {
                        for (const auto& cell : row["tableCells"].get<slimr::Json::array_t>()) {
                            if (cell.contains("startIndex") && cell.contains("endIndex")) {
                                int s = cell["startIndex"].holds<int64_t>() ? static_cast<int>(cell["startIndex"].get<int64_t>()) : static_cast<int>(cell["startIndex"].get<double>());
                                int e = cell["endIndex"].holds<int64_t>() ? static_cast<int>(cell["endIndex"].get<int64_t>()) : static_cast<int>(cell["endIndex"].get<double>());
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
    if (json.contains("tabs") && json["tabs"].holds<slimr::Json::array_t>()) {
        for (const auto& tab : json["tabs"].get<slimr::Json::array_t>()) {
            if (tab.contains("documentTab") && tab["documentTab"].contains("body") &&
                tab["documentTab"]["body"].contains("content") &&
                tab["documentTab"]["body"]["content"].holds<slimr::Json::array_t>()) {
                walk(walk, tab["documentTab"]["body"]["content"].get<slimr::Json::array_t>());
            }
        }
    }
    return ranges;
}

inline void checkShape(const DocSpec& spec) {
    auto json = docJsonBuild(spec);
    KeyAllocator keys;
    auto doc = docModelParse(json, DocModelParseOptions{.docId = "d1", .keys = keys});
    const auto& tab = doc.tabs[0];
    auto layout = layoutCompute(tab);

    std::vector<std::string> gotRanges;
    for (const auto& [_, r] : layout.blockRanges) gotRanges.push_back(std::format("{}-{}", r.start, r.end));
    for (const auto& [_, r] : layout.rowRanges) gotRanges.push_back(std::format("{}-{}", r.start, r.end));
    for (const auto& [_, r] : layout.cellRanges) gotRanges.push_back(std::format("{}-{}", r.start, r.end));
    std::sort(gotRanges.begin(), gotRanges.end());

    std::vector<std::string> wantRanges;
    for (const auto& r : rawRangesCollect(json)) {
        if (r.start != 0) {
            wantRanges.push_back(std::format("{}-{}", r.start, r.end));
        }
    }
    std::sort(wantRanges.begin(), wantRanges.end());
    CHECK(gotRanges == wantRanges);
}

TEST_CASE("layoutCompute - layout equals JSON indices for synthetic shapes") {
    using namespace gdocsmith::model;
    auto rng = rngCreate(42);
    for (int i = 0; i < 10; ++i) {
        DocSpec spec;
        spec.tabs.push_back(TabSpec{.blocks = randomBlockSpecs(rng, 3 + i)});
        checkShape(spec);
    }
}

TEST_CASE("layoutCompute - empty 2x3 table length = 16") {
    using namespace gdocsmith::model;
    TableBlockSpec tbs;
    for (int r = 0; r < 2; ++r) {
        std::vector<std::vector<BlockSpec>> row;
        for (int c = 0; c < 3; ++c) {
            row.push_back({BlockSpec::make_paragraph(ParagraphSpec{.content = {}})});
        }
        tbs.cells.push_back(std::move(row));
    }
    DocSpec spec;
    spec.tabs.push_back(TabSpec{.blocks = {BlockSpec::make_table(std::move(tbs))}});
    auto json = docJsonBuild(spec);
    KeyAllocator keys;
    auto doc = docModelParse(json, DocModelParseOptions{.docId = "d1", .keys = keys});
    CHECK(blockLength(doc.tabs[0].blocks[0]) == 16);
    CHECK(doc.tabs[0].blocks[0].as_table().rows.size() == 2);
}

} // namespace gdocsmith::model::test
