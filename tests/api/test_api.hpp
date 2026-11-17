#pragma once

#include "api.hpp"
#include "app.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <mcp_http.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <string>
#include <vector>

TEST_CASE("auth cache source contract coalesces concurrent refreshes and preserves pinned private state") {
    using namespace gdocsmith;
    struct Fixture {
        std::filesystem::path root = std::filesystem::current_path() / "build" / "auth-cache-tests" / engine::detail::unique_tmp_suffix();
        std::optional<std::string> previous;
        Fixture() {
            if (const char *value = getenv("GDOCSMITH_CONFIG_DIR"))
                previous = value;
            std::filesystem::create_directories(root);
            setenv("GDOCSMITH_CONFIG_DIR", root.c_str(), 1);
            token_cache_reset();
        }
        ~Fixture() {
            token_cache_reset();
            if (previous)
                setenv("GDOCSMITH_CONFIG_DIR", previous->c_str(), 1);
            else
                unsetenv("GDOCSMITH_CONFIG_DIR");
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
    } fixture;
    const auto config = fixture.root / "config.json";
    const std::string before = R"({"oauthToken":{"access_token":"synthetic-private","expires_at":1}})";
    engine::detail::atomic_write(config.string(), before);
    bool failing = false;
    SUBCASE("shared success, reuse and a later forced refresh") {}
    SUBCASE("shared failure wakes both callers and a later attempt can recover") { failing = true; }
    std::mutex gate;
    std::condition_variable changed;
    bool released = false;
    std::atomic<int> attempts{0};
    std::atomic<int> exports{0};
    bool denyExport = false;
    std::vector<std::string> exportedArgs;
    GwsRunner runner = [&](const std::vector<std::string> &args) {
        ++exports;
        exportedArgs = args;
        if (denyExport)
            throw std::runtime_error("synthetic credential export unavailable");
        return R"({"client_id":"synthetic","client_secret":"synthetic","refresh_token":"synthetic"})";
    };
    HttpTransport transport = [&](const HttpCall &) -> HttpCallResponse {
        const auto attempt = ++attempts;
        std::unique_lock lock(gate);
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
        if (failing)
            throw std::runtime_error("ENOTFOUND");
        return {.body = attempt == 1
                            ? R"({"access_token":"synthetic-initial","expires_in":3600})"
                            : R"({"access_token":"synthetic-later","expires_in":3600})"};
    };
    std::array<std::string, 2> values;
    std::array<std::string, 2> failures;
    auto call = [&](size_t index) {
        try {
            values[index] = valid_access_token_get(false, transport, runner);
        } catch (const std::exception &error) {
            failures[index] = error.what();
        }
    };
    std::jthread first([&] { call(0); });
    bool entered = false;
    {
        std::unique_lock lock(gate);
        entered = changed.wait_for(lock, std::chrono::seconds(3), [&] { return attempts.load() > 0; });
    }
    std::jthread second([&] { call(1); });
    bool joined = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard lock(detail::g_auth_mutex);
            // Global, leader and waiter ownership proves enrollment before releasing transport.
            joined = detail::g_pending_refresh && detail::g_pending_refresh.use_count() >= 3;
        }
        if (joined || attempts.load() > 1)
            break;
        std::this_thread::yield();
    }
    {
        std::lock_guard lock(gate);
        released = true;
    }
    changed.notify_all();
    first.join();
    second.join();
    CHECK(entered);
    CHECK(joined);
    CHECK(attempts == 1);
    CHECK(exports == 1);
    CHECK(exportedArgs == std::vector<std::string>{"auth", "export", "--unmasked"});
    if (failing) {
        CHECK(values[0].empty());
        CHECK(values[1].empty());
        CHECK(failures[0] == "Google OAuth token refresh failed: ENOTFOUND");
        CHECK(failures[1] == failures[0]);
        CHECK(!detail::g_cached_token);
    } else {
        CHECK(failures[0].empty());
        CHECK(failures[1].empty());
        CHECK(values[0] == "synthetic-initial");
        CHECK(values[1] == values[0]);
        CHECK(valid_access_token_get(false, transport, runner) == "synthetic-initial");
        CHECK(attempts == 1);
    }
    failing = false;
    denyExport = true;
    CHECK(valid_access_token_get(true, transport, runner) == "synthetic-later");
    CHECK(attempts == 2);
    CHECK(exports == 1);
    CHECK(valid_access_token_get(false, transport, runner) == "synthetic-later");
    CHECK(attempts == 2);
    CHECK(engine::detail::file_text_read(config.string()) == std::optional<std::string>{before});
}

TEST_CASE("auth refresh source contract adds context to an offline transport failure") {
    using namespace gdocsmith;
    token_cache_reset();
    const GwsCredentials credentials{
        .clientId = "synthetic",
        .clientSecret = "synthetic",
        .refreshToken = "synthetic",
    };
    int calls = 0;
    HttpTransport offline = [&](const HttpCall &) -> HttpCallResponse {
        ++calls;
        throw std::runtime_error("ENOTFOUND");
    };
    CHECK_THROWS_WITH(access_token_refresh(credentials, offline),
                      "Google OAuth token refresh failed: ENOTFOUND");
    CHECK(calls == 1);
    CHECK(!detail::g_cached_token);
    token_cache_reset();
}

