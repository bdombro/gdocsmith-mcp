#pragma once

#include "app.hpp"
#include "engine/import.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
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

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

// Keep every artifact under a unique synthetic fixture root, including on failure.
struct ImportSafetyFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("gdocsmith-import-safety-" + detail::unique_tmp_suffix());
    std::filesystem::path inputPath = root / "input.md";
    FileServices services;

    ImportSafetyFixture() {
        std::filesystem::create_directories(root);
        services.root = (root / "work").string();
    }

    ImportSafetyFixture(const ImportSafetyFixture &) = delete;
    ImportSafetyFixture &operator=(const ImportSafetyFixture &) = delete;

    ~ImportSafetyFixture() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    static void write(const std::filesystem::path &path, const std::string &text) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out << text;
        out.close();
        if (!out)
            throw std::runtime_error("Could not write synthetic import fixture");
    }
};

inline void importSafetyCheckRefusal(const FileWriteResult &result) {
    // Valid input cannot run because the workflow is unavailable, not malformed.
    CHECK_EQ(result.status, "failed");
    CHECK_EQ(result.writeState, "none");
    CHECK_EQ(result.verification, "notApplicable");
    CHECK(!result.exportPath.has_value());
    CHECK(!result.summary.empty());
    CHECK(result.errors.has_value());
    if (result.errors) {
        CHECK_EQ(result.errors->size(), 1u);
        if (!result.errors->empty()) {
            CHECK_EQ(result.errors->front().code, "operationFailed");
            CHECK(!result.errors->front().message.empty());
        }
    }
}

TEST_CASE("import - providerless existing document refuses and preserves edited input and base") {
    ImportSafetyFixture fixture;
    // t.0 reaches the current providerless path, rather than a missing-tab error.
    const std::string edited =
        "---\ngdocsmith: 1\ndoc: synthetic_doc\nrevision: r1\ntitle: Synthetic\n---\n\n"
        "::tab{id=t.0 title=\"Overview\"}\n\n"
        "**Unsaved synthetic edit**\n";
    const std::string base =
        "---\ngdocsmith: 1\ndoc: synthetic_doc\nrevision: r1\ntitle: Synthetic\n---\n\n"
        "::tab{id=t.0 title=\"Overview\"}\n\n"
        "Original synthetic text\n";
    const auto revisionDir = std::filesystem::path(fixture.services.root) / "exports" / "synthetic_doc" / "r1";
    const auto basePath = revisionDir / "export.orig.md";
    fixture.inputPath = revisionDir / "export.md";
    ImportSafetyFixture::write(fixture.inputPath, edited);
    ImportSafetyFixture::write(basePath, base);
    const auto inputBefore = inputFileRead(fixture.inputPath.string());
    const auto baseBefore = inputFileRead(basePath.string());

    ImportInput input;
    input.path = fixture.inputPath.string();
    const auto result = fileImport(input, fixture.services);

    // Inspect bytes independently of result labels: refusal must not re-export simulation.
    CHECK_EQ(inputFileRead(input.path).text, inputBefore.text);
    CHECK_EQ(inputFileRead(basePath.string()).text, baseBefore.text);
    importSafetyCheckRefusal(result);
}

inline void importSafetyCheckNewDocument(const std::string &baseline) {
    ImportSafetyFixture fixture;
    const std::string content =
        "---\ngdocsmith: 1\ntitle: Synthetic new document\n---\n\n"
        "::tab{id=t.0 title=\"Overview\"}\n\n"
        "**Synthetic new content**\n";
    ImportSafetyFixture::write(fixture.inputPath, content);
    const auto before = inputFileRead(fixture.inputPath.string());

    ImportInput input;
    input.path = fixture.inputPath.string();
    input.newDocStyleBaseline = baseline;
    const auto result = fileImport(input, fixture.services);

    CHECK_EQ(inputFileRead(input.path).text, before.text);
    CHECK(!result.doc.has_value()); // No provider ran; a provisional ID is not a created document.
    CHECK(!std::filesystem::exists(fixture.services.root));
    importSafetyCheckRefusal(result);
}

TEST_CASE("import - providerless generic creation refuses without invented document ID") {
    importSafetyCheckNewDocument("generic");
}

TEST_CASE("import - providerless account creation refuses without invented document ID") {
    importSafetyCheckNewDocument("account");
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

struct RemoteImportFixture : ImportSafetyFixture {
    emulator::FakeGoogle backend;
    ImportServices remote;
    int reads = 0;
    int batches = 0;
    int creates = 0;
    int deletes = 0;
    int commentWrites = 0;
    std::vector<std::string> locks;
    std::vector<std::vector<slimr::Json>> sentRequests;
    std::vector<DriveComment> comments;
    std::function<void()> beforeRead;
    std::function<void()> beforeBatch;

    RemoteImportFixture() {
        remote.root = services.root;
        auto json = emulator::blank_doc_template("Synthetic", "synthetic_doc");
        backend.seed_document("synthetic_doc", json, 1);
        remote.getDocument = [&](const std::string &id) {
            ++reads;
            if (beforeRead)
                beforeRead();
            auto json = backend.get_doc(id).json;
            json["revisionId"] = backend.revision_id(backend.get_doc(id));
            return json;
        };
        remote.getFileMetadata = [&](const std::string &id) {
            return slimr::Json::object_t{{"id", id}, {"name", backend.get_doc(id).json["title"]}};
        };
        remote.createDocument = [&](const std::string &title) {
            ++creates;
            return slimr::Json::object_t{{"documentId", backend.create_document(title)}};
        };
        remote.deleteFile = [&](const std::string &id) {
            ++deletes;
            backend.get_doc(id).deleted = true;
        };
        remote.batchUpdate = [&](const std::string &id, const std::vector<slimr::Json> &requests,
                                 const std::optional<std::string> &revision) {
            ++batches;
            sentRequests.push_back(requests);
            locks.push_back(revision.value_or(""));
            if (beforeBatch)
                beforeBatch();
            return *slimr::parse_json<slimr::Json>(backend.batch_update(id, requests, revision));
        };
        remote.commentsList = [&](const std::string &) { return comments; };
        remote.textExport = [](const std::string &) { return ""; };
        remote.commentSend = [&](const std::string &, const CommentAction &action) -> slimr::Json {
            ++commentWrites;
            if (action.kind == "create") {
                DriveComment comment;
                comment.id = "provider_comment_" + std::to_string(commentWrites);
                comment.content = action.text;
                comment.author = DriveCommentAuthor{.me = true};
                auto receipt = slimr::Json::object_t{{"id", comment.id}, {"content", comment.content.value_or("")}};
                comments.push_back(std::move(comment));
                return receipt;
            }
            return slimr::Json::object_t{};
        };
    }

    lens::DocumentFile file() {
        auto json = backend.get_doc("synthetic_doc").json;
        json["revisionId"] = backend.revision_id(backend.get_doc("synthetic_doc"));
        return exportDocumentFile(model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc"}),
                                  {}, ExportOptions{.sourceVersion = std::nullopt});
    }

    lens::DocumentFile newFile() {
        const auto existing = file();
        lens::DocumentFile fresh;
        fresh.frontmatter.sourceVersion = existing.frontmatter.sourceVersion;
        fresh.frontmatter.title = existing.frontmatter.title;
        for (const auto &tab : existing.tabs) {
            fresh.tabs.push_back(lens::DocumentFileTab{
                .id = tab.id,
                .title = tab.title,
                .markdown = tab.markdown,
                .parent = tab.parent,
            });
        }
        return fresh;
    }

    ImportInput save(lens::DocumentFile file) {
        write(inputPath, lens::documentFileExport(file));
        return ImportInput{.path = inputPath.string()};
    }
};

TEST_CASE("import parity - unavailable opaque tokens are omitted only in newly created tabs") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    bool newTab = true;
    SUBCASE("new tab") {}
    SUBCASE("existing tab remains protected") { newTab = false; }
    if (newTab)
        file.tabs.push_back(lens::DocumentFileTab{.id = "", .title = "Added", .markdown = "Kept :equation{id=unavailable}\n"});
    else
        file.tabs[0].markdown = "Kept :equation{id=unavailable}\n";
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    if (newTab) {
        REQUIRE(result.status == "applied");
        CHECK(result.verification == "match");
        CHECK_FALSE(result.warnings.empty());
        const auto actual = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
        REQUIRE(actual.tabs.size() == 2);
        CHECK(lens::markdownRender(lens::projectionBuild(actual.tabs[1])).find("Kept") != std::string::npos);
        CHECK(model::atomsDeep(actual.tabs[1].blocks).empty());
    } else {
        CHECK(result.status == "refused");
        CHECK(fixture.batches == 0);
    }
}

TEST_CASE("import parity - known created document is reverted after first content verification read fails") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.tabs[0].markdown = "Synthetic verified content\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    const auto original = inputFileRead(input.path).text;
    bool cleanupFails = false;
    SUBCASE("confirmed rollback") {}
    SUBCASE("failed rollback keeps known document and recovery") {
        cleanupFails = true;
        fixture.remote.deleteFile = [&](const std::string &) {
            ++fixture.deletes;
            throw std::runtime_error("Synthetic verification cleanup failed");
        };
    }
    const auto read = fixture.remote.getDocument;
    bool failed = false;
    fixture.remote.getDocument = [&](const std::string &id) {
        const auto json = read(id);
        if (!failed && fixture.creates > 0 && slimr::write_json(json).find("Synthetic verified content") != std::string::npos) {
            failed = true;
            throw std::runtime_error("Synthetic first verification read failed");
        }
        return json;
    };
    const auto result = fileImport(input, fixture.remote);
    REQUIRE(failed);
    CHECK(result.status == "failed");
    if (cleanupFails) {
        CHECK(result.writeState == "unknown");
        CHECK(result.verification == "inconclusive");
        REQUIRE(result.doc);
        REQUIRE(result.exportPath);
        CHECK(lens::documentFileParse(inputFileRead(*result.exportPath).text).frontmatter.doc == result.doc);
        CHECK_FALSE(result.warnings.empty());
    } else {
        CHECK(result.writeState == "reverted");
        CHECK(result.verification == "notApplicable");
        CHECK_FALSE(result.doc);
        CHECK_FALSE(result.exportPath);
        CHECK(inputFileRead(input.path).text == original);
    }
    REQUIRE(result.errors);
    REQUIRE_FALSE(result.errors->empty());
    CHECK(result.errors->front().phase == "verification");
    CHECK(fixture.creates == 1);
    CHECK(fixture.deletes == 1);
    CHECK(fixture.batches > 0);
}

// Source: import.test.ts712–731; transaction success precedes import's own comment read.
TEST_CASE("import parity - posttransaction comment read failure keeps a verified created document") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.tabs[0].markdown = "Synthetic completed transaction\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    bool wrote = false, verifiedRead = false, failed = false;
    fixture.beforeBatch = [&] { wrote = true; };
    const auto read = fixture.remote.getDocument;
    fixture.remote.getDocument = [&](const std::string &id) {
        auto json = read(id);
        if (wrote && slimr::write_json(json).find("Synthetic completed transaction") != std::string::npos)
            verifiedRead = true;
        return json;
    };
    const auto list = fixture.remote.commentsList;
    fixture.remote.commentsList = [&](const std::string &id) {
        if (wrote && !failed) {
            CHECK(verifiedRead);
            failed = true;
            throw std::runtime_error("Synthetic posttransaction comments unavailable");
        }
        return list(id);
    };
    const auto result = fileImport(input, fixture.remote);
    REQUIRE(failed);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "complete");
    CHECK(result.verification == "inconclusive");
    REQUIRE(result.doc);
    REQUIRE(result.exportPath);
    CHECK(std::filesystem::weakly_canonical(*result.exportPath) == std::filesystem::weakly_canonical(input.path));
    CHECK(result.summary.find("content landed") != std::string::npos);
    CHECK(lens::documentFileParse(inputFileRead(*result.exportPath).text).frontmatter.doc == result.doc);
    CHECK(fixture.creates == 1);
    CHECK(fixture.deletes == 0);
    CHECK_FALSE(fixture.backend.get_doc(*result.doc).deleted);
}

