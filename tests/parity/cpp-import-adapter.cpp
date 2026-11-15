#include "app.hpp"
#include "emulator/fake_google.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

namespace {
using slimr::Json;
namespace fs = std::filesystem;

std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read parity input");
    return {std::istreambuf_iterator<char>(in), {}};
}

void write(const fs::path& path, const std::string& value) {
    std::ofstream out(path, std::ios::binary);
    out << value;
    out.close();
    if (!out) throw std::runtime_error("Cannot write parity input");
}

void replace(std::string& value, const std::string& from, const std::string& to) {
    const auto at = value.find(from);
    if (at == std::string::npos) throw std::runtime_error("Parity edit did not reach expected text");
    value.replace(at, from.size(), to);
}

Json tabs(const gdocsmith::lens::DocumentFile& file) {
    Json::array_t out;
    for (const auto& tab : file.tabs) {
        out.push_back(Json::object_t{{"id", tab.id}, {"title", tab.title}, {"markdown", tab.markdown}});
    }
    return out;
}

std::string png(unsigned width, unsigned height) {
    std::string bytes(33, '\0');
    bytes.replace(0, 8, "\x89PNG\r\n\x1a\n", 8);
    bytes[11] = 13;
    bytes.replace(12, 4, "IHDR");
    for (unsigned i = 0; i < 4; ++i) {
        bytes[16 + i] = static_cast<char>((width >> ((3 - i) * 8)) & 255U);
        bytes[20 + i] = static_cast<char>((height >> ((3 - i) * 8)) & 255U);
    }
    return bytes;
}

Json provider_ids(const Json& document) {
    std::vector<std::string> ids;
    for (const auto& tab : document["tabs"].get<Json::array_t>()) {
        for (const auto& block : tab["documentTab"]["body"]["content"].get<Json::array_t>()) {
            if (!block.contains("paragraph")) continue;
            for (const auto& element : block["paragraph"]["elements"].get<Json::array_t>()) {
                if (element.contains("inlineObjectElement")) {
                    ids.push_back(element["inlineObjectElement"]["inlineObjectId"].get<std::string>());
                }
            }
        }
    }
    std::ranges::sort(ids);
    Json::array_t values;
    for (const auto& id : ids) values.emplace_back(id);
    return values;
}

std::string file_uri(const fs::path& path) {
    std::string uri = "file://";
    constexpr char digits[] = "0123456789ABCDEF";
    for (unsigned char byte : path.generic_string()) {
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '/' || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
            uri += static_cast<char>(byte);
        } else {
            uri += '%';
            uri += digits[byte >> 4];
            uri += digits[byte & 15];
        }
    }
    return uri;
}