TEST_CASE("public import defaults to injected Docs and Drive through revision-locked transaction") {
    using namespace gdocsmith;
    struct Fixture {
        std::filesystem::path root = std::filesystem::current_path() / "build" / "import-public-tests" / engine::detail::unique_tmp_suffix();
        std::optional<std::string> previous;
        Fixture() {
            if (const char *value = getenv("GDOCSMITH_WORK_DIR"))
                previous = value;
            std::filesystem::create_directories(root);
            setenv("GDOCSMITH_WORK_DIR", root.c_str(), 1);
            token_cache_reset();
        }
        ~Fixture() {
            if (previous)
                setenv("GDOCSMITH_WORK_DIR", previous->c_str(), 1);
            else
                unsetenv("GDOCSMITH_WORK_DIR");
            token_cache_reset();
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
    } fixture;
    emulator::FakeGoogle backend;
    backend.seed_document("public_doc", emulator::blank_doc_template("Synthetic", "public_doc"), 1);
    auto initialModel = model::docModelParse(backend.get_doc("public_doc").json,
                                             model::DocModelParseOptions{.docId = "public_doc"});
    initialModel.revisionId = "rev-1";
    auto file = lens::documentFileParse(lens::documentFileExport(engine::exportDocumentFile(initialModel)));
    int writes = 0;
    int reads = 0;
    bool unknown = false;
    bool editing = false;
    bool creating = false;
    bool comment = false;
    bool approved = false;
    bool revisionRejected = false;
    bool renameRejected = false;
    bool unconfirmedComment = false;
    bool replyWanted = false;
    bool createTimeout = false;
    bool unchangedComment = false;
    bool lateCommentLoss = false;
    bool readonlyRich = false;
    bool hiddenReviewEdit = false;
    int postWriteCommentReads = 0;
    int creates = 0;
    int commentWrites = 0;
    std::string activeId = "public_doc";
    slimr::Json::array_t publicComments;
    SUBCASE("edit") {
        file.tabs[0].markdown = "Synthetic public edit\n";
        editing = true;
    }
    SUBCASE("noop") {}
    SUBCASE("readonly settings styles and nonbody contexts allow unrelated body edit") {
        readonlyRich = true;
        editing = true;
        const auto baseline = backend.get_doc("public_doc").json;
        model::TableBlockSpec table;
        table.cells = {
            {{model::BlockSpec::make_paragraph(model::ParagraphSpec{.content = {"Merged anchor"}})},
             {model::BlockSpec::make_paragraph(model::ParagraphSpec{})}},
            {{model::BlockSpec::make_paragraph(model::ParagraphSpec{})},
             {model::BlockSpec::make_paragraph(model::ParagraphSpec{})}},
        };
        auto json = model::docJsonBuild(model::DocSpec{
            .documentId = "public_doc",
            .title = "Synthetic",
            .tabs = {model::TabSpec{.tabId = "t.0", .title = "Tab 1", .blocks = {
                                                                          model::BlockSpec::make_paragraph(model::ParagraphSpec{.content = {
                                                                                                                                    "Styled paragraph",
                                                                                                                                    model::ParagraphAtomSpec{.type = "footnoteRef"},
                                                                                                                                }}),
                                                                          model::BlockSpec::make_table(std::move(table)),
                                                                          model::BlockSpec::make_paragraph(model::ParagraphSpec{.content = {"Editable tail"}}),
                                                                      }}},
        });
        auto &dt = json["tabs"][0]["documentTab"];
        dt["documentStyle"] = baseline["tabs"][0]["documentTab"]["documentStyle"];
        dt["namedStyles"] = baseline["tabs"][0]["documentTab"]["namedStyles"];
        dt["documentStyle"]["marginHeader"] = model::dimensionFromPt(24);
        dt["documentStyle"]["opaqueSetting"] = "preserved";
        dt["documentStyle"]["defaultHeaderId"] = "synthetic_header";
        dt["documentStyle"]["defaultFooterId"] = "synthetic_footer";
        dt["body"]["content"][1]["paragraph"]["paragraphStyle"]["alignment"] = "END";
        dt["body"]["content"][1]["paragraph"]["paragraphStyle"]["keepWithNext"] = true;
        dt["body"]["content"][1]["paragraph"]["elements"][1]["footnoteReference"]["footnoteId"] = "synthetic_footnote";
        dt["body"]["content"][1]["paragraph"]["elements"][1]["footnoteReference"]["textStyle"] = slimr::Json::object_t{};
        auto &tableJson = dt["body"]["content"][2]["table"];
        auto &cellStyle = tableJson["tableRows"][0]["tableCells"][0]["tableCellStyle"];
        cellStyle["rowSpan"] = 2;
        cellStyle["columnSpan"] = 2;
        cellStyle["paddingTop"] = model::dimensionFromPt(9);
        tableJson["tableStyle"]["tableColumnProperties"] = slimr::Json::array_t{
            slimr::Json::object_t{{"columnWidth", model::dimensionFromPt(100)}, {"widthType", "FIXED_WIDTH"}},
            slimr::Json::object_t{{"columnWidth", model::dimensionFromPt(200)}, {"widthType", "FIXED_WIDTH"}},
        };
        const auto header = model::paragraphJsonBuild(model::ParagraphSpec{.content = {"Synthetic header"}}, 0).element;
        const auto footer = model::paragraphJsonBuild(model::ParagraphSpec{.content = {"Synthetic footer"}}, 0).element;
        const auto footnote = model::paragraphJsonBuild(model::ParagraphSpec{.content = {"Synthetic footnote"}}, 0).element;
        dt["headers"]["synthetic_header"]["content"] = slimr::Json::array_t{header};
        dt["footers"]["synthetic_footer"]["content"] = slimr::Json::array_t{footer};
        dt["footnotes"]["synthetic_footnote"]["content"] = slimr::Json::array_t{footnote};
        backend.seed_document("public_doc", json, 1);
        json["revisionId"] = "rev-1";
        initialModel = model::docModelParse(json, model::DocModelParseOptions{.docId = "public_doc"});
        file = lens::documentFileParse(lens::documentFileExport(engine::exportDocumentFile(initialModel)));
        REQUIRE(file.tabs[0].segments.size() == 2);
        REQUIRE(file.tabs[0].footnotes.size() == 1);
        REQUIRE(!file.frontmatter.paragraphStyles.empty());
        REQUIRE(!file.frontmatter.cellStyles.empty());
        CHECK(file.tabs[0].markdown.find(":merge{left}") != std::string::npos);
        const auto offset = file.tabs[0].markdown.find("Editable tail");
        REQUIRE(offset != std::string::npos);
        file.tabs[0].markdown.replace(offset, std::string("Editable tail").size(), "Changed tail");
    }
    SUBCASE("pending review no-op") {
        const auto baseline = backend.get_doc("public_doc").json;
        auto json = model::docJsonBuild(model::DocSpec{
            .documentId = "public_doc",
            .title = "Synthetic",
            .tabs = {model::TabSpec{.tabId = "t.0", .title = "Tab 1", .blocks = {model::BlockSpec::make_paragraph(model::ParagraphSpec{.content = {
                                                                                                                                           std::string("Synthetic "),
                                                                                                                                           model::ParagraphTextSpec{.text = "pending insertion", .sugIns = std::vector<std::string>{"synthetic_suggestion"}},
                                                                                                                                       }})}}},
        });
        json["tabs"][0]["documentTab"]["documentStyle"] = baseline["tabs"][0]["documentTab"]["documentStyle"];
        json["tabs"][0]["documentTab"]["namedStyles"] = baseline["tabs"][0]["documentTab"]["namedStyles"];
        backend.seed_document("public_doc", json, 1);
        auto pending = model::docModelParse(json, model::DocModelParseOptions{.docId = "public_doc"});
        file = engine::exportDocumentFile(pending);
        REQUIRE(!file.suggestions.empty());
        CHECK(std::any_of(file.suggestions.begin(), file.suggestions.end(), [](const auto &suggestion) {
            return suggestion.before.has_value() || suggestion.after.has_value();
        }));
        CHECK(file.tabs[0].markdown.find("synthetic_suggestion") != std::string::npos);
    }
    SUBCASE("hidden-only pending review survives unrelated public body edit") {
        editing = true;
        hiddenReviewEdit = true;
        const auto baseline = backend.get_doc("public_doc").json;
        auto json = model::docJsonBuild(model::DocSpec{
            .documentId = "public_doc",
            .title = "Synthetic",
            .tabs = {model::TabSpec{.tabId = "t.0", .title = "Tab 1", .blocks = {
                                                                          model::BlockSpec::make_paragraph(model::ParagraphSpec{
                                                                              .content = {model::ParagraphTextSpec{
                                                                                  .text = "Synthetic hidden insertion",
                                                                                  .sugIns = std::vector<std::string>{"synthetic_hidden_review"},
                                                                              }},
                                                                              .newlineSugIns = std::vector<std::string>{"synthetic_hidden_review"},
                                                                          }),
                                                                          model::BlockSpec::make_paragraph(model::ParagraphSpec{.content = {"Editable tail"}}),
                                                                      }}},
        });
        json["tabs"][0]["documentTab"]["documentStyle"] = baseline["tabs"][0]["documentTab"]["documentStyle"];
        json["tabs"][0]["documentTab"]["namedStyles"] = baseline["tabs"][0]["documentTab"]["namedStyles"];
        backend.seed_document("public_doc", json, 1);
        json["revisionId"] = "rev-1";
        initialModel = model::docModelParse(json, model::DocModelParseOptions{.docId = "public_doc"});
        file = lens::documentFileParse(lens::documentFileExport(engine::exportDocumentFile(initialModel)));
        REQUIRE(!file.suggestions.empty());
        const auto offset = file.tabs[0].markdown.find("Editable tail");
        REQUIRE(offset != std::string::npos);
        file.tabs[0].markdown.replace(offset, std::string("Editable tail").size(), "Changed tail");
    }
    SUBCASE("unknown write") {
        file.tabs[0].markdown = "Synthetic public edit\n";
        unknown = true;
        editing = true;
    }
    SUBCASE("concurrent revision rejection") {
        file.tabs[0].markdown = "Synthetic public edit\n";
        editing = true;
        revisionRejected = true;
    }
    SUBCASE("confirmed content and rejected rename") {
        file.tabs[0].markdown = "Synthetic public edit\n";
        file.frontmatter.title = "Synthetic rejected title";
        editing = true;
        renameRejected = true;
    }
    SUBCASE("create") {
        creating = true;
        editing = true;
        file = lens::DocumentFile{};
        file.frontmatter.title = "Synthetic";
        file.tabs.push_back(lens::DocumentFileTab{.id = "", .title = "Tab 1", .markdown = "Synthetic public creation\n"});
    }
    SUBCASE("comment refusal") { comment = true; }
    SUBCASE("approved comment") {
        comment = true;
        approved = true;
    }
    SUBCASE("approved reply") {
        replyWanted = true;
        approved = true;
    }
    SUBCASE("comment create missing ID with unchanged readback") {
        comment = true;
        approved = true;
        unconfirmedComment = true;
    }
    SUBCASE("comment reply missing ID with unchanged readback") {
        replyWanted = true;
        approved = true;
        unconfirmedComment = true;
    }
    SUBCASE("valid comment create receipt with unchanged readback") {
        comment = true;
        approved = true;
        unchangedComment = true;
    }
    SUBCASE("valid comment reply receipt with unchanged readback") {
        replyWanted = true;
        approved = true;
        unchangedComment = true;
    }
    SUBCASE("confirmed comment disappears before final readback") {
        comment = true;
        approved = true;
        lateCommentLoss = true;
    }
    SUBCASE("confirmed reply disappears before final readback") {
        replyWanted = true;
        approved = true;
        lateCommentLoss = true;
    }
    SUBCASE("comment creation timeout after landing") {
        comment = true;
        approved = true;
        unconfirmedComment = true;
        createTimeout = true;
    }
    if (replyWanted) {
        engine::DriveComment existing;
        existing.id = "existing_comment";
        existing.content = "Synthetic existing comment";
        existing.author = engine::DriveCommentAuthor{.me = true};
        file = engine::exportDocumentFile(initialModel, {existing});
        lens::CommentReply reply;
        reply.text = "Synthetic public reply";
        file.comments[0].replies.push_back(reply);
        publicComments.push_back(slimr::Json::object_t{
            {"id", "existing_comment"},
            {"content", "Synthetic existing comment"},
            {"author", slimr::Json::object_t{{"me", true}}},
        });
    }
    if (comment) {
        lens::CommentBlock block;
        block.text = "Synthetic public comment";
        file.comments.push_back(block);
    }
    const auto path = fixture.root / "input.md";
    engine::detail::atomic_write(path.string(), lens::documentFileExport(file));
    const auto original = engine::inputFileRead(path.string()).text;
    HttpTransport transport = [&](const HttpCall &request) -> HttpCallResponse {
        if (request.url == "https://oauth2.googleapis.com/token") {
            return {.body = R"({"access_token":"synthetic","expires_in":3600})"};
        }
        if (request.url.find("/comments?") != std::string::npos) {
            if (request.method == "POST") {
                CHECK(reads >= 2);
                ++commentWrites;
                if ((!unconfirmedComment && !unchangedComment) || createTimeout) {
                    publicComments.push_back(slimr::Json::object_t{
                        {"id", "synthetic_comment"},
                        {"content", "Synthetic public comment"},
                        {"author", slimr::Json::object_t{{"me", true}}},
                    });
                }
                if (createTimeout)
                    throw std::runtime_error("Synthetic response loss after comment creation");
                if (unconfirmedComment)
                    return {.body = R"({"content":"Synthetic public comment"})"};
                return {.body = R"({"id":"synthetic_comment","content":"Synthetic public comment"})"};
            }
            if (commentWrites > 0)
                ++postWriteCommentReads;
            if (lateCommentLoss && postWriteCommentReads >= 2) {
                if (replyWanted)
                    publicComments[0]["replies"] = slimr::Json::array_t{};
                else
                    publicComments.clear();
            }
            return {.body = slimr::write_json(slimr::Json::object_t{{"comments", publicComments}})};
        }
        if (request.url.find("/comments/existing_comment/replies?") != std::string::npos) {
            CHECK(request.method == "POST");
            CHECK(reads >= 2);
            ++commentWrites;
            if (!unconfirmedComment && !unchangedComment) {
                publicComments[0]["replies"] = slimr::Json::array_t{slimr::Json::object_t{
                    {"id", "synthetic_reply"},
                    {"content", "Synthetic public reply"},
                    {"author", slimr::Json::object_t{{"me", true}}},
                }};
                return {.body = R"({"id":"synthetic_reply","content":"Synthetic public reply"})"};
            }
            if (unchangedComment)
                return {.body = R"({"id":"synthetic_reply","content":"Synthetic public reply"})"};
            return {.body = R"({"content":"Synthetic public reply"})"};
        }
        if (request.method == "POST" && request.url == "https://docs.googleapis.com/v1/documents") {
            ++creates;
            activeId = backend.create_document("Synthetic");
            return {.body = slimr::write_json(slimr::Json::object_t{{"documentId", activeId}})};
        }
        if (request.url.find(":batchUpdate") != std::string::npos) {
            ++writes;
            auto body = *slimr::parse_json<slimr::Json>(request.body);
            CHECK(body["writeControl"]["requiredRevisionId"].get<std::string>() == "rev-1");
            if (unknown)
                throw std::runtime_error("Synthetic transport loss");
            if (revisionRejected) {
                return {.status = 400, .body = R"({"error":{"code":400,"message":"The required revision does not match"}})"};
            }
            return {.body = backend.batch_update(activeId, body["requests"].get<slimr::Json::array_t>(), "rev-1")};
        }
        if (renameRejected && request.method == "PATCH" &&
            request.url.find("/drive/v3/files/public_doc?") != std::string::npos) {
            return {.status = 403, .body = R"({"error":{"code":403,"message":"Synthetic rename denied"}})"};
        }
        if (request.method == "GET" && request.url.find("/documents/" + activeId + "?") != std::string::npos) {
            ++reads;
            auto json = backend.get_doc(activeId).json;
            json["revisionId"] = backend.revision_id(backend.get_doc(activeId));
            return {.body = slimr::write_json(json)};
        }
        if (request.method == "GET" && request.url.find("/drive/v3/files/" + activeId + "?") != std::string::npos) {
            return {.body = slimr::write_json(slimr::Json::object_t{
                        {"id", activeId}, {"name", backend.get_doc(activeId).json["title"]}})};
        }
        if (request.method == "GET" && request.url.find("/drive/v3/about?fields=user") != std::string::npos) {
            return {.body = R"({"user":{"emailAddress":""}})"};
        }
        FAIL("Unexpected public import HTTP request");
        return {.status = 500};
    };
    auto app = make_gdocsmith_app(transport, [](const std::vector<std::string> &) {
        return R"({"client_id":"synthetic","client_secret":"synthetic","refresh_token":"synthetic"})";
    });
    const auto *tool = app.findTool("import");
    REQUIRE(tool);
    slimr::Json arguments = slimr::Json::object_t{{"path", path.string()}};
    if (creating)
        arguments["newDocStyleBaseline"] = "account";
    if (approved)
        arguments["allow"] = slimr::Json::array_t{"commentChanges"};
    auto result = slimr::serve_result_to_mcp_result(tool->handler(arguments, slimr::RequestContext{}));
    REQUIRE(!result.content.empty());
    const auto parsed = slimr::parse_json<slimr::Json>(result.content.front().text);
    REQUIRE(parsed);
    const auto &output = *parsed;
    INFO(slimr::write_json(output));
    if (unconfirmedComment || unchangedComment || lateCommentLoss) {
        CHECK(result.isError);
        CHECK(output["writeState"].get<std::string>() == "unknown");
        CHECK(output["verification"].get<std::string>() == "inconclusive");
        CHECK(writes == 0);
        CHECK(commentWrites == 1);
        CHECK(reads >= 3);
        CHECK(engine::inputFileRead(path.string()).text == original);
        REQUIRE(output.contains("exportPath"));
        auto current = lens::documentFileParse(engine::inputFileRead(output["exportPath"].get<std::string>()).text);
        if (replyWanted) {
            REQUIRE(current.comments.size() == 1);
            CHECK(current.comments[0].replies.empty());
        } else if (createTimeout) {
            REQUIRE(current.comments.size() == 1);
            CHECK(current.comments[0].id == "synthetic_comment");
        } else
            CHECK(current.comments.empty());
    } else if (unknown) {
        CHECK(result.isError);
        CHECK(output["writeState"].get<std::string>() == "unknown");
        CHECK(writes == 1);
        CHECK(reads >= 2);
        CHECK(engine::inputFileRead(path.string()).text == original);
    } else if (revisionRejected || renameRejected) {
        CHECK(result.isError);
        CHECK(output["writeState"].get<std::string>() == (revisionRejected ? "none" : "partial"));
        CHECK(writes == 1);
        CHECK(reads >= 2);
        CHECK(output.contains("exportPath"));
        REQUIRE(output["errors"].is_array());
        const auto code = output["errors"][0]["code"].get<std::string>();
        if (revisionRejected)
            CHECK((code == "staleBase" || code == "concurrentEdits"));
        else
            CHECK(code == "remoteRejected");
        CHECK(engine::inputFileRead(path.string()).text == original);
    } else if (comment && !approved) {
        CHECK(result.isError);
        CHECK(output["status"].get<std::string>() == "refused");
        CHECK(writes == 0);
        CHECK(commentWrites == 0);
        CHECK(engine::inputFileRead(path.string()).text == original);
    } else {
        CHECK(!result.isError);
        CHECK(output["verification"].get<std::string>() == "match");
        CHECK(output["status"].get<std::string>() == (editing || approved ? "applied" : "unchanged"));
        CHECK(writes == (editing ? 1 : 0));
        CHECK(reads >= 2);
        CHECK(commentWrites == (approved ? 1 : 0));
        if (hiddenReviewEdit) {
            const auto actual = model::docModelParse(backend.get_doc(activeId).json,
                                                     model::DocModelParseOptions{.docId = activeId});
            REQUIRE(actual.tabs[0].blocks.size() == 2);
            CHECK(actual.tabs[0].blocks[0] == initialModel.tabs[0].blocks[0]);
            CHECK(!lens::documentFileParse(engine::inputFileRead(path.string()).text).suggestions.empty());
        }
        if (readonlyRich) {
            const auto actual = model::docModelParse(backend.get_doc(activeId).json,
                                                     model::DocModelParseOptions{.docId = activeId});
            CHECK(actual.tabs[0].documentStyle == initialModel.tabs[0].documentStyle);
            CHECK(actual.tabs[0].namedStyles == initialModel.tabs[0].namedStyles);
            CHECK(actual.tabs[0].blocks[0] == initialModel.tabs[0].blocks[0]);
            CHECK(actual.tabs[0].blocks[1] == initialModel.tabs[0].blocks[1]);
            REQUIRE(actual.tabs[0].segments.size() == initialModel.tabs[0].segments.size());
            for (size_t index = 0; index < actual.tabs[0].segments.size(); ++index) {
                REQUIRE(actual.tabs[0].segments[index].tab);
                REQUIRE(initialModel.tabs[0].segments[index].tab);
                CHECK(actual.tabs[0].segments[index].tab->blocks == initialModel.tabs[0].segments[index].tab->blocks);
            }
        }
        if (approved) {
            const auto actual = lens::documentFileParse(engine::inputFileRead(path.string()).text);
            REQUIRE(actual.comments.size() == 1);
            CHECK(actual.comments[0].id == (replyWanted ? "existing_comment" : "synthetic_comment"));
            if (replyWanted) {
                REQUIRE(actual.comments[0].replies.size() == 1);
                CHECK(actual.comments[0].replies[0].id == "synthetic_reply");
            }
        }
        if (creating) {
            CHECK(creates == 1);
            CHECK(output["doc"].get<std::string>() == activeId);
        }
    }
}

TEST_CASE("image downloads use bearer URLs without forwarding Google credentials") {
    using namespace gdocsmith;
    int calls = 0;
    DocsClient client([&](const HttpCall &call) {
        ++calls;
        CHECK(call.headers.empty());
        CHECK(call.method == "GET");
        return HttpCallResponse{.body = "synthetic image"}; }, [](const std::vector<std::string> &) -> std::string { throw std::runtime_error("image downloads must not request OAuth credentials"); });
    CHECK(client.imageDownload("https://lh3.googleusercontent.com/image") == "synthetic image");
    CHECK(calls == 1);
    for (const auto &uri : {"http://lh3.googleusercontent.com/image",
                            "https://googleusercontent.com.evil.example/image",
                            "https://localhost/image"}) {
        CHECK_THROWS_AS(client.imageDownload(uri), model::CoreError);
    }
    CHECK(calls == 1);
    client.transport = [](const HttpCall &) { return HttpCallResponse{.status = 403}; };
    CHECK_THROWS_AS(client.imageDownload("https://lh3.googleusercontent.com/image"), model::CoreError);
    client.transport = [](const HttpCall &) -> HttpCallResponse {
        throw std::runtime_error("transport failed for https://lh3.googleusercontent.com/private-bearer");
    };
    CHECK_THROWS_WITH_AS(client.imageDownload("https://lh3.googleusercontent.com/private-bearer"),
                         "Image download transport failed.", model::CoreError);
}

TEST_CASE("HTTP /mcp and /tools/status run status and reject per-tool CLI") {
    auto app = gdocsmith::make_gdocsmith_app();

    char arg0[] = "gdocsmith-mcp";
    char arg1[] = "status";
    char *argv[] = {arg0, arg1, nullptr};
    CHECK(app.run(2, argv) != 0);

    slimr::McpHttpServer server(app, slimr::HttpServerOptions{.host = "127.0.0.1", .port = 0});
    REQUIRE(server.start());

    httplib::Client client("127.0.0.1", server.boundPort());

    auto toolsRes = client.Post("/tools/status", "{}", "application/json");
    REQUIRE(toolsRes);
    CHECK(toolsRes->status == 200);
    auto toolsJson = slimr::parse_json<slimr::Json>(toolsRes->body);
    REQUIRE(toolsJson.has_value());
    CHECK((*toolsJson)["version"].get<std::string>() == gdocsmith::GDOCSMITH_VERSION);

    httplib::Headers mcpHeaders = {
        {"Accept", "application/json, text/event-stream"},
        {"MCP-Protocol-Version", "2026-07-28"},
        {"Mcp-Method", "tools/call"},
        {"Mcp-Name", "status"},
    };
    const std::string mcpBody = R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"status","arguments":{},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}})";
    auto mcpRes = client.Post("/mcp", mcpHeaders, mcpBody, "application/json");
    REQUIRE(mcpRes);
    CHECK(mcpRes->status == 200);
    auto mcpJson = slimr::parse_json<slimr::Json>(mcpRes->body);
    REQUIRE(mcpJson.has_value());
    CHECK((*mcpJson)["result"]["structuredContent"]["version"].get<std::string>() == gdocsmith::GDOCSMITH_VERSION);
}