// Source: import.test.ts 569–582 (stale) and 657–690 (new-document cleanup).
TEST_CASE("import parity - created document failure cleanup is tracked once with honest recovery") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.tabs[0].markdown = "Synthetic created content\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    const auto intent = inputFileRead(input.path).text;
    bool cleanupFails = false;
    SUBCASE("confirmed cleanup permits a new create from unchanged input") {}
    SUBCASE("unknown cleanup keeps provider identity and exports before retry") { cleanupFails = true; }
    if (cleanupFails)
        fixture.remote.deleteFile = [&](const std::string &) {
            ++fixture.deletes;
            throw std::runtime_error("Synthetic cleanup transport failure");
        };
    fixture.beforeBatch = [] { throw std::runtime_error("Synthetic batch transport failure"); };
    const auto result = fileImport(input, fixture.remote);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    CHECK(result.status == "failed");
    CHECK(fixture.creates == 1);
    CHECK(fixture.deletes == 1);
    CHECK(fixture.batches == 1);
    if (cleanupFails) {
        REQUIRE(result.doc);
        REQUIRE(result.exportPath);
        CHECK(std::filesystem::weakly_canonical(*result.exportPath) == std::filesystem::weakly_canonical(input.path));
        CHECK(lens::documentFileParse(inputFileRead(input.path).text).frontmatter.doc == result.doc);
        CHECK(result.writeState == "unknown");
        CHECK(std::any_of(result.warnings.begin(), result.warnings.end(), [](const auto &warning) {
            return warning.find("could not be deleted") != std::string::npos;
        }));
    } else {
        CHECK(!result.doc);
        CHECK(!result.exportPath);
        CHECK(result.writeState == "reverted");
        CHECK(inputFileRead(input.path).text == intent);
        fixture.beforeBatch = nullptr;
        const auto retried = fileImport(input, fixture.remote);
        INFO(retried.summary);
        if (retried.errors)
            for (const auto &error : *retried.errors)
                INFO(error.message);
        CHECK(retried.status == "applied");
        CHECK(fixture.creates == 2);
        CHECK(fixture.deletes == 1);
    }
}

TEST_CASE("import parity - unknown document creation identity never triggers cleanup or replay") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.tabs[0].markdown = "Synthetic content\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    fixture.remote.createDocument = [&](const std::string &) -> slimr::Json {
        ++fixture.creates;
        return slimr::Json::object_t{};
    };
    const auto intent = inputFileRead(input.path).text;
    const auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "unknown");
    CHECK(!result.doc);
    CHECK(fixture.creates == 1);
    CHECK(fixture.batches == 0);
    CHECK(fixture.deletes == 0);
    CHECK(inputFileRead(input.path).text == intent);
}

// Source: import.test.ts 1235–1320, segment lifecycle and first/even creation distinctions.
TEST_CASE("import parity - default segments create edit require explicit deletion and delete") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].segments.push_back({.kind = "header", .markdown = "Synthetic header\n"});
    file.tabs[0].segments.push_back({.kind = "footer", .markdown = "Synthetic footer\n"});
    auto input = fixture.save(file);
    const auto created = fileImport(input, fixture.remote);
    INFO(created.summary);
    if (created.errors)
        for (const auto &error : *created.errors)
            INFO(error.message);
    REQUIRE(created.status == "applied");
    CHECK(created.verification == "match");
    file = lens::documentFileParse(inputFileRead(input.path).text);
    REQUIRE(file.tabs[0].segments.size() == 2);
    CHECK(fixture.batches >= 3);
    const auto firstId = fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["documentStyle"]["defaultHeaderId"];
    file.tabs[0].segments[0].markdown = "Synthetic revised header\n";
    input = fixture.save(file);
    const auto edited = fileImport(input, fixture.remote);
    INFO(edited.summary);
    if (edited.errors)
        for (const auto &error : *edited.errors)
            INFO(error.message);
    REQUIRE(edited.status == "applied");
    CHECK(fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["documentStyle"]["defaultHeaderId"] == firstId);
    const int beforeMissing = fixture.batches;
    auto missing = lens::documentFileParse(inputFileRead(input.path).text);
    missing.tabs[0].segments.erase(missing.tabs[0].segments.begin());
    const auto refused = fileImport(fixture.save(missing), fixture.remote);
    REQUIRE(refused.errors);
    CHECK(refused.errors->front().code == "tabRequired");
    CHECK(fixture.batches == beforeMissing);
    file = fixture.file();
    for (auto &segment : file.tabs[0].segments)
        segment.isDelete = true;
    const auto deleted = fileImport(fixture.save(file), fixture.remote);
    INFO(deleted.summary);
    if (deleted.errors)
        for (const auto &error : *deleted.errors)
            INFO(error.message);
    REQUIRE(deleted.status == "applied");
    CHECK(fixture.file().tabs[0].segments.empty());
}

TEST_CASE("import parity - first and even segments cannot be invented") {
    for (const auto *type : {"first", "even"}) {
        RemoteImportFixture fixture;
        auto file = fixture.file();
        file.tabs[0].segments.push_back({.kind = "header", .type = type, .markdown = "Synthetic cover\n"});
        const auto result = fileImport(fixture.save(file), fixture.remote);
        REQUIRE(result.errors);
        CHECK(result.errors->front().code == "unsupportedSyntax");
        CHECK(fixture.batches == 0);
    }
}

// Source: import.test.ts 1085–1165, settings resets and page layout are independent of body text.
TEST_CASE("import parity - page geometry settings and named styles apply and reimport unchanged") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    auto &attrs = file.tabs[0].readonlyAttributes;
    attrs.flags.push_back("pageful");
    attrs.flags.push_back("landscape");
    attrs.values.emplace_back("size", "A4");
    attrs.values.emplace_back("margins", "36,40,44,48");
    for (const auto &[name, value] : {std::pair{"headerMargin", "24"}, {"footerMargin", "20"}, {"background", "#FFF4CC"}, {"pageNumberStart", "5"}}) {
        std::erase_if(attrs.values, [&](const auto &entry) { return entry.first == name; });
        attrs.values.emplace_back(name, value);
    }
    std::erase_if(attrs.values, [](const auto &entry) { return entry.first == "namedStyleSet"; });
    attrs.values.emplace_back("namedStyleSet", "1");
    file.frontmatter.namedStyleSets["1"]["HEADING_1"]["fontSize"] = 25;
    file.frontmatter.namedStyleSets["1"]["HEADING_1"]["spaceAbove"] = 30;
    auto input = fixture.save(file);
    const auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    if (result.errors)
        for (const auto &error : *result.errors)
            INFO(error.message);
    REQUIRE(result.status == "applied");
    CHECK(result.verification == "match");
    const auto actual = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json,
                                             model::DocModelParseOptions{.docId = "synthetic_doc"});
    const auto &style = actual.tabs[0].documentStyle;
    CHECK(model::dimensionPt(style["pageSize"]["width"]) > model::dimensionPt(style["pageSize"]["height"]));
    CHECK(model::dimensionPt(style["marginLeft"]) == 48);
    CHECK(model::dimensionPt(style["marginHeader"]) == 24);
    CHECK(style["pageNumberStart"] == slimr::Json(5));
    const auto named = namedStyleSetOf(actual.tabs[0]);
    INFO(slimr::write_json(named["HEADING_1"]));
    CHECK(named["HEADING_1"]["fontSize"].as<double>() == 25.0);
    CHECK(named["HEADING_1"]["spaceAbove"].as<double>() == 30.0);
    const int before = fixture.batches;
    const auto unchanged = fileImport(input, fixture.remote);
    if (fixture.batches != before) {
        INFO(slimr::write_json(fixture.backend.call_log.back().args[1]));
        CHECK(fixture.batches == before);
    }
    INFO(unchanged.summary);
    if (unchanged.errors)
        for (const auto &error : *unchanged.errors)
            INFO(error.message);
    CHECK(unchanged.status == "unchanged");
    CHECK(fixture.batches == before);
}

