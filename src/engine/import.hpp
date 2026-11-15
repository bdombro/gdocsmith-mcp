#pragma once

#include "api.hpp"
#include "engine/artifacts.hpp"
#include "engine/export.hpp"
#include "engine/load.hpp"
#include "engine/named_styles.hpp"
#include "engine/session.hpp"
#include "engine/transaction.hpp"
#include "lens/document.hpp"
#include "model/document.hpp"
#include "model/errors.hpp"
#include "model/types.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <map>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gdocsmith::engine {

enum class NewDocStyleBaseline {
    Account,
    Generic
};

inline std::optional<NewDocStyleBaseline> parseNewDocStyleBaseline(std::string_view s) {
    if (s == "account") return NewDocStyleBaseline::Account;
    if (s == "generic") return NewDocStyleBaseline::Generic;
    return std::nullopt;
}

struct ImportInput {
    std::string path;
    std::optional<std::string> newDocStyleBaseline;
    std::vector<std::string> allow;

    bool operator==(const ImportInput&) const = default;
};

struct CommentAction {
    std::string kind; // "create" | "delete" | "reopen" | "resolve" | "edit" | "reply" | "replyDelete" | "replyEdit"
    std::optional<std::string> id;
    std::optional<std::string> replyId;
    std::optional<std::string> text;
    int line{0};

    bool operator==(const CommentAction&) const = default;
};

/**
 * A document can't have zero tabs: when every tab line is marked delete, the original tab (`t.0`,
 * which Google can't delete without breaking later tab renames and reorders) or else the first line
 * is kept instead, cleared and renamed "Tab 1", and the substitution is noted.
 */
inline void lastTabKeep(lens::DocumentFile& file, std::vector<std::string>& notes) {
    if (file.tabs.empty()) return;
    bool allDelete = true;
    for (const auto& tab : file.tabs) {
        if (!tab.isDelete) {
            allDelete = false;
            break;
        }
    }
    if (!allDelete) return;

    size_t keepIdx = 0;
    for (size_t i = 0; i < file.tabs.size(); i++) {
        if (file.tabs[i].id == "t.0") {
            keepIdx = i;
            break;
        }
    }

    auto& keep = file.tabs[keepIdx];
    keep.isDelete = false;
    keep.markdown = "";
    keep.parent = std::nullopt;
    std::string from = keep.title;
    keep.title = "Tab 1";
    notes.push_back("A document must keep one tab, so \"" + from + "\" was cleared and renamed \"Tab 1\" instead of deleted.");
}

/**
 * Checks tab titles once on the final state, so swaps and reusing a deleted tab's title do not depend
 * on line order: kept lines' titles must be unique (trimmed, case-insensitive), except that existing
 * tabs already sharing a title may keep it unchanged.
 */
inline void tabTitlesValidate(
    const lens::DocumentFile& file,
    const model::DocModel& current,
    std::vector<model::FileProblem>& problems
) {
    auto to_lower_trim = [](std::string_view s) -> std::string {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        std::string res;
        for (size_t i = first; i <= last; i++) {
            res += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
        }
        return res;
    };

    std::map<std::string, std::vector<lens::DocumentFileTab>> groups;
    for (const auto& section : file.tabs) {
        if (section.isDelete) continue;
        groups[to_lower_trim(section.title)].push_back(section);
    }

    auto is_kept = [&](const lens::DocumentFileTab& section) {
        for (const auto& tab : current.tabs) {
            if (tab.tabId == section.id && !section.id.empty()) {
                return tab.title == section.title;
            }
        }
        return false;
    };

    for (const auto& [_, group] : groups) {
        if (group.size() > 1) {
            bool all_kept = true;
            for (const auto& sec : group) {
                if (!is_kept(sec)) {
                    all_kept = false;
                    break;
                }
            }
            if (!all_kept) {
                problems.push_back(model::FileProblem{
                    .code = std::string(model::error_codes::TAB_TITLE_TAKEN),
                    .line = group[1].line,
                    .message = std::to_string(group.size()) + " tabs would be titled \"" + group[0].title + "\"; tab titles must be unique (ignoring case)",
                    .tab = group[1].title,
                });
            }
        }
    }
}

/**
 * Checks an existing document's tab lines before any edit: every existing tab has a line, parents
 * are existing kept tabs whose lines come first (which also rules out cycles), a kept tab's final
 * parent is never deleted.
 */