TEST_SUITE("api::document_id") {
    TEST_CASE("extracts document ID from URLs and bare IDs") {
        using namespace gdocsmith;

        CHECK(document_id("1234567890abcdef_ABC-xyz") == "1234567890abcdef_ABC-xyz");
        CHECK(document_id("https://docs.google.com/document/d/1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms/edit") ==
              "1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms");
        CHECK(document_id("https://docs.google.com/document/d/my-doc-id_123?usp=sharing") == "my-doc-id_123");

        CHECK_THROWS_AS(document_id("invalid url / with / slashes"), model::CoreError);
        CHECK_THROWS_AS(document_id("not an id!"), model::CoreError);
    }
}

TEST_SUITE("api::entities_decode") {
    TEST_CASE("decodes HTML entities in quoted text") {
        using namespace gdocsmith;

        CHECK(entities_decode("Hello &amp; world") == "Hello & world");
        CHECK(entities_decode("&quot;Quoted&quot; and &apos;single&apos; and &#39;apostrophe&#39;") ==
              "\"Quoted\" and 'single' and 'apostrophe'");
        CHECK(entities_decode("&lt;tag&gt;") == "<tag>");
        CHECK(entities_decode("Decimal: &#65; &#66; &#67;") == "Decimal: A B C");
        CHECK(entities_decode("Hex: &#x41; &#x42; &#x43;") == "Hex: A B C");
        CHECK(entities_decode("Plain text without entities") == "Plain text without entities");
        CHECK(entities_decode("") == "");
    }
}