TEST_CASE("import parity - Node3 and legacy1 formats survive success and uncertain recovery") {
    RemoteImportFixture fixture;
    bool legacy = false;
    SUBCASE("strict source format") {}
    SUBCASE("legacy numeric format") { legacy = true; }
    auto file = fixture.file();
    if (!legacy) {
        auto raw = fixture.backend.get_doc("synthetic_doc").json;
        raw["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
        file = exportDocumentFile(model::docModelParse(raw, {.docId = "synthetic_doc"}));
        CHECK_FALSE(file.frontmatter.styleBaseline);
        CHECK(file.tabs[0].markdown.starts_with("::section{id=s1}"));
    }
    CHECK(file.tabs[0].markdown.find(":style[]{}") == std::string::npos);
    file.tabs[0].markdown += "\nSynthetic format body\n";
    const auto input = fixture.save(file);
    const auto applied = fileImport(input, fixture.remote);
    INFO(applied.summary);
    if (applied.errors)
        for (const auto &error : *applied.errors)
            INFO(error.message);
    REQUIRE(applied.status == "applied");
    auto written = lens::documentFileParse(inputFileRead(input.path).text);
    CHECK(written.frontmatter.sourceVersion == file.frontmatter.sourceVersion);
    CHECK(written.frontmatter.styleBaseline.has_value() == legacy);
    const auto body = written.tabs[0].markdown.find("Synthetic format body");
    REQUIRE(body != std::string::npos);
    written.tabs[0].markdown.replace(body, std::string("Synthetic format body").size(), "Pending format body");
    fixture.save(written);
    const auto pending = inputFileRead(input.path).text;
    const auto before = fixture.batches;
    fixture.beforeBatch = [] { throw std::runtime_error("Synthetic transport loss"); };
    const auto uncertain = fileImport(input, fixture.remote);
    CHECK(uncertain.writeState == "unknown");
    CHECK(uncertain.verification == "inconclusive");
    CHECK(fixture.batches == before + 1);
    CHECK(inputFileRead(input.path).text == pending);
    REQUIRE(uncertain.exportPath);
    const auto recovery = lens::documentFileParse(inputFileRead(*uncertain.exportPath).text);
    CHECK(recovery.frontmatter.sourceVersion == file.frontmatter.sourceVersion);
}

inline void seedSuggestion(RemoteImportFixture &fixture, const std::string &field, const std::string &id) {
    auto &raw = fixture.backend.get_doc("synthetic_doc").json;
    auto &content = raw["tabs"][0]["documentTab"]["body"]["content"];
    content = slimr::Json::array_t{
        content[0],
        slimr::Json::object_t{{"startIndex", 1}, {"endIndex", 11}, {"paragraph", slimr::Json::object_t{{"elements", slimr::Json::array_t{slimr::Json::object_t{{"startIndex", 1}, {"endIndex", 10}, {"textRun", slimr::Json::object_t{{"content", "Synthetic"}, {field, slimr::Json::array_t{id}}}}}, slimr::Json::object_t{{"startIndex", 10}, {"endIndex", 11}, {"textRun", slimr::Json::object_t{{"content", "\n"}}}}}}}}},
    };
}

// Source: import.test.ts 343–392: reject is explicit authority, not implicit removal.
TEST_CASE("import parity - explicit text rejection removes suggestion while implicit loss is refused") {
    RemoteImportFixture fixture;
    bool inserted = false;
    SUBCASE("reject a suggested deletion restores plain text") {}
    SUBCASE("reject a hidden insertion removes its hidden text") { inserted = true; }
    seedSuggestion(fixture, inserted ? "suggestedInsertionIds" : "suggestedDeletionIds", "synthetic_s1");
    auto file = fixture.file();
    REQUIRE(file.suggestions.size() == 1);
    file.suggestions[0].isReject = true;
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    if (result.errors)
        for (const auto &error : *result.errors)
            INFO(error.message);
    REQUIRE(result.status == "applied");
    CHECK(result.verification == "match");
    const auto after = fixture.file();
    CHECK(after.suggestions.empty());
    if (inserted)
        CHECK(after.tabs[0].markdown.find("Synthetic") == std::string::npos);
    else
        CHECK(after.tabs[0].markdown.find("Synthetic") != std::string::npos);
}

// Seeds one paragraph of suggestion-marked runs and returns its exported file text.
inline std::string seedReviewRuns(RemoteImportFixture &fixture, std::vector<model::ParagraphContentSpec> content) {
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0", .title = "Main", .blocks = {model::BlockSpec::make_paragraph({.content = std::move(content)})}}}});
    fixture.backend.seed_document("synthetic_doc", json, 1);
    return lens::documentFileExport(fixture.file());
}

struct ReviewRun {
    std::vector<std::string> del, ins, style;
    std::string text;
    bool operator==(const ReviewRun &) const = default;
};

inline std::ostream &operator<<(std::ostream &out, const ReviewRun &run) {
    auto ids = [&](const char *name, const std::vector<std::string> &values) {
        if (values.empty())
            return;
        out << name << "=[";
        for (const auto &value : values)
            out << value << ";";
        out << "] ";
    };
    ids("del", run.del);
    ids("ins", run.ins);
    ids("style", run.style);
    return out << '"' << run.text << '"';
}

// Backend text runs of the first paragraph, merging neighbors with equal marks (runsOf in the original).
inline std::vector<ReviewRun> reviewRuns(RemoteImportFixture &fixture) {
    const auto &content = fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["body"]["content"];
    std::vector<ReviewRun> out;
    for (const auto &element : content.get<slimr::Json::array_t>()) {
        if (!element.contains("paragraph"))
            continue;
        for (const auto &item : element["paragraph"]["elements"].get<slimr::Json::array_t>()) {
            const auto &run = item["textRun"];
            ReviewRun entry;
            auto list = [&](const char *field, std::vector<std::string> &values) {
                if (run.contains(field) && run[field].is_array())
                    for (const auto &id : run[field].get<slimr::Json::array_t>())
                        values.push_back(id.get<std::string>());
            };
            list("suggestedDeletionIds", entry.del);
            list("suggestedInsertionIds", entry.ins);
            if (run.contains("suggestedTextStyleChanges") && run["suggestedTextStyleChanges"].is_object())
                for (const auto &[id, value] : run["suggestedTextStyleChanges"].get<slimr::Json::object_t>()) {
                    (void)value;
                    entry.style.push_back(id);
                }
            entry.text = run["content"].get<std::string>();
            if (!out.empty() && out.back().del == entry.del && out.back().ins == entry.ins && out.back().style == entry.style)
                out.back().text += entry.text;
            else
                out.push_back(std::move(entry));
        }
        break;
    }
    return out;
}

inline std::string replaced(std::string text, const std::string &from, const std::string &to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

inline model::JsonObject boldSuggestion(const std::string &id) {
    return slimr::Json::object_t{{id, slimr::Json::object_t{{"textStyle", slimr::Json::object_t{{"bold", true}}}}}};
}

// Source: import.test.ts 352–371.
TEST_CASE("import parity - review carry - edit inside a replacement's deletion half keeps the insertion half") {
    RemoteImportFixture fixture;
    const auto markdown = seedReviewRuns(fixture, {"Launch date: ",
                                                   model::ParagraphTextSpec{.text = "March 3", .sugDel = std::vector<std::string>{"s1"}},
                                                   model::ParagraphTextSpec{.text = "March 10", .sugIns = std::vector<std::string>{"s1"}}, " ok"});
    CHECK(lens::markdownRender(lens::markdownParse(lens::documentFileParse(markdown).tabs[0].markdown))
              .find("Launch date: March 3 ok") != std::string::npos);
    CHECK(markdown.find("Launch date: :suggestion{id=s1 start}March 3:suggestion{id=s1 end} ok") != std::string::npos);
    fixture.write(fixture.inputPath, replaced(markdown, "start}March 3:suggestion", "start}April 1:suggestion"));
    const auto result = fileImport(ImportInput{.path = fixture.inputPath.string()}, fixture.remote);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().code + ": " + result.errors->front().message : ""));
    CHECK_FALSE(result.errors);
    CHECK(result.verification == "match");
    CHECK(reviewRuns(fixture) == std::vector<ReviewRun>{
                                     {.text = "Launch date: "}, {.del = {"s1"}, .text = "April 1"}, {.ins = {"s1"}, .text = "March 10"}, {.text = " ok\n"}});
}

// Source: import.test.ts 373–389, plus zero sends before the explicit reject.
TEST_CASE("import parity - review carry - edit crossing a suggestion edge refuses naming it; reject lands plain") {
    RemoteImportFixture fixture;
    const auto markdown = seedReviewRuns(fixture, {model::ParagraphTextSpec{.text = "Fast", .sugStyle = boldSuggestion("s2")}, " and safe"});
    const auto crossed = replaced(markdown, ":suggestion{id=s2 start}Fast:suggestion{id=s2 end} and safe", "Quick or safe");
    fixture.write(fixture.inputPath, crossed);
    const auto refused = fileImport(ImportInput{.path = fixture.inputPath.string()}, fixture.remote);
    INFO((refused.errors && !refused.errors->empty() ? refused.errors->front().code + ": " + refused.errors->front().message : ""));
    CHECK(refused.status == "refused");
    REQUIRE(refused.errors);
    CHECK(refused.errors->front().message.find("removes pending suggestions s2; add reject") != std::string::npos);
    CHECK(fixture.batches == 0);
    CHECK(fixture.sentRequests.empty());
    fixture.write(fixture.inputPath, std::regex_replace(crossed, std::regex(R"((::suggestion\{id=s2[^\n]*)\})"), "$1 reject}",
                                                        std::regex_constants::format_first_only));
    const auto result = fileImport(ImportInput{.path = fixture.inputPath.string()}, fixture.remote);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().code + ": " + result.errors->front().message : ""));
    CHECK_FALSE(result.errors);
    CHECK(reviewRuns(fixture) == std::vector<ReviewRun>{{.text = "Quick or safe\n"}});
}

// Source: import.test.ts 391–408.
TEST_CASE("import parity - review carry - edit inside a style suggestion keeps it on the new text") {
    RemoteImportFixture fixture;
    const auto markdown = seedReviewRuns(fixture, {"Go ",
                                                   model::ParagraphTextSpec{.text = "Fast", .sugStyle = boldSuggestion("s3")}, " now"});
    fixture.write(fixture.inputPath, replaced(markdown, "start}Fast:suggestion", "start}Fuel:suggestion"));
    const auto result = fileImport(ImportInput{.path = fixture.inputPath.string()}, fixture.remote);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().code + ": " + result.errors->front().message : ""));
    CHECK_FALSE(result.errors);
    CHECK(result.verification == "match");
    CHECK(reviewRuns(fixture) == std::vector<ReviewRun>{{.text = "Go "}, {.style = {"s3"}, .text = "Fuel"}, {.text = " now\n"}});
}

TEST_CASE("import parity - page-layout suggestion rejection remains unsupported and sends nothing") {
    RemoteImportFixture fixture;
    auto &tab = fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"];
    tab["suggestedDocumentStyleChanges"] = slimr::Json::object_t{
        {"synthetic_layout", slimr::Json::object_t{{"documentStyle", slimr::Json::object_t{{"flipPageOrientation", true}}}}}};
    auto file = fixture.file();
    REQUIRE(file.suggestions.size() == 1);
    file.suggestions[0].isReject = true;
    const auto result = fileImport(fixture.save(file), fixture.remote);
    REQUIRE(result.errors);
    CHECK(result.errors->front().code == "suggestionConflict");
    CHECK(fixture.batches == 0);
}

// Source: import.test.ts 1665–1680 and 1690–1715.
TEST_CASE("import parity - paragraph and table-row rejects use typed reconcile requests") {
    RemoteImportFixture fixture;
    bool row = false;
    SUBCASE("paragraph style rejection rebreaks and preserves both paragraphs") {}
    SUBCASE("row deletion rejection rebuilds its text and drops every row mark") { row = true; }
    std::vector<model::BlockSpec> blocks;
    if (row) {
        model::TableBlockSpec table;
        table.cells = {
            {{model::BlockSpec::make_paragraph({.content = {"Header"}})}},
            {{model::BlockSpec::make_paragraph({.content = {
                                                    model::ParagraphTextSpec{.text = "Retained", .sugDel = std::vector<std::string>{"synthetic_reject"}}}})}}};
        blocks.push_back(model::BlockSpec::make_table(std::move(table)));
    } else
        blocks.push_back(model::BlockSpec::make_paragraph({.content = {"Alpha"}}));
    blocks.push_back(model::BlockSpec::make_paragraph({.content = {"Bravo"}}));
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0", .title = "Main", .blocks = std::move(blocks)}}});
    auto &content = json["tabs"][0]["documentTab"]["body"]["content"];
    if (row) {
        auto &table = content[1]["table"];
        auto second = table["tableRows"].get<slimr::Json::array_t>().at(1);
        second["suggestedDeletionIds"] = slimr::Json::array_t{"synthetic_reject"};
        auto cell = second["tableCells"].get<slimr::Json::array_t>().at(0);
        cell["suggestedDeletionIds"] = slimr::Json::array_t{"synthetic_reject"};
        second["tableCells"] = slimr::Json::array_t{std::move(cell)};
        table["tableRows"].get<slimr::Json::array_t>().at(1) = std::move(second);
    } else {
        content[1]["paragraph"]["suggestedParagraphStyleChanges"] = slimr::Json::object_t{
            {"synthetic_reject", slimr::Json::object_t{{"paragraphStyle", slimr::Json::object_t{{"alignment", "CENTER"}}}}}};
    }
    fixture.backend.seed_document("synthetic_doc", json, 1);
    auto file = fixture.file();
    REQUIRE(file.suggestions.size() == 1);
    file.suggestions[0].isReject = true;
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    REQUIRE(result.status == "applied");
    CHECK(result.verification == "match");
    CHECK(fixture.file().suggestions.empty());
    const auto after = fixture.backend.get_doc("synthetic_doc").json;
    CHECK(slimr::write_json(after).find("synthetic_reject") == std::string::npos);
    CHECK(slimr::write_json(after).find(row ? "Retained" : "Alpha") != std::string::npos);
    CHECK(slimr::write_json(after).find("Bravo") != std::string::npos);
    CHECK(fixture.batches > 0);
}

TEST_CASE("import parity - rejecting a suggested paragraph break joins without reinserting it") {
    RemoteImportFixture fixture;
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0", .title = "Main", .blocks = {model::BlockSpec::make_paragraph({.content = {"Alpha"}, .newlineSugIns = std::vector<std::string>{"synthetic_break"}}), model::BlockSpec::make_paragraph({.content = {"Bravo"}}), model::BlockSpec::make_paragraph({.content = {"End"}})}}}});
    fixture.backend.seed_document("synthetic_doc", json, 1);
    auto file = fixture.file();
    CHECK(file.tabs[0].markdown.find("Alpha") != std::string::npos);
    REQUIRE(file.suggestions.size() == 1);
    file.suggestions[0].isReject = true;
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    REQUIRE(result.status == "applied");
    const auto after = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    REQUIRE(after.tabs[0].blocks.size() == 2);
    CHECK(after.tabs[0].blocks[0].as_paragraph().inlines[0].text == "AlphaBravo");
    CHECK(model::tabSuggestions(after.tabs[0]).empty());
}