inline std::vector<DeletedTab> tabLinesValidate(
    const lens::DocumentFile& file,
    const model::DocModel& current,
    std::vector<model::FileProblem>& problems
) {
    std::map<std::string, std::pair<size_t, lens::DocumentFileTab>> lines;
    for (size_t i = 0; i < file.tabs.size(); i++) {
        if (!file.tabs[i].id.empty() && lines.contains(file.tabs[i].id)) {
            problems.push_back(model::FileProblem{
                .code = "unsupportedSyntax", .line = file.tabs[i].line,
                .message = "tab ID \"" + file.tabs[i].id + "\" appears more than once",
                .tab = file.tabs[i].title,
            });
        }
        lines[file.tabs[i].id] = {i, file.tabs[i]};
    }

    for (const auto& section : file.tabs) {
        if (!section.id.empty()) {
            bool found = false;
            for (const auto& tab : current.tabs) {
                if (tab.tabId == section.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                problems.push_back(model::FileProblem{
                    .code = std::string(model::error_codes::TAB_NOT_FOUND),
                    .line = section.line,
                    .message = "tab ID \"" + section.id + "\" is not in this document; titles are not IDs",
                    .tab = section.title,
                });
            }
        }
    }

    for (const auto& tab : current.tabs) {
        if (!lines.contains(tab.tabId)) {
            problems.push_back(model::FileProblem{
                .code = std::string(model::error_codes::TAB_REQUIRED),
                .line = std::nullopt,
                .message = "tab " + tab.tabId + " \"" + tab.title + "\" is missing from the file; add delete to its tab line to remove it",
                .tab = std::nullopt,
            });
        }
    }

    auto get_title = [&](const std::string& id) -> std::string {
        for (const auto& t : current.tabs) {
            if (t.tabId == id) return t.title;
        }
        return id;
    };

    std::vector<lens::DocumentFileTab> deletedSections;
    for (size_t index = 0; index < file.tabs.size(); index++) {
        const auto& section = file.tabs[index];
        if (section.isDelete) {
            deletedSections.push_back(section);
            continue;
        }
        if (!section.parent.has_value()) continue;

        auto it = lines.find(*section.parent);
        if (it == lines.end()) {
            problems.push_back(model::FileProblem{
                .code = std::string(model::error_codes::TAB_NOT_FOUND),
                .line = section.line,
                .message = "parent \"" + *section.parent + "\" of tab \"" + section.title + "\" is not a tab ID in this document; a tab this import creates can be a parent only after export gives it an ID",
                .tab = section.title,
            });
        } else if (it->second.second.isDelete) {
            problems.push_back(model::FileProblem{
                .code = std::string(model::error_codes::INVALID_PLACEMENT),
                .line = section.line,
                .message = "tab \"" + section.title + "\" is under \"" + get_title(*section.parent) + "\", which is marked delete; child tabs are never deleted implicitly, so delete it too or give it another parent",
                .tab = section.title,
            });
        } else if (it->second.first >= index) {
            problems.push_back(model::FileProblem{
                .code = std::string(model::error_codes::INVALID_PLACEMENT),
                .line = section.line,
                .message = "tab \"" + section.title + "\" must come after its parent \"" + get_title(*section.parent) + "\" in the file; a tab cannot be its own ancestor",
                .tab = section.title,
            });
        }
    }

    std::vector<DeletedTab> result;
    for (const auto& sec : deletedSections) {
        result.push_back(DeletedTab{
            .id = sec.id,
            .title = get_title(sec.id),
        });
    }
    return result;
}

/**
 * Index of the first line in new or edited comment text, not already in its exported text, that is
 * document block syntax.
 */
inline std::optional<int> commentBodyLine(std::string_view text, std::string_view was = "") {
    std::unordered_set<std::string> old_lines;
    std::istringstream was_ss{std::string(was)};
    std::string l;
    while (std::getline(was_ss, l)) {
        old_lines.insert(l);
    }

    std::istringstream txt_ss{std::string(text)};
    static const std::regex block_re(R"(^(?:#{1,6}[ \t]|\|.*\|[ \t]*$|```|~~~|!\[))");
    int line_idx = 0;
    while (std::getline(txt_ss, l)) {
        if (!old_lines.contains(l)) {
            if (std::regex_search(l, block_re)) {
                return line_idx;
            }
        }
        line_idx++;
    }
    return std::nullopt;
}

/**
 * Checks the comment blocks against the document as exported and lists the actions they ask for.
 */
inline std::vector<CommentAction> commentActionsPlan(
    const lens::DocumentFile& file,
    const std::optional<lens::DocumentFile>& exported
) {
    std::vector<model::FileProblem> problems;
    std::vector<CommentAction> actions;

    std::unordered_map<std::string, lens::CommentBlock> baseline;
    if (exported.has_value()) {
        for (const auto& block : exported->comments) {
            baseline[block.id] = block;
        }
    }

    std::unordered_set<std::string> seen;

    for (const auto& block : file.comments) {
        auto add_prob = [&](const std::string& msg, int line) {
            problems.push_back(model::FileProblem{
                .code = std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                .line = line,
                .message = msg,
                .tab = std::nullopt,
            });
        };

        auto bodyless = [&](const std::string& txt, int line, const std::string& was = "") -> bool {
            auto found = commentBodyLine(txt, was);
            if (found.has_value()) {
                add_prob("comment text is plain text, not document content; put tab content before the comment blocks",
                         line + 1 + *found);
                return false;
            }
            return true;
        };

        if (block.id.empty()) {
            if (block.author.has_value() || block.mine || block.orphaned || block.ambiguous ||
                block.quote.has_value() || block.resolved || block.isDelete) {
                add_prob("a new comment block takes only its text: ::comment then the text", block.line);
            } else if (!block.replies.empty()) {
                add_prob("reply to a new comment after a later export gives it an id", block.line);
            } else if (block.text.empty()) {
                add_prob("a new comment needs text", block.line);
            } else if (bodyless(block.text, block.line)) {
                actions.push_back(CommentAction{
                    .kind = "create",
                    .text = block.text,
                    .line = block.line,
                });
            }
            continue;
        }

        if (seen.contains(block.id)) {
            add_prob("comment " + block.id + " appears more than once", block.line);
            continue;
        }
        seen.insert(block.id);

        auto it = baseline.find(block.id);
        if (it == baseline.end()) {
            add_prob("comment " + block.id + " is not in this document; export again for its current comments", block.line);
            continue;
        }

        const auto& base = it->second;

        std::vector<std::string> changed;
        if (block.author != base.author) changed.push_back("author");
        if (block.mine != base.mine) changed.push_back("mine");
        if (block.orphaned != base.orphaned) changed.push_back("orphaned");
        if (block.ambiguous != base.ambiguous) changed.push_back("ambiguous");
        if (block.quote != base.quote) changed.push_back("quote");

        if (!changed.empty()) {
            std::string field_str;
            for (size_t c_i = 0; c_i < changed.size(); c_i++) {
                if (c_i > 0) field_str += ", ";
                field_str += changed[c_i];
            }
            add_prob("comment " + block.id + "'s " + field_str + (changed.size() > 1 ? " are" : " is") + " read-only", block.line);
        }

        std::vector<CommentAction> replyActions;
        std::unordered_set<std::string> kept_replies;
        for (const auto& reply : block.replies) {
            if (!reply.id.has_value() || reply.id->empty()) {
                if (reply.author.has_value() || reply.mine || reply.isDelete) {
                    add_prob("a new reply is a bare ::reply line then the text", reply.line);
                } else if (reply.text.empty()) {
                    add_prob("a new reply needs text", reply.line);
                } else {
                    bodyless(reply.text, reply.line);
                }
                continue;
            }

            const lens::CommentReply* was_rep = nullptr;
            for (const auto& r : base.replies) {
                if (r.id == reply.id) {
                    was_rep = &r;
                    break;
                }
            }

            if (!was_rep || kept_replies.contains(*reply.id)) {
                add_prob("reply " + *reply.id + " isn't in comment " + block.id + "'s thread, or appears twice", reply.line);
                continue;
            }
            kept_replies.insert(*reply.id);

            if (reply.author != was_rep->author || reply.mine != was_rep->mine) {
                add_prob("reply " + *reply.id + "'s author and mine are read-only", reply.line);
            } else if (reply.isDelete || reply.text != was_rep->text) {
                if (!was_rep->mine) {
                    add_prob("only your own replies (mine) can be edited or deleted; add a new ::reply instead", reply.line);
                } else if (reply.isDelete) {
                    replyActions.push_back(CommentAction{
                        .kind = "replyDelete",
                        .id = block.id,
                        .replyId = *reply.id,
                    });
                } else if (reply.text.empty()) {
                    add_prob("reply " + *reply.id + " needs text; add delete to remove it", reply.line);
                } else if (bodyless(reply.text, reply.line, was_rep->text)) {
                    replyActions.push_back(CommentAction{
                        .kind = "replyEdit",
                        .id = block.id,
                        .replyId = *reply.id,
                        .text = reply.text,
                    });
                }
            }
        }

        for (const auto& rep : base.replies) {
            if (rep.id.has_value() && !kept_replies.contains(*rep.id)) {
                add_prob("comment " + block.id + "'s existing replies must stay in its block; add delete to remove your own", block.line);
            }
        }

        if (block.isDelete) {
            if (!block.mine) {
                add_prob("only your own comments (mine) can be deleted; resolve comment " + block.id + " instead", block.line);
            } else {
                actions.push_back(CommentAction{
                    .kind = "delete",
                    .id = block.id,
                });
            }
            continue;
        }

        if (block.text != base.text) {
            if (!block.mine) {
                add_prob("only your own comments (mine) can be edited; reply to comment " + block.id + " instead", block.line);
            } else if (block.text.empty()) {
                add_prob("comment " + block.id + " needs text; add delete to remove it", block.line);
            } else if (bodyless(block.text, block.line, base.text)) {
                actions.push_back(CommentAction{
                    .kind = "edit",
                    .id = block.id,
                    .text = block.text,
                });
            }
        }

        for (auto& ra : replyActions) {
            actions.push_back(std::move(ra));
        }

        for (const auto& reply : block.replies) {
            if ((!reply.id.has_value() || reply.id->empty()) && !reply.text.empty()) {
                actions.push_back(CommentAction{
                    .kind = "reply",
                    .id = block.id,
                    .text = reply.text,
                });
            }
        }

        if (block.resolved != base.resolved) {
            actions.push_back(CommentAction{
                .kind = block.resolved ? "resolve" : "reopen",
                .id = block.id,
            });
        }
    }

    for (const auto& [id, base] : baseline) {
        if (!base.resolved && !seen.contains(id)) {
            problems.push_back(model::FileProblem{
                .code = std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                .line = std::nullopt,
                .message = "comment " + id + "'s block is missing; blocks can't be removed: add resolved to resolve it, or delete to delete your own",
                .tab = std::nullopt,
            });
        }
    }

    model::problemsThrow(problems);
    return actions;
}

/**
 * Checks the suggestion blocks against the ones export writes for the same revision.
 */
inline void suggestionBlocksValidate(
    const lens::DocumentFile& file,
    const std::optional<lens::DocumentFile>& exported
) {
    bool has_file_sug = !file.suggestions.empty();
    bool has_exp_sug = exported.has_value() && !exported->suggestions.empty();

    if (!has_file_sug && !has_exp_sug) return;

    if (!has_exp_sug && has_file_sug) {
        model::problemsThrow({model::FileProblem{
            .code = std::string(model::error_codes::SUGGESTION_CONFLICT),
            .line = file.suggestions[0].line,
            .message = "this document has no pending suggestions; remove the suggestion blocks",
            .tab = std::nullopt,
        }});
    }

    if (has_exp_sug && !has_file_sug) {
        model::problemsThrow({model::FileProblem{
            .code = std::string(model::error_codes::SUGGESTION_CONFLICT),
            .line = std::nullopt,
            .message = "the suggestion blocks are missing; keep every block as exported (add reject to a header to reject one)",
            .tab = std::nullopt,
        }});
    }

    const auto& f_sugs = file.suggestions;
    const auto& e_sugs = exported->suggestions;

    if (f_sugs.size() != e_sugs.size()) {
        model::problemsThrow({model::FileProblem{
            .code = std::string(model::error_codes::SUGGESTION_CONFLICT),
            .line = f_sugs.empty() ? 1 : f_sugs[0].line,
            .message = "a suggestion block was removed; keep every block (add reject to its header to reject one)",
            .tab = std::nullopt,
        }});
    }

    for (size_t i = 0; i < f_sugs.size(); i++) {
        const auto& fs = f_sugs[i];
        const auto& es = e_sugs[i];
        if (fs.id != es.id || fs.kind != es.kind || fs.segment != es.segment) {
            model::problemsThrow({model::FileProblem{
                .code = std::string(model::error_codes::SUGGESTION_CONFLICT),
                .line = fs.line,
                .message = "a suggestion block's header changed; only reject can be added to it",
                .tab = std::nullopt,
            }});
        }
        if (fs.before != es.before || fs.after != es.after || fs.notice != es.notice) {
            model::problemsThrow({model::FileProblem{
                .code = std::string(model::error_codes::SUGGESTION_CONFLICT),
                .line = fs.line,
                .message = "edit the tab body, not a suggestion block; to reject a suggestion, add reject to its header",
                .tab = std::nullopt,
            }});
        }
    }
}

inline std::string exportSnapshotPath(const ExportWritten& exported, std::vector<std::string>& warnings) {
    if (detail::file_text_read(exported.exportPath) == exported.markdown) return exported.exportPath;
    const auto directory = exported.folderPath.empty()
        ? std::filesystem::path(exported.exportPath).parent_path() : std::filesystem::path(exported.folderPath);
    const auto path = directory / ("export.remote." + detail::unique_tmp_suffix() + ".md");
    detail::atomic_write(path.string(), exported.markdown);
    warnings.push_back("The canonical working copy has local edits; exportPath is a separate current snapshot.");
    return path.string();
}

/** Replaces unchanged input only; a separately edited working copy is never returned as fresh. */
inline std::string exportPlace(
    const ExportWritten& exported,
    const FileInputSnapshot& input,
    bool inputChanged,
    std::vector<std::string>& warnings
) {
    if (!inputChanged) {
        if (!inputFileChanged(input)) return fileReplace(input.path, exported.markdown);
        if (detail::file_text_read(input.path) == exported.markdown) return input.path;
    }
    warnings.push_back("The input changed during import, so it was kept; exportPath is the fresh export.");
    return exportSnapshotPath(exported, warnings);
}

struct ImportServices : FileServices {
    std::function<slimr::Json(const std::string&)> getDocument;
    std::function<slimr::Json(const std::string&)> getFileMetadata;
    std::function<slimr::Json(const std::string&)> createDocument;
    std::function<void(const std::string&)> deleteFile;
    std::function<ImagesUploadResult(const std::string&, const std::vector<DocxImage>&)> imagesUpload;
    std::function<std::string(const std::string&)> fetchImage;
    std::function<slimr::Json(const std::string&, const std::vector<slimr::Json>&,
                              const std::optional<std::string>&)> batchUpdate;
    std::function<std::vector<DriveComment>(const std::string&)> commentsList;
    std::function<std::string(const std::string&)> textExport;
    std::function<slimr::Json(const std::string&, const CommentAction&)> commentSend;
    std::function<std::string(const std::string&, const std::string&)> rename;
};

namespace import_detail {

struct ImportImageUploads {
    const ImportServices& services;
    std::map<std::string, DocxImage> prepared;
    std::optional<std::map<std::string, std::string>> uploaded;
    std::optional<std::string> documentId;
    std::vector<std::string> notes;
    WriteTracker* tracker = nullptr;

    void prepare(const std::vector<std::string>& placeholders) {
        std::vector<model::FileProblem> problems;
        for (const auto& placeholder : placeholders) {
            if (prepared.contains(placeholder)) continue;
            const auto source = placeholder.substr(lens::IMAGE_UPLOAD_SCHEME.size());
            try {
                std::string bytes;
                if (source.starts_with("http://") || source.starts_with("HTTP://")) {
                    if (!services.fetchImage) throw std::runtime_error("this client can't read HTTP images");
                    bytes = services.fetchImage(source);
                } else {
                    const auto read = detail::file_text_read(source);
                    if (!read) throw std::runtime_error("file is missing or unreadable");
                    bytes = *read;
                }
                const auto info = image_info(bytes);
                std::string problem;
                if (!info || info->widthPx <= 0 || info->heightPx <= 0) problem = " isn't a PNG, JPEG or GIF";
                else if (bytes.size() > 50U * 1024U * 1024U) problem = " is over Google's 50 MB limit";
                else if (static_cast<int64_t>(info->widthPx) * info->heightPx > 25'000'000) {
                    problem = " is over Google's 25-megapixel limit";
                }
                if (!problem.empty()) {
                    problems.push_back({.code = "unsupportedSyntax", .message = "image " + source + problem});
                    continue;
                }
                if (std::max(info->widthPx, info->heightPx) > 2048) {
                    notes.push_back("Google stores image " + source + " downscaled to at most 2048 px per side.");
                }
                prepared.emplace(placeholder, DocxImage{.bytes = std::move(bytes), .widthPx = info->widthPx,
                    .heightPx = info->heightPx, .type = info->type});
            } catch (const std::exception& error) {
                problems.push_back({.code = "unsupportedSyntax",
                    .message = "image " + source + " can't be read: " + error.what()});
            }
        }
        model::problemsThrow(problems);
        if (!placeholders.empty() && (!services.imagesUpload || !services.deleteFile)) {
            throw model::CoreError("unsupportedSyntax", "this client can't upload and clean up images");
        }
    }

    std::map<std::string, std::string> upload(const std::vector<std::string>& placeholders) {
        if (uploaded) {
            for (const auto& placeholder : placeholders) if (!uploaded->contains(placeholder)) {
                throw model::CoreError("unsupportedSyntax", "An image upload changed after planning; export again.");
            }
            return *uploaded;
        }
        prepare(placeholders);
        std::vector<DocxImage> images;
        for (const auto& placeholder : placeholders) images.push_back(prepared.at(placeholder));
        ImagesUploadResult result;
        const auto attempt = tracker ? tracker->writes.size() : 0;
        if (tracker) tracker->writes.push_back(WriteAttempt{.action = "imagesUpload", .outcome = "pending", .requestCount = 1});
        try {
            result = services.imagesUpload("[gdocsmith upload]", images);
        } catch (const ImagesUploadError& error) {
            if (!error.documentId.empty()) documentId = error.documentId;
            if (tracker) tracker->writes[attempt].outcome = "unknown";
            throw;
        } catch (...) {
            if (tracker) tracker->writes[attempt].outcome = "unknown";
            throw;
        }
        if (!result.documentId.empty()) documentId = result.documentId;
        if (!documentId || result.contentUris.size() != placeholders.size() ||
            std::any_of(result.contentUris.begin(), result.contentUris.end(),
                [](const auto& uri) { return !uri.starts_with("https://"); })) {
            if (tracker) tracker->writes[attempt].outcome = "unknown";
            throw model::CoreError("operationFailed", "Google didn't convert every uploaded image");
        }
        if (tracker) tracker->writes[attempt].outcome = "confirmed";
        std::map<std::string, std::string> mapped;
        for (size_t i = 0; i < placeholders.size(); ++i) mapped[placeholders[i]] = result.contentUris[i];
        uploaded = mapped;
        return mapped;
    }

    void cleanup(std::vector<std::string>& warnings) {
        warnings.insert(warnings.end(), notes.begin(), notes.end());
        if (!documentId) return;
        const auto id = std::exchange(documentId, std::nullopt);
        try {
            services.deleteFile(*id);
        } catch (const std::exception& error) {
            warnings.push_back("The image upload document " + *id + " could not be deleted: " + error.what());
        } catch (...) {
            warnings.push_back("The image upload document " + *id + " could not be deleted.");
        }
    }
};

inline std::vector<std::string> imageUploadPlaceholders(const Plan& plan) {
    std::vector<std::string> pending;
    for (const auto& doc : plan.docs) for (const auto& request : doc.plan.contentRequests) {
        for (const auto* kind : {"insertInlineImage", "replaceImage"}) {
            if (!request.contains(kind) || !request[kind].contains("uri") || !request[kind]["uri"].is_string()) continue;
            const auto uri = request[kind]["uri"].get<std::string>();
            if (uri.starts_with(lens::IMAGE_UPLOAD_SCHEME)) pending.push_back(uri);
        }
    }
    return pending;
}

inline void envelopeValidate(const FileInputSnapshot& snapshot) {
    if (std::filesystem::path(snapshot.path).filename() == "export.orig.md") {
        throw model::CoreError("unsupportedSyntax", "Import the working copy, not export.orig.md.");
    }
}

inline void readonlyValidate(const FileInputSnapshot& snapshot, const lens::DocumentFile& file,
                             const std::optional<lens::DocumentFile>& exported) {
    const lens::DocumentFile empty;
    const auto& original = exported ? *exported : empty;
    if (file.frontmatter.styleBaseline != original.frontmatter.styleBaseline ||
        file.frontmatter.guards != original.frontmatter.guards ||
        file.frontmatter.variables != original.frontmatter.variables) {
        throw model::CoreError("unsupportedSyntax", "Preserve the exported style baseline, variables and guards.");
    }
    const auto inputYaml = slimr::parse_frontmatter(snapshot.text);
    const auto originalYaml = slimr::parse_frontmatter(lens::documentFileExport(original));
    const auto inputFields = inputYaml && inputYaml->frontmatter ? *inputYaml->frontmatter : slimr::json_obj();
    const auto originalFields = originalYaml && originalYaml->frontmatter ? *originalYaml->frontmatter : slimr::json_obj();
    for (const auto* key : {"apiUser", "guards", "styleBaseline", "variables"}) {
        if (inputFields.contains(key) != originalFields.contains(key) ||
            (inputFields.contains(key) && inputFields[key] != originalFields[key])) {
            throw model::CoreError("unsupportedSyntax", std::string(key) + " must exactly preserve its exported read-only context.");
        }
    }
}

inline std::string sectionMarkdown(const lens::DocumentFile& file, const lens::DocumentFileTab& section,
                                  const model::DocModel& doc, const std::string& tabId, const std::string& markdown) {
    auto copy = section;
    copy.markdown = markdown;
    return lens::documentTabMarkdown(file, copy, doc, tabId);
}

inline void segmentsWrite(const lens::DocumentFile& file, const lens::DocumentFileTab& section,
                          TabHandle& target, const model::DocModel& doc, std::set<std::string>& applied) {
    const auto same = [](const auto& a, const auto& b) {
        return a.kind == b.kind && a.type == b.type && a.section == b.section;
    };
    for (const auto& segment : target.model().segments) {
        if (segment.kind == "footnote") continue;
        if (std::none_of(section.segments.begin(), section.segments.end(), [&](const auto& line) { return same(segment, line); })) {
            throw model::CoreError("tabRequired", "An existing " + segment.type + " " + segment.kind +
                " is missing from the file; retain its line or add delete explicitly.");
        }
    }
    target.segmentFlagsSet(section.readonlyAttributes.has_flag("oddEven"), section.readonlyAttributes.has_flag("firstPage"));
    std::set<std::string> seen;
    for (const auto& segment : section.segments) {
        const auto identity = segment.kind + ":" + segment.type + ":" + std::to_string(segment.section.value_or(1));
        if (!seen.insert(identity).second) throw model::CoreError("unsupportedSyntax", "A header/footer is declared twice.");
        const bool exists = std::any_of(target.model().segments.begin(), target.model().segments.end(),
            [&](const auto& current) { return same(current, segment); });
        if (segment.isDelete) {
            if (exists) target.segmentDelete(segment.kind, segment.type, segment.section);
            continue;
        }
        const auto breaks = std::count_if(target.model().blocks.begin(), target.model().blocks.end(),
            [](const auto& block) { return block.is_section_break(); });
        if (segment.section && (*segment.section < 2 || *segment.section > breaks + 1)) {
            throw model::CoreError("unsupportedSyntax",
                "tab \"" + section.title + "\" has " + std::to_string(breaks + 1) + " section" +
                (breaks ? "s" : "") + ", so there's no section " + std::to_string(*segment.section));
        }
        if (!exists) {
            if (segment.type != "default") throw model::CoreError("unsupportedSyntax", "Google can create only default headers and footers.");
            target.segmentCreate(segment.kind, segment.section);
        }
        const auto report = target.segmentWriteMarkdown(segment.kind, segment.type,
            sectionMarkdown(file, section, doc, target.tabId, segment.markdown), segment.section);
        applied.insert(report.rejected.begin(), report.rejected.end());
    }
}

inline void footnotesWrite(const lens::DocumentFile& file, const lens::DocumentFileTab& section, TabHandle& target,
                          const model::DocModel& doc, const std::vector<std::string>& before, std::set<std::string>& applied) {
    const auto keptOrder = footnoteOrder(target.model());
    const std::set<std::string> kept(keptOrder.begin(), keptOrder.end());
    std::map<std::string, std::string> created;
    for (const auto& paragraph : model::paragraphsDeep(target.model().blocks)) for (const auto& item : paragraph.inlines) {
        if (item.type != "footnoteRef" || !item.create) continue;
        const auto label = item.create->label.value_or("");
        if (!created.emplace(label, "new:fn:" + item.key).second) {
            throw model::CoreError("unsupportedSyntax", "A new footnote is referenced more than once.");
        }
    }
    std::set<std::string> defined;
    for (const auto& footnote : section.footnotes) {
        std::string id;
        if (!footnote.label.empty() && footnote.label.front() >= '1' && footnote.label.front() <= '9' &&
            footnote.label.find_first_not_of("0123456789") == std::string::npos) {
            size_t ordinal = 0;
            const auto [end, error] = std::from_chars(footnote.label.data(), footnote.label.data() + footnote.label.size(), ordinal);
            if (error == std::errc{} && end == footnote.label.data() + footnote.label.size() && ordinal <= before.size()) {
                id = before[ordinal - 1];
            }
        }
        if ((id.empty() || !kept.contains(id)) && created.contains(footnote.label)) id = created.at(footnote.label);
        if (id.empty()) throw model::CoreError("unsupportedSyntax", "Footnote definition has no reference.");
        if (!defined.insert(id).second) throw model::CoreError("unsupportedSyntax", "A footnote is defined twice.");
        if (!kept.contains(id)) throw model::CoreError("unsupportedSyntax", "Remove the definition of a removed footnote reference.");
        if (footnote.markdown.find_first_not_of(" \t\r\n") == std::string::npos) {
            bool blank = true;
            for (const auto& segment : target.model().segments) {
                if (segment.id != id || !segment.tab) continue;
                blank = footnoteBodyBlank(*segment.tab);
            }
            if (blank) continue;
        }
        const auto report = target.footnoteWriteMarkdown(id, sectionMarkdown(file, section, doc, target.tabId, footnote.markdown));
        applied.insert(report.rejected.begin(), report.rejected.end());
    }
    if (defined.size() != kept.size()) throw model::CoreError("unsupportedSyntax", "Every retained footnote needs exactly one definition.");
}

inline void tabSettingsWrite(const lens::DocumentFileTab& section, TabHandle& target, bool creating) {
    const auto& attrs = section.readonlyAttributes;
    const auto parsed = lens::sectionSettingsParse(attrs.flags, attrs.values);
    PageSetupPatch page;
    const auto& style = target.model().documentStyle;
    const bool pageful = attrs.has_flag("pageful");
    const bool stored = style.contains("documentFormat") && style["documentFormat"].contains("documentMode") &&
        style["documentFormat"]["documentMode"] == slimr::Json("PAGES");
    if (creating || pageful != stored || section.settings.pageless) page.pageless = !pageful;
    if (pageful) {
        auto size = attrs.get_value("size").value_or("LETTER");
        const auto cross = size.find('x');
        if (cross != std::string::npos) {
            auto number = [](const std::string& text) {
                size_t end = 0;
                double result = 0;
                try { result = std::stod(text, &end); } catch (...) { throw model::CoreError("unsupportedSyntax", "Invalid custom page size."); }
                if (end != text.size() || !std::isfinite(result) || result <= 0) throw model::CoreError("unsupportedSyntax", "Invalid custom page size.");
                return result;
            };
            page.widthPt = number(size.substr(0, cross));
            page.heightPt = number(size.substr(cross + 1));
        } else page.size = size;
        if (cross == std::string::npos || attrs.has_flag("landscape")) {
            page.orientation = attrs.has_flag("landscape") ? "LANDSCAPE" : "PORTRAIT";
        }
        const auto margins = parsed.margins.value_or(std::array<double, 4>{72, 72, 72, 72});
        page.margins = PageSetupMargins{.bottom = margins[2], .left = margins[3], .right = margins[1], .top = margins[0]};
    }
    target.pageSetupSet(page);
    auto settings = section.settings;
    if (parsed.pageNumberStart) settings.pageNumberStart = *parsed.pageNumberStart;
    if (auto background = attrs.get_value("background")) settings.background = *background;
    auto margin = [&](const char* name, double& value) {
        if (const auto text = attrs.get_value(name)) {
            size_t end = 0;
            try { value = std::stod(*text, &end); } catch (...) { throw model::CoreError("unsupportedSyntax", "Invalid header/footer margin."); }
            if (end != text->size()) throw model::CoreError("unsupportedSyntax", "Invalid header/footer margin.");
        }
    };
    margin("headerMargin", settings.headerMargin);
    margin("footerMargin", settings.footerMargin);
    target.settingsSet(settings);
    auto leading = lens::sectionSettingsOf(target.model().leadingSectionStyle, target.model().documentStyle);
    const auto previous = leading;
    leading.columns = parsed.columns;
    leading.columnGap = parsed.columnGap;
    leading.columnSeparator = parsed.columnSeparator;
    leading.rtl = parsed.rtl;
    if (leading != previous) target.model().leadingSectionStyle = lens::sectionStyleWith(target.model().leadingSectionStyle, leading);
    target.model().iconEmoji = attrs.get_value("emoji");
}

template <typename Fn>
inline slimr::Json tracked(WriteTracker& tracker, WriteAttempt attempt, Fn&& send) {
    const auto index = tracker.writes.size();
    tracker.writes.push_back(std::move(attempt));
    try {
        auto response = send();
        tracker.writes[index].receipt = response;
        tracker.writes[index].outcome = "confirmed";
        return response;
    } catch (const model::CoreError& error) {
        tracker.failureCode = error.code;
        tracker.writes[index].outcome =
            error.code == "remoteRejected" || error.code == "concurrentEdits" ? "rejected" : "unknown";
        throw;
    } catch (...) {
        tracker.writes[index].outcome = "unknown";
        throw;
    }
}

inline void commentReceiptValidate(const CommentAction& action, const slimr::Json& receipt) {
    if (!receipt.is_object()) {
        throw model::CoreError("operationFailed", "Drive returned an invalid comment receipt; the outcome is unknown.");
    }
    if (action.kind == "delete" || action.kind == "replyDelete") return;
    if (!receipt.contains("id") || !receipt["id"].is_string() || receipt["id"].get<std::string>().empty()) {
        throw model::CoreError("operationFailed", "Drive returned no comment/reply ID; the outcome is unknown. Do not replay this action blindly.");
    }
    const auto id = receipt["id"].get<std::string>();
    if ((action.kind == "edit" && action.id != id) ||
        (action.kind == "replyEdit" && action.replyId != id)) {
        throw model::CoreError("operationFailed", "Drive returned a different comment/reply identity; the outcome is unknown.");
    }
    if (action.kind == "resolve" || action.kind == "reopen") {
        if (!receipt.contains("action") || !receipt["action"].is_string() ||
            receipt["action"].get<std::string>() != action.kind) {
            throw model::CoreError("operationFailed", "Drive did not confirm the requested comment action; the outcome is unknown.");
        }
    } else if (!receipt.contains("content") || !receipt["content"].is_string() ||
               receipt["content"].get<std::string>() != action.text.value_or("")) {
        throw model::CoreError("operationFailed", "Drive did not confirm the requested comment text; the outcome is unknown.");
    }
}

inline void commentOutcomeValidate(const CommentAction& action, const slimr::Json& receipt,
                                   const std::vector<DriveComment>& comments) {
    const auto threadId = action.kind == "create" ? receipt["id"].get<std::string>() : action.id.value_or("");
    const auto thread = std::find_if(comments.begin(), comments.end(),
                                    [&](const auto& comment) { return comment.id == threadId; });
    const bool live = thread != comments.end() && !thread->deleted.value_or(false);
    bool match = false;
    if (action.kind == "delete") {
        match = !live;
    } else if (live) {
        if (action.kind == "create" || action.kind == "edit") {
            match = thread->content == action.text;
        } else {
            const auto replyId = action.kind == "replyDelete" ? action.replyId.value_or("")
                                                            : receipt["id"].get<std::string>();
            const auto reply = std::find_if(thread->replies.begin(), thread->replies.end(),
                                           [&](const auto& item) { return item.id == replyId; });
            const bool replyLive = reply != thread->replies.end() && !reply->deleted.value_or(false);
            if (action.kind == "replyDelete") {
                match = !replyLive;
            } else if (action.kind == "resolve" || action.kind == "reopen") {
                match = replyLive && reply->action == action.kind &&
                        thread->resolved == std::make_optional(action.kind == "resolve");
            } else if (action.kind == "reply" || action.kind == "replyEdit") {
                match = replyLive && reply->content == action.text;
            }
        }
    }
    if (!match) {
        throw model::CoreError("operationFailed",
            "The authoritative Drive readback did not confirm the requested comment outcome; "
            "the outcome is unknown. Preserve this intent and do not replay blindly.");
    }
}

inline FileServices recoveryServices(const ImportServices& services) {
    FileServices recovery = services;
    const auto root = std::filesystem::path(services.root.empty() ? artifactRoot() : services.root) / "recoveries";
    const auto operation = root / detail::unique_tmp_suffix();
    std::error_code error;
    std::filesystem::create_directories(operation, error);
    if (error || chmod(root.c_str(), 0700) != 0 || chmod(operation.c_str(), 0700) != 0) {
        throw model::CoreError("operationFailed", "A private recovery directory could not be created.");
    }
    recovery.root = operation.string();
    return recovery;
}

struct Docs {
    const ImportServices& services;
    WriteTracker& tracker;

    slimr::Json getDocument(const std::string& id) {
        auto json = services.getDocument(id);
        if (!json.is_object() || !json.contains("documentId") || !json["documentId"].is_string() ||
            json["documentId"].get<std::string>() != id) {
            throw model::CoreError("operationFailed", "The document readback has the wrong identity.");
        }
        auto metadata = services.getFileMetadata(id);
        if (!metadata.is_object() || !metadata.contains("id") || !metadata["id"].is_string() ||
            metadata["id"].get<std::string>() != id || !metadata.contains("name") ||
            !metadata["name"].is_string()) {
            throw model::CoreError("operationFailed", "Drive metadata did not confirm the document identity and title.");
        }
        json["title"] = metadata["name"];
        tracker.documents[id] = json;
        return json;
    }

    slimr::Json createDocument(const std::string& title) {
        const auto index = tracker.writes.size();
        auto json = tracked(tracker, WriteAttempt{.action = "createDocument", .requestCount = 1},
                            [&] { return services.createDocument(title); });
        if (!json.contains("documentId") || !json["documentId"].is_string() ||
            json["documentId"].get<std::string>().empty()) {
            tracker.writes[index].outcome = "unknown";
            throw model::CoreError("operationFailed", "Creation returned no document identity; do not repeat it blindly.");
        }
        tracker.writes[index].createdId = json["documentId"].get<std::string>();
        return json;
    }

    slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                           const std::optional<std::string>& revision) {
        if (requests.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw model::CoreError("unsupportedSyntax", "Too many requests for one write.");
        }
        if (!revision || revision->empty()) {
            throw model::CoreError("staleBase", "A Docs write requires a revision lock.");
        }
        const auto index = tracker.writes.size();
        auto json = tracked(tracker, WriteAttempt{.action = "batchUpdate", .doc = id,
                            .requestCount = static_cast<int>(requests.size())},
                            [&] { return services.batchUpdate(id, requests, revision); });
        if (!json.contains("writeControl") || !json["writeControl"].contains("requiredRevisionId") ||
            !json["writeControl"]["requiredRevisionId"].is_string() ||
            json["writeControl"]["requiredRevisionId"].get<std::string>().empty()) {
            tracker.writes[index].outcome = "unknown";
            throw model::CoreError("staleBase", "The write returned no revision; stop and export fresh.");
        }
        auto next = json["writeControl"]["requiredRevisionId"].get<std::string>();
        tracker.writes[index].revision = next;
        tracker.revisions[id] = next;
        return json;
    }
};

struct Drive {
    const ImportServices& services;
    std::vector<DriveComment> commentsList(const std::string& id) { return services.commentsList(id); }
    std::string textExport(const std::string& id) {
        if (!services.textExport) throw model::CoreError("operationFailed", "Plain-text export is unavailable.");
        return services.textExport(id);
    }
};

inline lens::DocumentFile snapshotFile(const model::DocModel& model,
                                      const std::vector<DriveComment>& comments,
                                      std::optional<std::string> plainText = std::nullopt,
                                      std::optional<std::string> sourceVersion = std::string(lens::GDOCSMITH_NODE_VERSION),
                                      std::optional<std::string> apiUser = std::nullopt) {
    return lens::documentFileParse(lens::documentFileExport(
        exportDocumentFile(model, comments, ExportOptions{.commentsIncludeResolved = true,
            .sourceVersion = std::move(sourceVersion),
            .apiUser = std::move(apiUser)}, plainText)));
}

inline std::vector<std::string> gdocSourceIds(std::string_view text) {
    auto isIdChar = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    };
    auto isWs = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    };

    std::vector<std::string> ids;
    std::unordered_set<std::string> seen;
    auto record = [&](std::string_view id) {
        std::string key(id);
        if (seen.insert(key).second) ids.push_back(std::move(key));
    };

    const size_t n = text.size();
    // Pattern 1: :image(?:\[(?:[^\]\\]|\\.)*\])?\{(?:[^}]*\s)?src="?gdoc:([A-Za-z0-9_-]+)\/
    size_t cachedBrace = 0;
    for (size_t i = 0; i < n;) {
        const size_t pos = text.find(":image", i);
        if (pos == std::string_view::npos) break;
        size_t p = pos + 6;
        if (p < n && text[p] == '[') {
            size_t q = p + 1;
            bool closed = false;
            while (q < n) {
                if (text[q] == '\\') {
                    q += 2;
                    continue;
                }
                if (text[q] == ']') {
                    ++q;
                    closed = true;
                    break;
                }
                ++q;
            }
            if (!closed) {
                i = pos + 1;
                continue;
            }
            p = q;
        }
        if (p >= n || text[p] != '{') {
            i = pos + 1;
            continue;
        }
        ++p;
        if (cachedBrace < p) {
            cachedBrace = text.find('}', p);
            if (cachedBrace == std::string_view::npos) cachedBrace = n;
        }
        const size_t limit = cachedBrace;
        std::optional<std::string_view> matchedId;
        size_t matchedEnd = pos + 1;
        for (size_t k = p; k + 4 <= limit; ++k) {
            if (k > p && !isWs(text[k - 1])) continue;
            if (text.compare(k, 4, "src=") != 0) continue;
            size_t m = k + 4;
            if (m < limit && text[m] == '"') ++m;
            if (m + 5 <= limit && text.compare(m, 5, "gdoc:") == 0) {
                m += 5;
                const size_t idStart = m;
                while (m < limit && isIdChar(text[m])) ++m;
                if (m > idStart && m < limit && text[m] == '/') {
                    matchedId = text.substr(idStart, m - idStart);
                    matchedEnd = m + 1;
                }
            }
        }
        if (matchedId) {
            record(*matchedId);
            i = matchedEnd;
        } else {
            i = pos + 1;
        }
    }

    // Pattern 2: \]\(\s*<?gdoc:([A-Za-z0-9_-]+)
    for (size_t i = 0; i < n;) {
        const size_t pos = text.find("](", i);
        if (pos == std::string_view::npos) break;
        size_t p = pos + 2;
        while (p < n && isWs(text[p])) ++p;
        if (p < n && text[p] == '<') ++p;
        if (p + 5 <= n && text.compare(p, 5, "gdoc:") == 0) {
            p += 5;
            const size_t idStart = p;
            while (p < n && isIdChar(text[p])) ++p;
            if (p > idStart) {
                record(text.substr(idStart, p - idStart));
                i = p;
                continue;
            }
        }
        i = pos + 2;
    }

    return ids;
}

