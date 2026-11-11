#pragma once

#include "storage.hpp"
#include "api.hpp"
#include "test.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>

TEST_CASE("SHA-256 snapshots use portable byte-exact hashing") {
    CHECK(gdocsmith::detail::sha256_hex("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(gdocsmith::detail::sha256_hex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(gdocsmith::detail::sha256_hex(std::string_view("a\0b", 3)) ==
          "59b271ae1bbcb1d31d41929817f4b16fb439eb4f31520b5ad1d5ce98920a7138");
}

TEST_CASE("artifact_root precedence and permissions") {
    slimr::testing::TempDirGuard temp;
    const char* prevEnv = getenv("GDOCSMITH_WORK_DIR");
    std::string envBackup = prevEnv ? prevEnv : "";
    bool hadEnv = (prevEnv != nullptr);

    // 1. Explicit GDOCSMITH_WORK_DIR
    std::string explicitDir = (temp.path / "explicit_work").string();
    setenv("GDOCSMITH_WORK_DIR", explicitDir.c_str(), 1);
    CHECK(gdocsmith::artifact_root() == explicitDir);

    // 2. Workspace root
    unsetenv("GDOCSMITH_WORK_DIR");
    std::string wsDir = (temp.path / "workspace").string();
    std::filesystem::create_directories(wsDir);
    gdocsmith::set_workspace_root(wsDir);

    std::string wsRoot = gdocsmith::artifact_root();
    CHECK(wsRoot == wsDir + "/.gdocsmith");

    struct stat st{};
    CHECK(stat(wsRoot.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0700);

    std::string gitignorePath = wsRoot + "/.gitignore";
    CHECK(stat(gitignorePath.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    std::ifstream f(gitignorePath);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(content == "*\n");

    // 3. Root or Home workspace falls back to temp
    gdocsmith::set_workspace_root("/");
    std::string rootFallback = gdocsmith::artifact_root();
    CHECK(rootFallback.find("gdocsmith-") != std::string::npos);

    const char* home = getenv("HOME");
    if (home) {
        gdocsmith::set_workspace_root(home);
        std::string homeFallback = gdocsmith::artifact_root();
        CHECK(homeFallback == rootFallback);
    }

    // Restore
    gdocsmith::set_workspace_root(std::nullopt);
    if (hadEnv) {
        setenv("GDOCSMITH_WORK_DIR", envBackup.c_str(), 1);
    } else {
        unsetenv("GDOCSMITH_WORK_DIR");
    }
}

TEST_CASE("export directory and export_folder_write layout") {
    slimr::testing::TempDirGuard temp;
    std::string root = temp.path.string();

    // Invalid doc ID rejected
    CHECK_THROWS_WITH_AS(gdocsmith::export_directory(root, "invalid doc id!", "r1"), "invalid document ID", std::runtime_error);
    // Invalid revision ID rejected
    CHECK_THROWS_WITH_AS(gdocsmith::export_directory(root, "doc123", "r1/bad"), "the document revision ID contains characters a folder name can't hold", std::runtime_error);

    // Missing revision generates no-revision. timestamp without colons
    std::string noRevDir = gdocsmith::export_directory(root, "doc123", std::nullopt);
    CHECK(noRevDir.find("no-revision.") != std::string::npos);
    CHECK(noRevDir.find(':') == std::string::npos);

    // Full export_folder_write with base.md, next.md, comments.yaml, suggestions.yaml
    std::string docId = "test_doc_123";
    std::string revId = "rev_001";
    std::string initialMd = "---\ntitle: Doc\n---\nHello world\n";
    std::string commentsYaml = "comments:\n  - id: c1\n    content: test comment\n";
    std::string suggestionsYaml = "suggestions:\n  - id: s1\n";

    auto res = gdocsmith::export_folder_write(root, docId, revId, initialMd, commentsYaml, suggestionsYaml);
    CHECK(res.warnings.empty());
    CHECK(std::filesystem::exists(res.baseFilePath));
    CHECK(std::filesystem::exists(res.nextFilePath));
    CHECK(res.commentsFilePath.has_value());
    CHECK(std::filesystem::exists(*res.commentsFilePath));
    CHECK(res.suggestionsFilePath.has_value());
    CHECK(std::filesystem::exists(*res.suggestionsFilePath));

    // Verify layout files
    std::string dir = gdocsmith::export_directory(root, docId, revId);
    CHECK(std::filesystem::exists(dir + "/base.md"));
    CHECK(std::filesystem::exists(dir + "/next.md"));
    CHECK(std::filesystem::exists(dir + "/export.md"));
    CHECK(std::filesystem::exists(dir + "/export.orig.md"));
    CHECK(std::filesystem::exists(dir + "/comments.yaml"));
    CHECK(std::filesystem::exists(dir + "/suggestions.yaml"));

    // Verify unedited re-export is a clean refresh without warning
    std::string updatedMd = "---\ntitle: Doc Updated\n---\nHello world updated\n";
    auto res2 = gdocsmith::export_folder_write(root, docId, revId, updatedMd);
    CHECK(res2.warnings.empty());
    CHECK(gdocsmith::detail::read_file_string(res2.nextFilePath) == updatedMd);
    CHECK(gdocsmith::detail::read_file_string(res2.baseFilePath) == updatedMd);

    // Verify that local edits to next.md are preserved and generate a warning
    std::string localEdit = "---\ntitle: Local Edits\n---\nLocal changes made by agent\n";
    {
        std::ofstream f(res2.nextFilePath);
        f << localEdit;
    }
    std::string cloudNewMd = "---\ntitle: Cloud Changed\n---\nCloud content\n";
    auto res3 = gdocsmith::export_folder_write(root, docId, revId, cloudNewMd);
    CHECK_FALSE(res3.warnings.empty());
    CHECK(res3.warnings[0].find("local edits") != std::string::npos);
    // next.md should keep localEdit!
    CHECK(gdocsmith::detail::read_file_string(res3.nextFilePath) == localEdit);
    // base.md should receive cloudNewMd
    CHECK(gdocsmith::detail::read_file_string(res3.baseFilePath) == cloudNewMd);
}

TEST_CASE("file_replace atomic and permission preserving") {
    slimr::testing::TempDirGuard temp;
    std::string target = (temp.path / "file.md").string();

    // Rejects relative path
    CHECK_THROWS_AS(gdocsmith::file_replace("relative.md", "content"), std::runtime_error);

    // New file created
    gdocsmith::file_replace(target, "first version");
    CHECK(gdocsmith::detail::read_file_string(target) == "first version");

    // Existing file mode preserved
    chmod(target.c_str(), 0640);
    struct stat stBefore{};
    stat(target.c_str(), &stBefore);
    CHECK((stBefore.st_mode & 0777) == 0640);

    gdocsmith::file_replace(target, "second version");
    CHECK(gdocsmith::detail::read_file_string(target) == "second version");
    struct stat stAfter{};
    stat(target.c_str(), &stAfter);
    CHECK((stAfter.st_mode & 0777) == 0640);
}

TEST_CASE("input_file_read and input_file_changed") {
    slimr::testing::TempDirGuard temp;
    std::string path = (temp.path / "input.md").string();

    // Rejects relative path
    CHECK_THROWS_AS(gdocsmith::input_file_read("relative.md"), std::runtime_error);

    // Create file
    std::string content = "---\ntitle: Test\n---\nInitial content\n";
    {
        std::ofstream f(path);
        f << content;
    }

    auto snapshot = gdocsmith::input_file_read(path);
    CHECK(snapshot.text == content);
    CHECK_FALSE(snapshot.hash.empty());
    CHECK_FALSE(gdocsmith::input_file_changed(snapshot));

    // Modify file
    {
        std::ofstream f(path);
        f << "---\ntitle: Modified\n---\n";
    }
    CHECK(gdocsmith::input_file_changed(snapshot));

    // Delete file
    std::filesystem::remove(path);
    CHECK(gdocsmith::input_file_changed(snapshot));
}


TEST_CASE("document_id normalization") {
    CHECK(gdocsmith::document_id("1abcXYZ_-123") == "1abcXYZ_-123");
    CHECK(gdocsmith::document_id("https://docs.google.com/document/d/1abcXYZ_-123/edit") == "1abcXYZ_-123");
    CHECK(gdocsmith::document_id("https://docs.google.com/document/d/1abcXYZ_-123/edit?tab=t.0") == "1abcXYZ_-123");
    CHECK_THROWS_AS(gdocsmith::document_id("invalid id spaces"), gdocsmith::CoreError);
}

TEST_CASE("entities_decode for Drive comment quotes") {
    CHECK(gdocsmith::entities_decode("&quot;Hello&quot; &amp; &lt;World&gt;") == "\"Hello\" & <World>");
    CHECK(gdocsmith::entities_decode("It&#39;s &apos;good&apos;") == "It's 'good'");
    CHECK(gdocsmith::entities_decode("&#65;&#66;&#67;") == "ABC");
    CHECK(gdocsmith::entities_decode("&#x41;&#x42;&#x43;") == "ABC");
    CHECK(gdocsmith::entities_decode("Plain text without entities") == "Plain text without entities");
}

TEST_CASE("gws_error_format actionable error messages") {
    // 403
    std::string err403 = R"({"error": {"code": 403, "message": "The caller does not have permission"}})";
    std::string formatted403 = gdocsmith::gws_error_format(err403, "doc123");
    CHECK(formatted403.find("Permission denied on \"doc123\"") != std::string::npos);

    // 404
    std::string err404 = R"({"error": {"code": 404, "message": "File not found: doc123"}})";
    std::string formatted404 = gdocsmith::gws_error_format(err404, "doc123");
    CHECK(formatted404.find("Document or file not found \"doc123\"") != std::string::npos);

    // 401
    std::string err401 = R"({"error": {"code": 401, "message": "invalid_grant"}})";
    std::string formatted401 = gdocsmith::gws_error_format(err401);
    CHECK(formatted401.find("gws auth login") != std::string::npos);
}

TEST_CASE("mock DocsClient and WriteTracker") {
    gdocsmith::token_cache_reset();

    gdocsmith::HttpTransport mockTransport = [](const gdocsmith::HttpCall& req) -> gdocsmith::HttpCallResponse {
        // OAuth token refresh
        if (req.url == "https://oauth2.googleapis.com/token") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"access_token": "mock_token_123", "expires_in": 3600, "token_type": "Bearer"})",
            };
        }
        // batchUpdate
        if (req.url.find(":batchUpdate") != std::string::npos) {
            CHECK(req.headers.at("Authorization") == "Bearer mock_token_123");
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"writeControl": {"requiredRevisionId": "rev_2"}})",
            };
        }
        // getDocument
        if (req.url.find("docs.googleapis.com/v1/documents/doc_abc") != std::string::npos) {
            CHECK(req.headers.at("Authorization") == "Bearer mock_token_123");
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"documentId": "doc_abc", "title": "Mock Title", "revisionId": "rev_1"})",
            };
        }
        return gdocsmith::HttpCallResponse{.status = 404, .headers = {}, .body = "Not found"};
    };

    gdocsmith::GwsRunner mockRunner = [](const std::vector<std::string>&) -> std::string {
        return R"({"client_id": "test_id", "client_secret": "test_secret", "refresh_token": "test_refresh"})";
    };

    gdocsmith::DocsClient client(mockTransport, mockRunner);
    auto doc = client.getDocument("doc_abc");
    CHECK(doc["documentId"].get<std::string>() == "doc_abc");
    CHECK(doc["title"].get<std::string>() == "Mock Title");

    // batchUpdate
    slimr::Json::array_t reqs;
    auto batchRes = client.batchUpdate("doc_abc", reqs, "rev_1");
    CHECK(batchRes["writeControl"]["requiredRevisionId"].get<std::string>() == "rev_2");

    // WriteTracker
    gdocsmith::WriteTracker tracker;
    CHECK(tracker.writeState(true) == "none");

    tracker.recordWrite(gdocsmith::WriteAttempt{
        .action = "batchUpdate",
        .doc = "doc_abc",
        .outcome = "confirmed",
        .requestCount = 1,
        .revision = "rev_2",
    });
    CHECK(tracker.writeState(true) == "complete");
    CHECK(tracker.writeState(false) == "partial");

    tracker.recordWrite(gdocsmith::WriteAttempt{
        .action = "batchUpdate",
        .doc = "doc_abc",
        .outcome = "unknown",
        .requestCount = 1,
    });
    CHECK(tracker.writeState(true) == "unknown");
}