Json run(const Json& input) {
    using namespace gdocsmith;
    const auto caseId = input["caseId"].get<std::string>();
    const auto fault = input["fault"].get<std::string>();
    const std::set<std::string> cases{
        "V001-noop", "V002-body-and-title", "V003-stale", "V004-landed-lost-response-replay",
        "V005-font-default", "V006-font-nondefault", "V007-cell-style-edit", "V008-reject-hidden-insertion",
        "V009-image-upload-lifecycle", "V010-cross-doc-image-copy",
        "W2-rename-only", "W2-delayed-docs-title", "W2-rename-fail-after-content",
        "W1-create-success-replay", "W1-create-rollback-retry", "W1-create-rollback-rejected",
        "W1-create-mismatch-readback", "W1-create-unknown-outcome", "W1-create-invalid-syntax",
        "W3-reparent-order-noop", "W3-orphan-refusal", "W3-delete-subtree-and-all",
        "W4-comment-allow-and-actions", "W4-comment-ownership-and-readonly",
        "W4-comment-resolved-reopen", "W4-comment-partial-unknown-outcome"};
    if (!cases.contains(caseId) || (fault != "none" && fault != "ack-without-mutation" &&
        fault != "race-before-send" && fault != "leak-upload-document" && fault != "known-upload-error" &&
        fault != "delayed-docs-title" && fault != "fail-rename-after-content" &&
        fault != "ack-rename-without-mutation" && fault != "fail-content-after-create" &&
        fault != "fail-content-and-delete" && fault != "mismatch-after-create" &&
        fault != "lost-create-response" && fault != "ack-delete-without-mutation" &&
        fault != "ack-move-without-mutation" && fault != "fail-second-comment-action" &&
        fault != "ack-comment-without-mutation")) {
        throw std::runtime_error("Unknown parity case/fault");
    }
    const fs::path relativeRoot(input["workRoot"].get<std::string>());
    const auto root = fs::canonical(relativeRoot);
    const auto cwd = fs::canonical(fs::current_path());
    const auto rel = root.lexically_relative(cwd);
    if (relativeRoot.is_absolute() || root != fs::absolute(relativeRoot).lexically_normal() ||
        rel.empty() || *rel.begin() == ".." || root == cwd || !fs::exists(root / ".parity-owned") ||
        (fs::status(root).permissions() & (fs::perms::group_all | fs::perms::others_all)) != fs::perms::none) {
        throw std::runtime_error("Expected private canonical project-relative owned run root");
    }
    const auto seedBytes = read(input["seedPath"].get<std::string>());
    const auto parsedSeed = slimr::parse_json<Json>(seedBytes);
    if (!parsedSeed) throw std::runtime_error("Invalid parity seed JSON");
    auto seed = *parsedSeed;
    const auto sourceSeed = seed.contains("source") ? seed["source"] : Json(nullptr);
    std::vector<engine::DriveComment> reviewComments;
    if (seed.contains("comments")) {
        const auto comments = slimr::parse_json<std::vector<engine::DriveComment>>(slimr::write_json(seed["comments"]));
        if (!comments) throw std::runtime_error("Invalid synthetic comments");
        reviewComments = *comments;
    }
    if (seed.contains("document")) seed = seed["document"];
    if (seed.contains("target")) seed = seed["target"];
    if (seed.contains("variants")) seed = seed["variants"][caseId == "V005-font-default" ? "11" : "22"];
    const bool isW1 = caseId.starts_with("W1-");
    emulator::FakeGoogle backend;
    if (!isW1) {
        backend.seed_document("synthetic-doc", seed, 1);
        if (!sourceSeed.is_null()) backend.seed_document("synthetic-source", sourceSeed, 1);
    }
    auto externalEdit = [&](const std::string& id) {
        auto& state = backend.get_doc(id);
        state.json = emulator::requests_emulate(state.json, {}).json;
        ++state.revision;
    };
    engine::ImportServices services;
    services.root = (root / "work").string();
    services.apiUser = "parity@example.test";
    Json::array_t mutations;
    Json::array_t calls;
    Json::array_t fetched;
    Json::array_t uploadReceipts;
    Json::array_t deletions;
    std::set<std::string> completedDeletes;
    std::vector<std::string> createdIds;
    std::string phase = fault == "known-upload-error" ? "known-upload-failure" : "positive";
    int faultCalls = 0;
    int commentSends = 0;
    int nextCommentId = 1;
    bool injectedContentFailure = false;
    bool wrote = false;
    services.getDocument = [&](const std::string& id) {
        calls.push_back(Json::object_t{{"method", "getDocument"}, {"args", Json::array_t{id}}});
        auto json = backend.get_doc(id).json;
        json["revisionId"] = backend.revision_id(backend.get_doc(id));
        if (fault == "mismatch-after-create" && wrote) {
            ++faultCalls;
            for (auto& tab : json["tabs"].get<Json::array_t>()) {
                for (auto& element : tab["documentTab"]["body"]["content"].get<Json::array_t>()) {
                    if (!element.contains("paragraph")) continue;
                    for (auto& inl : element["paragraph"]["elements"].get<Json::array_t>()) {
                        if (!inl.contains("textRun") || !inl["textRun"].contains("content")) continue;
                        auto text = inl["textRun"]["content"].get<std::string>();
                        replace(text, "Body", "Bodx");
                        inl["textRun"]["content"] = std::move(text);
                    }
                }
            }
        }
        if (!mutations.empty() && (caseId == "V005-font-default" || caseId == "V006-font-nondefault")) {
            for (auto& tab : json["tabs"].get<Json::array_t>()) {
                for (auto& element : tab["documentTab"]["body"]["content"].get<Json::array_t>()) {
                    if (!element.contains("paragraph")) continue;
                    for (auto& inl : element["paragraph"]["elements"].get<Json::array_t>()) {
                        if (!inl.contains("textRun") || !inl["textRun"].contains("textStyle")) continue;
                        auto& style = inl["textRun"]["textStyle"].get<Json::object_t>();
                        style.erase("fontSize");
                        style.erase("weightedFontFamily");
                    }
                }
            }
        }
        if (fault == "delayed-docs-title" && id == "synthetic-doc" &&
            std::ranges::any_of(mutations, [](const Json& m) { return m["kind"].get<std::string>() == "rename"; })) {
            ++faultCalls;
            json["title"] = "Example";
        }
        return json;
    };
    services.getFileMetadata = [&](const std::string& id) {
        calls.push_back(Json::object_t{{"method", "getFileMetadata"}, {"args", Json::array_t{id}}});
        return Json::object_t{{"id", id}, {"name", backend.get_doc(id).json["title"]}};
    };
    services.createDocument = [&](const std::string& title) -> Json {
        if (!isW1) throw std::runtime_error("Unexpected createDocument in existing-document vector");
        calls.push_back(Json::object_t{{"method", "createDocument"}, {"args", Json::array_t{title}}});
        const auto id = backend.create_document(title);
        createdIds.push_back(id);
        if (fault == "lost-create-response") {
            ++faultCalls;
            throw std::runtime_error("socket hang up");
        }
        return backend.get_doc(id).json;
    };
    services.deleteFile = [&](const std::string& id) {
        if (caseId != "V009-image-upload-lifecycle" && !isW1) throw std::runtime_error("Unexpected deleteFile");
        calls.push_back(Json::object_t{{"method", "deleteFile"}, {"args", Json::array_t{id}}});
        deletions.push_back(Json::object_t{{"phase", phase}, {"id", id}});
        if (fault == "fail-content-and-delete") {
            ++faultCalls;
            throw std::runtime_error("delete rejected");
        }
        if (fault == "leak-upload-document" || fault == "ack-delete-without-mutation") {
            ++faultCalls;
            return;
        }
        backend.get_doc(id).deleted = true;
        completedDeletes.insert(id);
    };
    services.fetchImage = [&](const std::string& uri) {
        if (caseId != "V009-image-upload-lifecycle") throw std::runtime_error("Unexpected external image fetch");
        fetched.push_back(Json::object_t{{"phase", phase}, {"url", uri}});
        return png(10, 10);
    };
    services.imagesUpload = [&](const std::string& name, const std::vector<DocxImage>& images) {
        if (caseId != "V009-image-upload-lifecycle") throw std::runtime_error("Unexpected image upload");
        calls.push_back(Json::object_t{{"method", "imagesUpload"}, {"args", Json::array_t{name, images.size()}}});
        const auto id = backend.create_document(name);
        calls.push_back(Json::object_t{{"method", "createDocument"}, {"args", Json::array_t{name}}});
        ImagesUploadResult receipt;
        receipt.documentId = id;
        Json::array_t uris;
        Json::array_t metadata;
        for (std::size_t i = 0; i < images.size(); ++i) {
            const auto uri = "https://fake.googleusercontent.com/" + id + "/" + std::to_string(i);
            receipt.contentUris.push_back(uri);
            uris.emplace_back(uri);
            metadata.push_back(Json::object_t{{"type", images[i].type},
                {"widthPx", images[i].widthPx}, {"heightPx", images[i].heightPx},
                {"bytesDigest", engine::detail::sha256_hex(images[i].bytes)}});
        }
        uploadReceipts.push_back(Json::object_t{{"documentId", id}, {"contentUris", std::move(uris)},
            {"phase", phase}, {"images", std::move(metadata)}});
        if (phase == "known-upload-failure") throw ImagesUploadError("synthetic upload response failure", id);
        return receipt;
    };
    services.commentsList = [&](const std::string& id) {
        calls.push_back(Json::object_t{{"method", "commentsList"}, {"args", Json::array_t{id}}});
        return id == "synthetic-doc" ? reviewComments : std::vector<engine::DriveComment>{};
    };
    services.textExport = [&](const std::string& id) {
        calls.push_back(Json::object_t{{"method", "textExport"}, {"args", Json::array_t{id}}});
        return "";
    };
    services.commentSend = [&](const std::string& id, const engine::CommentAction& action) -> Json {
        if (!caseId.starts_with("W4-")) throw std::runtime_error("Unexpected comment mutation");
        ++commentSends;
        calls.push_back(Json::object_t{{"method", "commentSend"}, {"args", Json::array_t{id, action.kind}}});
        if (fault == "fail-second-comment-action" && commentSends == 2) {
            ++faultCalls;
            throw std::runtime_error("network down");
        }
        if (fault == "ack-comment-without-mutation") {
            ++faultCalls;
            const auto cid = action.id.value_or("");
            if (action.kind == "create") return Json::object_t{{"id", "c-new-1"}, {"content", action.text.value_or("")}};
            if (action.kind == "reply") return Json::object_t{{"id", "r-new-1"}, {"content", action.text.value_or("")}};
            if (action.kind == "resolve") return Json::object_t{{"id", cid}, {"action", "resolve"}};
            if (action.kind == "reopen") return Json::object_t{{"id", cid}, {"action", "reopen"}};
            if (action.kind == "edit" || action.kind == "replyEdit") {
                return Json::object_t{{"id", action.replyId.value_or(cid)}, {"content", action.text.value_or("")}};
            }
            return Json::object_t{{"id", action.replyId.value_or(cid)}, {"deleted", true}};
        }
        if (action.kind == "create") {
            const auto newId = "c-new-" + std::to_string(nextCommentId++);
            engine::DriveComment nc;
            nc.id = newId;
            nc.author = {.displayName = "Me", .me = true};
            nc.content = action.text.value_or("");
            nc.resolved = false;
            reviewComments.push_back(std::move(nc));
            return Json::object_t{{"id", newId}, {"content", action.text.value_or("")}};
        }
        const auto cid = action.id.value_or("");
        auto it = std::ranges::find_if(reviewComments, [&](const auto& c) { return c.id == cid; });
        if (it == reviewComments.end()) throw std::runtime_error("Comment not found: " + cid);
        if (action.kind == "edit") {
            it->content = action.text.value_or("");
            return Json::object_t{{"id", it->id}, {"content", it->content.value_or("")}};
        }
        if (action.kind == "resolve" || action.kind == "reopen") {
            const auto newId = "r-new-" + std::to_string(nextCommentId++);
            it->resolved = (action.kind == "resolve");
            typename decltype(it->replies)::value_type nr;
            nr.id = newId;
            nr.author = {.displayName = "Me", .me = true};
            nr.action = action.kind;
            it->replies.push_back(std::move(nr));
            return Json::object_t{{"id", newId}, {"action", action.kind}};
        }
        if (action.kind == "delete") {
            const auto deletedId = it->id;
            reviewComments.erase(it);
            return Json::object_t{{"id", deletedId}, {"deleted", true}};
        }
        if (action.kind == "reply") {
            const auto newId = "r-new-" + std::to_string(nextCommentId++);
            typename decltype(it->replies)::value_type nr;
            nr.id = newId;
            nr.author = {.displayName = "Me", .me = true};
            nr.content = action.text.value_or("");
            it->replies.push_back(std::move(nr));
            return Json::object_t{{"id", newId}, {"content", action.text.value_or("")}};
        }
        if (!action.replyId) throw std::runtime_error("Missing replyId");
        auto rit = std::ranges::find_if(it->replies, [&](const auto& r) { return r.id == *action.replyId; });
        if (rit == it->replies.end()) throw std::runtime_error("Reply not found: " + *action.replyId);
        if (action.kind == "replyEdit") {
            rit->content = action.text.value_or("");
            return Json::object_t{{"id", rit->id}, {"content", rit->content.value_or("")}};
        }
        if (action.kind == "replyDelete") {
            const auto deletedId = rit->id;
            it->replies.erase(rit);
            return Json::object_t{{"id", deletedId}, {"deleted", true}};
        }
        throw std::runtime_error("Unsupported comment action: " + action.kind);
    };
    services.rename = [&](const std::string& id, const std::string& title) {
        mutations.push_back(Json::object_t{{"kind", "rename"}, {"body", Json::object_t{{"name", title}}}});
        calls.push_back(Json::object_t{{"method", "updateFile"}, {"args", Json::array_t{id, Json::object_t{{"name", title}}}}});
        if (fault == "fail-rename-after-content") {
            ++faultCalls;
            throw std::runtime_error("Google API error (403): insufficient permissions");
        }
        if (fault == "ack-rename-without-mutation") {
            ++faultCalls;
            return title;
        }
        backend.get_doc(id).json["title"] = title;
        return title;
    };
    services.batchUpdate = [&](const std::string& id, const std::vector<Json>& requests,
                                const std::optional<std::string>& revision) -> Json {
        Json::array_t reqs(requests.begin(), requests.end());
        Json entry = Json::object_t{{"kind", "batchUpdate"}, {"requests", std::move(reqs)}};
        if (revision) entry["requiredRevisionId"] = *revision;
        mutations.push_back(std::move(entry));
        calls.push_back(Json::object_t{{"method", "batchUpdate"},
            {"args", Json::array_t{id, Json::array_t(requests.begin(), requests.end()),
                Json::object_t{{"requiredRevisionId", revision ? Json(*revision) : Json(nullptr)}}}}});
        const auto mutationIndex = mutations.size() - 1;
        if (fault == "race-before-send") {
            ++faultCalls;
            externalEdit(id);
        }
        if (fault == "ack-without-mutation" || fault == "ack-move-without-mutation") {
            ++faultCalls;
            return Json::object_t{{"documentId", id}, {"replies", Json::array_t{}},
                {"writeControl", Json::object_t{{"requiredRevisionId", "rev-1"}}}};
        }
        if ((fault == "fail-content-after-create" || fault == "ack-delete-without-mutation") && !injectedContentFailure) {
            injectedContentFailure = true;
            ++faultCalls;
            throw std::runtime_error("network down");
        }
        if (fault == "fail-content-and-delete") {
            ++faultCalls;
            throw std::runtime_error("network down");
        }
        const auto parsedResponse = slimr::parse_json<Json>(backend.batch_update(id, requests, revision));
        wrote = true;
        if (!parsedResponse) throw std::runtime_error("Invalid fake backend response JSON");
        auto response = *parsedResponse;
        if (response.contains("writeControl") && response["writeControl"].contains("requiredRevisionId")) {
            mutations[mutationIndex]["receiptRevision"] = response["writeControl"]["requiredRevisionId"];
        }
        if (response.contains("replies")) mutations[mutationIndex]["replies"] = response["replies"];
        if (caseId == "V004-landed-lost-response-replay") throw std::runtime_error("connection lost after sending");
        return response;
    };
    std::string sourceToken;
    if (!sourceSeed.is_null()) {
        const auto sourceModel = model::docModelParse(services.getDocument("synthetic-source"),
            model::DocModelParseOptions{.docId = "synthetic-source"});
        const auto sourceExport = engine::exportModel(sourceModel, {}, services);
        const auto sourceBytes = read(sourceExport.exportPath);
        std::smatch match;
        if (!std::regex_search(sourceBytes, match, std::regex(R"(:image(?:\[[^\]]*\])?\{id=([^ }]+)\})"))) {
            throw std::runtime_error("Source image not exported");
        }
        sourceToken = match[1].str();
    }
    fs::path path;
    std::string initialBytes;
    if (isW1) {
        path = root / "work" / "authored.md";
        fs::create_directories(path.parent_path());
        fs::permissions(path.parent_path(), fs::perms::owner_all);
        std::string body;
        if (caseId == "W1-create-success-replay") {
            std::size_t idx = 0;
            for (const auto& tab : seed["tabs"].get<Json::array_t>()) {
                if (!body.empty()) body += "\n\n";
                body += "::tab{id=tab-" + std::to_string(idx++) + " title=" + tab["title"].get<std::string>() + "}\n\n" + tab["body"].get<std::string>() + "\n";
            }
        } else if (caseId == "W1-create-invalid-syntax") {
            body = seed["invalidBody"].get<std::string>() + "\n";
        } else {
            body = seed["singleBody"].get<std::string>() + "\n";
        }
        initialBytes = "---\ngdocsmith: " + std::string(lens::GDOCSMITH_NODE_VERSION) +
            "\ndoc: null\nrevision: null\ntitle: " + seed["title"].get<std::string>() + "\n---\n\n" + body;
        write(path, initialBytes);
    } else {
        auto original = model::docModelParse(services.getDocument("synthetic-doc"),
            model::DocModelParseOptions{.docId = "synthetic-doc"});
        const auto exported = engine::exportModel(original, reviewComments, services,
            engine::ExportOptions{.commentsIncludeResolved = caseId == "W4-comment-resolved-reopen"});
        path = exported.exportPath;
        initialBytes = read(path);
    }
    std::optional<lens::DocumentFile> initialFile;
    try {
        initialFile = lens::documentFileParse(initialBytes);
    } catch (const std::exception&) {}
    const bool hasRedCellDeclaration = initialFile && std::ranges::any_of(initialFile->frontmatter.cellStyles,
        [](const auto& entry) { return entry.second.backgroundColor == "#FF0000"; });
    const auto initialEnvelope = slimr::parse_frontmatter(initialBytes);
    const auto initialFields = initialEnvelope && initialEnvelope->frontmatter
        ? *initialEnvelope->frontmatter : slimr::json_obj();
    auto edited = initialBytes;
    if (caseId == "V002-body-and-title" || caseId == "V003-stale" ||
        caseId == "V004-landed-lost-response-replay" || caseId == "V005-font-default" ||
        caseId == "V006-font-nondefault" || caseId == "W2-rename-fail-after-content") replace(edited, "Hello world", "Hello revised world");
    if (caseId == "V002-body-and-title" || caseId == "W2-rename-only" ||
        caseId == "W2-delayed-docs-title" || caseId == "W2-rename-fail-after-content") replace(edited, "title: Example", "title: Renamed");
    auto mark_tab_delete = [](std::string& text, const std::string& id) {
        const auto needle = "::tab{id=" + id;
        std::size_t pos = 0;
        while ((pos = text.find(needle, pos)) != std::string::npos) {
            const auto next = pos + needle.size();
            if (next < text.size() && (text[next] == ' ' || text[next] == '}')) {
                const auto close = text.find('}', pos);
                if (close != std::string::npos) {
                    text.insert(close, " delete");
                }
                break;
            }
            pos = next;
        }
    };
    if (caseId == "W3-reparent-order-noop") {
        const auto fmEnd = edited.find("\n---\n");
        if (fmEnd != std::string::npos) {
            const auto header = edited.substr(0, fmEnd + 5);
            const auto body = edited.substr(fmEnd + 5);
            std::map<std::string, std::string> byId;
            std::size_t pos = body.find("::tab{id=");
            while (pos != std::string::npos) {
                const auto next = body.find("\n::tab{id=", pos + 1);
                auto block = next == std::string::npos ? body.substr(pos) : body.substr(pos, next - pos);
                while (!block.empty() && (block.back() == '\n' || block.back() == '\r')) block.pop_back();
                const auto idStart = pos + 9;
                const auto idEnd = body.find_first_of(" }", idStart);
                if (idEnd != std::string::npos) byId[body.substr(idStart, idEnd - idStart)] = std::move(block);
                pos = next == std::string::npos ? std::string::npos : next + 1;
            }
            auto set_parent = [](std::string block, const std::string& parent) {
                const auto nl = block.find('\n');
                auto first = nl == std::string::npos ? block : block.substr(0, nl);
                const auto rest = nl == std::string::npos ? std::string{} : block.substr(nl);
                const auto pPos = first.find(" parent=");
                if (pPos != std::string::npos) {
                    const auto pEnd = first.find_first_of(" }", pPos + 1);
                    if (pEnd != std::string::npos) first.erase(pPos, pEnd - pPos);
                }
                if (!parent.empty()) {
                    const auto close = first.rfind('}');
                    if (close != std::string::npos) first.insert(close, " parent=" + parent);
                }
                return first + rest;
            };
            edited = header + "\n" +
                set_parent(byId["t.a"], "") + "\n\n" +
                set_parent(byId["t.b"], "t.a") + "\n\n" +
                set_parent(byId["t.a1"], "") + "\n\n" +
                set_parent(byId["t.c"], "") + "\n\n" +
                set_parent(byId["t.a2"], "t.c") + "\n";
        }
    }
    if (caseId == "W3-orphan-refusal") {
        mark_tab_delete(edited, "t.a");
        mark_tab_delete(edited, "t.a1");
    }
    if (caseId == "W3-delete-subtree-and-all") {
        mark_tab_delete(edited, "t.a");
        mark_tab_delete(edited, "t.a1");
        mark_tab_delete(edited, "t.a2");
    }
    if (caseId == "V007-cell-style-edit") {
        if (!hasRedCellDeclaration) throw std::runtime_error("Red cell declaration not exported");
        replace(edited, "#FF0000", "#00FF00");
    }
    Json observation = Json::object_t{};
    Json imageObservation = Json::object_t{};
    if (caseId == "V008-reject-hidden-insertion") {
        auto snippet = edited;
        replace(snippet, ":change[Hello world]", ":change[Hello there]");
        write(path, snippet);
        observation["snippetResult"] = detail::file_write_result_json(engine::fileImport(engine::ImportInput{.path = path.string()}, services));
        const auto start = edited.find("::suggestion{id=s1");
        if (start == std::string::npos) throw std::runtime_error("Suggestion block not exported");
        const auto end = edited.find('}', start);
        if (end == std::string::npos) throw std::runtime_error("Suggestion header malformed");
        edited.insert(end, " reject");
    }
    if (caseId == "V009-image-upload-lifecycle") {
        write(path.parent_path() / "pic.png", png(200, 100));
        write(path.parent_path() / "not.png", "plain text");
        auto invalid = edited;
        replace(invalid, "Hello world", "Hello world ![](not.png) ![](missing.png)");
        write(path, invalid);
        const auto refused = engine::fileImport(engine::ImportInput{.path = path.string()}, services);
        imageObservation["preflight"] = Json::object_t{{"result", detail::file_write_result_json(refused)},
            {"uploads", uploadReceipts.size()}, {"batches", mutations.size()}};
        replace(edited, "Hello world", "Hello world ![](pic.png) ![](" +
            file_uri(path.parent_path() / "pic.png") + ") ![](http://synthetic.invalid/image.png)");
    }
    std::vector<std::string> w4Allow;
    if (caseId == "W4-comment-allow-and-actions") {
        replace(edited, "My open comment", "My updated comment");
        const auto otherStart = edited.find("::comment{id=c-other");
        if (otherStart == std::string::npos) throw std::runtime_error("c-other block not exported");
        const auto otherEnd = edited.find('}', otherStart);
        if (otherEnd == std::string::npos) throw std::runtime_error("c-other header malformed");
        edited.insert(otherEnd, " resolved");
        replace(edited, "My reply", "My updated reply");
        edited += "\n::comment\nNew comment\n";
        write(path, edited);
        observation["refusedResult"] = detail::file_write_result_json(
            engine::fileImport(engine::ImportInput{.path = path.string()}, services));
        w4Allow = {"commentChanges"};
    }
    if (caseId == "W4-comment-ownership-and-readonly") {
        Json::array_t refusals;
        for (const auto& [label, apply] : std::vector<std::pair<std::string, std::function<void(std::string&)>>>{
            {"edit-other", [&](std::string& s) { replace(s, "Ada comment", "Tampered Ada"); }},
            {"delete-other", [&](std::string& s) {
                const auto p = s.find("::comment{id=c-other");
                const auto e = s.find('}', p);
                s.insert(e, " delete");
            }},
            {"mutate-quote", [&](std::string& s) { replace(s, "quote=Hello", "quote=Tampered"); }},
        }) {
            auto bad = edited;
            apply(bad);
            write(path, bad);
            refusals.push_back(Json::object_t{
                {"kind", label},
                {"result", detail::file_write_result_json(
                    engine::fileImport(engine::ImportInput{.path = path.string(), .allow = {"commentChanges"}}, services))}
            });
        }
        observation["ownershipRefusals"] = std::move(refusals);
        w4Allow = {"commentChanges"};
    }
    if (caseId == "W4-comment-resolved-reopen") {
        const auto resStart = edited.find("::comment{id=c-resolved");
        if (resStart == std::string::npos) throw std::runtime_error("c-resolved block not exported");
        const auto resEnd = edited.find('}', resStart);
        auto header = edited.substr(resStart, resEnd - resStart);
        replace(header, " resolved", "");
        edited.replace(resStart, resEnd - resStart, header);
        w4Allow = {"commentChanges"};
    }
    if (caseId == "W4-comment-partial-unknown-outcome") {
        replace(edited, "My open comment", "First updated");
        edited += "\n::comment\nSecond new comment\n";
        w4Allow = {"commentChanges"};
    }
    const auto sourceReads = [&] {
        return std::ranges::count_if(calls, [](const Json& call) {
            return call["method"].get<std::string>() == "getDocument" &&
                call["args"][0].get<std::string>() == "synthetic-source";
        });
    };
    if (caseId == "V010-cross-doc-image-copy") {
        replace(edited, "Hello world", "Hello world :image{src=\"gdoc:synthetic-source/" + sourceToken + "\"}");
        imageObservation["sourceReadsBeforeImport"] = sourceReads();
    }
    write(path, edited);
    if (caseId == "V003-stale") externalEdit("synthetic-doc");
    engine::ImportInput importInput{.path = path.string()};
    if (isW1 && caseId != "W1-create-invalid-syntax") importInput.newDocStyleBaseline = "generic";
    if (!w4Allow.empty()) importInput.allow = std::move(w4Allow);
    const auto result = engine::fileImport(importInput, services);
    observation["result"] = detail::file_write_result_json(result);
    const auto imageInsertions = [&] {
        std::size_t count = 0;
        for (const auto& mutation : mutations) {
            if (!mutation.contains("requests")) continue;
            for (const auto& request : mutation["requests"].get<Json::array_t>()) {
                if (request.contains("insertInlineImage")) ++count;
            }
        }
        return count;
    };
    const bool imageCase = caseId == "V009-image-upload-lifecycle" || caseId == "V010-cross-doc-image-copy";
    if (caseId == "V010-cross-doc-image-copy") imageObservation["sourceReadsAfterImport"] = sourceReads();
    if (imageCase) {
        imageObservation["insertionsBeforeReplay"] = imageInsertions();
        imageObservation["providerIdsBeforeReplay"] = provider_ids(services.getDocument("synthetic-doc"));
    }
    const auto countBatches = [&] {
        return std::ranges::count_if(mutations, [](const auto& m) {
            return m.contains("kind") && m["kind"].template get<std::string>() == "batchUpdate";
        });
    };
    const auto batchesBeforeReplay = countBatches();
    if ((caseId == "V004-landed-lost-response-replay" || imageCase) && fault != "known-upload-error") {
        observation["replay"] = detail::file_write_result_json(engine::fileImport(engine::ImportInput{.path = path.string()}, services));
    }
    if (caseId == "W1-create-success-replay" && result.exportPath) {
        observation["replay"] = detail::file_write_result_json(
            engine::fileImport(engine::ImportInput{.path = *result.exportPath}, services));
    }
    if (caseId == "W1-create-rollback-retry" && fault != "ack-delete-without-mutation") {
        write(path, edited);
        observation["replay"] = detail::file_write_result_json(
            engine::fileImport(engine::ImportInput{.path = path.string(), .newDocStyleBaseline = "generic"}, services));
    }
    Json w3Subtree = nullptr;
    std::optional<std::string> finalExportPath = result.exportPath;
    if (caseId == "W3-reparent-order-noop" && result.exportPath) {
        observation["replay"] = detail::file_write_result_json(
            engine::fileImport(engine::ImportInput{.path = *result.exportPath}, services));
    }
    if (caseId == "W3-delete-subtree-and-all" && result.exportPath) {
        auto subBytes = read(*result.exportPath);
        const auto subFile = lens::documentFileParse(subBytes);
        Json::array_t subTabs;
        for (const auto& tab : subFile.tabs) {
            subTabs.push_back(Json::object_t{
                {"id", tab.id},
                {"title", tab.title},
                {"parent", tab.parent ? Json(*tab.parent) : Json(nullptr)},
                {"markdown", tab.markdown}
            });
        }
        w3Subtree = Json::object_t{
            {"status", observation["result"]["status"]},
            {"deletedTabs", observation["result"].contains("deletedTabs") ? observation["result"]["deletedTabs"] : Json(Json::array_t{})},
            {"tabs", std::move(subTabs)}
        };
        mark_tab_delete(subBytes, "t.b");
        mark_tab_delete(subBytes, "t.c");
        write(*result.exportPath, subBytes);
        const auto replayResult = engine::fileImport(engine::ImportInput{.path = *result.exportPath}, services);
        observation["replay"] = detail::file_write_result_json(replayResult);
        if (replayResult.exportPath) finalExportPath = replayResult.exportPath;
    }
    const auto batchesAfterReplay = countBatches();
    if (imageCase) {
        imageObservation["insertionsAfterReplay"] = imageInsertions();
        imageObservation["providerIdsAfterReplay"] = provider_ids(services.getDocument("synthetic-doc"));
        Json::array_t imageErrors;
        if (caseId == "V010-cross-doc-image-copy" && result.exportPath) {
            const auto fresh = read(*result.exportPath);
            for (const std::string source : {"gdoc:synthetic-source/unknown-image", "gdoc:missing-source/unknown-image"}) {
                const auto beforeCount = mutations.size();
                auto bad = fresh;
                replace(bad, "Hello world", "Hello world :image{src=\"" + source + "\"}");
                write(path, bad);
                const auto refused = engine::fileImport(engine::ImportInput{.path = path.string()}, services);
                imageErrors.push_back(Json::object_t{{"source", source},
                    {"result", detail::file_write_result_json(refused)}, {"mutations", mutations.size() - beforeCount}});
            }
            write(path, fresh);
        }
        Json::array_t temporary;
        for (const auto& receipt : uploadReceipts) {
            bool exists = false;
            const auto id = receipt["documentId"].get<std::string>();
            try { (void)backend.get_doc(id); exists = true; }
            catch (const std::runtime_error&) {}
            temporary.push_back(Json::object_t{{"id", id}, {"actualDeleteMethodCompleted", completedDeletes.contains(id)},
                {"backendExists", exists}});
        }
        imageObservation["imageErrors"] = std::move(imageErrors);
        imageObservation["fetched"] = std::move(fetched);
        imageObservation["uploadReceipts"] = std::move(uploadReceipts);
        imageObservation["deletions"] = std::move(deletions);
        imageObservation["temporaryDocuments"] = std::move(temporary);
        imageObservation["deletionEvidence"] = "actual FakeGoogle deleted flag and delete callback completion";
        observation["images"] = std::move(imageObservation);
    }
    if (caseId.starts_with("W4-")) {
        observation["commentSends"] = commentSends;
        Json::array_t commentsJson;
        for (const auto& c : reviewComments) {
            Json::array_t repliesJson;
            for (const auto& r : c.replies) {
                repliesJson.push_back(Json::object_t{{"id", r.id}, {"content", r.content.value_or("")}});
            }
            commentsJson.push_back(Json::object_t{
                {"id", c.id}, {"content", c.content.value_or("")},
                {"resolved", c.resolved.value_or(false)}, {"replies", std::move(repliesJson)}});
        }
        observation["comments"] = std::move(commentsJson);
    }
    observation["schema"] = "engine-parity-observation-v1";
    observation["side"] = "cpp";
    observation["caseId"] = caseId;
    observation["seedDigest"] = engine::detail::sha256_hex(seedBytes);
    observation["faultCalls"] = faultCalls;
    observation["fault"] = fault;
    observation["initial"] = Json::object_t{{"version", initialFields["gdocsmith"]},
        {"markdown", initialBytes},
        {"tabs", initialFile ? tabs(*initialFile) : Json::array_t{}}, {"hasSuggestionBlock", initialBytes.find("::suggestion{") != std::string::npos},
        {"hasCommentBlock", initialBytes.find("::comment{id=c1") != std::string::npos},
        {"hasSyntheticReply", initialBytes.find("::reply{id=r1") != std::string::npos},
        {"hasInsertedSnippet", initialBytes.find(":change[Hello world]") != std::string::npos},
        {"hasCellStyleDeclaration", hasRedCellDeclaration &&
            initialBytes.find("| A :cell{") != std::string::npos}};
    const bool workingExists = fs::exists(path);
    observation["working"] = Json::object_t{{"exists", workingExists},
        {"path", path.string()},
        {"beforeDigest", engine::detail::sha256_hex(edited)},
        {"afterDigest", workingExists ? Json(engine::detail::sha256_hex(read(path))) : Json(nullptr)},
        {"unchanged", workingExists && read(path) == edited}};
    const bool outputExists = result.exportPath && fs::exists(*result.exportPath);
    Json output = Json::object_t{{"exists", outputExists},
        {"pathRole", !result.exportPath ? "absent" : fs::canonical(*result.exportPath) == fs::canonical(path) ? "same" : "distinct"},
        {"hasSuggestionBlock", outputExists && read(*result.exportPath).find("::suggestion{") != std::string::npos}};
    if (outputExists) {
        const auto origPath = fs::path(*result.exportPath).parent_path() / "export.orig.md";
        if (fs::exists(origPath)) {
            const auto origBytes = read(origPath);
            const auto origFile = lens::documentFileParse(origBytes);
            output["origMarkdown"] = origBytes;
            output["origTabs"] = tabs(origFile);
            if (origFile.frontmatter.revision) output["origRevision"] = *origFile.frontmatter.revision;
        }
        const auto file = lens::documentFileParse(read(*finalExportPath));
        output["markdown"] = read(*finalExportPath);
        if (file.frontmatter.revision) output["revision"] = *file.frontmatter.revision;
        output["title"] = file.frontmatter.title;
        output["tabs"] = tabs(file);
    } else output["markdown"] = nullptr;
    observation["exported"] = std::move(output);
    if (isW1) {
        Json::array_t temporary;
        for (const auto& id : createdIds) {
            bool exists = false;
            try { (void)backend.get_doc(id); exists = true; }
            catch (const std::runtime_error&) {}
            temporary.push_back(Json::object_t{{"id", id},
                {"actualDeleteMethodCompleted", completedDeletes.contains(id)},
                {"backendExists", exists}});
        }
        Json::array_t createdJson;
        for (const auto& id : createdIds) createdJson.emplace_back(id);
        observation["w1"] = Json::object_t{{"createdIds", std::move(createdJson)},
            {"deletions", deletions}, {"temporaryDocuments", std::move(temporary)}};
        std::optional<std::string> targetId;
        if (result.doc && !completedDeletes.contains(*result.doc)) {
            targetId = *result.doc;
        } else if (!createdIds.empty() && !completedDeletes.contains(createdIds.back())) {
            try {
                (void)backend.get_doc(createdIds.back());
                targetId = createdIds.back();
            } catch (const std::runtime_error&) {}
        }
        if (targetId) {
            observation["remote"] = services.getDocument(*targetId);
            auto stored = backend.get_doc(*targetId).json;
            stored["revisionId"] = backend.revision_id(backend.get_doc(*targetId));
            observation["remoteStored"] = std::move(stored);
        } else {
            observation["remote"] = nullptr;
            observation["remoteStored"] = nullptr;
        }
    } else {
        observation["remote"] = services.getDocument("synthetic-doc");
        auto stored = backend.get_doc("synthetic-doc").json;
        stored["revisionId"] = backend.revision_id(backend.get_doc("synthetic-doc"));
        observation["remoteStored"] = std::move(stored);
    }
    observation["mutations"] = std::move(mutations);
    observation["callLog"] = std::move(calls);
    if (caseId.starts_with("W3-")) {
        Json w3Obj = Json::object_t{
            {"batchesBeforeReplay", batchesBeforeReplay},
            {"batchesAfterReplay", batchesAfterReplay}
        };
        if (!w3Subtree.is_null()) w3Obj["subtree"] = std::move(w3Subtree);
        observation["w3"] = std::move(w3Obj);
    }
    return observation;
}
}

int main() {
    umask(0077);
    try {
        const std::string bytes{std::istreambuf_iterator<char>(std::cin), {}};
        const auto input = slimr::parse_json<Json>(bytes);
        if (!input) throw std::runtime_error("Invalid parity JSON");
        std::cout << slimr::to_json(run(*input));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Parity adapter failed: " << error.what() << '\n';
        return 2;
    }
}
