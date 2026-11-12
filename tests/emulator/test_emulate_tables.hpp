#pragma once

#include "test.hpp"
#include "test_helpers.hpp"

using namespace gdocsmith::emulator;
using namespace gdocsmith::emulator::test_util;

TEST_SUITE("emulator::emulate_tables") {
    TEST_CASE("insertTable creates the right table structure") {
        slimr::Json doc = make_simple_doc({"before"});
        std::vector<slimr::Json> reqs = {
            slimr::Json::object_t{
                {"insertTable", slimr::Json::object_t{
                    {"columns", static_cast<int64_t>(3)},
                    {"rows", static_cast<int64_t>(2)},
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(4)}}}
                }}
            }
        };
        auto res = requests_emulate(doc, reqs);
        const auto& content = res.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                                  .get<slimr::Json::array_t>();
        // Section break + paragraph ("bef") + table + paragraph ("ore\n")
        CHECK(content.size() == 4);
        CHECK(content[2].contains("table"));
        const auto& tbl = content[2]["table"];
        CHECK(tbl["rows"].as<int>() == 2);
        CHECK(tbl["columns"].as<int>() == 3);
        CHECK(tbl["tableRows"].get<slimr::Json::array_t>().size() == 2);
        CHECK(tbl["tableRows"].get<slimr::Json::array_t>()[0]["tableCells"].get<slimr::Json::array_t>().size() == 3);
    }

    TEST_CASE("insertTableRow, insertTableColumn, deleteTableRow, deleteTableColumn") {
        slimr::Json doc = make_simple_doc({"before"});
        // Insert 2x2 table at index 7 (inside "before\n")
        auto res1 = requests_emulate(doc, {
            slimr::Json::object_t{
                {"insertTable", slimr::Json::object_t{
                    {"columns", static_cast<int64_t>(2)},
                    {"rows", static_cast<int64_t>(2)},
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(7)}}}
                }}
            }
        });

        // Insert row below row 0
        auto res2 = requests_emulate(res1.json, {
            slimr::Json::object_t{
                {"insertTableRow", slimr::Json::object_t{
                    {"insertBelow", true},
                    {"tableCellLocation", slimr::Json::object_t{
                        {"rowIndex", static_cast<int64_t>(0)},
                        {"columnIndex", static_cast<int64_t>(0)},
                        {"tableStartLocation", slimr::Json::object_t{{"index", static_cast<int64_t>(8)}}}
                    }}
                }}
            }
        });
        const auto& tbl2 = res2.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                               .get<slimr::Json::array_t>()[2]["table"];
        CHECK(tbl2["rows"].as<int>() == 3);

        // Delete row 1
        auto res3 = requests_emulate(res2.json, {
            slimr::Json::object_t{
                {"deleteTableRow", slimr::Json::object_t{
                    {"tableCellLocation", slimr::Json::object_t{
                        {"rowIndex", static_cast<int64_t>(1)},
                        {"tableStartLocation", slimr::Json::object_t{{"index", static_cast<int64_t>(8)}}}
                    }}
                }}
            }
        });
        const auto& tbl3 = res3.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                               .get<slimr::Json::array_t>()[2]["table"];
        CHECK(tbl3["rows"].as<int>() == 2);
    }

    TEST_CASE("mergeTableCells and unmergeTableCells") {
        slimr::Json doc = make_simple_doc({"before"});
        auto res1 = requests_emulate(doc, {
            slimr::Json::object_t{
                {"insertTable", slimr::Json::object_t{
                    {"columns", static_cast<int64_t>(2)},
                    {"rows", static_cast<int64_t>(2)},
                    {"location", slimr::Json::object_t{{"index", static_cast<int64_t>(7)}}}
                }}
            }
        });

        // Merge row 0 col 0..1 (span 1x2)
        auto res2 = requests_emulate(res1.json, {
            slimr::Json::object_t{
                {"mergeTableCells", slimr::Json::object_t{
                    {"tableRange", slimr::Json::object_t{
                        {"rowSpan", static_cast<int64_t>(1)},
                        {"columnSpan", static_cast<int64_t>(2)},
                        {"tableCellLocation", slimr::Json::object_t{
                            {"rowIndex", static_cast<int64_t>(0)},
                            {"columnIndex", static_cast<int64_t>(0)},
                            {"tableStartLocation", slimr::Json::object_t{{"index", static_cast<int64_t>(8)}}}
                        }}
                    }}
                }}
            }
        });
        const auto& head_cell = res2.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                                    .get<slimr::Json::array_t>()[2]["table"]["tableRows"].get<slimr::Json::array_t>()[0]["tableCells"]
                                    .get<slimr::Json::array_t>()[0];
        CHECK(head_cell["tableCellStyle"]["columnSpan"].as<int>() == 2);

        // Unmerge
        auto res3 = requests_emulate(res2.json, {
            slimr::Json::object_t{
                {"unmergeTableCells", slimr::Json::object_t{
                    {"tableRange", slimr::Json::object_t{
                        {"rowSpan", static_cast<int64_t>(1)},
                        {"columnSpan", static_cast<int64_t>(2)},
                        {"tableCellLocation", slimr::Json::object_t{
                            {"rowIndex", static_cast<int64_t>(0)},
                            {"columnIndex", static_cast<int64_t>(0)},
                            {"tableStartLocation", slimr::Json::object_t{{"index", static_cast<int64_t>(8)}}}
                        }}
                    }}
                }}
            }
        });
        const auto& unmerged_cell = res3.json["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"]
                                        .get<slimr::Json::array_t>()[2]["table"]["tableRows"].get<slimr::Json::array_t>()[0]["tableCells"]
                                        .get<slimr::Json::array_t>()[0];
        CHECK(unmerged_cell["tableCellStyle"]["columnSpan"].as<int>() == 1);
    }
}
