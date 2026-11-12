#pragma once

#include "engine/artifacts.hpp"
#include "engine/export.hpp"
#include "engine/load.hpp"
#include "engine/session.hpp"
#include "lens/document.hpp"
#include "model/document.hpp"
#include "model/errors.hpp"
#include "model/types.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
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
        if (fs.before != es.before || fs.after != es.after) {
            model::problemsThrow({model::FileProblem{
                .code = std::string(model::error_codes::SUGGESTION_CONFLICT),
                .line = fs.line,
                .message = "edit the tab body, not a suggestion block; to reject a suggestion, add reject to its header",
                .tab = std::nullopt,
            }});
        }
    }
}

/**
 * Overwrites the imported file with the fresh export unless it changed during import; returns the export's path.
 */
inline std::string exportPlace(
    const ExportWritten& exported,
    const FileInputSnapshot& input,
    bool inputChanged,
    std::vector<std::string>& warnings
) {
    if (!inputChanged) {
        return fileReplace(input.path, exported.markdown);
    }
    warnings.push_back("The input changed during import, so it was kept; exportPath is the fresh export.");
    return exported.exportPath;
}

/**
 * Imports one document file; every failure states whether writes may have landed.
 */
inline FileWriteResult fileImport(
    const ImportInput& input,
    const FileServices& services = {}
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
    } else {
        docId = "new_doc_" + detail::unique_tmp_suffix();
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

    std::vector<DeletedTab> deletedTabs;
    if (!creating) {
        deletedTabs = tabLinesValidate(file, currentDoc, tabProblems);
    }
    tabTitlesValidate(file, currentDoc, tabProblems);
    model::problemsThrow(tabProblems);

    // Apply markdown to document model
    CoreSession session;
    std::shared_ptr<DocHandle> handle;
    if (creating) {
        handle = session.docCreate("import", file.frontmatter.title);
    } else {
        handle = session.docOpen(docId);
    }

    for (size_t idx = 0; idx < file.tabs.size(); idx++) {
        const auto& sec = file.tabs[idx];
        if (sec.isDelete) continue;
        session.tabBegin(static_cast<int>(idx));

        TabHandle tabHandle(nullptr, nullptr, "");
        if (creating && idx == 0) {
            tabHandle = handle->tab(handle->tabs()[0].tabId);
        } else if (creating) {
            tabHandle = handle->tabCreate(sec.title, sec.parent);
        } else {
            tabHandle = handle->tab(sec.id);
        }
        tabHandle.rename(sec.title);
        tabHandle.writeMarkdown(sec.markdown);
    }

    // Prepare export and result
    auto& docState = session.docs[handle->docId()];
    bool inputChanged = inputFileChanged(snapshot);

    ExportOptions exportOpts;
    for (const auto& c : file.comments) {
        if (c.resolved) {
            exportOpts.commentsIncludeResolved = true;
            break;
        }
    }

    auto written = exportModel(docState.current, {}, services, exportOpts);
    std::string finalPath = exportPlace(written, snapshot, inputChanged, fileNotes);

    FileWriteResult result;
    if (creating) {
        result.doc = handle->docId();
    }
    result.exportPath = finalPath;
    result.status = "applied";
    result.summary = "Document changes applied.";
    result.verification = "match";
    result.writeState = "complete";
    result.warnings = std::move(fileNotes);
    if (!deletedTabs.empty()) {
        result.deletedTabs = std::move(deletedTabs);
    }
    if (written.commentsLine.has_value()) {
        result.commentsLine = written.commentsLine;
    }
    if (written.suggestionsLine.has_value()) {
        result.suggestionsLine = written.suggestionsLine;
    }

    return result;
}

} // namespace gdocsmith::engine