TEST_CASE("mock DriveClient operations") {
    gdocsmith::token_cache_reset();

    gdocsmith::HttpTransport mockTransport = [](const gdocsmith::HttpCall& req) -> gdocsmith::HttpCallResponse {
        // OAuth token refresh
        if (req.url == "https://oauth2.googleapis.com/token") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"access_token": "mock_token_drive", "expires_in": 3600, "token_type": "Bearer"})",
            };
        }
        // about / user email
        if (req.url.find("drive/v3/about") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"user": {"emailAddress": "account@example.com"}})",
            };
        }
        // copy file
        if (req.url.find("/copy") != std::string::npos && req.method == "POST") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"id": "copied_doc_id", "name": "Copied Doc"})",
            };
        }
        // update file
        if (req.method == "PATCH" && req.url.find("/comments") == std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"id": "doc_123", "name": "Updated", "trashed": true})",
            };
        }
        // delete file
        if (req.method == "DELETE" && req.url.find("/permissions") == std::string::npos && req.url.find("/comments") == std::string::npos) {
            return gdocsmith::HttpCallResponse{.status = 204, .headers = {}, .body = ""};
        }
        // list permissions
        if (req.method == "GET" && req.url.find("/permissions") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"permissions": [{"id": "perm_1", "role": "reader", "type": "user", "emailAddress": "test@example.com"}]})",
            };
        }
        // create permission
        if (req.method == "POST" && req.url.find("/permissions") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"id": "perm_new", "role": "writer", "type": "user", "emailAddress": "writer@example.com"})",
            };
        }
        // comments list
        if (req.method == "GET" && req.url.find("/comments?") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"comments": [{"id": "comm_1", "content": "Thread 1", "resolved": false, "quotedFileContent": {"value": "&quot;Hello World&quot;"}}]})",
            };
        }
        // text export
        if (req.url.find("/export?mimeType=text%2Fplain") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = "\xEF\xBB\xBFLine 1\r\nLine 2\rLine 3\n",
            };
        }
        return gdocsmith::HttpCallResponse{.status = 404, .headers = {}, .body = "Not found"};
    };

    gdocsmith::GwsRunner mockRunner = [](const std::vector<std::string>&) -> std::string {
        return R"({"client_id": "test_id", "client_secret": "test_secret", "refresh_token": "test_refresh"})";
    };

    gdocsmith::DriveClient drive(mockTransport, mockRunner);

    // userEmailGet
    CHECK(drive.userEmailGet() == "account@example.com");

    // copyFile
    auto copied = drive.copyFile("source_id", "Copied Doc");
    CHECK(copied["id"].get<std::string>() == "copied_doc_id");

    // updateFile
    auto updated = drive.updateFile("doc_123", slimr::Json::object_t{{"trashed", true}});
    CHECK(updated["trashed"].get<bool>() == true);

    // deleteFile (void return)
    drive.deleteFile("doc_123");

    // listPermissions
    auto perms = drive.listPermissions("doc_123");
    REQUIRE(perms.size() == 1);
    CHECK(perms[0].id == "perm_1");
    CHECK(perms[0].role == "reader");
    CHECK(perms[0].emailAddress == "test@example.com");

    // createPermission
    gdocsmith::DrivePermission newPerm{
        .id = "",
        .role = "writer",
        .type = "user",
        .displayName = std::nullopt,
        .domain = std::nullopt,
        .emailAddress = "writer@example.com",
    };
    auto createdPerm = drive.createPermission("doc_123", newPerm);
    CHECK(createdPerm.id == "perm_new");
    CHECK(createdPerm.role == "writer");

    // commentsList with HTML entity decoded quote
    auto comments = drive.commentsList("doc_123");
    REQUIRE(comments.size() == 1);
    CHECK(comments[0].id == "comm_1");
    CHECK(comments[0].quotedText == "\"Hello World\"");

    // textExport (BOM stripped and newlines normalized)
    std::string exported = drive.textExport("doc_123");
    CHECK(exported == "Line 1\nLine 2\nLine 3\n");
}