inline void baseValidate(const lens::DocumentFile& file, const ImportServices& services,
                         const model::DocModel& live, const FileInputSnapshot& input) {
    if (live.revisionId != file.frontmatter.revision) {
        throw model::CoreError("staleBase", "the cloud revision changed; reapply the edit to a fresh export, without replacing the revision in this file");
    }
    auto path = std::filesystem::path(services.root.empty() ? artifactRoot() : services.root)
        / "exports" / live.docId / *file.frontmatter.revision / "export.orig.md";
    const auto sibling = std::filesystem::path(input.path).parent_path() / "export.orig.md";
    if (std::filesystem::path(input.path).filename() == "export.md" && std::filesystem::exists(sibling)) {
        const auto siblingFile = lens::documentFileParse(inputFileRead(sibling.string()).text);
        if (siblingFile.frontmatter.doc == file.frontmatter.doc &&
            siblingFile.frontmatter.revision == file.frontmatter.revision) path = sibling;
        else if (!std::filesystem::exists(path)) {
            throw model::CoreError("staleBase", "The export's original snapshot does not match the import frontmatter.");
        }
    }
    if (std::filesystem::exists(path)) {
        auto base = lens::documentFileParse(inputFileRead(path.string()).text);
        if (base.frontmatter.doc != file.frontmatter.doc || base.frontmatter.revision != file.frontmatter.revision) {
            throw model::CoreError("staleBase", "The export's original snapshot does not match the import frontmatter.");
        }
        std::set<std::string> expected, actual;
        for (const auto& tab : base.tabs) expected.insert(tab.id);
        for (const auto& tab : live.tabs) actual.insert(tab.tabId);
        if (expected != actual) {
            throw model::CoreError("staleBase", "The original snapshot's tabs do not match the live revision.");
        }
        auto current = lens::documentFileParse(lens::documentFileExport(exportDocumentFile(
            live, {}, ExportOptions{.sourceVersion = base.frontmatter.sourceVersion})));
        auto trimTrailingNewlines = [](std::string_view text) {
            while (!text.empty() && text.back() == '\n') text.remove_suffix(1);
            return text;
        };
        for (size_t i = 0; i < base.tabs.size(); ++i) {
            if (i >= current.tabs.size() || base.tabs[i].id != current.tabs[i].id ||
                trimTrailingNewlines(base.tabs[i].markdown) != trimTrailingNewlines(current.tabs[i].markdown)) {
                throw model::CoreError("staleBase", "The original snapshot content does not match the live revision; export again.");
            }
        }
    }
}

} // namespace import_detail