TEST_CASE("import parity - cellStyles frontmatter declaration edit applies on import") {
    RemoteImportFixture fixture;
    model::TableBlockSpec table;
    table.cells = {{{model::BlockSpec::make_paragraph({.content = {"Cell A"}})},
                    {model::BlockSpec::make_paragraph({.content = {"Cell B"}})}}};
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0", .title = "Main", .blocks = {model::BlockSpec::make_table(std::move(table))}}}});
    fixture.backend.seed_document("synthetic_doc", json, 1);
    auto file = fixture.file();
    const auto pos = file.tabs[0].markdown.find("Cell A");
    REQUIRE(pos != std::string::npos);
    file.tabs[0].markdown.replace(pos, std::string("Cell A").size(), "Cell A :cell{.highlight}");
    file.frontmatter.cellStyles["highlight"].backgroundColor = "#FFF4CC";
    file.frontmatter.cellStyles["highlight"].paddingTop = 12;
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    if (result.errors)
        for (const auto &error : *result.errors)
            INFO(error.message);
    REQUIRE(result.status == "applied");
    CHECK(result.verification == "match");
    const auto after = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    REQUIRE(after.tabs[0].blocks.size() >= 1);
    REQUIRE(after.tabs[0].blocks[0].is_table());
    const auto &cellStyle = after.tabs[0].blocks[0].as_table().rows[0].cells[0].style;
    CHECK(cellStyle["paddingTop"]["magnitude"].as<double>() == 12.0);
    CHECK(model::colorHexFromOptional(cellStyle["backgroundColor"]) == std::optional<std::string>("#FFF4CC"));
}

// Source: import.test.ts 1717–1754, with target guard-negative controls for lossy/unknown sources.
TEST_CASE("import parity - image deletion reject recreates known sources with loss guards") {
    RemoteImportFixture fixture;
    bool lossy = false, missing = false, approved = false, fallback = false, insecure = false;
    SUBCASE("lossless known source") {}
    SUBCASE("lossy source needs approval") { lossy = true; }
    SUBCASE("explicit approval permits known lossy source") {
        lossy = true;
        approved = true;
    }
    SUBCASE("unknown source cannot be waived") {
        missing = true;
        approved = true;
    }
    SUBCASE("authoritative HTTPS source URI recreates without invented dimensions") { fallback = true; }
    SUBCASE("authoritative fallback still needs approval for actual losses") {
        fallback = true;
        lossy = true;
    }
    SUBCASE("approved authoritative fallback preserves loss guard provenance") {
        fallback = true;
        lossy = true;
        approved = true;
    }
    SUBCASE("HTTP source URI is not authoritative fallback") {
        fallback = true;
        insecure = true;
    }
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0", .title = "Main", .blocks = {model::BlockSpec::make_paragraph({.content = {model::ParagraphAtomSpec{.type = "image"}, "Picture"}}), model::BlockSpec::make_paragraph({.content = {"End"}})}}}});
    auto &dt = json["tabs"][0]["documentTab"];
    dt["body"]["content"][1]["paragraph"]["elements"][0]["inlineObjectElement"] = slimr::Json::object_t{
        {"inlineObjectId", "synthetic_image"}, {"suggestedDeletionIds", slimr::Json::array_t{"synthetic_image_reject"}}};
    auto embedded = slimr::Json::object_t{
        {"size", slimr::Json::object_t{{"width", model::dimensionFromPt(100)}, {"height", model::dimensionFromPt(50)}}},
        {"imageProperties", missing ? slimr::Json(slimr::Json::object_t{}) : slimr::Json(slimr::Json::object_t{{"contentUri", "https://example.invalid/synthetic.png"}})}};
    if (lossy)
        embedded["rotationAngle"] = 0.5;
    if (fallback)
        embedded["imageProperties"] = slimr::Json::object_t{
            {"sourceUri", insecure ? "http://example.invalid/synthetic-source.png" : "https://example.invalid/synthetic-source.png"}};
    dt["inlineObjects"] = slimr::Json::object_t{
        {"synthetic_image", slimr::Json::object_t{{"objectId", "synthetic_image"},
                                                  {"inlineObjectProperties", slimr::Json::object_t{{"embeddedObject", embedded}}}}}};
    fixture.backend.seed_document("synthetic_doc", json, 1);
    auto file = fixture.file();
    REQUIRE(file.suggestions.size() == 1);
    file.suggestions[0].isReject = true;
    auto input = fixture.save(file);
    if (approved)
        input.allow = {"lossyImageMove"};
    const auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    if (missing || insecure || (lossy && !approved)) {
        REQUIRE(result.errors);
        CHECK(fixture.batches == 0);
        CHECK(result.writeState == "none");
    } else {
        REQUIRE(result.status == "applied");
        CHECK(result.verification == "match");
        CHECK(fixture.file().suggestions.empty());
        const auto actual = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json,
                                                 {.docId = "synthetic_doc"});
        const auto atoms = model::atomsDeep(actual.tabs[0].blocks);
        REQUIRE(atoms.size() == 1);
        CHECK(atoms[0].type == "image");
        const auto createdId = model::googleIdOf(atoms[0]);
        REQUIRE(createdId);
        CHECK(*createdId != "synthetic_image");
        CHECK(actual.tabs[0].inlineObjects.contains(*createdId));
        size_t inserts = 0;
        for (const auto &requests : fixture.sentRequests)
            for (const auto &request : requests) {
                if (!request.contains("insertInlineImage"))
                    continue;
                ++inserts;
                const auto &image = request["insertInlineImage"];
                CHECK(image["uri"] == slimr::Json(fallback ? "https://example.invalid/synthetic-source.png" : "https://example.invalid/synthetic.png"));
                CHECK(image.contains("objectSize") == !fallback);
            }
        CHECK(inserts == 1);
    }
}

// Source: import.test.ts 2034–2105, retaining reference/definition correspondence.
TEST_CASE("import parity - footnotes create phased bodies edit move and remove") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Body[^note]\n\nTail\n";
    file.tabs[0].footnotes.push_back({.label = "note", .markdown = "Synthetic *note*.\n"});
    auto input = fixture.save(file);
    const auto created = fileImport(input, fixture.remote);
    INFO(created.summary);
    INFO((created.errors && !created.errors->empty() ? created.errors->front().message : ""));
    REQUIRE(created.status == "applied");
    CHECK(created.verification == "match");
    file = lens::documentFileParse(inputFileRead(input.path).text);
    REQUIRE(file.tabs[0].footnotes.size() == 1);
    CHECK(file.tabs[0].markdown.find("[^1]") != std::string::npos);
    CHECK(file.tabs[0].footnotes[0].markdown.find("Synthetic") != std::string::npos);
    const auto oldId = footnoteOrder(model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"}).tabs[0])[0];
    file.tabs[0].footnotes[0].markdown = "Revised **note**.\n";
    const auto edited = fileImport(fixture.save(file), fixture.remote);
    INFO((edited.errors && !edited.errors->empty() ? edited.errors->front().message : ""));
    REQUIRE(edited.status == "applied");
    const auto current = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    CHECK(footnoteOrder(current.tabs[0])[0] == oldId);
    file = fixture.file();
    file.tabs[0].footnotes[0].markdown.clear();
    const auto cleared = fileImport(fixture.save(file), fixture.remote);
    INFO((cleared.errors && !cleared.errors->empty() ? cleared.errors->front().message : ""));
    REQUIRE(cleared.status == "applied");
    file = fixture.file();
    REQUIRE(file.tabs[0].footnotes.size() == 1);
    CHECK(file.tabs[0].footnotes[0].markdown.find_first_not_of(" \t\r\n") == std::string::npos);
    file.tabs[0].footnotes[0].markdown = "Revised **note**.\n";
    REQUIRE(fileImport(fixture.save(file), fixture.remote).status == "applied");
    file = fixture.file();
    file.tabs[0].markdown = "Body\n\nTail[^1]\n";
    const auto moved = fileImport(fixture.save(file), fixture.remote);
    INFO((moved.errors && !moved.errors->empty() ? moved.errors->front().message : ""));
    REQUIRE(moved.status == "applied");
    file = fixture.file();
    REQUIRE(file.tabs[0].footnotes.size() == 1);
    CHECK(file.tabs[0].footnotes[0].markdown.find("Revised") != std::string::npos);
    CHECK(footnoteOrder(model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"}).tabs[0])[0] != oldId);
    const auto beforeMissing = fixture.batches;
    auto missing = file;
    missing.tabs[0].footnotes.clear();
    const auto rejected = fileImport(fixture.save(missing), fixture.remote);
    REQUIRE(rejected.errors);
    CHECK(fixture.batches == beforeMissing);
    file.tabs[0].markdown = "Body\n\nTail\n";
    file.tabs[0].footnotes.clear();
    const auto removed = fileImport(fixture.save(file), fixture.remote);
    INFO((removed.errors && !removed.errors->empty() ? removed.errors->front().message : ""));
    REQUIRE(removed.status == "applied");
    CHECK(fixture.file().tabs[0].footnotes.empty());
}

TEST_CASE("import - real adapters apply revision-locked edits and authoritative export") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Synthetic changed text\n";
    auto input = fixture.save(file);
    auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    if (result.errors)
        for (const auto &error : *result.errors)
            INFO(error.message);
    CHECK(result.status == "applied");
    CHECK(result.writeState == "complete");
    CHECK(result.verification == "match");
    CHECK(fixture.batches > 0);
    REQUIRE(!fixture.locks.empty());
    CHECK(fixture.locks.front() == "rev-1");
    CHECK(fixture.reads >= 2);
    auto actual = lens::documentFileParse(inputFileRead(input.path).text);
    CHECK(actual.frontmatter.revision != "rev-1");
    CHECK(actual.tabs[0].markdown.find("Synthetic changed text") != std::string::npos);
}

TEST_CASE("W4 - baseValidate succeeds on a commented document without false staleBase") {
    RemoteImportFixture fixture;
    DriveComment comment;
    comment.id = "c1";
    comment.content = "Needs review";
    comment.author = DriveCommentAuthor{.me = true};
    fixture.comments.push_back(comment);
    auto json = fixture.backend.get_doc("synthetic_doc").json;
    json["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
    const auto exported = exportModel(
        model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc"}),
        fixture.comments, fixture.remote);
    const auto result = fileImport(ImportInput{.path = exported.exportPath}, fixture.remote);
    INFO(result.summary);
    if (result.errors)
        for (const auto &error : *result.errors)
            INFO(error.message);
    CHECK_FALSE(result.errors);
    CHECK(result.status == "unchanged");
}

TEST_CASE("import - no-op performs authoritative reads and zero mutations") {
    RemoteImportFixture fixture;
    auto input = fixture.save(fixture.file());
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "unchanged");
    CHECK(result.writeState == "none");
    CHECK(result.verification == "match");
    CHECK(fixture.reads == 2);
    CHECK(fixture.batches == 0);
    CHECK(fixture.creates == 0);
    CHECK(fixture.commentWrites == 0);
}

TEST_CASE("import - concurrent revision change during no-op refreshes without overwriting input") {
    RemoteImportFixture fixture;
    auto input = fixture.save(fixture.file());
    const auto before = inputFileRead(input.path).text;
    fixture.beforeRead = [&] {
        if (fixture.reads == 2)
            ++fixture.backend.get_doc("synthetic_doc").revision;
    };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "refused");
    CHECK(result.verification == "inconclusive");
    CHECK(result.writeState == "none");
    CHECK(fixture.batches == 0);
    CHECK(result.exportPath.has_value());
    CHECK(inputFileRead(input.path).text == before);
}