TEST_SUITE("api::gws_error_format") {
    TEST_CASE("formats standard Google API error messages") {
        using namespace gdocsmith;

        std::string permErr = R"({"error": {"code": 403, "message": "The caller does not have permission"}})";
        CHECK(gws_error_format(permErr, "doc-123").find("Permission denied on \"doc-123\"") != std::string::npos);

        std::string notFoundErr = R"({"error": {"code": 404, "message": "File not found: abc"}})";
        CHECK(gws_error_format(notFoundErr, "abc").find("Document or file not found \"abc\"") != std::string::npos);

        std::string authErr = R"({"error": {"code": 401, "message": "invalid_grant"}})";
        CHECK(gws_error_format(authErr).find("Google Workspace authentication expired") != std::string::npos);

        std::string genericErr = R"({"error": {"code": 400, "message": "Invalid request field"}})";
        CHECK(gws_error_format(genericErr) == "Google API error (400): Invalid request field");

        CHECK(gws_error_format("").find("GWS operation failed") != std::string::npos);
        CHECK(gws_error_format("Custom error string") == "Custom error string");
    }
}

TEST_SUITE("api::zip_and_docx") {
    TEST_CASE("zip_crc32 computes standard CRC-32") {
        using namespace gdocsmith;

        CHECK(zip_crc32("") == 0x00000000);
        // Standard check: "123456789" CRC-32 is 0xcbf43926
        CHECK(zip_crc32("123456789") == 0xcbf43926);
    }

    TEST_CASE("zip_store creates valid uncompressed ZIP package") {
        using namespace gdocsmith;

        std::vector<std::pair<std::string, std::string>> files = {
            {"hello.txt", "Hello World!\n"},
            {"dir/sub.txt", "Sub content\n"}};

        std::string zip = zip_store(files);
        CHECK(zip.size() > 72);
        // Check local header magic (PK\x03\x04)
        CHECK(static_cast<uint8_t>(zip[0]) == 0x50);
        CHECK(static_cast<uint8_t>(zip[1]) == 0x4b);
        CHECK(static_cast<uint8_t>(zip[2]) == 0x03);
        CHECK(static_cast<uint8_t>(zip[3]) == 0x04);

        // Check file name appears in archive
        CHECK(zip.find("hello.txt") != std::string::npos);
        CHECK(zip.find("Hello World!\n") != std::string::npos);
        CHECK(zip.find("dir/sub.txt") != std::string::npos);
    }

    TEST_CASE("image_info detects PNG, GIF, and JPEG dimensions") {
        using namespace gdocsmith;
        using namespace std::string_literals;

        // Minimal PNG: 8-byte magic + 4-byte chunk len + 4-byte "IHDR" + 4-byte width + 4-byte height
        std::string pngData = "\x89PNG\r\n\x1a\n"s;
        pngData += std::string("\x00\x00\x00\x0dIHDR", 8);
        // 100 px width (0x00000064), 200 px height (0x000000c8)
        pngData += std::string("\x00\x00\x00\x64\x00\x00\x00\xc8", 8);
        pngData += std::string(10, '\0');

        auto pngInfo = image_info(pngData);
        REQUIRE(pngInfo.has_value());
        CHECK(pngInfo->type == "png");
        CHECK(pngInfo->widthPx == 100);
        CHECK(pngInfo->heightPx == 200);

        // Minimal GIF: "GIF89a" + width (uint16 LE) + height (uint16 LE)
        std::string gifData = "GIF89a"s;
        gifData.push_back(static_cast<char>(50 & 0xff)); // 50 px width
        gifData.push_back(static_cast<char>((50 >> 8) & 0xff));
        gifData.push_back(static_cast<char>(75 & 0xff)); // 75 px height
        gifData.push_back(static_cast<char>((75 >> 8) & 0xff));
        gifData += std::string(10, '\0');

        auto gifInfo = image_info(gifData);
        REQUIRE(gifInfo.has_value());
        CHECK(gifInfo->type == "gif");
        CHECK(gifInfo->widthPx == 50);
        CHECK(gifInfo->heightPx == 75);

        // Minimal JPEG with SOF0 marker (0xFF, 0xC0):
        // SOI: 0xFF, 0xD8
        // Marker: 0xFF, 0xC0
        // Length: 2 bytes (big endian) = 17 (0x0011)
        // Precision: 1 byte = 8
        // Height: 2 bytes (big endian) = 300 (0x012c)
        // Width: 2 bytes (big endian) = 400 (0x0190)
        std::string jpegData = std::string("\xff\xd8\xff\xc0\x00\x11\x08\x01\x2c\x01\x90", 11);
        jpegData += std::string(10, '\0');

        auto jpegInfo = image_info(jpegData);
        REQUIRE(jpegInfo.has_value());
        CHECK(jpegInfo->type == "jpeg");
        CHECK(jpegInfo->heightPx == 300);
        CHECK(jpegInfo->widthPx == 400);

        // Random non-image bytes
        CHECK(!image_info("not an image at all").has_value());
    }

    TEST_CASE("docx_build packages body and images into valid OpenXML docx") {
        using namespace gdocsmith;

        DocxImage img1{
            .bytes = "fake_png_data",
            .widthPx = 100,
            .heightPx = 100,
            .type = "png"};
        DocxImage img2{
            .bytes = "fake_jpeg_data",
            .widthPx = 200,
            .heightPx = 150,
            .type = "jpeg"};

        std::string docx = docx_build("<w:p><w:r><w:t>Hello Docx</w:t></w:r></w:p>", {img1, img2});
        CHECK(docx.size() > 500);
        CHECK(docx.find("[Content_Types].xml") != std::string::npos);
        CHECK(docx.find("word/document.xml") != std::string::npos);
        CHECK(docx.find("word/media/image0.png") != std::string::npos);
        CHECK(docx.find("word/media/image1.jpeg") != std::string::npos);
        CHECK(docx.find("Hello Docx") != std::string::npos);
        CHECK(docx.find("image1") != std::string::npos);
        CHECK(docx.find("image2") != std::string::npos);
    }
}