/**
 * Validates one document file, then fails closed while remote import is unavailable.
 */
inline FileWriteResult fileImport(
    const ImportInput& input,
    const FileServices& = {}
) {
    auto snapshot = inputFileRead(input.path);
    auto file = lens::documentFileParse(snapshot.text);

    bool creating = !file.frontmatter.doc.has_value() || file.frontmatter.doc->empty();

    if (creating && !input.newDocStyleBaseline.has_value()) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                              "This file creates a document; pass newDocStyleBaseline. Use generic when it should look like an existing document (a copy, recreation or split of an export); account would merge the source's styles with the user's saved ones. Use account for new content written for the user.");
    }

    if (!creating && input.newDocStyleBaseline.has_value()) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                              "newDocStyleBaseline applies only when the file creates a document (doc: null); remove it");
    }

    std::string docId;
    if (!creating) {
        docId = documentId(*file.frontmatter.doc);
        if (docId != *file.frontmatter.doc) {
            throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                                  "frontmatter doc must contain the ID, not a URL");
        }
        if (!file.frontmatter.revision.has_value() || file.frontmatter.revision->empty()) {
            throw model::CoreError(std::string(model::error_codes::STALE_BASE),
                                  "existing-document imports require the exported revision");
        }
    }

    std::vector<std::string> fileNotes;
    if (!creating) {
        lastTabKeep(file, fileNotes);
    }

    std::vector<model::FileProblem> tabProblems;
    model::DocModel currentDoc;
    currentDoc.docId = docId;
    currentDoc.title = file.frontmatter.title;
    currentDoc.revisionId = file.frontmatter.revision.value_or("r1");

    for (const auto& t : file.tabs) {
        model::TabModel tm;
        tm.tabId = t.id;
        tm.title = t.title;
        currentDoc.tabs.push_back(std::move(tm));
    }

    if (!creating) {
        tabLinesValidate(file, currentDoc, tabProblems);
    }
    tabTitlesValidate(file, currentDoc, tabProblems);
    model::problemsThrow(tabProblems);

    // A providerless session cannot apply or verify remote changes. Do not
    // simulate success, invent a created ID, or replace input/base artifacts.
    ResultError error;
    error.code = "operationFailed";
    error.message = "Remote import is unavailable in this build. Nothing was sent; the input and existing export/base files were preserved.";
    FileWriteResult result;
    result.status = "failed";
    result.summary = error.message;
    result.errors = std::vector<ResultError>{std::move(error)};
    result.verification = "notApplicable";
    result.writeState = "none";
    return result;
}

