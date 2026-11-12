#pragma once

#include "engine/artifacts.hpp"
#include "engine/load.hpp"
#include "lens/document.hpp"
#include "model/document.hpp"
#include "model/errors.hpp"
#include "model/types.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gdocsmith::engine {

struct FileExportResult {
    std::optional<int> commentsLine;
    std::string exportPath;
    std::optional<std::unordered_map<std::string, std::string>> images;
    std::optional<std::string> imagesDir;
    std::optional<int> suggestionsLine;
    std::vector<std::string> warnings;

    bool operator==(const FileExportResult&) const = default;
};

struct ExportOptions {
    bool commentsIncludeResolved{false};
    bool imagesDownload{false};
    std::optional<std::vector<std::string>> imageIds;

    bool operator==(const ExportOptions&) const = default;
};

struct ExportWritten : FileExportResult {
    std::string folderPath;
    std::string markdown;

    bool operator==(const ExportWritten&) const = default;
};

struct FileServices {
    std::string apiUser;
    std::string root;

    bool operator==(const FileServices&) const = default;
};

/** Accepts only a document ID or canonical Google Docs URL. */
inline std::string documentId(std::string_view reference) {
    static const std::regex id_re("^[A-Za-z0-9_-]+$");
    if (std::regex_match(reference.begin(), reference.end(), id_re)) {
        return std::string(reference);
    }
    static const std::regex url_re(R"(^https://docs\.google\.com/document/d/([A-Za-z0-9_-]+)(?:/.*)?$)");
    std::cmatch match;
    std::string ref_str(reference);
    if (std::regex_match(ref_str.c_str(), match, url_re)) {
        return match[1].str();
    }
    throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                          "doc must be a document ID or https://docs.google.com/document/d/... URL");
}

/** A thread's replies as comment blocks show them: not deleted, with text. */
inline std::vector<DriveCommentReply> commentReplies(const DriveComment& comment) {
    std::vector<DriveCommentReply> result;
    for (const auto& rep : comment.replies) {
        if (!rep.deleted.value_or(false) && rep.content.has_value()) {
            size_t first = rep.content->find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                result.push_back(rep);
            }
        }
    }
    return result;
}

/**
 * The comment blocks of a document: unresolved threads (resolved ones too when asked) in document
 * order of their quote's first occurrence; threads with no located quote come last.
 */
inline std::vector<lens::CommentBlock> commentBlocksBuild(
    const model::DocModel& model,
    const std::vector<DriveComment>& comments,
    bool includeResolved,
    std::optional<std::string> plainText = std::nullopt
) {
    auto anchors = commentAnchorsMatch(model, comments, plainText);
    std::unordered_map<std::string, const CommentAnchor*> anchor_map;
    for (const auto& a : anchors) {
        anchor_map[a.commentId] = &a;
    }

    std::unordered_map<std::string, int> tab_order;
    for (size_t i = 0; i < model.tabs.size(); i++) {
        tab_order[model.tabs[i].tabId] = static_cast<int>(i);
    }

    struct Item {
        lens::CommentBlock block;
        int tabIndex{1000000};
        int rangeStart{1000000};
        size_t originalOrder{0};
    };

    std::vector<Item> items;

    for (size_t order = 0; order < comments.size(); order++) {
        const auto& comment = comments[order];
        if (comment.deleted.value_or(false)) continue;
        if (!includeResolved && comment.resolved.value_or(false)) continue;

        lens::CommentBlock cb;
        cb.id = comment.id;
        cb.text = comment.content.value_or("");
        cb.resolved = comment.resolved.value_or(false);
        if (comment.author) {
            cb.author = comment.author->displayName;
            cb.mine = comment.author->me.value_or(false);
        }

        auto it = anchor_map.find(comment.id);
        const CommentAnchor* anchor = (it != anchor_map.end()) ? it->second : nullptr;

        int t_idx = 1000000;
        int r_start = 1000000;

        if (comment.quotedFileContent) {
            cb.quote = comment.quotedFileContent->value;
            if (!anchor || anchor->occurrences.empty()) {
                cb.orphaned = true;
            }
        } else if (comment.quotedText) {
            cb.quote = *comment.quotedText;
            if (!anchor || anchor->occurrences.empty()) {
                cb.orphaned = true;
            }
        }

        if (anchor) {
            cb.ambiguous = anchor->ambiguous;
            if (!anchor->occurrences.empty()) {
                const auto& first = anchor->occurrences[0];
                auto t_it = tab_order.find(first.tabId);
                if (t_it != tab_order.end()) t_idx = t_it->second;
                r_start = first.range.start;
            }
        }

        for (const auto& rep : commentReplies(comment)) {
            lens::CommentReply cr;
            cr.id = rep.id;
            cr.text = rep.content.value_or("");
            if (rep.author) {
                cr.author = rep.author->displayName;
                cr.mine = rep.author->me.value_or(false);
            }
            cb.replies.push_back(std::move(cr));
        }

        items.push_back(Item{
            .block = std::move(cb),
            .tabIndex = t_idx,
            .rangeStart = r_start,
            .originalOrder = order,
        });
    }

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.tabIndex != b.tabIndex) return a.tabIndex < b.tabIndex;
        if (a.rangeStart != b.rangeStart) return a.rangeStart < b.rangeStart;
        return a.originalOrder < b.originalOrder;
    });

    std::vector<lens::CommentBlock> result;
    result.reserve(items.size());
    for (auto& item : items) {
        result.push_back(std::move(item.block));
    }
    return result;
}

