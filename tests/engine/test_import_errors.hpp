#pragma once

// Requires engine/test_import.hpp (RemoteImportFixture) to be included first.
#include "engine/import.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace gdocsmith::engine::test {

namespace import_errors {

// `declarations` seeds a styled paragraph so the export carries paragraphStyles frontmatter lines
inline void seedTwoTabs(RemoteImportFixture& fixture, bool declarations = false) {
    const auto blank = emulator::blank_doc_template("Synthetic", "synthetic_doc");
    const auto& blankTab = blank["tabs"][0]["documentTab"];
    model::TabSpec first{.tabId = "t.0", .title = "First Tab"};
    if (declarations) {
        model::ParagraphSpec styled{.content = {std::string("Styled seed")}};
        styled.style = slimr::Json::object_t{{"alignment", "CENTER"}};
        first.blocks.push_back(model::BlockSpec::make_paragraph(std::move(styled)));
    }
    slimr::Json json = model::docJsonBuild(model::DocSpec{
        .documentId = "synthetic_doc",
        .title = "Synthetic",
        .tabs = {first, model::TabSpec{.tabId = "t.1", .title = "Second Tab"}},
    });
    for (std::size_t i = 0; i < 2; ++i) {
        auto& docTab = json["tabs"][i]["documentTab"];
        docTab["documentStyle"] = blankTab["documentStyle"];
        docTab["namedStyles"] = blankTab["namedStyles"];
    }
    fixture.backend.seed_document("synthetic_doc", json, 1);
    fixture.backend.seed_document("synthetic_source", emulator::blank_doc_template("Source", "synthetic_source"), 1);
}

// 1-based lines of the full saved file (frontmatter included) whose text matches
inline std::vector<int> fileLinesOf(const std::string& text, const std::function<bool(const std::string&)>& match) {
    std::vector<int> lines;
    std::istringstream stream(text);
    std::string line;
    int number = 0;
    while (std::getline(stream, line)) {
        ++number;
        if (match(line)) lines.push_back(number);
    }
    return lines;
}

inline int fileLineOf(const std::string& text, const std::function<bool(const std::string&)>& match) {
    const auto lines = fileLinesOf(text, match);
    REQUIRE(lines.size() == 1);
    return lines.front();
}

// Lines through the closing frontmatter delimiter
inline int fmLinesOf(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    int number = 0;
    int fences = 0;
    while (fences < 2 && std::getline(stream, line)) {
        ++number;
        if (number == 1 && line != "---") return 0;
        if (line == "---") ++fences;
    }
    return fences == 2 ? number : 0;
}

inline auto startsWith(std::string prefix) {
    return [prefix = std::move(prefix)](const std::string& line) { return line.starts_with(prefix); };
}

struct Variant {
    bool declarations = false;
    std::string firstTabExtra;
    std::string secondTabExtra;
    bool sourceEnvelope = false;
};

struct Outcome {
    int batches = 0;
    FileWriteResult result;
    std::string text;
    int uploads = 0;
};

inline Outcome importWith(const Variant& variant) {
    RemoteImportFixture fixture;
    seedTwoTabs(fixture, variant.declarations);
    int uploads = 0;
    fixture.remote.imagesUpload = [&](const std::string&, const std::vector<DocxImage>&) {
        ++uploads;
        return ImagesUploadResult{};
    };
    const auto canonical = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    const auto initial = exportModel(canonical, {}, fixture.remote, variant.sourceEnvelope
        ? ExportOptions{} : ExportOptions{.sourceVersion = std::nullopt});
    fixture.inputPath = initial.exportPath;
    auto file = variant.sourceEnvelope ? lens::documentFileParse(inputFileRead(initial.exportPath).text) : fixture.file();
    REQUIRE(file.tabs.size() == 2);
    file.tabs[0].markdown += variant.firstTabExtra;
    file.tabs[1].markdown += variant.secondTabExtra;
    const auto input = fixture.save(file);
    const auto batchesBefore = fixture.batches;
    Outcome outcome;
    outcome.result = fileImport(input, fixture.remote);
    outcome.text = inputFileRead(input.path).text;
    outcome.batches = fixture.batches - batchesBefore;
    outcome.uploads = uploads;
    return outcome;
}

inline std::string markerImage(const std::string& src) { return "\n\nBefore marker :image{src=" + src + "}\n"; }

inline bool localized(const ResultError& error, int batches, int uploads, int expectedLine) {
    return error.code == "anchorNotFound" && error.tab == std::optional<std::string>("Second Tab") &&
        error.line == std::optional<int>(expectedLine) && batches == 0 && uploads == 0;
}

inline const ResultError& onlyError(const Outcome& outcome) {
    INFO(outcome.result.summary);
    REQUIRE(outcome.result.errors);
    REQUIRE(outcome.result.errors->size() == 1);
    return outcome.result.errors->front();
}

} // namespace import_errors