/** Runs frozen file intent through the guarded transaction and its authoritative readback. */
inline FileWriteResult fileImportRun(const ImportInput& input, const ImportServices& services,
                                     import_detail::ImportImageUploads& uploads) {
    if (!services.getDocument || !services.getFileMetadata || !services.createDocument ||
        !services.batchUpdate || !services.commentsList) {
        return fileImport(input, static_cast<const FileServices&>(services));
    }
    const auto snapshot = inputFileRead(input.path);
    auto file = lens::documentFileParse(snapshot.text);
    const bool creating = !file.frontmatter.doc;
    if (creating && (!input.newDocStyleBaseline || !parseNewDocStyleBaseline(*input.newDocStyleBaseline))) {
        throw model::CoreError("unsupportedSyntax", "A new document requires newDocStyleBaseline: generic or account.");
    }
    if (!creating && input.newDocStyleBaseline) {
        throw model::CoreError("unsupportedSyntax", "newDocStyleBaseline applies only to doc: null.");
    }
    std::string doc;
    if (!creating) {
        doc = documentId(*file.frontmatter.doc);
        if (doc != *file.frontmatter.doc) throw model::CoreError("unsupportedSyntax", "frontmatter doc must contain the ID, not a URL");
        if (!file.frontmatter.revision || file.frontmatter.revision->empty()) {
            throw model::CoreError("staleBase", "Existing-document imports require the exported revision.");
        }
    } else if (file.frontmatter.revision) {
        throw model::CoreError("unsupportedSyntax", "A new document must not have a revision.");
    }
    WriteTracker tracker;
    uploads.tracker = &tracker;
    import_detail::Docs docs{services, tracker};
    import_detail::Drive drive{services};
    FileWriteResult output;
    std::string phase = "validation";
    bool complete = false;
    std::optional<std::string> transactedCreation;
    bool includeResolved = std::any_of(file.comments.begin(), file.comments.end(),
                                      [](const auto& comment) { return comment.resolved; });
    std::optional<std::vector<CommentAction>> commentActions;
    std::vector<DeletedTab> deletedTabs;
    std::vector<std::string> notes;
    std::map<std::string, model::DocModel> imageDocuments;
    std::unordered_map<std::string, std::string> unreadableSources;
    std::map<size_t, std::map<std::string, std::string>> dropdownAliases;
    if (!creating) lastTabKeep(file, notes);

    auto refresh = [&](const std::string& id) {
        auto json = docs.getDocument(id);
        auto actual = model::docModelParse(json, model::DocModelParseOptions{.docId = id});
        auto comments = drive.commentsList(id);
        std::optional<std::string> text;
        const bool hasHiddenChips = std::any_of(actual.tabs.begin(), actual.tabs.end(), [](const auto& tab) {
            return std::any_of(tab.blocks.begin(), tab.blocks.end(), [](const auto& block) {
                return block.is_paragraph() && std::any_of(block.as_paragraph().inlines.begin(),
                    block.as_paragraph().inlines.end(), [](const auto& item) {
                        return item.is_text() && item.text.find("\uE907") != std::string::npos;
                    });
            });
        });
        if (hasHiddenChips ||
            std::any_of(comments.begin(), comments.end(), [](const auto& c) { return !c.quote().empty(); })) {
            try { text = drive.textExport(id); }
            catch (const std::exception&) { output.warnings.push_back("Plain-text comment anchors could not be refreshed."); }
        }
        const bool uploadOnly = !creating && !tracker.writes.empty() &&
            actual.revisionId == file.frontmatter.revision &&
            std::all_of(tracker.writes.begin(), tracker.writes.end(),
                [](const auto& write) { return write.action == "imagesUpload"; });
        const FileServices placement = uploadOnly ? static_cast<const FileServices&>(services)
                                                 : import_detail::recoveryServices(services);
        auto exported = exportModel(actual, comments, placement,
                                   ExportOptions{.commentsIncludeResolved = includeResolved,
                                                 .sourceVersion = file.frontmatter.sourceVersion}, text);
        output.warnings.insert(output.warnings.end(), exported.warnings.begin(), exported.warnings.end());
        output.exportPath = uploadOnly ? exported.exportPath : exportSnapshotPath(exported, output.warnings);
    };

    try {
        import_detail::envelopeValidate(snapshot);
        for (const auto& sourceId : import_detail::gdocSourceIds(snapshot.text)) {
            if (sourceId == doc || imageDocuments.contains(sourceId) || unreadableSources.contains(sourceId)) continue;
            try {
                imageDocuments.emplace(sourceId, model::docModelParse(services.getDocument(sourceId), {.docId = sourceId}));
            } catch (const std::exception& error) {
                unreadableSources.emplace(sourceId, error.what());
            }
        }
        phase = "planning";
        TransactionOptions<import_detail::Docs, import_detail::Drive> options;
        options.client = &docs;
        options.drive = &drive;
        options.allow = input.allow;
        options.onPlan = [&](const Plan& plan) { uploads.prepare(import_detail::imageUploadPlaceholders(plan)); };
        options.imageUploads = [&](const std::vector<std::string>& pending) { return uploads.upload(pending); };
        auto result = transactionRun<int>([&](CoreSession& session) {
            session.tabBegin(0);
            session.atomOptions.baseDir = std::filesystem::path(snapshot.path).parent_path().string();
            for (const auto& suggestion : file.suggestions) if (suggestion.isReject) {
                session.rejectedSuggestions.insert(suggestion.id);
            }
            auto handle = creating ? session.docCreate("import", file.frontmatter.title) : session.docOpen(doc);
            auto& state = session.docs.at(handle->docId());
            session.atomOptions.images = [original = state.original, &imageDocuments, &unreadableSources](const std::string& source) -> std::optional<lens::ImageSource> {
                if (source.starts_with(lens::IMAGE_DOC_SCHEME)) {
                    const auto slash = source.find('/', lens::IMAGE_DOC_SCHEME.size());
                    if (slash == std::string::npos || slash == lens::IMAGE_DOC_SCHEME.size() || slash + 1 == source.size()) {
                        throw model::CoreError("anchorNotFound", "An image source must be gdoc:<doc id>/<image id>.");
                    }
                    const auto sourceId = source.substr(lens::IMAGE_DOC_SCHEME.size(), slash - lens::IMAGE_DOC_SCHEME.size());
                    if (sourceId != original.docId) {
                        if (auto it = unreadableSources.find(sourceId); it != unreadableSources.end()) {
                            throw model::CoreError("anchorNotFound", "document " + sourceId + " couldn't be read: " + it->second);
                        }
                    }
                }
                std::vector<model::DocModel> sources{original};
                for (const auto& [id, sourceDoc] : imageDocuments) { (void)id; sources.push_back(sourceDoc); }
                auto found = lens::imageResolverBuild(sources)(source);
                if (!found && source.starts_with(lens::IMAGE_DOC_SCHEME)) {
                    throw model::CoreError("anchorNotFound", "no single image with that id in that document");
                }
                return found;
            };
            if (!state.isNew) {
                if (!creating && !tracker.revisions.contains(state.docId)) {
                    import_detail::baseValidate(file, services, state.original, snapshot);
                } else if (tracker.revisions.contains(state.docId) &&
                           state.original.revisionId != tracker.revisions.at(state.docId)) {
                    throw model::CoreError("staleBase", "The document changed between confirmed transaction phases.");
                }
            }
            if (!commentActions) {
                std::optional<lens::DocumentFile> exported;
                if (!creating) exported = import_detail::snapshotFile(
                    state.original, session.commentsFor(state), session.exportTextFor(state),
                    file.frontmatter.sourceVersion,
                    services.apiUser.empty() ? std::nullopt : std::optional<std::string>(services.apiUser));
                import_detail::readonlyValidate(snapshot, file, exported);
                suggestionBlocksValidate(file, exported);
                commentActions = commentActionsPlan(file, exported);
                if (!commentActions->empty() && !services.commentSend) {
                    throw model::CoreError("unsupportedSyntax", "Comment mutations require a configured Drive adapter.");
                }
            }
            for (const auto& action : *commentActions) {
                session.findingAdd(GuardFinding{
                    .anchor = action.id.value_or("new comment at line " + std::to_string(action.line)),
                    .docId = handle->docId(), .kind = "commentAction",
                    .message = action.kind + " comment action requires approval",
                    .allowKind = "commentChanges", .severity = "block",
                });
            }
            std::vector<model::FileProblem> problems;
            if (!creating) deletedTabs = tabLinesValidate(file, state.current, problems);
            tabTitlesValidate(file, state.current, problems);
            model::problemsThrow(problems);
            if (state.current.title != file.frontmatter.title && !services.rename && !creating) {
                throw model::CoreError("unsupportedSyntax", "Document renaming requires a configured Drive adapter.");
            }
            std::vector<std::string> order;
            std::set<std::string> appliedRejects;
            std::map<std::string, std::string> createdTabIds;
            std::set<std::string> sectionIds;
            for (size_t index = 0; index < file.tabs.size(); ++index) {
                const auto& section = file.tabs[index];
                session.tabBegin(static_cast<int>(index));
                session.allowTokenOmission = creating || section.id.empty();
                if (!section.id.empty() && !sectionIds.insert(section.id).second) {
                    throw model::CoreError("unsupportedSyntax", "A tab ID appears more than once.");
                }
                if (section.isDelete) {
                    if (creating) throw model::CoreError("unsupportedSyntax", "A new document cannot delete tabs.");
                    handle->tab(section.id).remove();
                    continue;
                }
                auto parent = section.parent;
                if (creating && parent) {
                    auto found = createdTabIds.find(*parent);
                    if (found == createdTabIds.end()) {
                        throw model::CoreError("invalidPlacement", "A new document's parent tab must have an ID on an earlier tab line.");
                    }
                    parent = found->second;
                }
                auto target = creating && index == 0 ? handle->tab(handle->tabs().front().tabId)
                    : creating || section.id.empty() ? handle->tabCreate(section.title, parent)
                    : handle->tab(section.id);
                target.rename(section.title);
                target.model().parentTabId = parent;
                if (creating && !section.id.empty()) createdTabIds[section.id] = target.tabId;
                if (creating && input.newDocStyleBaseline == "generic") {
                    auto generic = model::docModelParse(emulator::blank_doc_template(file.frontmatter.title, "generic"),
                                                       model::DocModelParseOptions{.docId = "generic"});
                    target.model().namedStyles = generic.tabs.front().namedStyles;
                }
                import_detail::tabSettingsWrite(section, target, creating || section.id.empty());
                if (!creating) {
                    const auto lists = file.frontmatter.dropdowns.contains(section.id)
                        ? file.frontmatter.dropdowns[section.id] : model::make_json_object();
                    dropdownDefinitionsApply(target.model(), lists, session.keys, dropdownAliases[index]);
                    session.atomOptions.createdDropdownIds = dropdownAliases[index];
                } else session.atomOptions.createdDropdownIds.clear();
                const auto footnotesBefore = footnoteOrder(target.model());
                WriteReport report;
                std::vector<model::FileProblem> tabProblems;
                const auto composed = lens::documentTabMarkdown(file, section, state.current, target.tabId);
                // Composed-body lines before section.markdown (declarations frontmatter tail, opening section)
                int prelude = -1;
                if (const auto split = slimr::parse_frontmatter(composed); split && split->body.ends_with(section.markdown)) {
                    const auto head = std::string_view(split->body).substr(0, split->body.size() - section.markdown.size());
                    prelude = static_cast<int>(std::count(head.begin(), head.end(), '\n'));
                }
                session.atomOptions.locateBlockProblems = true;
                try {
                    model::problemCollect(tabProblems, {}, [&] { report = target.writeMarkdown(composed); });
                } catch (...) {
                    session.atomOptions.locateBlockProblems = false;
                    throw;
                }
                session.atomOptions.locateBlockProblems = false;
                if (!tabProblems.empty()) {
                    // Block lines are composed-body lines; map them onto the file from section.markdownLine.
                    // Anything unlocated points at the tab's ::tab directive line
                    for (auto& problem : tabProblems) {
                        if (!problem.tab) problem.tab = section.title;
                        problem.line = problem.line && prelude >= 0 && *problem.line > prelude
                            ? section.markdownLine + (*problem.line - prelude) - 1 : section.line;
                    }
                    throw model::FileProblems(std::move(tabProblems));
                }
                appliedRejects.insert(report.rejected.begin(), report.rejected.end());
                notes.insert(notes.end(), report.notes.begin(), report.notes.end());
                import_detail::segmentsWrite(file, section, target, state.current, appliedRejects);
                import_detail::footnotesWrite(file, section, target, state.current, footnotesBefore, appliedRejects);
                dropdownChipsCheck(target.model());
                auto generic = model::docModelParse(emulator::blank_doc_template("generic", "generic"),
                    model::DocModelParseOptions{.docId = "generic"}).tabs.front();
                const auto beforeStyles = namedStyleSetOf(target.model());
                auto declared = model::make_json_object();
                if (const auto set = section.readonlyAttributes.get_value("namedStyleSet")) {
                    if (!file.frontmatter.namedStyleSets.contains(*set)) {
                        throw model::CoreError("unsupportedSyntax", "The tab names an undeclared named style set.");
                    }
                    declared = file.frontmatter.namedStyleSets[*set];
                }
                if (creating && input.newDocStyleBaseline == "account") {
                    auto merged = beforeStyles;
                    for (const auto& [type, fields] : declared.get<slimr::Json::object_t>()) {
                        if (!fields.is_object()) throw model::CoreError("unsupportedSyntax", "A named style declaration must be a mapping.");
                        for (const auto& [field, value] : fields.get<slimr::Json::object_t>()) merged[type][field] = value;
                    }
                    declared = std::move(merged);
                }
                namedStyleSetApply(target.model(), beforeStyles, declared, generic);
                order.push_back(target.tabId);
            }
            for (const auto& id : session.rejectedSuggestions) if (!appliedRejects.contains(id)) {
                throw model::CoreError("suggestionConflict", "Suggestion " + id +
                    " cannot be rejected from this document body, header or footer.");
            }
            std::vector<model::TabModel> arranged;
            for (const auto& id : order) {
                auto it = std::find_if(state.current.tabs.begin(), state.current.tabs.end(),
                                       [&](const auto& tab) { return tab.tabId == id; });
                if (it != state.current.tabs.end()) arranged.push_back(*it);
            }
            state.current.tabs = std::move(arranged);
            return 0;
        }, options);
        output.warnings.insert(output.warnings.end(), notes.begin(), notes.end());
        for (const auto& finding : result.findings) {
            if (finding.severity == "warn") output.warnings.push_back(finding.message);
            if (finding.severity == "block" && !finding.waived) {
                if (!output.errors) output.errors.emplace();
                output.errors->push_back(ResultError{.code = "guardRefused", .message = finding.message});
            }
        }
        if (result.refused) {
            output.status = "refused";
            output.summary = "Import refused before sending; nothing was sent.";
            output.writeState = tracker.writeState(false);
            return output;
        }
        if (creating) {
            auto it = result.newIds.docs.find("new_import");
            if (it == result.newIds.docs.end() || it->second.empty()) {
                throw model::CoreError("operationFailed", "The transaction did not report the created document ID.");
            }
            doc = it->second;
            output.doc = doc;
            transactedCreation = doc;
        }
        phase = "verification";
        auto it = result.documents.find(doc);
        if (it == result.documents.end()) {
            throw model::CoreError("operationFailed", "The transaction did not retain an authoritative readback.");
        }
        auto json = it->second;
        output.verification = "match";
        for (const auto& check : result.verification) {
            if (check.status != "match") {
                output.verification = check.status == "mismatch" ? "mismatch" : "inconclusive";
            }
        }
        if (output.verification != "match") {
            throw model::CoreError("operationFailed", "The authoritative readback could not verify the intended document.");
        }
        if (json.contains("title") && json["title"].is_string() &&
            json["title"].get<std::string>() != file.frontmatter.title) {
            phase = "rename";
            std::string name;
            import_detail::tracked(tracker, WriteAttempt{.action = "rename", .doc = doc, .requestCount = 1}, [&] {
                name = services.rename(doc, file.frontmatter.title);
                if (name != file.frontmatter.title) {
                    throw model::CoreError("operationFailed", "Drive did not confirm the requested title.");
                }
                return slimr::Json::object_t{};
            });
            json = docs.getDocument(doc);
            if (json["title"].get<std::string>() != name) {
                throw model::CoreError("operationFailed", "The authoritative Drive title changed after renaming.");
            }
        }
        phase = "comments";
        std::vector<ResultError> commentErrors;
        std::vector<std::pair<CommentAction, size_t>> commentAttempts;
        for (const auto& action : *commentActions) {
            try {
                const auto attempt = tracker.writes.size();
                commentAttempts.emplace_back(action, attempt);
                import_detail::tracked(tracker, WriteAttempt{.action = action.kind, .doc = doc, .requestCount = 1}, [&] {
                    auto receipt = services.commentSend(doc, action);
                    tracker.writes[attempt].receipt = receipt;
                    import_detail::commentReceiptValidate(action, receipt);
                    import_detail::commentOutcomeValidate(action, receipt, drive.commentsList(doc));
                    return receipt;
                });
            } catch (const std::exception& error) {
                const auto* core = dynamic_cast<const model::CoreError*>(&error);
                commentErrors.push_back(ResultError{
                    .code = core ? core->code : "operationFailed", .line = action.line,
                    .message = action.kind + " comment action failed or has an unknown outcome; inspect the fresh export before retrying: " + error.what(),
                    .phase = phase,
                });
            }
        }
        if (!commentErrors.empty()) {
            output.errors = std::move(commentErrors);
            output.status = "failed";
            output.summary = "Some comment actions failed; inspect the current export and retry only outstanding actions.";
            output.writeState = tracker.writeState(false);
            if (output.writeState == "unknown") output.verification = "inconclusive";
            try { refresh(doc); }
            catch (const std::exception&) { output.warnings.push_back("The current comments could not be exported."); }
            return output;
        }
        if (!commentActions->empty()) json = docs.getDocument(doc);
        phase = "export";
        auto actual = model::docModelParse(json, model::DocModelParseOptions{.docId = doc});
        std::vector<DriveComment> comments;
        try {
            comments = drive.commentsList(doc);
        } catch (...) {
            for (const auto& [action, attempt] : commentAttempts) {
                (void)action;
                tracker.writes[attempt].outcome = "unknown";
            }
            output.verification = "inconclusive";
            throw;
        }
        bool commentsMatch = true;
        for (const auto& [action, attempt] : commentAttempts) {
            try {
                import_detail::commentOutcomeValidate(action, *tracker.writes[attempt].receipt, comments);
            } catch (const model::CoreError&) {
                tracker.writes[attempt].outcome = "unknown";
                commentsMatch = false;
            }
        }
        if (!commentsMatch) {
            output.verification = "inconclusive";
            throw model::CoreError("operationFailed", "The final Drive comment readback is inconclusive; do not replay blindly.");
        }
        complete = true;
        const bool changed = inputFileChanged(snapshot);
        const auto placement = changed ? import_detail::recoveryServices(services)
                                       : static_cast<const FileServices&>(services);
        auto exported = exportModel(actual, comments, placement,
                                    ExportOptions{.commentsIncludeResolved = includeResolved,
                                                  .sourceVersion = file.frontmatter.sourceVersion});
        output.warnings.insert(output.warnings.end(), exported.warnings.begin(), exported.warnings.end());
        output.exportPath = exportPlace(exported, snapshot, changed, output.warnings);
        output.commentsLine = exported.commentsLine;
        output.suggestionsLine = exported.suggestionsLine;
        if (!deletedTabs.empty()) output.deletedTabs = deletedTabs;
        output.writeState = tracker.writeState(true);
        output.status = output.writeState == "none" ? "unchanged" : "applied";
        output.summary = output.status == "unchanged" ? "Document already matches; no cloud writes were sent." : "Document changes applied.";
        return output;
    } catch (const std::exception& error) {
        output.status = "failed";
        output.writeState = tracker.writeState(complete || transactedCreation.has_value());
        if (const auto* transaction = dynamic_cast<const TransactionError*>(&error)) phase = transaction->phase;
        if (std::string_view(error.what()).find("did not match the intended document") != std::string_view::npos) {
            output.verification = "mismatch";
            output.writeState = tracker.writeState(true);
        } else if (phase == "verification" && output.verification == "notApplicable") {
            output.verification = "inconclusive";
        }
        output.summary = transactedCreation
            ? "Document " + *output.doc + " was created and its content landed, but the import failed afterwards. Continue from the fresh export."
            : complete
            ? "Cloud changes were verified, but the local export failed; do not reapply the content blindly."
            : "Import failed; preserve this file and reapply only outstanding changes to a fresh export.";
        if (const auto* problems = dynamic_cast<const model::FileProblems*>(&error)) {
            output.errors.emplace();
            for (const auto& problem : problems->problems) {
                output.errors->push_back(ResultError{.code = problem.code, .line = problem.line,
                    .message = problem.message, .phase = phase, .tab = problem.tab});
            }
        } else {
            const auto* core = dynamic_cast<const model::CoreError*>(&error);
            output.errors = std::vector<ResultError>{ResultError{
                .code = core ? core->code : tracker.failureCode.value_or("operationFailed"),
                .message = error.what(), .phase = phase}};
        }
        for (const auto& write : tracker.writes) {
            if (write.createdId) {
                doc = *write.createdId;
                output.doc = doc;
            }
        }
        const auto* core = dynamic_cast<const model::CoreError*>(&error);
        const bool conflict = (core && (core->code == "staleBase" || core->code == "concurrentEdits")) ||
            tracker.failureCode == "staleBase" || tracker.failureCode == "concurrentEdits";
        if ((core || conflict) && output.writeState == "none") {
            output.status = "refused";
        }
        if (creating && output.doc && !transactedCreation && output.verification != "mismatch") {
            if (services.deleteFile) {
                try {
                    import_detail::tracked(tracker,
                        WriteAttempt{.action = "deleteFile", .doc = doc, .requestCount = 1}, [&] {
                            services.deleteFile(doc);
                            return slimr::Json::object_t{};
                        });
                    output.doc.reset();
                    output.writeState = "reverted";
                    output.verification = "notApplicable";
                    output.summary = "Import failed after creating a document, which was deleted; nothing remains. Fix the cause if the error is not transient, then import the same file again.";
                    return output;
                } catch (const std::exception&) {
                    output.writeState = tracker.writeState(false);
                }
            }
            output.warnings.push_back("The newly created document could not be deleted; inspect its fresh export before retrying.");
        } else if (creating && !output.doc && output.writeState == "unknown") {
            output.summary = "A document titled \"" + file.frontmatter.title +
                "\" may have been created; search Drive for it before importing this file again.";
        }
        if (output.writeState == "unknown" && output.verification == "notApplicable") output.verification = "inconclusive";
        if (!doc.empty() && (conflict || output.writeState == "partial" || output.writeState == "unknown" ||
                             (creating && output.verification == "mismatch") || output.verification == "inconclusive")) {
            try {
                refresh(doc);
                if (creating && output.doc && output.exportPath && !inputFileChanged(snapshot)) {
                    output.exportPath = fileReplace(snapshot.path, inputFileRead(*output.exportPath).text);
                }
            }
            catch (const std::exception&) { output.warnings.push_back("The current document could not be exported; check the cloud document before retrying."); }
        }
        if (output.verification == "mismatch") {
            output.summary = creating && output.doc
                ? "Document " + *output.doc + " was created, but its readback did not match the file. Inspect the fresh export before editing further."
                : output.exportPath
                    ? "Every write landed, but the readback did not match the file (see errors). The fresh export at exportPath shows the document now; continue from it."
                    : "Every write landed, but the readback did not match the file (see errors). Export fresh and continue from that.";
        } else if (!creating && !complete) {
            output.summary = output.exportPath && conflict && output.writeState == "none"
                ? "The document changed in the cloud; nothing was sent. Reapply this file's edits to the fresh export at exportPath and import that."
                : output.exportPath
                    ? "Some writes may have landed; the fresh export at exportPath shows the document now. Reapply only the edits it lacks there, and don't import this file again."
                    : output.writeState == "none"
                        ? "No cloud writes were confirmed."
                        : "Cloud work may have landed. Export fresh before reapplying edits, and don't import this file again.";
        }
        return output;
    }
}

inline FileWriteResult fileImport(const ImportInput& input, const ImportServices& services) {
    import_detail::ImportImageUploads uploads{services};
    FileWriteResult output;
    try {
        output = fileImportRun(input, services, uploads);
    } catch (const std::exception& error) {
        const auto* core = dynamic_cast<const model::CoreError*>(&error);
        output.status = core ? "refused" : "failed";
        output.summary = "No cloud writes were confirmed.";
        output.errors = std::vector<ResultError>{{.code = core ? core->code : "operationFailed",
                                                  .message = error.what(), .phase = "validation"}};
    }
    uploads.tracker = nullptr;
    uploads.cleanup(output.warnings);
    return output;
}

} // namespace gdocsmith::engine
