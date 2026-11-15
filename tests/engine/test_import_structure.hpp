#pragma once

// Requires engine/test_import.hpp (RemoteImportFixture) to be included first.
#include "engine/import.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace gdocsmith::engine::test {

namespace structure {

inline std::string exportText(RemoteImportFixture& fixture) {
    auto json = fixture.backend.get_doc("synthetic_doc").json;
    json["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
    return lens::documentFileExport(exportDocumentFile(
        model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc"})));
}

inline FileWriteResult writeAndImport(RemoteImportFixture& fixture, const std::string& text) {
    ImportSafetyFixture::write(fixture.inputPath, text);
    return fileImport(ImportInput{.path = fixture.inputPath.string()}, fixture.remote);
}

// First occurrence only, like String.replace in the original.
inline std::string replaceFirst(std::string text, const std::string& from, const std::string& to) {
    INFO(text);
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

inline std::string importClean(RemoteImportFixture& fixture, const std::string& text) {
    const auto result = writeAndImport(fixture, text);
    std::string errors;
    if (result.errors) for (const auto& error : *result.errors) errors += error.code + ": " + error.message + "\n";
    INFO(result.summary);
    INFO(errors);
    REQUIRE(!result.errors);
    CHECK(result.verification == "match");
    REQUIRE(result.exportPath);
    return inputFileRead(*result.exportPath).text;
}

inline std::vector<std::string> tabLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        if (line.starts_with("::tab{")) lines.push_back(line);
    return lines;
}

inline const slimr::Json& remoteTab(RemoteImportFixture& fixture, std::size_t index) {
    return fixture.backend.get_doc("synthetic_doc").json["tabs"][index]["documentTab"];
}

inline void checkNoWrites(const RemoteImportFixture& fixture) {
    CHECK(fixture.batches == 0);
    // Strengthening beyond the source's batchUpdate-only count.
    CHECK(fixture.creates == 0);
    CHECK(fixture.deletes == 0);
    CHECK(fixture.commentWrites == 0);
    CHECK(fixture.sentRequests.empty());
}

inline model::BlockSpec paragraphOf(const std::string& text) {
    return model::BlockSpec::make_paragraph(model::ParagraphSpec{.content = {text}});
}

inline void seedNested(RemoteImportFixture& fixture) {
    auto tab = [](const char* id, const char* title, std::optional<std::string> parent) {
        return model::TabSpec{.tabId = id, .title = title, .parentTabId = std::move(parent),
            .blocks = {paragraphOf(std::string(title) + " body")}};
    };
    const slimr::Json json = model::docJsonBuild(model::DocSpec{
        .documentId = "synthetic_doc",
        .title = "Nested",
        .tabs = {tab("t.a", "A", std::nullopt), tab("t.a1", "A1", "t.a"), tab("t.a2", "A2", "t.a"),
            tab("t.b", "B", std::nullopt), tab("t.c", "C", std::nullopt)},
    });
    fixture.backend.seed_document("synthetic_doc", json, 1);
}

inline void seedPaged(RemoteImportFixture& fixture, const std::string& mode) {
    const auto blank = emulator::blank_doc_template("Synthetic", "synthetic_doc");
    const auto& blankTab = blank["tabs"][0]["documentTab"];
    slimr::Json json = model::docJsonBuild(model::DocSpec{
        .documentId = "synthetic_doc",
        .title = "Synthetic",
        .tabs = {
            model::TabSpec{.tabId = "t.0", .title = "Overview", .blocks = {paragraphOf("Hello world")}},
            model::TabSpec{.tabId = "t.1", .title = "Other Tab", .blocks = {paragraphOf("Protected by omission")}},
        },
    });
    for (std::size_t i = 0; i < 2; ++i) {
        auto& docTab = json["tabs"][i]["documentTab"];
        slimr::Json style = blankTab["documentStyle"];
        style["documentFormat"] = slimr::Json::object_t{{"documentMode", mode}};
        docTab["documentStyle"] = style;
        docTab["namedStyles"] = blankTab["namedStyles"];
    }
    fixture.backend.seed_document("synthetic_doc", json, 1);
}

} // namespace structure

// Source: import.test.ts 787–815 (nested fixture), 955–976 (refusal rows).
TEST_CASE("import structure parity - tab parent refusals send nothing") {
    struct Row {
        const char* from;
        const char* to;
        const char* code;
        const char* needle;
    };
    const Row rows[] = {
        {"title=B}", "title=B parent=t.missing}", "tabNotFound", "not a tab ID"},
        {"title=B}", "title=B parent=New}", "tabNotFound", "not a tab ID"},
        {"title=B}", "title=B parent=t.c}", "invalidPlacement", "must come after its parent"},
        {"title=A}", "title=A parent=t.a}", "invalidPlacement", "own ancestor"},
        {"title=A}", "title=A parent=t.a2}", "invalidPlacement", "own ancestor"},
        {"title=C}", "title=C delete}\n\n::tab{id=t.zz title=Z parent=t.c}", "tabNotFound", "not in this"},
    };
    for (std::size_t i = 0; i < std::size(rows); ++i) {
        const auto& row = rows[i];
        CAPTURE(i);
        CAPTURE(row.to);
        RemoteImportFixture fixture;
        structure::seedNested(fixture);
        const auto text = structure::replaceFirst(structure::exportText(fixture), row.from, row.to)
            + "\n::tab{id=\"\" title=New}\n\nNew\n";
        const auto result = structure::writeAndImport(fixture, text);
        INFO(result.summary);
        CHECK(result.status == "refused");
        REQUIRE(result.errors);
        REQUIRE(!result.errors->empty());
        const auto& error = result.errors->front();
        INFO(error.message);
        CHECK(error.code == row.code);
        CHECK(error.message.find(row.needle) != std::string::npos);
        structure::checkNoWrites(fixture);
    }
}

// Source: import.test.ts 1067–1083 (paged fixture), 1142–1152.
TEST_CASE("import structure parity - rtl keeps pageless tab pageless") {
    RemoteImportFixture fixture;
    structure::seedPaged(fixture, "PAGELESS");
    const auto text = structure::importClean(fixture,
        structure::replaceFirst(structure::exportText(fixture), "::section{id=s1}", "::section{id=s1 rtl}"));
    INFO(text);
    CHECK(text.find("\n::section{id=s1 rtl}\n") != std::string::npos);
    const auto lines = structure::tabLines(text);
    REQUIRE(!lines.empty());
    CHECK(lines[0].find("pageful") == std::string::npos);
    const auto& style = structure::remoteTab(fixture, 0)["documentStyle"];
    REQUIRE(style.contains("documentFormat"));
    CHECK(style["documentFormat"]["documentMode"].get<std::string>() == "PAGELESS");
}

// Source: import.test.ts 1067–1083 (paged fixture), 1154–1172.
TEST_CASE("import structure parity - new pageful landscape tab writes LETTER size") {
    RemoteImportFixture fixture;
    structure::seedPaged(fixture, "PAGES");
    const auto before = structure::exportText(fixture);
    INFO(before);
    const auto beforeLines = structure::tabLines(before);
    REQUIRE(!beforeLines.empty());
    REQUIRE(beforeLines[0].ends_with("title=Overview pageful}"));
    const auto text = structure::importClean(fixture, before + "\n::tab{id=\"\" title=Added pageful landscape}\n\nNew\n");
    INFO(text);
    const auto lines = structure::tabLines(text);
    REQUIRE(lines.size() >= 3);
    CHECK(lines[0].ends_with("title=Overview pageful}"));
    CHECK(lines[2].ends_with("title=Added pageful landscape}"));
    // Raw Google key, exact LETTER landscape (not the parsed documentSize, not width > height).
    const auto& style = structure::remoteTab(fixture, 2)["documentStyle"];
    REQUIRE(style.contains("pageSize"));
    const auto& size = style["pageSize"];
    CHECK(size["width"]["magnitude"].as<double>() == 792.0);
    CHECK(size["width"]["unit"].get<std::string>() == "PT");
    CHECK(size["height"]["magnitude"].as<double>() == 612.0);
    CHECK(size["height"]["unit"].get<std::string>() == "PT");
}

// Source: import.test.ts 1182–1232 (jsonFixture, header), 1273–1321.
TEST_CASE("import structure parity - second section header and footer lifecycle") {
    RemoteImportFixture fixture;
    {
        const auto blank = emulator::blank_doc_template("Synthetic", "synthetic_doc");
        slimr::Json json = model::docJsonBuild(model::DocSpec{
            .documentId = "synthetic_doc",
            .title = "Example",
            .tabs = {model::TabSpec{.tabId = "t.0", .title = "Main", .blocks = {
                structure::paragraphOf("One"),
                // defaultHeaderId sits on the second body sectionBreak, not the tab documentStyle.
                model::BlockSpec::make_section_break(slimr::Json::object_t{
                    {"sectionType", "NEXT_PAGE"}, {"defaultHeaderId", "kix.h1"}}),
                structure::paragraphOf("Two"),
            }}},
        });
        auto& docTab = json["tabs"][0]["documentTab"];
        docTab["namedStyles"] = blank["tabs"][0]["documentTab"]["namedStyles"];
        // The original header() leaves an empty documentStyle after moving defaultHeaderId.
        docTab["documentStyle"] = slimr::Json::object_t{};
        docTab["headers"] = slimr::Json::object_t{{"kix.h1", slimr::Json::object_t{
            {"content", slimr::Json::array_t{slimr::Json::object_t{
                {"endIndex", static_cast<int64_t>(6)},
                {"paragraph", slimr::Json::object_t{
                    {"elements", slimr::Json::array_t{slimr::Json::object_t{
                        {"endIndex", static_cast<int64_t>(6)},
                        {"startIndex", static_cast<int64_t>(0)},
                        {"textRun", slimr::Json::object_t{{"content", "Draft\n"}, {"textStyle", slimr::Json::object_t{}}}},
                    }}},
                    {"paragraphStyle", slimr::Json::object_t{{"namedStyleType", "NORMAL_TEXT"}}},
                }},
                {"startIndex", static_cast<int64_t>(0)},
            }}},
            {"headerId", "kix.h1"},
        }}};
        fixture.backend.seed_document("synthetic_doc", json, 1);
    }

    const auto text = structure::exportText(fixture);
    INFO(text);
    REQUIRE(text.find("\nTwo\n\n::header{kind=default section=2}\n\nDraft\n") != std::string::npos);

    const auto edited = structure::importClean(fixture, structure::replaceFirst(text, "Draft", "Final"));
    INFO(edited);
    REQUIRE(edited.find("::header{kind=default section=2}\n\nFinal\n") != std::string::npos);

    const auto footed = structure::importClean(fixture, edited + "::footer{kind=default section=2}\n\nSection footer\n");
    INFO(footed);
    CHECK(footed.find("::footer{kind=default section=2}\n\nSection footer\n") != std::string::npos);
    CHECK(footed.find("::footer{kind=default}") == std::string::npos);

    const auto deleted = structure::importClean(fixture,
        structure::replaceFirst(footed, "::header{kind=default section=2}", "::header{kind=default section=2 delete}"));
    INFO(deleted);
    CHECK(deleted.find("::header") == std::string::npos);
    const auto& after = structure::remoteTab(fixture, 0);
    CHECK((!after.contains("headers") || after["headers"].get<slimr::Json::object_t>().empty()));

    const auto missing = structure::writeAndImport(fixture, deleted + "::header{kind=default section=3}\n\nX\n");
    INFO(missing.summary);
    INFO("status=" << missing.status);
    REQUIRE(missing.errors);
    REQUIRE(!missing.errors->empty());
    INFO("code=" << missing.errors->front().code);
    CHECK(missing.errors->front().message.find("no section 3") != std::string::npos);
}

// Source: import.test.ts 657–690; provider-state checks are a labeled strengthening.
TEST_CASE("import structure parity - created document cleanup leaves authoritative provider state") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.tabs[0].markdown = "Synthetic created content\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    std::vector<std::string> created;
    auto create = fixture.remote.createDocument;
    fixture.remote.createDocument = [&created, create](const std::string& title) {
        auto receipt = create(title);
        created.push_back(receipt["documentId"].get<std::string>());
        return receipt;
    };
    bool cleanupFails = false;
    SUBCASE("confirmed cleanup deletes the created document") {}
    SUBCASE("failed cleanup reports the surviving document") { cleanupFails = true; }
    if (cleanupFails) fixture.remote.deleteFile = [&](const std::string&) {
        ++fixture.deletes;
        throw std::runtime_error("Synthetic cleanup transport failure");
    };
    fixture.beforeBatch = [] { throw std::runtime_error("Synthetic batch transport failure"); };
    const auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    CHECK(result.status == "failed");
    REQUIRE(created.size() == 1);
    if (cleanupFails) {
        REQUIRE(result.doc);
        REQUIRE(result.exportPath);
        CHECK(std::filesystem::weakly_canonical(*result.exportPath) == std::filesystem::weakly_canonical(input.path));
        CHECK(lens::documentFileParse(inputFileRead(input.path).text).frontmatter.doc == result.doc);
        CHECK(std::any_of(result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
            return warning.find("could not be deleted") != std::string::npos;
        }));
        // Weak: the throwing override never touches provider state, so this only confirms it is untouched.
        CHECK(*result.doc == created[0]);
        CHECK_FALSE(fixture.backend.get_doc(*result.doc).deleted);
    } else {
        CHECK(result.writeState == "reverted");
        CHECK(!result.doc);
        CHECK(fixture.deletes == 1);
        // Strengthening: cleanup deleted exactly the created ID (the emulator reports deleted docs as not found).
        CHECK_THROWS_WITH_AS(fixture.backend.get_doc(created[0]), ("File not found: " + created[0]).c_str(),
            std::runtime_error);
        fixture.beforeBatch = nullptr;
        const auto retried = fileImport(input, fixture.remote);
        INFO(retried.summary);
        INFO((retried.errors && !retried.errors->empty() ? retried.errors->front().message : ""));
        CHECK(retried.status == "applied");
        CHECK(retried.doc);
        CHECK(fixture.creates == 2);
    }
}

} // namespace gdocsmith::engine::test
