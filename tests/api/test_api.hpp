#pragma once

#include "api.hpp"
#include "test.hpp"

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>

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
            {"dir/sub.txt", "Sub content\n"}
        };

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
            .type = "png"
        };
        DocxImage img2{
            .bytes = "fake_jpeg_data",
            .widthPx = 200,
            .heightPx = 150,
            .type = "jpeg"
        };

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
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url == "https://docs.googleapis.com/v1/documents" && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"documentId":"doc_new_123","title":"Test Doc"})"
                };
            }
            if (req.url.find("drive/v3/files/doc_new_123") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"doc_new_123","name":"Test Doc"})"
                };
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
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
        for (const auto& c : calls) {
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
        for (const auto& c : calls) {
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
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find(":batchUpdate") != std::string::npos) {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"documentId":"doc-123","replies":[]})"
                };
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DocsClient client(mockTransport, mockRunner);

        slimr::Json::array_t reqs = {
            slimr::Json::object_t{{"insertText", slimr::Json::object_t{{"text", "hello"}}}}
        };

        auto res = client.batchUpdate("doc-123", reqs, "rev-456");
        CHECK(res["documentId"].get<std::string>() == "doc-123");

        bool foundBatch = false;
        for (const auto& c : calls) {
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
        GwsRunner mockRunner = [&](const std::vector<std::string>& args) {
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
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("drive/v3/files/file-123") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"file-123","name":"Report.docx","mimeType":"application/vnd.google-apps.document","trashed":false,"parents":["folder-1"],"webViewLink":"https://drive.google.com/view","version":"10"})"
                };
            }
            if (req.url.find("drive/v3/files/file-123/copy") != std::string::npos) {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"file-123-copy","name":"Report Copy.docx"})"
                };
            }
            if (req.url.find("drive/v3/files/file-123") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"file-123","name":"Report Renamed.docx","trashed":true})"
                };
            }
            if (req.url.find("drive/v3/files/file-123") != std::string::npos && req.method == "DELETE") {
                return HttpCallResponse{.status = 204, .body = ""};
            }
            if (req.url.find("drive/v3/files?") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"files":[{"id":"f1","name":"Doc 1","mimeType":"text/plain"},{"id":"f2","name":"Doc 2","mimeType":"text/plain"}]})"
                };
            }
            if (req.url.find("drive/v3/about") != std::string::npos) {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"user":{"emailAddress":"user@example.com"}})"
                };
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
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
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("/comments?") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"comments":[{"id":"c1","content":"Great point","author":{"displayName":"Alice","emailAddress":"alice@example.com"},"quotedFileContent":{"value":"&quot;quote &amp; text&quot;"},"resolved":false,"replies":[{"id":"r1","content":"Thanks!","author":{"displayName":"Bob"}}]}]})"
                };
            }
            if (req.url.find("/comments/c1?") != std::string::npos && req.method == "GET") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"c1","content":"Great point"})"
                };
            }
            if (req.url.find("/comments?") != std::string::npos && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"c_new","content":"New comment"})"
                };
            }
            if (req.url.find("/comments/c1/replies") != std::string::npos && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"r_new","content":"Reply content","action":"resolve"})"
                };
            }
            if (req.url.find("/comments/c1/replies/r1") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"r1","content":"Updated reply content"})"
                };
            }
            if (req.url.find("/comments/c1/replies/r1") != std::string::npos && req.method == "DELETE") {
                return HttpCallResponse{.status = 204, .body = ""};
            }
            if (req.url.find("/comments/c1") != std::string::npos && req.method == "PATCH") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"c1","content":"Edited comment"})"
                };
            }
            if (req.url.find("/comments/c1") != std::string::npos && req.method == "DELETE") {
                return HttpCallResponse{.status = 204, .body = ""};
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
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
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
            calls.push_back(req);
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            if (req.url.find("upload/drive/v3/files") != std::string::npos && req.method == "POST") {
                return HttpCallResponse{
                    .status = 200,
                    .body = R"({"id":"upload_doc_777"})"
                };
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
                    })"
                };
            }
            return HttpCallResponse{.status = 404, .body = "not found"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        DriveClient drive(mockTransport, mockRunner);

        std::vector<DocxImage> images = {
            DocxImage{.bytes = "img1_bytes", .widthPx = 100, .heightPx = 100, .type = "png"},
            DocxImage{.bytes = "img2_bytes", .widthPx = 200, .heightPx = 150, .type = "jpeg"}
        };

        auto res = drive.imagesUpload("Image Upload Batch", images);
        CHECK(res.documentId == "upload_doc_777");
        REQUIRE(res.contentUris.size() == 2);
        CHECK(res.contentUris[0] == "https://lh3.googleusercontent.com/img1");
        CHECK(res.contentUris[1] == "https://lh3.googleusercontent.com/img2");

        // Verify upload call used multipart/related and included boundary
        bool foundMultipart = false;
        for (const auto& c : calls) {
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

        HttpTransport mockTransport = [](const HttpCall& req) -> HttpCallResponse {
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

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
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
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
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

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        auto res = fetch_with_retry(mockTransport, mockRunner, "https://example.com/api", "GET", "", {}, "", FetchRetryOptions{.backoffMs = 1, .retries = 3});
        CHECK(res.status == 200);
        CHECK(res.body == "Success!");
        CHECK(attempts == 3);
    }

    TEST_CASE("does not retry on permanent 400 error") {
        using namespace gdocsmith;

        int attempts = 0;
        HttpTransport mockTransport = [&](const HttpCall& req) -> HttpCallResponse {
            if (req.url == "https://oauth2.googleapis.com/token") {
                return HttpCallResponse{.status = 200, .body = R"({"access_token":"fake_token","expires_in":3600})"};
            }
            attempts++;
            return HttpCallResponse{.status = 400, .body = R"({"error":{"code":400,"message":"Bad request"}})"};
        };

        GwsRunner mockRunner = [](const std::vector<std::string>&) {
            return R"({"client_id":"id","client_secret":"sec","refresh_token":"tok"})";
        };

        token_cache_reset();
        CHECK_THROWS_AS(
            fetch_with_retry(mockTransport, mockRunner, "https://example.com/api", "GET", "", {}, "", FetchRetryOptions{.backoffMs = 1, .retries = 3}),
            std::runtime_error
        );
        CHECK(attempts == 1);
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