TEST_CASE("import - stale frontmatter and immutable original refresh without replacing input") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "unsent edit\n";
    SUBCASE("stale live revision") { file.frontmatter.revision = "rev-0"; }
    SUBCASE("frontmatter revision changed relative to sibling original") {
        const auto dir = std::filesystem::path(fixture.remote.root) / "exports" / "synthetic_doc" / "rev-0";
        fixture.inputPath = dir / "export.md";
        auto original = fixture.file();
        original.frontmatter.revision = "rev-0";
        fixture.write(dir / "export.orig.md", lens::documentFileExport(original));
    }
    auto input = fixture.save(file);
    const auto before = inputFileRead(input.path).text;
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "refused");
    CHECK(result.writeState == "none");
    CHECK(fixture.batches == 0);
    CHECK(inputFileRead(input.path).text == before);
    CHECK(result.exportPath.has_value());
}

TEST_CASE("import - comment approval refuses before any write then executes after readback") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    lens::CommentBlock comment;
    comment.text = "Synthetic comment";
    file.comments.push_back(comment);
    auto input = fixture.save(file);
    auto refused = fileImport(input, fixture.remote);
    CHECK(refused.status == "refused");
    CHECK(fixture.commentWrites == 0);
    CHECK(fixture.batches == 0);
    input.allow = {"commentChanges"};
    const auto send = fixture.remote.commentSend;
    fixture.remote.commentSend = [&, send](const std::string &id, const CommentAction &action) {
        CHECK(fixture.reads >= 3);
        return send(id, action);
    };
    auto applied = fileImport(input, fixture.remote);
    CHECK(applied.status == "applied");
    CHECK(fixture.commentWrites == 1);
    CHECK(fixture.batches == 0);
}

TEST_CASE("import - unknown write refreshes remote and keeps unsent input without blind retry") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Synthetic changed text\n";
    auto input = fixture.save(file);
    const auto before = inputFileRead(input.path).text;
    bool landed = false;
    SUBCASE("transport loss before write") {
        fixture.beforeBatch = [] { throw std::runtime_error("Synthetic transport loss"); };
    }
    SUBCASE("transport loss after write") {
        landed = true;
        fixture.remote.batchUpdate = [&](const std::string &id, const std::vector<slimr::Json> &requests,
                                         const std::optional<std::string> &revision) -> slimr::Json {
            ++fixture.batches;
            fixture.backend.batch_update(id, requests, revision);
            throw std::runtime_error("Synthetic response loss");
        };
    }
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "unknown");
    CHECK(fixture.batches == 1);
    CHECK(result.exportPath.has_value());
    CHECK(inputFileRead(input.path).text == before);
    if (landed) {
        REQUIRE(result.exportPath);
        auto current = lens::documentFileParse(inputFileRead(*result.exportPath).text);
        CHECK(current.tabs[0].markdown.find("Synthetic changed text") != std::string::npos);
        CHECK(current.frontmatter.revision == "rev-2");
    }
}

TEST_CASE("import - known revision rejection refreshes without retry or replacing input") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Outstanding synthetic edit\n";
    auto input = fixture.save(file);
    const auto before = inputFileRead(input.path).text;
    fixture.beforeBatch = [] { throw model::CoreError("concurrentEdits", "Synthetic revision conflict"); };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "refused");
    CHECK(result.writeState == "none");
    CHECK(fixture.batches == 1);
    CHECK(result.exportPath.has_value());
    CHECK(inputFileRead(input.path).text == before);
    REQUIRE(result.errors);
    REQUIRE(!result.errors->empty());
    CHECK(result.errors->front().code == "concurrentEdits");
}

TEST_CASE("import - confirmed content then rejected title reports partial and refreshes") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Synthetic content landed\n";
    file.frontmatter.title = "Renamed";
    fixture.remote.rename = [](const std::string &, const std::string &) -> std::string {
        throw model::CoreError("remoteRejected", "Synthetic rename refusal");
    };
    auto input = fixture.save(file);
    const auto before = inputFileRead(input.path).text;
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "partial");
    CHECK(fixture.batches == 1);
    REQUIRE(result.exportPath);
    auto actual = lens::documentFileParse(inputFileRead(*result.exportPath).text);
    CHECK(actual.tabs[0].markdown.find("Synthetic content landed") != std::string::npos);
    CHECK(actual.frontmatter.title == "Synthetic");
    CHECK(inputFileRead(input.path).text == before);
}

TEST_CASE("import - local export failure does not misreport verified cloud writes as partial") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Synthetic verified content\n";
    auto input = fixture.save(file);
    const auto before = inputFileRead(input.path).text;
    const auto blockedRoot = fixture.root / "not-a-directory";
    fixture.write(blockedRoot, "Synthetic blocking file");
    fixture.beforeBatch = [&] { fixture.remote.root = blockedRoot.string(); };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "complete");
    CHECK(result.verification == "match");
    CHECK(fixture.batches == 1);
    CHECK(inputFileRead(input.path).text == before);
}

TEST_CASE("import - concurrent local input edits survive verified remote success") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    file.tabs[0].markdown = "Synthetic remote intent\n";
    auto input = fixture.save(file);
    const auto changed = inputFileRead(input.path).text + "\nLocal edit during import\n";
    fixture.beforeRead = [&] {
        if (fixture.reads == 2)
            fixture.write(input.path, changed);
    };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "applied");
    CHECK(inputFileRead(input.path).text == changed);
    REQUIRE(result.exportPath);
    CHECK(*result.exportPath != input.path);
}

TEST_CASE("import - uncertain refresh preserves edited canonical working copy and original snapshot") {
    RemoteImportFixture fixture;
    auto base = fixture.file();
    auto exported = exportModel(model::docModelParse(fixture.backend.get_doc("synthetic_doc").json,
                                                     model::DocModelParseOptions{.docId = "synthetic_doc"}),
                                {}, fixture.remote,
                                ExportOptions{.sourceVersion = base.frontmatter.sourceVersion});
    fixture.inputPath = exported.exportPath;
    base.tabs[0].markdown = "Synthetic unimported edit\n";
    auto input = fixture.save(base);
    const auto before = inputFileRead(input.path).text;
    const auto originalPath = fixture.inputPath.parent_path() / "export.orig.md";
    const auto original = inputFileRead(originalPath.string()).text;
    fixture.beforeBatch = [] { throw std::runtime_error("Synthetic transport loss"); };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.writeState == "unknown");
    CHECK(inputFileRead(input.path).text == before);
    CHECK(inputFileRead(originalPath.string()).text == original);
    REQUIRE(result.exportPath);
    CHECK(*result.exportPath != input.path);
    CHECK(inputFileRead(*result.exportPath).text == original);
    struct stat status{};
    REQUIRE(stat(result.exportPath->c_str(), &status) == 0);
    CHECK((status.st_mode & 0777) == 0600);
}

TEST_CASE("import - same-revision partial comments preserve canonical input and immutable original") {
    RemoteImportFixture fixture;
    auto exported = exportModel(model::docModelParse(fixture.backend.get_doc("synthetic_doc").json,
                                                     model::DocModelParseOptions{.docId = "synthetic_doc"}),
                                {}, fixture.remote);
    fixture.inputPath = exported.exportPath;
    const auto originalPath = fixture.inputPath.parent_path() / "export.orig.md";
    const auto original = inputFileRead(originalPath.string()).text;
    auto file = lens::documentFileParse(original);
    for (const auto *text : {"Synthetic first comment", "Synthetic second comment"}) {
        lens::CommentBlock block;
        block.text = text;
        file.comments.push_back(block);
    }
    auto input = fixture.save(file);
    input.allow = {"commentChanges"};
    const auto before = inputFileRead(input.path).text;
    const auto send = fixture.remote.commentSend;
    bool secondUnknown = false;
    SUBCASE("known second-action rejection") {}
    SUBCASE("unconfirmed second-action receipt") { secondUnknown = true; }
    fixture.remote.commentSend = [&, send](const std::string &id, const CommentAction &action) -> slimr::Json {
        if (fixture.commentWrites == 1) {
            ++fixture.commentWrites;
            if (secondUnknown)
                return slimr::Json::object_t{};
            throw model::CoreError("remoteRejected", "Synthetic second comment rejection");
        }
        return send(id, action);
    };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == (secondUnknown ? "unknown" : "partial"));
    CHECK(fixture.batches == 0);
    CHECK(fixture.commentWrites == 2);
    REQUIRE(fixture.comments.size() == 1);
    CHECK(inputFileRead(input.path).text == before);
    CHECK(inputFileRead(originalPath.string()).text == original);
    REQUIRE(result.exportPath);
    CHECK(*result.exportPath != input.path);
    auto actual = lens::documentFileParse(inputFileRead(*result.exportPath).text);
    CHECK(actual.frontmatter.revision == "rev-1");
    REQUIRE(actual.comments.size() == 1);
    CHECK(actual.comments[0].id == fixture.comments[0].id);
    CHECK(actual.comments[0].text == "Synthetic first comment");
}

TEST_CASE("import - unsupported dropped envelope metadata refuses with zero mutations") {
    RemoteImportFixture fixture;
    auto input = fixture.save(fixture.file());
    auto text = inputFileRead(input.path).text;
    text.insert(4, "apiUser: synthetic\n");
    fixture.write(input.path, text);
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "refused");
    CHECK(fixture.batches == 0);
    CHECK(fixture.creates == 0);
    CHECK(fixture.reads >= 1);
}

TEST_CASE("import - matching apiUser succeeds while tampered apiUser refuses") {
    RemoteImportFixture fixture;
    fixture.remote.apiUser = "agent@example.com";
    const auto canonicalModel = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    const auto exported = exportModel(canonicalModel, {}, fixture.remote);
    fixture.inputPath = exported.exportPath;
    auto file = lens::documentFileParse(exported.markdown);
    REQUIRE(file.frontmatter.apiUser == "agent@example.com");
    file.tabs[0].markdown = "Updated body\n";
    auto input = fixture.save(file);
    auto applied = fileImport(input, fixture.remote);
    REQUIRE(applied.status == "applied");

    auto text = inputFileRead(input.path).text;
    const auto pos = text.find("apiUser: agent@example.com");
    REQUIRE(pos != std::string::npos);
    text.replace(pos, std::string("apiUser: agent@example.com").size(), "apiUser: tampered@example.com");
    fixture.write(input.path, text);
    auto refused = fileImport(input, fixture.remote);
    CHECK(refused.status == "refused");

    auto removed = text;
    removed.erase(pos, std::string("apiUser: tampered@example.com\n").size());
    fixture.write(input.path, removed);
    CHECK(fileImport(input, fixture.remote).status == "refused");
}

