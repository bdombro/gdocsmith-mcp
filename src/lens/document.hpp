#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <sstream>
#include "json.hpp"
#include "yaml.hpp"
#include "lens/json_helper.hpp"
#include "directives.hpp"
#include "frontmatter.hpp"
#include "comment_serializer.hpp"
#include "suggestion_blocks.hpp"
#include "sections.hpp"

namespace gdocsmith::lens {

struct TabSettings {
    std::string background{"#FFFFFF"};
    double footerMargin{36.0};
    double headerMargin{36.0};
    int pageNumberStart{1};
    bool pageless{false};

    bool operator==(const TabSettings& other) const = default;
};

struct DocumentFileTab {
    std::string id{"t.0"};
    std::string title;
    std::string markdown;
    TabSettings settings;
    std::optional<std::string> parent;
    bool isDelete{false};
    int line{1};
    int bodyLine{1};

    bool operator==(const DocumentFileTab& other) const = default;
};

struct DocumentFile {
    DocumentFrontmatter frontmatter;
    std::vector<DocumentFileTab> tabs;
    std::vector<CommentBlock> comments;
    std::vector<SuggestionBlock> suggestions;

    bool operator==(const DocumentFile& other) const = default;
};

inline DocumentFile documentFileParse(std::string_view markdown) {
    auto split = slimr::parse_frontmatter(markdown);
    if (!split) {
        throw CoreError("unsupportedSyntax", "Invalid markdown frontmatter: " + split.error());
    }

    DocumentFrontmatter df;
    if (split->frontmatter.has_value()) {
        df = frontmatterValidate(*split->frontmatter, "document");
    }

    DocumentFile file;
    file.frontmatter = std::move(df);

    std::string_view body = split->body;

    std::istringstream ss{std::string(body)};
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    static const std::unordered_set<std::string> line_names = {"tab", "comment", "suggestion", "reply"};

    std::vector<DocumentFileTab> tabs;
    std::vector<CommentBlock> comments;
    std::vector<SuggestionBlock> suggestions;

    auto trim = [](std::string_view s) -> std::string {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return std::string(s.substr(first, last - first + 1));
    };

    size_t i = 0;
    while (i < lines.size()) {
        const auto& cur_line = lines[i];

        if (cur_line.find_first_not_of(" \t\r\n") == std::string::npos) {
            i++;
            continue;
        }

        // Check if tab directive
        if (cur_line.starts_with("::tab") || cur_line.starts_with(":::tab")) {
            auto ld = lineDirectiveMatch(cur_line, line_names);
            if (ld && ld->name == "tab") {
                DocumentFileTab tab;
                tab.line = static_cast<int>(i + 1);
                tab.bodyLine = static_cast<int>(i + 2);
                tab.id = ld->attrs.get_value("id").value_or("t." + std::to_string(tabs.size()));
                tab.title = ld->attrs.get_value("title").value_or(
                    ld->attrs.get_value("name").value_or(file.frontmatter.title)
                );
                if (ld->attrs.has_flag("pageless")) tab.settings.pageless = true;
                if (ld->attrs.has_flag("delete")) tab.isDelete = true;
                tab.parent = ld->attrs.get_value("parent");

                i++;
                std::string tab_body;
                while (i < lines.size()) {
                    const auto& sub_line = lines[i];
                    if (sub_line.starts_with("::tab") || sub_line.starts_with(":::tab") ||
                        sub_line.starts_with("::comment") || sub_line.starts_with(":::comment") ||
                        sub_line.starts_with("::suggestion") || sub_line.starts_with(":::suggestion")) {
                        break;
                    }
                    if (!tab_body.empty()) tab_body += "\n";
                    tab_body += sub_line;
                    i++;
                }
                while (!tab_body.empty() && tab_body.back() == '\n') tab_body.pop_back();
                if (!tab_body.empty()) tab_body += "\n";
                tab.markdown = std::move(tab_body);
                tabs.push_back(std::move(tab));
                continue;
            }
        }

        // Check if comment block
        if (cur_line.starts_with(":::comment") || cur_line.starts_with("::comment")) {
            auto ld = lineDirectiveMatch(cur_line, line_names);
            if (ld && ld->name == "comment") {
                CommentBlock cb;
                cb.line = static_cast<int>(i + 1);
                cb.id = ld->attrs.get_value("id").value_or("");
                cb.author = ld->attrs.get_value("author");
                cb.resolved = ld->attrs.has_flag("resolved");
                cb.orphaned = ld->attrs.has_flag("orphaned");
                cb.ambiguous = ld->attrs.has_flag("ambiguous");
                cb.mine = ld->attrs.has_flag("mine");
                cb.isDelete = ld->attrs.has_flag("delete");

                i++;
                std::string c_text;
                std::string quote_text;
                while (i < lines.size()) {
                    const auto& sub_line = lines[i];
                    if (sub_line == ":::" || sub_line.starts_with("::comment") || sub_line.starts_with(":::comment") ||
                        sub_line.starts_with("::suggestion") || sub_line.starts_with(":::suggestion")) {
                        if (sub_line == ":::") i++;
                        break;
                    }
                    if (sub_line.starts_with("::reply")) {
                        auto rld = lineDirectiveMatch(sub_line, line_names);
                        CommentReply rep;
                        rep.line = static_cast<int>(i + 1);
                        if (rld) {
                            rep.author = rld->attrs.get_value("author");
                            rep.id = rld->attrs.get_value("id");
                            rep.mine = rld->attrs.has_flag("mine");
                            rep.isDelete = rld->attrs.has_flag("delete");
                        }
                        i++;
                        std::string rep_text;
                        while (i < lines.size()) {
                            if (lines[i] == ":::" || lines[i].starts_with("::reply") ||
                                lines[i].starts_with(":::comment") || lines[i].starts_with("::comment")) {
                                break;
                            }
                            if (!rep_text.empty()) rep_text += "\n";
                            rep_text += lines[i];
                            i++;
                        }
                        rep.text = trim(rep_text);
                        cb.replies.push_back(std::move(rep));
                        continue;
                    }
                    if (sub_line.starts_with("> ")) {
                        if (!quote_text.empty()) quote_text += "\n";
                        quote_text += sub_line.substr(2);
                    } else if (sub_line == ">") {
                        if (!quote_text.empty()) quote_text += "\n";
                    } else {
                        if (!c_text.empty()) c_text += "\n";
                        c_text += sub_line;
                    }
                    i++;
                }
                if (!quote_text.empty()) cb.quote = trim(quote_text);
                cb.text = trim(c_text);
                comments.push_back(std::move(cb));
                continue;
            }
        }

        // Check if suggestion block
        if (cur_line.starts_with(":::suggestion") || cur_line.starts_with("::suggestion")) {
            auto ld = lineDirectiveMatch(cur_line, line_names);
            if (ld && ld->name == "suggestion") {
                SuggestionBlock sb;
                sb.line = static_cast<int>(i + 1);
                sb.id = ld->attrs.get_value("id").value_or("");
                sb.kind = ld->attrs.get_value("kind").value_or("text");
                sb.segment = ld->attrs.get_value("segment");
                sb.isReject = ld->attrs.has_flag("reject");

                i++;
                std::string before_text;
                std::string after_text;
                std::string* target = nullptr;
                while (i < lines.size()) {
                    const auto& sub_line = lines[i];
                    if (sub_line == ":::") {
                        i++;
                        break;
                    }
                    if (sub_line == "::before") {
                        target = &before_text;
                        i++;
                        continue;
                    }
                    if (sub_line == "::after") {
                        target = &after_text;
                        i++;
                        continue;
                    }
                    if (target) {
                        if (!target->empty()) *target += "\n";
                        *target += sub_line;
                    }
                    i++;
                }
                if (!before_text.empty()) sb.before = trim(before_text);
                if (!after_text.empty()) sb.after = trim(after_text);
                suggestions.push_back(std::move(sb));
                continue;
            }
        }

        i++;
    }

    if (tabs.empty()) {
        DocumentFileTab tab;
        tab.id = "t.0";
        tab.title = file.frontmatter.title.empty() ? "Tab 1" : file.frontmatter.title;
        tab.markdown = std::string(body);
        tabs.push_back(std::move(tab));
    }

    file.tabs = std::move(tabs);
    file.comments = std::move(comments);
    file.suggestions = std::move(suggestions);
    return file;
}

inline std::string documentFileExport(const DocumentFile& file) {
    slimr::Json fm = slimr::json_obj();
    fm["gdocsmith"] = file.frontmatter.gdocsmith;
    if (file.frontmatter.doc) fm["doc"] = *file.frontmatter.doc;
    else fm["doc"] = slimr::Json(nullptr);
    if (file.frontmatter.revision) fm["revision"] = *file.frontmatter.revision;
    else fm["revision"] = slimr::Json(nullptr);
    fm["title"] = file.frontmatter.title;

    std::string out = "---\n" + slimr::to_yaml(fm) + "---\n\n";

    for (size_t i = 0; i < file.tabs.size(); i++) {
        const auto& tab = file.tabs[i];
        out += "::tab{id=" + directiveValueRender(tab.id) + " title=" + directiveValueRender(tab.title);
        if (tab.parent) out += " parent=" + directiveValueRender(*tab.parent);
        if (tab.isDelete) out += " delete";
        if (tab.settings.pageless) out += " pageless";
        out += "}\n\n";
        out += tab.markdown;
        if (!tab.markdown.ends_with('\n')) out += "\n";
        out += "\n";
    }

    for (const auto& c : file.comments) {
        out += commentBlockRender(c) + "\n";
    }

    for (const auto& s : file.suggestions) {
        out += suggestionBlockRender(s) + "\n";
    }

    return out;
}

} // namespace gdocsmith::lens