#include "app.hpp"

TEST_CASE("make_gdocsmith_app metadata and status tool") {
    auto app = gdocsmith::make_gdocsmith_app();
    CHECK(app.key == "gdocsmith-mcp");
    CHECK(app.version == "4.0.0");
    CHECK(app.description.find("Google Docs") != std::string::npos);
    CHECK(app.instructions.find("gdocsmith://guide") != std::string::npos);
    CHECK(app.bugsinkDsn.empty());

    // Guide resource
    REQUIRE(app.resources.size() >= 1);
    CHECK(app.resources[0].uri == "gdocsmith://guide");
    CHECK(app.resources[0].name == "guide");
    CHECK_FALSE(app.resources[0].load().empty());

    // Tools registered
    const auto& tools = app.tools();
    CHECK(tools.contains("status"));
    CHECK(tools.contains("export"));
    CHECK(tools.contains("import"));
    CHECK(tools.contains("file"));

    // status invocation
    auto res = tools.at("status").handler(slimr::Json::object_t{});
    CHECK_FALSE(res.isError);
    auto parsed = slimr::parse_json<slimr::Json>(res.content[0].text);
    REQUIRE(parsed.has_value());
    CHECK((*parsed)["version"].get<std::string>() == "4.0.0");
}

TEST_CASE("make_gdocsmith_app file lifecycle and guards") {
    gdocsmith::token_cache_reset();

    gdocsmith::HttpTransport mockTransport = [](const gdocsmith::HttpCall& req) -> gdocsmith::HttpCallResponse {
        if (req.url == "https://oauth2.googleapis.com/token") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"access_token": "mock_tok", "expires_in": 3600, "token_type": "Bearer"})",
            };
        }
        if (req.url.find("/permissions") != std::string::npos && req.method == "GET") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"permissions": [{"id": "p1", "role": "reader", "type": "user", "emailAddress": "user@example.com"}]})",
            };
        }
        if (req.url.find("/permissions") != std::string::npos && req.method == "POST") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"id": "p_new", "role": "writer", "type": "user"})",
            };
        }
        if (req.url.find("/permissions/p1") != std::string::npos && req.method == "DELETE") {
            return gdocsmith::HttpCallResponse{.status = 204, .headers = {}, .body = ""};
        }
        if (req.url.find("/copy") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"id": "copied_id", "name": "Copy Title"})",
            };
        }
        if (req.method == "DELETE") {
            return gdocsmith::HttpCallResponse{.status = 204, .headers = {}, .body = ""};
        }
        if (req.method == "PATCH") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"id": "doc123", "name": "doc123", "trashed": true})",
            };
        }
        return gdocsmith::HttpCallResponse{.status = 404, .headers = {}, .body = "Not found"};
    };

    gdocsmith::GwsRunner mockRunner = [](const std::vector<std::string>&) -> std::string {
        return R"({"client_id": "test_id", "client_secret": "test_secret", "refresh_token": "test_refresh"})";
    };

    auto app = gdocsmith::make_gdocsmith_app(mockTransport, mockRunner);
    const auto& fileTool = app.tools().at("file");

    // 1. permissions
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "permissions"},
            {"doc", "doc123"},
        };
        auto res = fileTool.handler(args);
        CHECK_FALSE(res.isError);
        auto j = *slimr::parse_json<slimr::Json>(res.content[0].text);
        CHECK(j["status"].get<std::string>() == "unchanged");
        CHECK(j["permissions"].get<slimr::Json::array_t>().size() == 1);
    }

    // 2. copy
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "copy"},
            {"doc", "doc123"},
            {"title", "My Copy"},
        };
        auto res = fileTool.handler(args);
        CHECK_FALSE(res.isError);
        auto j = *slimr::parse_json<slimr::Json>(res.content[0].text);
        CHECK(j["status"].get<std::string>() == "applied");
        CHECK(j["doc"].get<std::string>() == "copied_id");
        CHECK(j["warnings"].get<slimr::Json::array_t>().size() == 1);
    }

    // 3. delete refused without allow
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "delete"},
            {"doc", "doc123"},
        };
        auto res = fileTool.handler(args);
        CHECK(res.isError);
        CHECK(res.content[0].text.find("File delete needs the user's explicit approval") != std::string::npos);
    }

    // 4. delete succeeds with allow: ["delete"]
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "delete"},
            {"doc", "doc123"},
            {"allow", slimr::Json::array_t{"delete"}},
        };
        auto res = fileTool.handler(args);
        CHECK_FALSE(res.isError);
        auto j = *slimr::parse_json<slimr::Json>(res.content[0].text);
        CHECK(j["status"].get<std::string>() == "applied");
    }

    // 5. grant to "anyone" refused even with allow
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "grant"},
            {"doc", "doc123"},
            {"type", "anyone"},
            {"role", "reader"},
            {"allow", slimr::Json::array_t{"share"}},
        };
        auto res = fileTool.handler(args);
        CHECK(res.isError);
        CHECK(res.content[0].text.find("Granting anyone access is never done by this tool") != std::string::npos);
    }

    // 6. grant ownership refused even with allow
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "grant"},
            {"doc", "doc123"},
            {"type", "user"},
            {"role", "owner"},
            {"email", "owner@example.com"},
            {"allow", slimr::Json::array_t{"share"}},
        };
        auto res = fileTool.handler(args);
        CHECK(res.isError);
        CHECK(res.content[0].text.find("Ownership transfer is never done by this tool") != std::string::npos);
    }

    // 7. grant without allow refused
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "grant"},
            {"doc", "doc123"},
            {"type", "user"},
            {"role", "writer"},
            {"email", "collaborator@example.com"},
        };
        auto res = fileTool.handler(args);
        CHECK(res.isError);
        CHECK(res.content[0].text.find("File grant needs the user's explicit approval") != std::string::npos);
    }

    // 8. grant succeeds with allow: ["share"]
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "grant"},
            {"doc", "doc123"},
            {"type", "user"},
            {"role", "writer"},
            {"email", "collaborator@example.com"},
            {"allow", slimr::Json::array_t{"share"}},
        };
        auto res = fileTool.handler(args);
        CHECK_FALSE(res.isError);
        auto j = *slimr::parse_json<slimr::Json>(res.content[0].text);
        CHECK(j["status"].get<std::string>() == "applied");
    }

    // 9. revoke succeeds
    {
        slimr::Json args = slimr::Json::object_t{
            {"action", "revoke"},
            {"doc", "doc123"},
            {"permissionId", "p1"},
        };
        auto res = fileTool.handler(args);
        CHECK_FALSE(res.isError);
        auto j = *slimr::parse_json<slimr::Json>(res.content[0].text);
        CHECK(j["status"].get<std::string>() == "applied");
    }
}