TEST_CASE("import - every existing comment action requires authoritative outcome not just receipt") {
    for (const auto *kind : {"edit", "delete", "reply", "replyEdit", "replyDelete", "resolve", "reopen"}) {
        for (const bool landed : {false, true}) {
            CAPTURE(kind);
            CAPTURE(landed);
            RemoteImportFixture fixture;
            DriveComment thread;
            thread.id = "existing_comment";
            thread.content = "Synthetic old comment";
            thread.author = DriveCommentAuthor{.me = true};
            thread.resolved = std::string_view(kind) == "reopen";
            thread.replies.push_back(DriveCommentReply{
                .id = "existing_reply",
                .content = "Synthetic old reply",
                .author = DriveCommentAuthor{.me = true},
            });
            fixture.comments.push_back(thread);
            auto json = fixture.remote.getDocument("synthetic_doc");
            auto file = exportDocumentFile(model::docModelParse(json,
                                                                model::DocModelParseOptions{.docId = "synthetic_doc"}),
                                           fixture.comments,
                                           ExportOptions{.commentsIncludeResolved = true});
            REQUIRE(file.comments.size() == 1);
            auto &block = file.comments[0];
            const std::string actionKind = kind;
            if (actionKind == "edit")
                block.text = "Synthetic new text";
            if (actionKind == "delete")
                block.isDelete = true;
            if (actionKind == "reply") {
                lens::CommentReply reply;
                reply.text = "Synthetic new text";
                block.replies.push_back(reply);
            }
            if (actionKind == "replyEdit")
                block.replies[0].text = "Synthetic new text";
            if (actionKind == "replyDelete")
                block.replies[0].isDelete = true;
            if (actionKind == "resolve")
                block.resolved = true;
            if (actionKind == "reopen")
                block.resolved = false;
            fixture.remote.commentSend = [&](const std::string &, const CommentAction &action) -> slimr::Json {
                ++fixture.commentWrites;
                CHECK(action.kind == actionKind);
                const bool existingReply = action.kind == "replyEdit" || action.kind == "replyDelete";
                const auto replyId = existingReply ? "existing_reply" : "provider_action_reply";
                if (landed) {
                    if (action.kind == "delete")
                        fixture.comments.clear();
                    else if (action.kind == "edit")
                        fixture.comments[0].content = action.text;
                    else if (action.kind == "replyDelete")
                        fixture.comments[0].replies.clear();
                    else if (action.kind == "replyEdit")
                        fixture.comments[0].replies[0].content = action.text;
                    else {
                        fixture.comments[0].replies.push_back(DriveCommentReply{
                            .id = replyId,
                            .content = action.text,
                            .action = action.kind == "reply" ? std::nullopt : std::make_optional(action.kind),
                            .author = DriveCommentAuthor{.me = true},
                        });
                        if (action.kind != "reply")
                            fixture.comments[0].resolved = action.kind == "resolve";
                    }
                }
                if (action.kind == "delete" || action.kind == "replyDelete")
                    return slimr::Json::object_t{};
                if (action.kind == "resolve" || action.kind == "reopen") {
                    return slimr::Json::object_t{{"id", replyId}, {"action", action.kind}};
                }
                return slimr::Json::object_t{
                    {"id", action.kind == "edit" ? "existing_comment" : replyId},
                    {"content", action.text.value_or("")}};
            };
            auto input = fixture.save(file);
            input.allow = {"commentChanges"};
            const auto intent = inputFileRead(input.path).text;
            const auto result = fileImport(input, fixture.remote);
            CHECK(result.status == (landed ? "applied" : "failed"));
            CHECK(result.writeState == (landed ? "complete" : "unknown"));
            CHECK(fixture.commentWrites == 1);
            CHECK(fixture.batches == 0);
            if (!landed) {
                CHECK(result.verification == "inconclusive");
                CHECK(inputFileRead(input.path).text == intent);
                REQUIRE(result.exportPath);
                CHECK(*result.exportPath != input.path);
            }
        }
    }
}

TEST_CASE("import - immutable snapshots and orphan footnotes refuse before all cloud mutations") {
    RemoteImportFixture fixture;
    auto file = lens::documentFileParse(lens::documentFileExport(fixture.file()));
    file.tabs[0].markdown = "Synthetic unsent body edit\n";
    bool creating = false;
    SUBCASE("raw settings snapshot change") {
        auto raw = fixture.backend.get_doc("synthetic_doc").json;
        raw["revisionId"] = "rev-1";
        file = exportDocumentFile(model::docModelParse(raw, {.docId = "synthetic_doc"}), {},
                                  ExportOptions{.sourceVersion = std::nullopt});
        REQUIRE(file.frontmatter.styleBaseline);
        (*file.frontmatter.styleBaseline)["tabs"]["t.0"]["documentStyle"]["opaqueSetting"] = "changed";
    }
    SUBCASE("variables snapshot change") { file.frontmatter.variables["synthetic"] = "changed"; }
    SUBCASE("footnote creation") { file.tabs[0].footnotes.push_back({.label = "1", .markdown = "Synthetic footnote\n"}); }
    SUBCASE("new document cannot recreate exported contexts") {
        creating = true;
        file.frontmatter.doc.reset();
        file.frontmatter.revision.reset();
    }
    auto input = fixture.save(file);
    if (creating)
        input.newDocStyleBaseline = "account";
    const auto intent = inputFileRead(input.path).text;
    const auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "refused");
    CHECK(result.writeState == "none");
    CHECK(fixture.batches == 0);
    CHECK(fixture.creates == 0);
    CHECK(fixture.commentWrites == 0);
    CHECK(inputFileRead(input.path).text == intent);
    REQUIRE(result.errors);
    CHECK(result.errors->front().code == "unsupportedSyntax");
}

TEST_CASE("import - recovery uses current Drive title despite same-revision lagging Docs") {
    RemoteImportFixture fixture;
    std::string driveTitle = "Synthetic";
    bool concurrentTitle = false;
    SUBCASE("confirmed rename remains current") {}
    SUBCASE("concurrent Drive rename supersedes confirmed rename") { concurrentTitle = true; }
    fixture.remote.getFileMetadata = [&](const std::string &id) {
        return slimr::Json::object_t{{"id", id}, {"name", driveTitle}};
    };
    auto exported = exportModel(model::docModelParse(fixture.remote.getDocument("synthetic_doc"),
                                                     model::DocModelParseOptions{.docId = "synthetic_doc"}),
                                {}, fixture.remote);
    fixture.inputPath = exported.exportPath;
    const auto canonicalOriginal = fixture.inputPath.parent_path() / "export.orig.md";
    const auto original = inputFileRead(canonicalOriginal.string()).text;
    auto file = lens::documentFileParse(original);
    file.frontmatter.title = "Synthetic confirmed rename";
    lens::CommentBlock block;
    block.text = "Synthetic failed comment";
    file.comments.push_back(block);
    int renames = 0;
    fixture.remote.rename = [&](const std::string &, const std::string &title) {
        ++renames;
        driveTitle = title;
        return title;
    };
    fixture.remote.commentSend = [&](const std::string &, const CommentAction &) -> slimr::Json {
        ++fixture.commentWrites;
        if (concurrentTitle)
            driveTitle = "Synthetic concurrent rename";
        throw model::CoreError("remoteRejected", "Synthetic comment rejection");
    };
    auto input = fixture.save(file);
    input.allow = {"commentChanges"};
    const auto intent = inputFileRead(input.path).text;
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "partial");
    CHECK(renames == 1);
    CHECK(fixture.commentWrites == 1);
    CHECK(inputFileRead(input.path).text == intent);
    CHECK(inputFileRead(canonicalOriginal.string()).text == original);
    REQUIRE(result.exportPath);
    const auto recovered = inputFileRead(*result.exportPath).text;
    const auto recoveryOriginal = std::filesystem::path(*result.exportPath).parent_path() / "export.orig.md";
    CHECK(inputFileRead(recoveryOriginal.string()).text == recovered);
    auto current = lens::documentFileParse(recovered);
    CHECK(current.frontmatter.title == driveTitle);
    CHECK(current.frontmatter.revision == "rev-1");
    CHECK(fixture.backend.get_doc("synthetic_doc").json["title"].get<std::string>() == "Synthetic");
    auto retried = fileImport(ImportInput{.path = *result.exportPath}, fixture.remote);
    CHECK(retried.status == "unchanged");
    CHECK(retried.writeState == "none");
    CHECK(renames == 1);
    CHECK(fixture.commentWrites == 1);
    CHECK(fixture.batches == 0);
}

TEST_CASE("import - created document reports actual provider identity and preserves baseline choice") {
    for (const auto *baseline : {"generic", "account"}) {
        RemoteImportFixture fixture;
        fixture.remote.createDocument = [&](const std::string &title) {
            ++fixture.creates;
            auto id = fixture.backend.create_document(title);
            auto &json = fixture.backend.get_doc(id).json;
            json["tabs"][0]["documentTab"]["namedStyles"]["styles"][0]["textStyle"]["fontSize"]["magnitude"] = 17;
            return slimr::Json::object_t{{"documentId", id}};
        };
        auto file = fixture.newFile();
        file.frontmatter.doc.reset();
        file.frontmatter.revision.reset();
        file.tabs[0].markdown = "Synthetic new content\n";
        auto input = fixture.save(file);
        input.newDocStyleBaseline = baseline;
        auto result = fileImport(input, fixture.remote);
        INFO(baseline, result.summary);
        if (result.errors)
            for (const auto &error : *result.errors)
                INFO(error.message);
        CHECK(result.status == "applied");
        CHECK(result.doc == "doc_1");
        CHECK(fixture.creates == 1);
        REQUIRE(!fixture.locks.empty());
        CHECK(fixture.locks.front() == "rev-1");
        CHECK(lens::documentFileParse(inputFileRead(input.path).text).frontmatter.doc == "doc_1");
        const auto &json = fixture.backend.get_doc("doc_1").json;
        const auto &size = json["tabs"][0]["documentTab"]["namedStyles"]["styles"][0]["textStyle"]["fontSize"]["magnitude"];
        CHECK(std::stod(slimr::write_json(size)) == (std::string_view(baseline) == "generic" ? 11.0 : 17.0));
    }
}

TEST_CASE("import - creation with unknown identity never invents or retries an ID") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.frontmatter.doc.reset();
    file.frontmatter.revision.reset();
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    const auto before = inputFileRead(input.path).text;
    fixture.remote.createDocument = [&](const std::string &) {
        ++fixture.creates;
        return slimr::Json::object_t{};
    };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "unknown");
    CHECK(!result.doc);
    CHECK(fixture.creates == 1);
    CHECK(fixture.batches == 0);
    CHECK(inputFileRead(input.path).text == before);
}

TEST_CASE("import - mismatched authoritative readback never becomes verified success") {
    RemoteImportFixture fixture;
    bool creating = false;
    SUBCASE("existing document") {}
    SUBCASE("known created document is retained") { creating = true; }
    auto file = creating ? fixture.newFile() : fixture.file();
    file.tabs[0].markdown = "Synthetic intended content\n";
    auto input = fixture.save(file);
    if (creating)
        input.newDocStyleBaseline = "generic";
    const auto before = inputFileRead(input.path).text;
    fixture.remote.batchUpdate = [&](const std::string &id, const std::vector<slimr::Json> &requests,
                                     const std::optional<std::string> &revision) {
        ++fixture.batches;
        auto response = *slimr::parse_json<slimr::Json>(fixture.backend.batch_update(id, requests, revision));
        fixture.backend.get_doc(id).json = emulator::blank_doc_template("Synthetic", id);
        return response;
    };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "complete");
    CHECK(result.verification == "mismatch");
    CHECK(fixture.deletes == 0);
    if (creating) {
        REQUIRE(result.doc);
        REQUIRE(result.exportPath);
        CHECK(lens::documentFileParse(inputFileRead(*result.exportPath).text).frontmatter.doc == result.doc);
    } else {
        CHECK_FALSE(result.exportPath.has_value());
        CHECK(inputFileRead(input.path).text == before);
    }
}

TEST_CASE("import - new tab uses provider ID and chains create revision into content") {
    RemoteImportFixture fixture;
    auto file = fixture.file();
    lens::DocumentFileTab added;
    added.id = "";
    added.title = "Synthetic added tab";
    added.markdown = "Synthetic new tab content\n";
    file.tabs.push_back(added);
    auto input = fixture.save(file);
    auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    std::string errors;
    if (result.errors)
        for (const auto &error : *result.errors)
            errors += error.message + "\n";
    INFO(errors);
    REQUIRE(result.status == "applied");
    REQUIRE(fixture.locks.size() >= 2);
    CHECK(fixture.locks[0] == "rev-1");
    CHECK(fixture.locks[1] == "rev-2");
    auto actual = lens::documentFileParse(inputFileRead(input.path).text);
    REQUIRE(actual.tabs.size() == 2);
    CHECK(!actual.tabs[1].id.empty());
    CHECK(actual.tabs[1].id != "new_tab_1");
    const auto provider = fixture.file();
    REQUIRE(provider.tabs.size() == 2);
    CHECK(actual.tabs[1].id == provider.tabs[1].id);
}

