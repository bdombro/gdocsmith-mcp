#pragma once

#include "engine/import.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace gdocsmith::engine::test {

TEST_CASE("import - lastTabKeep preserves single tab") {
    lens::DocumentFile doc;
    lens::DocumentFileTab t0;
    t0.id = "t.0";
    t0.title = "Sole Tab";
    t0.isDelete = true;
    doc.tabs = {t0};

    std::vector<std::string> notes;
    lastTabKeep(doc, notes);

    CHECK(!doc.tabs[0].isDelete);
    CHECK_EQ(doc.tabs[0].title, "Tab 1");
    CHECK_EQ(notes.size(), 1u);
}

TEST_CASE("import - tabTitlesValidate rejects duplicate titles") {
    lens::DocumentFile file;
    lens::DocumentFileTab t1;
    t1.id = "t.0";
    t1.title = "Overview";
    t1.line = 10;

    lens::DocumentFileTab t2;
    t2.id = "t.1";
    t2.title = "overview "; // duplicate case-insensitive
    t2.line = 20;

    file.tabs = {t1, t2};

    model::DocModel current;
    model::TabModel m1;
    m1.tabId = "t.0";
    m1.title = "Overview";
    current.tabs = {m1};

    std::vector<model::FileProblem> problems;
    tabTitlesValidate(file, current, problems);

    CHECK_EQ(problems.size(), 1u);
    CHECK_EQ(problems[0].code, std::string(model::error_codes::TAB_TITLE_TAKEN));
    CHECK_EQ(problems[0].line, std::optional<int>(20));
}

TEST_CASE("import - tabLinesValidate hierarchy and missing tabs") {
    lens::DocumentFile file;
    lens::DocumentFileTab t1;
    t1.id = "t.0";
    t1.title = "Overview";
    t1.line = 10;

    lens::DocumentFileTab t2;
    t2.id = "t.1";
    t2.title = "Child";
    t2.parent = "nonexistent";
    t2.line = 20;

    file.tabs = {t1, t2};

    model::DocModel current;
    model::TabModel m1;
    m1.tabId = "t.0";
    m1.title = "Overview";
    model::TabModel m3;
    m3.tabId = "t.3";
    m3.title = "Required Tab";
    current.tabs = {m1, m3};

    std::vector<model::FileProblem> problems;
    auto deleted = tabLinesValidate(file, current, problems);

    CHECK_EQ(deleted.size(), 0u);
    // 1 problem for missing t.3 (tabRequired), 1 for parent nonexistent (tabNotFound)
    CHECK_GE(problems.size(), 2u);
}

TEST_CASE("import - commentActionsPlan validation") {
    lens::DocumentFile file;
    lens::CommentBlock c1;
    c1.id = "c1";
    c1.text = "Updated comment text";
    c1.mine = true;
    c1.line = 30;
    file.comments = {c1};

    lens::DocumentFile exported;
    lens::CommentBlock exp_c1;
    exp_c1.id = "c1";
    exp_c1.text = "Original comment text";
    exp_c1.mine = true;
    exp_c1.line = 30;
    exported.comments = {exp_c1};

    auto actions = commentActionsPlan(file, exported);
    CHECK_EQ(actions.size(), 1u);
    CHECK_EQ(actions[0].kind, "edit");
    CHECK_EQ(actions[0].id, std::optional<std::string>("c1"));
    CHECK_EQ(actions[0].text, std::optional<std::string>("Updated comment text"));
}

TEST_CASE("import - suggestionBlocksValidate rejects editing suggestion body") {
    lens::DocumentFile file;
    lens::SuggestionBlock s1;
    s1.id = "s1";
    s1.kind = "text";
    s1.before = "edited before";
    s1.after = "new";
    s1.line = 50;
    file.suggestions = {s1};

    lens::DocumentFile exported;
    lens::SuggestionBlock exp_s1;
    exp_s1.id = "s1";
    exp_s1.kind = "text";
    exp_s1.before = "original before";
    exp_s1.after = "new";
    exp_s1.line = 50;
    exported.suggestions = {exp_s1};

    CHECK_THROWS_AS(suggestionBlocksValidate(file, exported), model::FileProblems);
}

TEST_CASE("import - newDocStyleBaseline validation") {
    auto tmpDir = std::filesystem::temp_directory_path() / ("gdocsmith-import-test-" + detail::unique_tmp_suffix());
    std::filesystem::create_directories(tmpDir);

    auto filePath = tmpDir / "test_doc.md";
    std::string content =
        "---\n"
        "gdocsmith: 1\n"
        "title: New Doc\n"
        "---\n\n"
        "::tab{id=t.0 title=\"Overview\"}\n\n"
        "Some text here\n";

    {
        std::ofstream ofs(filePath);
        ofs << content;
    }

    // Creating document without newDocStyleBaseline throws
    ImportInput in1;
    in1.path = filePath.string();
    CHECK_THROWS_AS(fileImport(in1), model::CoreError);

    // Creating document with newDocStyleBaseline succeeds
    ImportInput in2;
    in2.path = filePath.string();
    in2.newDocStyleBaseline = "generic";
    auto res = fileImport(in2);
    CHECK_EQ(res.status, "applied");
    CHECK(res.doc.has_value());
    CHECK(std::filesystem::exists(*res.exportPath));

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

TEST_CASE("import - existing doc revision required") {
    auto tmpDir = std::filesystem::temp_directory_path() / ("gdocsmith-import-test2-" + detail::unique_tmp_suffix());
    std::filesystem::create_directories(tmpDir);

    auto filePath = tmpDir / "existing_doc.md";
    std::string content =
        "---\n"
        "doc: doc123\n"
        "gdocsmith: 1\n"
        "title: Existing Doc\n"
        "---\n\n"
        "::tab{id=t.0 title=\"Overview\"}\n\n"
        "Some text here\n";

    {
        std::ofstream ofs(filePath);
        ofs << content;
    }

    // Missing revision throws staleBase
    ImportInput in;
    in.path = filePath.string();
    CHECK_THROWS_AS(fileImport(in), model::CoreError);

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

} // namespace gdocsmith::engine::test