TEST_SUITE("api::docs_client") {
    TEST_CASE("createDocument with and without folderId") {
        using namespace gdocsmith;

        std::vector<HttpCall> calls;
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url == "https://docs.googleapis.com/v1/documents" && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"documentId":"doc_new_123","title":"Test Doc"})"};
            }
            if (req.url.find("drive/v3/files/doc_new_123") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"doc_new_123","name":"Test Doc"})"};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DocsClient client(mockTransport, mockRunner);

        // 1. Create document without folder
        auto res1 = client.createDocument("Test Doc");
        CHECK(res1["documentId"].get<std::string>() == "doc_new_123");
        CHECK(res1["title"].get<std::string>() == "Test Doc");

        bool foundDocsCreate = false;
        bool foundDriveMove = false;
        for (const auto &c : calls) {
            if (c.url == "https://docs.googleapis.com/v1/documents" && c.method == "POST") {
                foundDocsCreate = true;
                CHECK(c.body.find("\"title\":\"Test Doc\"") != std::string::npos);
            }
            if (c.url.find("addParents") != std::string::npos) {
                foundDriveMove = true;
            }
        }
        CHECK(foundDocsCreate);
        CHECK(!foundDriveMove);

        // 2. Create document with folderId
        calls.clear();
        auto res2 = client.createDocument("Test Doc In Folder", "folder-abc-999");
        CHECK(res2["documentId"].get<std::string>() == "doc_new_123");

        foundDocsCreate = false;
        foundDriveMove = false;
        for (const auto &c : calls) {
            if (c.url == "https://docs.googleapis.com/v1/documents" && c.method == "POST") {
                foundDocsCreate = true;
            }
            if (c.url.find("addParents=folder-abc-999") != std::string::npos) {
                foundDriveMove = true;
                CHECK(c.method == "PATCH");
            }
        }
        CHECK(foundDocsCreate);
        CHECK(foundDriveMove);
    }

    TEST_CASE("batchUpdate sends writeControl and requests payload") {
        using namespace gdocsmith;

        std::vector<HttpCall> calls;
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find(":batchUpdate") != std::string::npos) {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"documentId":"doc-123","replies":[]})"};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DocsClient client(mockTransport, mockRunner);

        slimr::Json::array_t reqs = {
            slimr::Json::object_t{{"insertText", slimr::Json::object_t{{"text", "hello"}}}}};

        auto res = client.batchUpdate("doc-123", reqs, "rev-456");
        CHECK(res["documentId"].get<std::string>() == "doc-123");

        bool foundBatch = false;
        for (const auto &c : calls) {
            if (c.url.find("documents/doc-123:batchUpdate") != std::string::npos) {
                foundBatch = true;
                CHECK(c.body.find("\"requiredRevisionId\":\"rev-456\"") != std::string::npos);
                CHECK(c.body.find("\"insertText\"") != std::string::npos);
            }
        }
        CHECK(foundBatch);
    }

    TEST_CASE("run delegates to runner or exec_gws") {
        using namespace gdocsmith;

        std::vector<std::string> capturedArgs;
        GwsRunner mockRunner = [&](const std::vector<std::string> &args) {
            capturedArgs = args;
            return "gws_out";
        };

        DocsClient client(nullptr, mockRunner);
        std::string out = client.run({"drive", "files", "list"});
        CHECK(out == "gws_out");
        CHECK(capturedArgs == std::vector<std::string>{"drive", "files", "list"});
    }
}