TEST_CASE("import - known created identity with unknown content is deleted once without replaying content") {
    RemoteImportFixture fixture;
    auto file = fixture.newFile();
    file.frontmatter.doc.reset();
    file.frontmatter.revision.reset();
    file.tabs[0].markdown = "Synthetic new content\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "account";
    const auto before = inputFileRead(input.path).text;
    fixture.beforeBatch = [] { throw std::runtime_error("Synthetic transport loss"); };
    auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "failed");
    CHECK(result.writeState == "reverted");
    CHECK(!result.doc);
    CHECK(!result.exportPath);
    CHECK(fixture.deletes == 1);
    CHECK(fixture.creates == 1);
    CHECK(fixture.batches == 1);
    CHECK(inputFileRead(input.path).text == before);
}

inline std::string importPng(int width = 10, int height = 10) {
    std::string bytes(25, '\0');
    bytes[0] = static_cast<char>(0x89);
    bytes.replace(1, 3, "PNG");
    for (int i = 0; i < 4; ++i) {
        bytes[16 + i] = static_cast<char>((static_cast<unsigned int>(width) >> (24 - i * 8)) & 255U);
        bytes[20 + i] = static_cast<char>((static_cast<unsigned int>(height) >> (24 - i * 8)) & 255U);
    }
    return bytes;
}

inline void seedImportDropdown(RemoteImportFixture &fixture) {
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0", .title = "Main", .blocks = {model::BlockSpec::make_paragraph({.content = {"Status ", model::ParagraphAtomSpec{.type = "dropdown"}}})}}}});
    auto &tab = json["tabs"][0]["documentTab"];
    tab["body"]["content"][1]["paragraph"]["elements"][1]["dropdown"] = slimr::Json::object_t{
        {"dropdownId", "kix.dd1"}, {"dropdownProperties", slimr::Json::object_t{{"displayValue", "Todo"}, {"dropdownDefinitionId", "kix.list1"}, {"selectedOptionId", "dropdownItem.todo"}}}};
    tab["dropdownDefinitions"] = slimr::Json::object_t{{"kix.list1", slimr::Json::object_t{
                                                                         {"dropdownDefinitionId", "kix.list1"}, {"dropdownDefinitionProperties", slimr::Json::object_t{{"title", "Status"}, {"options", slimr::Json::array_t{slimr::Json::object_t{{"displayValue", "Todo"}, {"optionId", "dropdownItem.todo"}, {"textStyle", slimr::Json::object_t{{"foregroundColor", model::colorOptionalFromHex("#FF0000")}}}}, slimr::Json::object_t{{"displayValue", "Done"}, {"optionId", "dropdownItem.done"}}}}}}}}};
    fixture.backend.seed_document("synthetic_doc", json, 1);
}

// Source: import.test.ts 2149–2265, including same-title definitions keyed by provider identity.
TEST_CASE("import dropdown parity - list creation selection and canonical replay use native identities") {
    RemoteImportFixture fixture;
    seedImportDropdown(fixture);
    auto file = fixture.file();
    REQUIRE(file.frontmatter.dropdowns.contains("t.0"));
    CHECK(file.frontmatter.dropdowns["t.0"]["kix.list1"]["title"] == slimr::Json("Status"));
    const auto unchanged = fileImport(fixture.save(file), fixture.remote);
    INFO(unchanged.summary);
    INFO((unchanged.errors && !unchanged.errors->empty() ? unchanged.errors->front().message : ""));
    CHECK(unchanged.status == "unchanged");
    auto &lists = file.frontmatter.dropdowns["t.0"];
    lists["size"] = slimr::Json::object_t{{"title", "Status"}, {"options", slimr::Json::array_t{"S", "L"}}};
    lists["fruit"] = slimr::Json::object_t{{"options", slimr::Json::array_t{"Apple", "Pear"}}};
    const auto select = file.tabs[0].markdown.find("list=kix.list1}");
    REQUIRE(select != std::string::npos);
    file.tabs[0].markdown.replace(select, std::string("list=kix.list1}").size(), "list=kix.list1 option=Done}");
    file.tabs[0].markdown = "Pick :dropdown{list=size option=L} :dropdown{list=fruit option=Pear}\n\n" + file.tabs[0].markdown;
    const auto input = fixture.save(file);
    const auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    REQUIRE(result.status == "applied");
    CHECK(result.verification == "match");
    const auto actual = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    REQUIRE(actual.tabs[0].dropdownDefinitions.size() == 3);
    CHECK(actual.tabs[0].dropdownDefinitions.contains("kix.list1"));
    size_t sameTitles = 0, defaultTitles = 0;
    for (const auto &[id, definition] : actual.tabs[0].dropdownDefinitions) {
        CHECK(id.starts_with("kix."));
        const auto title = definition["dropdownDefinitionProperties"]["title"].get<std::string>();
        sameTitles += title == "Status";
        defaultTitles += title == "fruit";
    }
    CHECK(sameTitles == 2);
    CHECK(defaultTitles == 1);
    const auto atoms = model::atomsDeep(actual.tabs[0].blocks);
    REQUIRE(atoms.size() == 3);
    for (const auto &atom : atoms) {
        REQUIRE(atom.raw);
        const auto &props = (*atom.raw)["dropdown"]["dropdownProperties"];
        const auto definition = props["dropdownDefinitionId"].get<std::string>();
        REQUIRE(actual.tabs[0].dropdownDefinitions.contains(definition));
        CHECK(lens::optionIdOf(actual.tabs[0].dropdownDefinitions.at(definition),
                               props["displayValue"].get<std::string>()) == std::optional<std::string>{props["selectedOptionId"].get<std::string>()});
    }
    const auto before = fixture.batches;
    CHECK(fileImport(input, fixture.remote).status == "unchanged");
    CHECK(fixture.batches == before);
    auto later = fixture.file();
    later.frontmatter.dropdowns["t.0"]["priority"] = slimr::Json::object_t{{"options", slimr::Json::array_t{"Low", "High"}}};
    later.tabs[0].markdown += "\n:dropdown{list=priority option=High}\n";
    const auto added = fileImport(fixture.save(later), fixture.remote);
    REQUIRE(added.status == "applied");
    const auto final = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    REQUIRE(final.tabs[0].dropdownDefinitions.size() == 4);
    for (const auto &[id, definition] : actual.tabs[0].dropdownDefinitions) {
        REQUIRE(final.tabs[0].dropdownDefinitions.contains(id));
        CHECK(final.tabs[0].dropdownDefinitions.at(id) == definition);
    }
}

TEST_CASE("import dropdown parity - option rename recolor preserves provider IDs") {
    RemoteImportFixture fixture;
    seedImportDropdown(fixture);
    auto file = fixture.file();
    file.frontmatter.dropdowns["t.0"]["kix.list1"]["title"] = "New status";
    file.frontmatter.dropdowns["t.0"]["kix.list1"]["options"][0] =
        slimr::Json::object_t{{"To do", slimr::Json::object_t{{"color", "#00FF00"}}}};
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    REQUIRE(result.status == "applied");
    const auto actual = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    const auto &definition = actual.tabs[0].dropdownDefinitions.at("kix.list1");
    CHECK(lens::optionIdOf(definition, "To do") == std::optional<std::string>{"dropdownItem.todo"});
    CHECK(lens::optionIdOf(definition, "Done") == std::optional<std::string>{"dropdownItem.done"});
    CHECK(definition["dropdownDefinitionProperties"]["title"] == slimr::Json("New status"));
    CHECK(fixture.file().tabs[0].markdown.find(":dropdown[To do]") != std::string::npos);
}

TEST_CASE("import dropdown parity - removed selected option list and invalid cardinality send nothing") {
    RemoteImportFixture fixture;
    seedImportDropdown(fixture);
    auto file = fixture.file();
    SUBCASE("selected option removed") {
        file.frontmatter.dropdowns["t.0"]["kix.list1"]["options"] = slimr::Json::array_t{"Done", "Later"};
    }
    SUBCASE("selected list removed") { file.frontmatter.dropdowns = model::make_json_object(); }
    SUBCASE("too few options") { file.frontmatter.dropdowns["t.0"]["kix.list1"]["options"] = slimr::Json::array_t{"Todo"}; }
    SUBCASE("too many options") {
        slimr::Json::array_t options;
        for (int i = 0; i < 51; ++i)
            options.emplace_back("Option " + std::to_string(i));
        file.frontmatter.dropdowns["t.0"]["kix.list1"]["options"] = std::move(options);
    }
    SUBCASE("duplicate names") { file.frontmatter.dropdowns["t.0"]["kix.list1"]["options"] = slimr::Json::array_t{"Todo", "Todo"}; }
    SUBCASE("unknown option") { file.tabs[0].markdown += "\n:dropdown{list=kix.list1 option=Unknown}\n"; }
    SUBCASE("unknown list") { file.tabs[0].markdown += "\n:dropdown{list=missing option=Unknown}\n"; }
    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    CHECK(result.status == "refused");
    CHECK(result.writeState == "none");
    REQUIRE(result.errors);
    CHECK(fixture.batches == 0);
    CHECK(fixture.creates == 0);
}

// Source: import.test.ts 1917–1955 and import.ts 562–619.
TEST_CASE("import upload parity - three source kinds upload once and clean exact provider identity") {
    RemoteImportFixture fixture;
    bool cleanupFails = false, conversionFails = false;
    SUBCASE("success and unchanged second import") {}
    SUBCASE("cleanup failure retains known identity in warning") { cleanupFails = true; }
    SUBCASE("post-create conversion failure still cleans known identity") { conversionFails = true; }
    fixture.write(fixture.root / "pic.png", importPng());
    int uploads = 0, downloads = 0;
    std::string temporary;
    fixture.remote.fetchImage = [&](const std::string &uri) {
        ++downloads;
        CHECK(uri == "http://example.invalid/pic.png");
        return importPng();
    };
    fixture.remote.imagesUpload = [&](const std::string &, const std::vector<DocxImage> &images) {
        ++uploads;
        REQUIRE(images.size() == 2);
        for (const auto &image : images)
            CHECK(image.bytes == importPng());
        temporary = fixture.backend.create_document("Synthetic image conversion");
        if (conversionFails)
            throw ImagesUploadError("Synthetic conversion read failure", temporary);
        return ImagesUploadResult{.contentUris = {
                                      "https://example.invalid/" + temporary + "/one",
                                      "https://example.invalid/" + temporary + "/two"},
                                  .documentId = temporary};
    };
    fixture.remote.deleteFile = [&](const std::string &id) {
        ++fixture.deletes;
        CHECK(id == temporary);
        if (cleanupFails)
            throw std::runtime_error("Synthetic cleanup failure");
        fixture.backend.get_doc(id).deleted = true;
    };
    if (conversionFails) {
        const auto current = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
        fixture.inputPath = exportModel(current, {}, fixture.remote, ExportOptions{.sourceVersion = std::nullopt}).exportPath;
    }
    const auto sourceImage = fixture.inputPath.parent_path() / "pic.png";
    fixture.write(sourceImage, importPng());
    auto file = fixture.file();
    file.tabs[0].markdown = "Body ![](pic.png) ![](file://" + sourceImage.string() +
                            ") ![](http://example.invalid/pic.png)\n";
    const auto input = fixture.save(file);
    const auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    CHECK(uploads == 1);
    CHECK(downloads == 1);
    CHECK(fixture.deletes == 1);
    REQUIRE(!temporary.empty());
    if (cleanupFails)
        CHECK_FALSE(fixture.backend.get_doc(temporary).deleted);
    else
        CHECK_THROWS(fixture.backend.get_doc(temporary));
    if (conversionFails) {
        CHECK(result.status == "failed");
        CHECK(result.writeState == "unknown");
        CHECK(result.verification == "inconclusive");
        CHECK(fixture.batches == 0);
        CHECK(result.exportPath == std::optional<std::string>{input.path});
        CHECK(inputFileRead(input.path).text == lens::documentFileExport(file));
        CHECK(std::any_of(result.warnings.begin(), result.warnings.end(), [](const auto &warning) {
            return warning == "export.md already had local edits for this revision, so it was kept; delete it and export again for a clean copy.";
        }));
    } else {
        REQUIRE(result.status == "applied");
        CHECK(result.verification == "match");
        const auto actual = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
        const auto atoms = model::atomsDeep(actual.tabs[0].blocks);
        REQUIRE(atoms.size() == 3);
        for (const auto &atom : atoms) {
            const auto id = model::googleIdOf(atom);
            REQUIRE(id);
            CHECK(actual.tabs[0].inlineObjects.contains(*id));
            const auto &uri = actual.tabs[0].inlineObjects.at(*id)["inlineObjectProperties"]["embeddedObject"]["imageProperties"]["sourceUri"];
            CHECK(uri.get<std::string>().find(temporary) != std::string::npos);
        }
        if (cleanupFails) {
            CHECK(std::any_of(result.warnings.begin(), result.warnings.end(),
                              [&](const auto &warning) { return warning.find(temporary) != std::string::npos; }));
        } else
            CHECK(result.warnings.empty());
        const auto before = fixture.batches;
        const auto second = fileImport(input, fixture.remote);
        CHECK(second.status == "unchanged");
        CHECK(fixture.batches == before);
        CHECK(uploads == 1);
        CHECK(fixture.deletes == 1);
    }
}

