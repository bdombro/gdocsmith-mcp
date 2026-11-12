#pragma once

#include "types.hpp"
#include "document_emulator.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gdocsmith::emulator {

struct FakeGoogleCall {
    std::string method;
    std::vector<slimr::Json> args;
};

struct DriveComment {
    std::string id;
    std::string content;
    std::string author_display_name = "Me";
    bool author_me = true;
};

struct FakeDoc {
    bool deleted = false;
    slimr::Json json;
    int revision = 1;
    bool trashed = false;
};

inline slimr::Json blank_doc_template(const std::string& title = "Untitled document",
                                      const std::string& doc_id = "EXAMPLE_DOC_ID") {
    slimr::Json tab0 = blank_tab_template();
    slimr::Json doc = slimr::Json::object_t{
        {"documentId", doc_id},
        {"revisionId", "rev-1"},
        {"suggestionsViewMode", "SUGGESTIONS_INLINE"},
        {"title", title},
        {"tabs", slimr::Json::array_t{
            slimr::Json::object_t{
                {"documentTab", tab0.contains("documentTab") ? std::move(tab0["documentTab"]) : std::move(tab0)},
                {"tabProperties", slimr::Json::object_t{
                    {"index", static_cast<int64_t>(0)},
                    {"tabId", "t.0"},
                    {"title", "Tab 1"}
                }}
            }
        }}
    };
    return doc;
}

class FakeGoogle {
public:
    std::vector<FakeGoogleCall> call_log;
    std::function<void(const std::string&)> before_batch_update;

    FakeGoogle() = default;

    void seed_document(const std::string& document_id, const slimr::Json& json, int revision = 1) {
        FakeDoc doc;
        doc.json = json;
        doc.revision = revision;
        docs_[document_id] = std::move(doc);
    }

    std::string revision_id(const FakeDoc& doc) const {
        return "rev-" + std::to_string(doc.revision);
    }

    std::string batch_update(const std::string& document_id,
                             const std::vector<slimr::Json>& requests,
                             const std::optional<std::string>& required_revision_id = std::nullopt) {
        slimr::Json::array_t reqs_arr;
        for (const auto& r : requests) reqs_arr.push_back(r);
        slimr::Json opts_json = slimr::Json::object_t{};
        if (required_revision_id) opts_json["requiredRevisionId"] = *required_revision_id;

        call_log.push_back(FakeGoogleCall{
            .method = "batchUpdate",
            .args = {document_id, slimr::Json{std::move(reqs_arr)}, std::move(opts_json)}
        });

        if (before_batch_update) {
            before_batch_update(document_id);
        }

        auto it = docs_.find(document_id);
        if (it == docs_.end() || it->second.deleted) {
            throw std::runtime_error("File not found: " + document_id);
        }
        auto& doc = it->second;

        if (required_revision_id && *required_revision_id != revision_id(doc)) {
            throw std::runtime_error("The required revision ID '" + *required_revision_id + "' does not match the latest revision.");
        }

        EmulateResult emulated;
        try {
            emulated = requests_emulate(doc.json, requests);
        } catch (const EmulatorError& err) {
            throw std::runtime_error("Invalid requests[" + std::to_string(err.request_index) + "]: " + err.what());
        }

        doc.json = std::move(emulated.json);
        doc.revision += 1;

        slimr::Json::array_t replies_arr;
        for (auto& rep : emulated.replies) replies_arr.push_back(std::move(rep));

        slimr::Json response = slimr::Json::object_t{
            {"documentId", document_id},
            {"replies", std::move(replies_arr)},
            {"writeControl", slimr::Json::object_t{
                {"requiredRevisionId", revision_id(doc)}
            }}
        };
        return slimr::write_json(response);
    }

    std::string create_document(const std::string& title = "Untitled document") {
        std::string doc_id = "doc_" + std::to_string(++next_doc_num_);
        FakeDoc doc;
        doc.json = blank_doc_template(title, doc_id);
        doc.revision = 1;
        docs_[doc_id] = std::move(doc);
        return doc_id;
    }

    std::string copy_file(const std::string& file_id, const std::string& title) {
        auto it = docs_.find(file_id);
        if (it == docs_.end() || it->second.deleted) {
            throw std::runtime_error("File not found: " + file_id);
        }
        std::string new_id = "doc_" + std::to_string(++next_doc_num_);
        FakeDoc copy;
        copy.json = it->second.json;
        copy.json["documentId"] = new_id;
        copy.json["title"] = title;
        copy.revision = 1;
        docs_[new_id] = std::move(copy);
        return new_id;
    }

    void external_edit(const std::string& doc_id, std::function<void(slimr::Json&)> edit_fn) {
        auto it = docs_.find(doc_id);
        if (it == docs_.end() || it->second.deleted) {
            throw std::runtime_error("File not found: " + doc_id);
        }
        edit_fn(it->second.json);
        it->second.revision += 1;
    }

    const FakeDoc& get_doc(const std::string& document_id) const {
        auto it = docs_.find(document_id);
        if (it == docs_.end() || it->second.deleted) {
            throw std::runtime_error("File not found: " + document_id);
        }
        return it->second;
    }

    FakeDoc& get_doc(const std::string& document_id) {
        auto it = docs_.find(document_id);
        if (it == docs_.end() || it->second.deleted) {
            throw std::runtime_error("File not found: " + document_id);
        }
        return it->second;
    }

    DriveComment comment_create(const std::string& file_id, const std::string& text) {
        call_log.push_back(FakeGoogleCall{
            .method = "commentCreate",
            .args = {file_id, text}
        });
        DriveComment c;
        c.id = "fc" + std::to_string(++next_comment_num_);
        c.content = text;
        comments_[file_id].push_back(c);
        return c;
    }

    void comment_delete(const std::string& file_id, const std::string& thread_id) {
        call_log.push_back(FakeGoogleCall{
            .method = "commentDelete",
            .args = {file_id, thread_id}
        });
        auto& list = comments_[file_id];
        list.erase(std::remove_if(list.begin(), list.end(), [&](const DriveComment& c) {
            return c.id == thread_id;
        }), list.end());
    }

    slimr::Json comment_reply(const std::string& file_id, const std::string& thread_id, const slimr::Json& body) {
        call_log.push_back(FakeGoogleCall{
            .method = "commentReply",
            .args = {file_id, thread_id, body}
        });
        std::string reply_id = "fr" + std::to_string(++next_comment_num_);
        slimr::Json reply = body.is_object() ? body : slimr::Json::object_t{};
        reply["id"] = reply_id;
        reply["author"] = slimr::Json::object_t{{"displayName", "Me"}, {"me", true}};
        return reply;
    }

private:
    std::map<std::string, FakeDoc> docs_;
    std::map<std::string, std::vector<DriveComment>> comments_;
    int next_doc_num_ = 0;
    int next_comment_num_ = 0;
};

} // namespace gdocsmith::emulator