TEST_SUITE("api::drive_client") {
    TEST_CASE("file metadata and CRUD operations") {
        using namespace gdocsmith;

        std::vector<HttpCall> calls;
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("drive/v3/files/file-123") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"file-123","name":"Report.docx","mimeType":"application/vnd.google-apps.document","trashed":false,"parents":["folder-1"],"webViewLink":"https://drive.google.com/view","version":"10"})"};
            }
            if (req.url.find("drive/v3/files/file-123/copy") != std::string::npos) {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"file-123-copy","name":"Report Copy.docx"})"};
            }
            if (req.url.find("drive/v3/files/file-123") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"file-123","name":"Report Renamed.docx","trashed":true})"};
            }
            if (req.url.find("drive/v3/files/file-123") != std::string::npos && req.method == "DELETE") {
                return HttpCallResponse{.status = 204, .body = ""};
            }
            if (req.url.find("drive/v3/files?") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"files":[{"id":"f1","name":"Doc 1","mimeType":"text/plain"},{"id":"f2","name":"Doc 2","mimeType":"text/plain"}]})"};
            }
            if (req.url.find("drive/v3/about") != std::string::npos) {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"user":{"emailAddress":"user@example.com"}})"};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DriveClient drive(mockTransport, mockRunner);

        // userEmailGet
        CHECK(drive.userEmailGet() == "user@example.com");

        // getFile and getFileMetadata
        auto jsonFile = drive.getFile("file-123");
        CHECK(jsonFile["name"].get<std::string>() == "Report.docx");

        DriveFile meta = drive.getFileMetadata("file-123");
        CHECK(meta.id == "file-123");
        CHECK(meta.name == "Report.docx");
        CHECK(meta.mimeType == "application/vnd.google-apps.document");
        CHECK(meta.trashed == false);
        CHECK(meta.parents == std::vector<std::string>{"folder-1"});
        CHECK(meta.webViewLink == "https://drive.google.com/view");
        CHECK(meta.version == "10");

        // copyFile
        auto copyRes = drive.copyFile("file-123", "Report Copy.docx");
        CHECK(copyRes["id"].get<std::string>() == "file-123-copy");

        // updateFile
        auto updRes = drive.updateFile("file-123", slimr::Json::object_t{{"name", "Report Renamed.docx"}, {"trashed", true}});
        CHECK(updRes["name"].get<std::string>() == "Report Renamed.docx");
        CHECK(updRes["trashed"].get<bool>() == true);

        // deleteFile
        drive.deleteFile("file-123");

        // listFiles
        auto files = drive.listFiles("name contains 'Doc'");
        CHECK(files.size() == 2);
        CHECK(files[0].id == "f1");
        CHECK(files[1].id == "f2");
    }

    TEST_CASE("comments API full lifecycle (list, get, create, reply, update, delete)") {
        using namespace gdocsmith;

        std::vector<HttpCall> calls;
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("/comments?") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"comments":[{"id":"c1","content":"Great point","author":{"displayName":"Alice","emailAddress":"alice@example.com"},"quotedFileContent":{"value":"&quot;quote &amp; text&quot;"},"resolved":false,"replies":[{"id":"r1","content":"Thanks!","author":{"displayName":"Bob"}}]}]})"};
            }
            if (req.url.find("/comments/c1?") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"c1","content":"Great point"})"};
            }
            if (req.url.find("/comments?") != std::string::npos && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"c_new","content":"New comment"})"};
            }
            if (req.url.find("/comments/c1/replies") != std::string::npos && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"r_new","content":"Reply content","action":"resolve"})"};
            }
            if (req.url.find("/comments/c1/replies/r1") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"r1","content":"Updated reply content"})"};
            }
            if (req.url.find("/comments/c1/replies/r1") != std::string::npos && req.method == "DELETE") {
                return HttpCallResponse{.status = 204, .body = ""};
            }
            if (req.url.find("/comments/c1") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"c1","content":"Edited comment"})"};
            }
            if (req.url.find("/comments/c1") != std::string::npos && req.method == "DELETE") {
                return HttpCallResponse{.status = 204, .body = ""};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DriveClient drive(mockTransport, mockRunner);

        // 1. listComments / commentsList
        auto comments = drive.listComments("doc-123");
        REQUIRE(comments.size() == 1);
        CHECK(comments[0].id == "c1");
        CHECK(comments[0].content == "Great point");
        CHECK(comments[0].author->displayName == "Alice");
        CHECK(comments[0].quotedText == "\"quote & text\"");
        CHECK(comments[0].quotedFileContent->value == "\"quote & text\"");
        REQUIRE(comments[0].replies.size() == 1);
        CHECK(comments[0].replies[0].id == "r1");
        CHECK(comments[0].replies[0].author->displayName == "Bob");

        // 2. commentGet / getComment
        auto cGet = drive.getComment("doc-123", "c1");
        CHECK(cGet["id"].get<std::string>() == "c1");

        // 3. createComment / commentCreate
        auto cNew = drive.createComment("doc-123", "New comment", "anchored quote");
        CHECK(cNew["id"].get<std::string>() == "c_new");

        // 4. createReply / commentReply
        auto rNew = drive.createReply("doc-123", "c1", "Reply content", "resolve");
        CHECK(rNew["id"].get<std::string>() == "r_new");

        // 5. updateComment / commentUpdate
        auto cUpd = drive.updateComment("doc-123", "c1", "Edited comment");
        CHECK(cUpd["content"].get<std::string>() == "Edited comment");

        // 6. updateReply / replyUpdate
        auto rUpd = drive.updateReply("doc-123", "c1", "r1", "Updated reply content");
        CHECK(rUpd["content"].get<std::string>() == "Updated reply content");

        // 7. deleteReply / replyDelete
        drive.deleteReply("doc-123", "c1", "r1");

        // 8. deleteComment / commentDelete
        drive.deleteComment("doc-123", "c1");
    }

    TEST_CASE("imagesUpload performs multipart upload and returns contentUris") {
        using namespace gdocsmith;

        std::vector<HttpCall> calls;
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("upload/drive/v3/files") != std::string::npos && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"upload_doc_777"})"};
            }
            if (req.url.find("documents/upload_doc_777") != std::string::npos && req.method == "GET") {
                // Return document with inlineObjects matching images
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({
                        "tabs": [{
                            "documentTab": {
                                "body": {
                                    "content": [
                                        {
                                            "paragraph": {
                                                "elements": [
                                                    {"inlineObjectElement": {"inlineObjectId": "obj1"}}
                                                ]
                                            }
                                        },
                                        {
                                            "paragraph": {
                                                "elements": [
                                                    {"inlineObjectElement": {"inlineObjectId": "obj2"}}
                                                ]
                                            }
                                        }
                                    ]
                                },
                                "inlineObjects": {
                                    "obj1": {
                                        "inlineObjectProperties": {
                                            "embeddedObject": {
                                                "imageProperties": {
                                                    "contentUri": "https://lh3.googleusercontent.com/img1"
                                                }
                                            }
                                        }
                                    },
                                    "obj2": {
                                        "inlineObjectProperties": {
                                            "embeddedObject": {
                                                "imageProperties": {
                                                    "contentUri": "https://lh3.googleusercontent.com/img2"
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }]
                    })"};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DriveClient drive(mockTransport, mockRunner);

        std::vector<DocxImage> images = {
            DocxImage{.bytes = "img1_bytes", .widthPx = 100, .heightPx = 100, .type = "png"},
            DocxImage{.bytes = "img2_bytes", .widthPx = 200, .heightPx = 150, .type = "jpeg"}};

        auto res = drive.imagesUpload("Image Upload Batch", images);
        CHECK(res.documentId == "upload_doc_777");
        REQUIRE(res.contentUris.size() == 2);
        CHECK(res.contentUris[0] == "https://lh3.googleusercontent.com/img1");
        CHECK(res.contentUris[1] == "https://lh3.googleusercontent.com/img2");

        // Verify upload call used multipart/related and included boundary
        bool foundMultipart = false;
        for (const auto &c : calls) {
            if (c.url.find("upload/drive/v3/files") != std::string::npos) {
                foundMultipart = true;
                auto it = c.headers.find("Content-Type");
                REQUIRE(it != c.headers.end());
                CHECK(it->second.find("multipart/related; boundary=gdocsmith-upload") != std::string::npos);
                CHECK(c.body.find("--gdocsmith-upload") != std::string::npos);
            }
        }
        CHECK(foundMultipart);
    }

    TEST_CASE("textExport strips UTF-8 BOM and normalizes CRLF") {
        using namespace gdocsmith;

        HttpTransport mockTransport = [](const HttpCall &req) -> HttpCallResponse {
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("export?mimeType=text%2Fplain") != std::string::npos) {
                // Send with BOM (\xEF\xBB\xBF) and \r\n line endings
                std::string body = "\xEF\xBB\xBFLine 1\r\nLine 2\r\nLine 3\n";
                return HttpCallResponse{.status = 200, .body = std::move(body)};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DriveClient drive(mockTransport, mockRunner);

        std::string text = drive.textExport("doc-123");
        CHECK(text == "Line 1\nLine 2\nLine 3\n");
    }
}

TEST_SUITE("api::fetch_with_retry") {
    TEST_CASE("retries on 429 and transient 500 error then succeeds") {
        using namespace gdocsmith;

        int attempts = 0;
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            attempts++;
            if (attempts == 1) {
                return HttpCallResponse{.status = 429, .body = "Rate limit exceeded"};
            }
            if (attempts == 2) {
                return HttpCallResponse{.status = 503, .body = "Service temporarily unavailable"};
            }
            return HttpCallResponse{.status = 200, .body = "Success!"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        auto res = fetch_with_retry(mockTransport, mockRunner, "https://example.com/api", "GET", "", {}, "", FetchRetryOptions{.backoffMs = 1, .retries = 3});
        CHECK(res.status == 200);
        CHECK(res.body == "Success!");
        CHECK(attempts == 3);
    }

    TEST_CASE("returns permanent 400 error raw without retry") {
        using namespace gdocsmith;

        int attempts = 0;
        const std::string body = R"({"error":{"code":400,"message":"Bad request"}})";
        HttpTransport mockTransport = [&](const HttpCall &req) -> HttpCallResponse {
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            attempts++;
            return HttpCallResponse{.status = 400, .body = body};
        };

        GwsRunner mockRunner = [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        auto res = fetch_with_retry(mockTransport, mockRunner, "https://example.com/api", "GET", "", {}, "", FetchRetryOptions{.backoffMs = 1, .retries = 3});
        CHECK(res.status == 400);
        CHECK(res.body == body);
        CHECK(attempts == 1);
    }
}

namespace gdocsmith::api_boundary_test {

// Mock Google API: token URL answers 200; resource calls are counted and answered by `respond`
struct ApiMock {
    int calls = 0;
    int tokenCalls = 0;
    std::function<HttpCallResponse(const HttpCall &)> respond;
    HttpTransport transport() {
        return [this](const HttpCall &req) -> HttpCallResponse {
            if (req.url == "https://oauth2.googleapis.com/token") {
                tokenCalls++;
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            calls++;
            return respond(req);
        };
    }
    static GwsRunner runner() {
        return [](const std::vector<std::string> &) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };
    }
};

inline FetchRetryOptions fastRetry() { return FetchRetryOptions{.backoffMs = 1, .retries = 3}; }

} // namespace gdocsmith::api_boundary_test

TEST_SUITE("api::raw_checked_boundary") {
    TEST_CASE("raw fetch_with_retry returns 404 with body and headers after 1 call") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        const std::string body = R"({"error":{"code":404,"message":"Requested entity was not found."}})";
        mock.respond = [&](const HttpCall &) {
            return HttpCallResponse{.status = 404, .headers = {{"x-synthetic", "kept"}}, .body = body};
        };
        token_cache_reset();
        auto res = fetch_with_retry(mock.transport(), mock.runner(), "https://example.com/api", "GET", "", {}, "", api_boundary_test::fastRetry());
        CHECK(res.status == 404);
        CHECK(res.body == body);
        CHECK(res.headers["x-synthetic"] == "kept");
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("getDocument 404 throws plain runtime_error before parsing after 1 call") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        const std::string body = R"({"error":{"code":404,"message":"Requested entity was not found.","status":"NOT_FOUND"}})";
        mock.respond = [&](const HttpCall &) { return HttpCallResponse{.status = 404, .body = body}; };
        token_cache_reset();
        DocsClient docs(mock.transport(), mock.runner());
        bool threw = false;
        try {
            (void)docs.getDocument("doc-404");
        } catch (const GoogleApiError &) {
            FAIL("getDocument must keep a plain runtime_error");
        } catch (const std::runtime_error &error) {
            threw = true;
            CHECK(std::string(error.what()) == gws_error_format(body, "doc-404"));
        }
        CHECK(threw);
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("checked fetch throws GoogleApiError on permanent 400 after 1 call") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        const std::string body = R"({"error":{"code":400,"message":"Bad request"}})";
        mock.respond = [&](const HttpCall &) { return HttpCallResponse{.status = 400, .body = body}; };
        token_cache_reset();
        try {
            (void)google_api_fetch_checked(mock.transport(), mock.runner(), "https://example.com/api", "GET", "", {}, "target-1");
            FAIL("Expected GoogleApiError");
        } catch (const GoogleApiError &error) {
            CHECK(error.status == 400);
            CHECK(std::string(error.what()) == gws_error_format(body, "target-1"));
        }
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("raw fetch_with_retry returns exhausted 503 after 3 calls") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        mock.respond = [&](const HttpCall &) {
            if (mock.calls == 1)
                return HttpCallResponse{.status = 429, .body = "rate"};
            return HttpCallResponse{.status = 503, .body = "unavailable"};
        };
        token_cache_reset();
        auto res = fetch_with_retry(mock.transport(), mock.runner(), "https://example.com/api", "GET", "", {}, "", api_boundary_test::fastRetry());
        CHECK(res.status == 503);
        CHECK(res.body == "unavailable");
        CHECK(mock.calls == 3);
        token_cache_reset();
    }

    TEST_CASE("raw fetch_with_retry returns 501 after 1 call") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        mock.respond = [&](const HttpCall &) { return HttpCallResponse{.status = 501, .body = "not implemented"}; };
        token_cache_reset();
        auto res = fetch_with_retry(mock.transport(), mock.runner(), "https://example.com/api", "GET", "", {}, "", api_boundary_test::fastRetry());
        CHECK(res.status == 501);
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("permanent 403 mentioning timeout is not retried") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        const std::string body = R"({"error":{"code":403,"message":"Quota timeout window exceeded"}})";
        mock.respond = [&](const HttpCall &) { return HttpCallResponse{.status = 403, .body = body}; };

        token_cache_reset();
        auto res = fetch_with_retry(mock.transport(), mock.runner(), "https://example.com/api", "GET", "", {}, "", api_boundary_test::fastRetry());
        CHECK(res.status == 403);
        CHECK(mock.calls == 1);

        mock.calls = 0;
        DocsClient docs(mock.transport(), mock.runner());
        CHECK_THROWS_AS((void)docs.getDocument("doc-timeout"), std::runtime_error);
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("raw fetch returns persistent 401 after one forced refresh") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        mock.respond = [&](const HttpCall &) { return HttpCallResponse{.status = 401, .body = "unauthorized"}; };
        token_cache_reset();
        auto res = google_api_fetch(mock.transport(), mock.runner(), "https://example.com/api");
        CHECK(res.status == 401);
        CHECK(res.body == "unauthorized");
        CHECK(mock.calls == 2);
        CHECK(mock.tokenCalls == 2);
        token_cache_reset();
    }

    TEST_CASE("batchUpdate permanent 400 throws GoogleApiError without replay") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        const std::string body = R"({"error":{"code":400,"message":"Invalid requests[0]"}})";
        mock.respond = [&](const HttpCall &) { return HttpCallResponse{.status = 400, .body = body}; };
        token_cache_reset();
        DocsClient docs(mock.transport(), mock.runner());
        try {
            (void)docs.batchUpdate("doc-batch", slimr::Json::array_t{});
            FAIL("Expected GoogleApiError");
        } catch (const GoogleApiError &error) {
            CHECK(error.status == 400);
            CHECK(std::string(error.what()) == gws_error_format(body, "doc-batch"));
        }
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("discarded deleteFile 404 throws GoogleApiError after 1 call") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        mock.respond = [&](const HttpCall &) {
            return HttpCallResponse{.status = 404, .body = R"({"error":{"code":404,"message":"File not found"}})"};
        };
        token_cache_reset();
        DriveClient drive(mock.transport(), mock.runner());
        try {
            drive.deleteFile("file-404");
            FAIL("Expected GoogleApiError");
        } catch (const GoogleApiError &error) {
            CHECK(error.status == 404);
        }
        CHECK(mock.calls == 1);
        token_cache_reset();
    }

    TEST_CASE("imagesUpload readback 404 throws ImagesUploadError with formatted error") {
        using namespace gdocsmith;
        api_boundary_test::ApiMock mock;
        const std::string body = R"({"error":{"code":404,"message":"Requested entity was not found."}})";
        mock.respond = [&](const HttpCall &req) {
            if (req.url.find("upload/drive/v3/files") != std::string::npos) {
                return HttpCallResponse{.status = 200, .body = R"({"id":"upload_doc_404"})"};
            }
            return HttpCallResponse{.status = 404, .body = body};
        };
        token_cache_reset();
        DriveClient drive(mock.transport(), mock.runner());
        try {
            (void)drive.imagesUpload("Synthetic", {DocxImage{.bytes = "img", .widthPx = 1, .heightPx = 1, .type = "png"}});
            FAIL("Expected ImagesUploadError");
        } catch (const ImagesUploadError &error) {
            CHECK(error.documentId == "upload_doc_404");
            CHECK(std::string(error.what()) == gws_error_format(body, "upload_doc_404"));
        }
        CHECK(mock.calls == 2);
        token_cache_reset();
    }
}

TEST_SUITE("api::write_tracker") {
    TEST_CASE("tracks writes and states") {
        using namespace gdocsmith;

        WriteTracker tracker;
        CHECK(tracker.writeState(true) == "none");

        tracker.recordWrite(WriteAttempt{
            .action = "export",
            .doc = "doc-1",
            .outcome = "confirmed",
            .revision = "rev-1",
        });
        CHECK(tracker.writeState(true) == "complete");
        CHECK(tracker.writeState(false) == "partial");
        CHECK(tracker.revisions["doc-1"] == "rev-1");

        tracker.recordWrite(WriteAttempt{
            .action = "import",
            .doc = "doc-2",
            .outcome = "unknown",
        });
        CHECK(tracker.writeState(true) == "unknown");
    }
}