TEST_CASE("make_gdocsmith_app export and import workflow") {
    gdocsmith::token_cache_reset();

    slimr::testing::TempDirGuard temp;
    std::string workDir = (temp.path / "work").string();
    setenv("GDOCSMITH_WORK_DIR", workDir.c_str(), 1);

    gdocsmith::HttpTransport mockTransport = [](const gdocsmith::HttpCall& req) -> gdocsmith::HttpCallResponse {
        if (req.url == "https://oauth2.googleapis.com/token") {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"access_token": "mock_tok", "expires_in": 3600, "token_type": "Bearer"})",
            };
        }
        if (req.url.find("/documents/doc_export_test") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"documentId": "doc_export_test", "title": "Export Test", "revisionId": "rev_exp_1"})",
            };
        }
        if (req.url.find("/comments?") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = R"({"comments": [{"id": "c1", "content": "Note on export", "resolved": false}]})",
            };
        }
        if (req.url.find("/export?mimeType=text%2Fplain") != std::string::npos) {
            return gdocsmith::HttpCallResponse{
                .status = 200,
                .headers = {},
                .body = "Paragraph 1\n\nParagraph 2\n",
            };
        }
        return gdocsmith::HttpCallResponse{.status = 404, .headers = {}, .body = "Not found"};
    };

    gdocsmith::GwsRunner mockRunner = [](const std::vector<std::string>&) -> std::string {
        return R"({"client_id": "test_id", "client_secret": "test_secret", "refresh_token": "test_refresh"})";
    };

    auto app = gdocsmith::make_gdocsmith_app(mockTransport, mockRunner);

    // 1. export
    slimr::Json expArgs = slimr::Json::object_t{
        {"doc", "doc_export_test"},
    };
    auto expRes = app.tools().at("export").handler(expArgs);
    CHECK_FALSE(expRes.isError);
    auto expJson = *slimr::parse_json<slimr::Json>(expRes.content[0].text);
    std::string exportPath = expJson["exportPath"].get<std::string>();
    CHECK(exportPath.find("next.md") != std::string::npos);
    CHECK(std::filesystem::exists(exportPath));
    CHECK(expJson.contains("commentsPath"));

    // Check directory layout: base.md, next.md, comments.yaml
    std::filesystem::path dir = std::filesystem::path(exportPath).parent_path();
    CHECK(std::filesystem::exists(dir / "base.md"));
    CHECK(std::filesystem::exists(dir / "next.md"));
    CHECK(std::filesystem::exists(dir / "comments.yaml"));

    // 2. import
    slimr::Json impArgs = slimr::Json::object_t{
        {"path", exportPath},
    };
    auto impRes = app.tools().at("import").handler(impArgs);
    CHECK_FALSE(impRes.isError);
    auto impJson = *slimr::parse_json<slimr::Json>(impRes.content[0].text);
    CHECK(impJson["status"].get<std::string>() == "unchanged");
    CHECK(impJson["verification"].get<std::string>() == "match");

    unsetenv("GDOCSMITH_WORK_DIR");
}