TEST_CASE("import errors - image refusals report the tab and full-file line of the failing block") {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"gdoc:synthetic_source/unknown", "no single image"},
        {"gdoc:synthetic_missing/unknown", "document synthetic_missing couldn't be read"},
    };
    for (const auto& [src, message] : cases) {
        SUBCASE(src.c_str()) {
            const auto outcome = import_errors::importWith({.secondTabExtra = import_errors::markerImage(src)});
            const auto& result = outcome.result;
            CHECK(result.status == "refused");
            CHECK(result.writeState == "none");
            const auto& error = import_errors::onlyError(outcome);
            CHECK(error.code == "anchorNotFound");
            CHECK(error.message.find(message) != std::string::npos);
            CHECK(error.phase == std::optional<std::string>("planning"));
            CHECK(outcome.batches == 0);
            CHECK(outcome.uploads == 0);
            const auto blockLine = import_errors::fileLineOf(outcome.text, import_errors::startsWith("Before marker"));
            CHECK(error.tab == std::optional<std::string>("Second Tab"));
            CHECK(error.line == std::optional<int>(blockLine));
            CHECK(import_errors::localized(error, outcome.batches, outcome.uploads, blockLine));
        }
    }
}

TEST_CASE("import errors - image refusal in a multi-line paragraph reports its first line") {
    const auto outcome = import_errors::importWith({
        .secondTabExtra = "\n\nLead line\nsecond :image{src=gdoc:synthetic_source/unknown}\n"});
    const auto& error = import_errors::onlyError(outcome);
    const auto lead = import_errors::fileLineOf(outcome.text, [](const auto& line) { return line == "Lead line"; });
    const auto second = import_errors::fileLineOf(outcome.text, import_errors::startsWith("second :image"));
    CHECK(second == lead + 1);
    CHECK(import_errors::localized(error, outcome.batches, outcome.uploads, lead));
}

TEST_CASE("import errors - image refusal line follows frontmatter and earlier-tab length") {
    const auto image = import_errors::markerImage("gdoc:synthetic_source/unknown");
    const std::string longer = "\nExtra paragraph one.\n\nExtra paragraph two.\n\nExtra paragraph three.\n";
    std::vector<int> reported;
    std::vector<int> frontmatter;
    for (const bool declarations : {false, true}) {
        for (const auto& extra : {std::string(), longer}) {
            CAPTURE(declarations);
            CAPTURE(extra.size());
            const auto outcome = import_errors::importWith({
                .declarations = declarations, .firstTabExtra = extra, .secondTabExtra = image});
            const auto& error = import_errors::onlyError(outcome);
            const auto blockLine = import_errors::fileLineOf(outcome.text, import_errors::startsWith("Before marker"));
            CHECK(import_errors::localized(error, outcome.batches, outcome.uploads, blockLine));
            reported.push_back(error.line.value_or(0));
            frontmatter.push_back(import_errors::fmLinesOf(outcome.text));
        }
    }
    REQUIRE(reported.size() == 4);
    CHECK(frontmatter[0] > 0);
    CHECK(frontmatter[2] > frontmatter[0]);
    CHECK(reported[1] != reported[0]);
    CHECK(reported[2] != reported[0]);
    CHECK(reported[3] != reported[2]);
}

TEST_CASE("import errors - image refusal line with an opening section envelope") {
    const auto outcome = import_errors::importWith({
        .secondTabExtra = import_errors::markerImage("gdoc:synthetic_source/unknown"), .sourceEnvelope = true});
    CHECK(import_errors::fileLinesOf(outcome.text, import_errors::startsWith("::section{")).size() == 2);
    const auto& error = import_errors::onlyError(outcome);
    const auto blockLine = import_errors::fileLineOf(outcome.text, import_errors::startsWith("Before marker"));
    CHECK(import_errors::localized(error, outcome.batches, outcome.uploads, blockLine));
}