TEST_CASE("import upload parity - all invalid sources aggregate before any upload or document send") {
    RemoteImportFixture fixture;
    int uploads = 0;
    fixture.write(fixture.root / "bad.png", "Synthetic nonimage");
    fixture.remote.imagesUpload = [&](const std::string &, const std::vector<DocxImage> &) {
        ++uploads;
        return ImagesUploadResult{};
    };
    auto file = fixture.newFile();
    file.tabs[0].markdown = "![](bad.png) ![](missing.png)\n";
    auto input = fixture.save(file);
    input.newDocStyleBaseline = "generic";
    const auto result = fileImport(input, fixture.remote);
    CHECK(result.status == "refused");
    CHECK(result.writeState == "none");
    REQUIRE(result.errors);
    CHECK(result.errors->size() == 2);
    CHECK(uploads == 0);
    CHECK(fixture.batches == 0);
    CHECK(fixture.creates == 0);
    CHECK(fixture.deletes == 0);
}

TEST_CASE("import image parity - foreign image sources are read once and retain native provider identities") {
    RemoteImportFixture fixture;
    auto source = model::docJsonBuild({.documentId = "synthetic_source", .title = "Synthetic source", .tabs = {{.tabId = "t.source", .title = "Source", .blocks = {model::BlockSpec::make_paragraph({.content = {model::ParagraphAtomSpec{.type = "image"}}})}}}});
    auto &tab = source["tabs"][0]["documentTab"];
    tab["body"]["content"][1]["paragraph"]["elements"][0]["inlineObjectElement"]["inlineObjectId"] = "foreign_image";
    tab["inlineObjects"]["foreign_image"] = slimr::Json::object_t{{"inlineObjectProperties", slimr::Json::object_t{
                                                                                                 {"embeddedObject", slimr::Json::object_t{{"imageProperties", slimr::Json::object_t{
                                                                                                                                                                  {"contentUri", "https://example.invalid/foreign.png"}}}}}}}};
    fixture.backend.seed_document("synthetic_source", source, 1);
    const auto sourceModel = model::docModelParse(source, {.docId = "synthetic_source"});
    const auto sourceAtoms = model::atomsDeep(sourceModel.tabs[0].blocks);
    REQUIRE(sourceAtoms.size() == 1);
    const auto sourceToken = lens::atomTokenProject(sourceAtoms[0], sourceModel.tabs[0]);
    const auto exportedId = sourceToken.attrs.get_value("id");
    REQUIRE(exportedId);
    int sourceReads = 0;
    std::vector<std::string> getDocOrder;
    const auto getDocument = fixture.remote.getDocument;
    fixture.remote.getDocument = [&](const std::string &id) {
        getDocOrder.push_back(id);
        if (id == "synthetic_source")
            ++sourceReads;
        return getDocument(id);
    };
    const auto canonicalModel = model::docModelParse(fixture.backend.get_doc("synthetic_doc").json, {.docId = "synthetic_doc"});
    const auto initial = exportModel(canonicalModel, {}, fixture.remote, ExportOptions{.sourceVersion = std::nullopt});
    fixture.inputPath = initial.exportPath;
    const auto originalPath = fixture.inputPath.parent_path() / "export.orig.md";
    const auto originalBytes = inputFileRead(originalPath.string()).text;
    auto file = fixture.file();
    file.tabs[0].markdown = "Target :image{src=gdoc:synthetic_source/" + *exportedId +
                            "} :image{src=gdoc:synthetic_source/" + *exportedId + "}\n";
    auto input = fixture.save(file);
    const auto result = fileImport(input, fixture.remote);
    INFO(result.summary);
    INFO((result.errors && !result.errors->empty() ? result.errors->front().message : ""));
    REQUIRE(result.status == "applied");
    CHECK(result.verification == "match");
    CHECK(sourceReads == 1);
    REQUIRE(getDocOrder.size() >= 2);
    CHECK(getDocOrder[0] == "synthetic_source");
    CHECK(getDocOrder[1] == "synthetic_doc");
    const auto before = fixture.batches;
    CHECK(fileImport(input, fixture.remote).status == "unchanged");
    CHECK(inputFileRead(originalPath.string()).text == originalBytes);
    CHECK(sourceReads == 1);
    CHECK(fixture.batches == before);
    file = fixture.file();
    file.tabs[0].markdown += "\n:image{src=gdoc:synthetic_source/missing}\n";
    const auto missing = fileImport(fixture.save(file), fixture.remote);
    CHECK(missing.status == "refused");
    CHECK(fixture.batches == before);
    REQUIRE(missing.errors);
    CHECK(missing.errors->front().message.find("no single image") != std::string::npos);
    file = fixture.file();
    file.tabs[0].markdown += "\n:image{src=gdoc:synthetic_missing/unknown}\n";
    const auto unreadable = fileImport(fixture.save(file), fixture.remote);
    CHECK(unreadable.status == "refused");
    REQUIRE(unreadable.errors);
    CHECK(unreadable.errors->front().message.find("document synthetic_missing couldn't be read") != std::string::npos);
    CHECK(fixture.batches == before);
}

TEST_CASE("import upload parity - real app adapter fetches bytes without Google authorization and cleans conversion") {
    RemoteImportFixture fixture;
    struct WorkRoot {
        std::optional<std::string> previous;
        explicit WorkRoot(const std::string &path) {
            if (const char *value = std::getenv("GDOCSMITH_WORK_DIR"))
                previous = value;
            setenv("GDOCSMITH_WORK_DIR", path.c_str(), 1);
        }
        WorkRoot(const WorkRoot &) = delete;
        WorkRoot &operator=(const WorkRoot &) = delete;
        ~WorkRoot() {
            if (previous)
                setenv("GDOCSMITH_WORK_DIR", previous->c_str(), 1);
            else
                unsetenv("GDOCSMITH_WORK_DIR");
        }
    } root(fixture.remote.root);
    auto file = fixture.file();
    file.tabs[0].markdown = "Body ![](http://example.invalid/app.png)\n";
    const auto input = fixture.save(file);
    int downloads = 0, uploads = 0, deletes = 0;
    HttpTransport transport = [&](const HttpCall &request) -> HttpCallResponse {
        if (request.url == "http://example.invalid/app.png") {
            ++downloads;
            CHECK(request.headers.empty());
            return {.body = importPng()};
        }
        if (request.url == "https://oauth2.googleapis.com/token") {
            return {.body = R"({"access_token":"synthetic-access","expires_in":3600})"};
        }
        if (request.url.find("upload/drive/v3/files") != std::string::npos) {
            ++uploads;
            CHECK(request.body.find("gdocsmith-upload") != std::string::npos);
            return {.body = R"({"id":"synthetic_conversion"})"};
        }
        if (request.url.find("documents/synthetic_conversion") != std::string::npos) {
            return {.body = R"({"body":{"content":[{"paragraph":{"elements":[{"inlineObjectElement":{"inlineObjectId":"converted"}}]}}]},"inlineObjects":{"converted":{"inlineObjectProperties":{"embeddedObject":{"imageProperties":{"contentUri":"https://example.invalid/converted.png"}}}}}})"};
        }
        if (request.url.find("files/synthetic_conversion") != std::string::npos && request.method == "DELETE") {
            ++deletes;
            return {.status = 204};
        }
        if (request.url.find("/comments") != std::string::npos)
            return {.body = R"({"comments":[]})"};
        if (request.url.find("documents/synthetic_doc:batchUpdate") != std::string::npos) {
            const auto body = *slimr::parse_json<slimr::Json>(request.body);
            return {.body = fixture.backend.batch_update("synthetic_doc",
                                                         body["requests"].get<slimr::Json::array_t>(),
                                                         body["writeControl"]["requiredRevisionId"].get<std::string>())};
        }
        if (request.url.find("documents/synthetic_doc") != std::string::npos) {
            auto document = fixture.backend.get_doc("synthetic_doc").json;
            document["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
            return {.body = slimr::write_json(document)};
        }
        if (request.url.find("files/synthetic_doc") != std::string::npos) {
            return {.body = R"({"id":"synthetic_doc","name":"Synthetic"})"};
        }
        return {.status = 404, .body = R"({"error":{"message":"Synthetic missing route"}})"};
    };
    const GwsRunner runner = [](const std::vector<std::string> &) {
        return R"({"client_id":"synthetic-id","client_secret":"synthetic-secret","refresh_token":"synthetic-refresh"})";
    };
    token_cache_reset();
    const auto result = gdocsmith::detail::file_import(gdocsmith::ImportInput{.path = input.path}, transport, runner, true);
    INFO(slimr::write_json(result));
    CHECK(result["status"] == slimr::Json("applied"));
    CHECK(result["verification"] == slimr::Json("match"));
    CHECK(downloads == 1);
    CHECK(uploads == 1);
    CHECK(deletes == 1);
    token_cache_reset();
}

TEST_CASE("import upload parity - API post-create failure preserves exact temporary identity") {
    token_cache_reset();
    const HttpTransport transport = [](const HttpCall &request) -> HttpCallResponse {
        if (request.url == "https://oauth2.googleapis.com/token") {
            return {.body = R"({"access_token":"synthetic-access","expires_in":3600})"};
        }
        if (request.url.find("upload/drive/v3/files") != std::string::npos) {
            return {.body = R"({"id":"synthetic_known_upload"})"};
        }
        throw std::runtime_error("Synthetic readback transport loss");
    };
    const GwsRunner runner = [](const std::vector<std::string> &) {
        return R"({"client_id":"synthetic-id","client_secret":"synthetic-secret","refresh_token":"synthetic-refresh"})";
    };
    DriveClient drive(transport, runner);
    try {
        drive.imagesUpload("Synthetic conversion", {{.bytes = importPng(), .widthPx = 10, .heightPx = 10, .type = "png"}});
        FAIL("Expected a typed post-create failure");
    } catch (const ImagesUploadError &error) {
        CHECK(error.documentId == "synthetic_known_upload");
    }
    token_cache_reset();
}

} // namespace gdocsmith::engine::test