/** Writes an already fetched snapshot without introducing an extra cloud read. */
inline ExportWritten exportModel(
    const model::DocModel& model,
    const std::vector<DriveComment>& comments,
    const FileServices& services,
    const ExportOptions& options = {},
    std::optional<std::string> plainText = std::nullopt
) {
    lens::DocumentFile docFile;
    docFile.frontmatter.doc = model.docId;
    docFile.frontmatter.revision = model.revisionId;
    docFile.frontmatter.title = model.title;
    docFile.frontmatter.gdocsmith = lens::GDOCSMITH_VERSION;

    for (const auto& tab : model.tabs) {
        lens::DocumentFileTab dft;
        dft.id = tab.tabId;
        dft.title = tab.title;
        // Construct markdown from paragraphs
        std::string md;
        for (const auto& block : tab.blocks) {
            if (block.is_paragraph()) {
                std::string para_text;
                for (const auto& in : block.as_paragraph().inlines) {
                    if (in.is_text()) {
                        para_text += in.text;
                    }
                }
                if (!para_text.empty()) {
                    if (!md.empty()) {
                        if (!md.ends_with("\n\n")) {
                            if (!md.ends_with('\n')) md += "\n\n";
                            else md += "\n";
                        }
                    }
                    md += para_text;
                }
            }
        }
        if (!md.empty() && !md.ends_with('\n')) md += "\n";
        dft.markdown = std::move(md);
        docFile.tabs.push_back(std::move(dft));
    }

    docFile.comments = commentBlocksBuild(model, comments, options.commentsIncludeResolved, plainText);

    std::string markdown = lens::documentFileExport(docFile);

    std::string rootDir = services.root.empty() ? artifactRoot() : services.root;
    auto folder = exportFolderWrite(rootDir, model.docId, model.revisionId.value_or(""), markdown);

    std::vector<std::string> warnings = folder.warnings;
    if (!model.revisionId.has_value() || model.revisionId->empty()) {
        warnings.push_back("This export has no writable revision; it is read-only for this document.");
    }

    ExportWritten written;
    written.exportPath = folder.exportFilePath;
    written.folderPath = std::filesystem::path(folder.exportFilePath).parent_path().string();
    written.markdown = markdown;
    written.warnings = std::move(warnings);

    std::istringstream ss{markdown};
    std::string line;
    int line_num = 1;
    while (std::getline(ss, line)) {
        if (line.starts_with(":::comment") && !written.commentsLine) {
            written.commentsLine = line_num;
        } else if (line.starts_with(":::suggestion") && !written.suggestionsLine) {
            written.suggestionsLine = line_num;
        }
        line_num++;
    }

    return written;
}

} // namespace gdocsmith::engine