TEST_CASE("import errors - tab and comment header problems report full-file lines") {
    RemoteImportFixture fixture;
    import_errors::seedTwoTabs(fixture, true);
    const auto canonical = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    const auto initial = exportModel(canonical, {}, fixture.remote, ExportOptions{.sourceVersion = std::nullopt});
    fixture.inputPath = initial.exportPath;

    SUBCASE("duplicate tab title") {
        auto file = fixture.file();
        REQUIRE(file.tabs.size() == 2);
        file.tabs[1].title = "first tab";
        const auto input = fixture.save(file);
        const auto result = fileImport(input, fixture.remote);
        const auto text = inputFileRead(input.path).text;
        const auto lines = import_errors::fileLinesOf(text, import_errors::startsWith("::tab"));
        const auto fmLines = import_errors::fmLinesOf(text);
        CHECK(result.status == "refused");
        REQUIRE(result.errors);
        REQUIRE(!result.errors->empty());
        REQUIRE(lines.size() == 2);
        REQUIRE(fmLines > 0);
        const auto& error = result.errors->front();
        CHECK(error.code == "tabTitleTaken");
        CHECK(error.tab == std::optional<std::string>("first tab"));
        CHECK(error.line == std::optional<int>(lines[1]));
        CHECK(error.line != std::optional<int>(lines[1] - fmLines));
    }
    SUBCASE("new comment without text") {
        const auto saved = fixture.save(fixture.file());
        RemoteImportFixture::write(saved.path, inputFileRead(saved.path).text + "\n::comment\n");
        const auto input = saved;
        const auto result = fileImport(input, fixture.remote);
        const auto text = inputFileRead(input.path).text;
        const auto line = import_errors::fileLineOf(text, import_errors::startsWith("::comment"));
        const auto fmLines = import_errors::fmLinesOf(text);
        CHECK(result.status == "refused");
        CHECK(fixture.batches == 0);
        REQUIRE(result.errors);
        REQUIRE(result.errors->size() == 1);
        REQUIRE(fmLines > 0);
        const auto& error = result.errors->front();
        CHECK(error.message.find("a new comment needs text") != std::string::npos);
        CHECK(error.line == std::optional<int>(line));
        CHECK(error.line != std::optional<int>(line - fmLines));
    }
}

TEST_CASE("import errors - block source lines align with parsed blocks") {
    const auto projection = lens::markdownParse("\n\n# Title\n\nLead\nwrapped\n\n- item\n\n```\ncode\n```\n\n| a |\n| --- |\n| b |\n");
    REQUIRE(projection.blockLines.lines.size() == projection.blocks.size());
    CHECK(projection.blockLines.lines == std::vector<int>{3, 5, 8, 11, 14});
}

TEST_CASE("import errors - localization predicate rejects mutated outcomes") {
    const auto outcome = import_errors::importWith({.secondTabExtra = import_errors::markerImage("gdoc:synthetic_source/unknown")});
    const auto error = import_errors::onlyError(outcome);
    const auto blockLine = import_errors::fileLineOf(outcome.text, import_errors::startsWith("Before marker"));
    const auto directives = import_errors::fileLinesOf(outcome.text, import_errors::startsWith("::tab"));
    const auto fmLines = import_errors::fmLinesOf(outcome.text);
    REQUIRE(directives.size() == 2);
    REQUIRE(fmLines > 0);
    REQUIRE(directives[1] != blockLine);
    CHECK(import_errors::localized(error, outcome.batches, outcome.uploads, blockLine));

    // C-P2-a: tab reset; C-P2-b(iv): wrong tab
    auto mutated = error;
    mutated.tab.reset();
    CHECK_FALSE(import_errors::localized(mutated, outcome.batches, outcome.uploads, blockLine));
    mutated.tab = "First Tab";
    CHECK_FALSE(import_errors::localized(mutated, outcome.batches, outcome.uploads, blockLine));

    // C-P2-b(i) body-relative, (ii) directive line, (iii) off by one
    for (const auto line : {blockLine - fmLines, directives[1], blockLine - 1, blockLine + 1}) {
        mutated = error;
        mutated.line = line;
        CHECK_FALSE(import_errors::localized(mutated, outcome.batches, outcome.uploads, blockLine));
    }

    // C-P2-c: any send
    CHECK_FALSE(import_errors::localized(error, outcome.batches + 1, outcome.uploads, blockLine));
    CHECK_FALSE(import_errors::localized(error, outcome.batches, outcome.uploads + 1, blockLine));
}

} // namespace gdocsmith::engine::test
