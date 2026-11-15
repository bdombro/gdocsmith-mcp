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
#include "model/types.hpp"
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

struct DocumentFileSegment {
    std::string kind;
    std::string type{"default"};
    std::optional<int> section;
    std::string markdown;
    bool isDelete{false};
    bool operator==(const DocumentFileSegment&) const = default;
};

struct DocumentFileFootnote {
    std::string label;
    std::string markdown;
    bool operator==(const DocumentFileFootnote&) const = default;
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
    // File-absolute line of the first line of the final `markdown`
    int markdownLine{1};
    DirectiveAttrs readonlyAttributes;
    std::vector<DocumentFileSegment> segments;
    std::vector<DocumentFileFootnote> footnotes;
    std::optional<SectionSettings> openingSettings;

    bool operator==(const DocumentFileTab& other) const = default;
};

inline bool tabReadonlyEqual(const DocumentFileTab& a, const DocumentFileTab& b) {
    return a.settings == b.settings && a.readonlyAttributes == b.readonlyAttributes &&
           a.segments == b.segments && a.footnotes == b.footnotes && a.openingSettings == b.openingSettings;
}

inline bool frontmatterReadonlyEqual(const DocumentFrontmatter& a, const DocumentFrontmatter& b) {
    return a.textStyles == b.textStyles && a.paragraphStyles == b.paragraphStyles &&
           a.cellStyles == b.cellStyles && a.namedStyleSets == b.namedStyleSets &&
           a.styleBaseline == b.styleBaseline && a.variables == b.variables && a.guards == b.guards;
}

struct DocumentFile {
    DocumentFrontmatter frontmatter;
    std::vector<DocumentFileTab> tabs;
    std::vector<CommentBlock> comments;
    std::vector<SuggestionBlock> suggestions;

    bool operator==(const DocumentFile& other) const = default;
};

inline void tabAttributesValidate(const DirectiveAttrs& attrs) {
    const std::set<std::string> flags = {"delete", "firstPage", "landscape", "oddEven", "pageful", "pageless"};
    const std::set<std::string> values = {"background", "emoji", "footerMargin", "headerMargin", "id",
        "margins", "namedStyleSet", "pageNumberStart", "parent", "size", "title", "name"};
    if (!attrs.classes.empty()) throw CoreError("unsupportedSyntax", "Tab lines do not take style classes.");
    for (const auto& flag : attrs.flags) if (!flags.contains(flag)) {
        throw CoreError("unsupportedSyntax", "Unknown tab flag: " + flag);
    }
    for (const auto& [key, value] : attrs.values) {
        (void)value;
        if (!values.contains(key)) throw CoreError("unsupportedSyntax", "Unknown tab attribute: " + key);
    }
    if (attrs.has_flag("pageful") && attrs.has_flag("pageless")) throw CoreError("unsupportedSyntax", "Conflicting page modes.");
    const auto number = [](const std::string& value, bool positive) {
        size_t end = 0;
        double result = 0;
        try { result = std::stod(value, &end); }
        catch (const std::exception&) { throw CoreError("unsupportedSyntax", "Invalid page setting number."); }
        if (end != value.size() || !std::isfinite(result) || (positive ? result <= 0 : result < 0)) {
            throw CoreError("unsupportedSyntax", "Invalid page setting number.");
        }
        return result;
    };
    for (const auto* key : {"footerMargin", "headerMargin", "pageNumberStart"}) if (const auto value = attrs.get_value(key)) {
        const auto parsed = number(*value, false);
        if (std::string_view(key) == "pageNumberStart" &&
            (parsed != std::floor(parsed) || parsed > std::numeric_limits<int>::max())) {
            throw CoreError("unsupportedSyntax", "Page number must be a whole number.");
        }
    }
    if (const auto color = attrs.get_value("background"); color && !isValidHexColor(*color)) {
        throw CoreError("unsupportedSyntax", "Background must be #RRGGBB.");
    }
    if (!attrs.has_flag("pageful") &&
        (attrs.has_flag("landscape") || attrs.has_value("size") || attrs.has_value("margins"))) {
        throw CoreError("unsupportedSyntax", "Page size, orientation and margins require pageful.");
    }
    if (const auto size = attrs.get_value("size")) {
        const auto cross = size->find('x');
        if (cross != std::string::npos) {
            if (attrs.has_flag("landscape")) throw CoreError("unsupportedSyntax", "Landscape applies only to named sizes.");
            number(size->substr(0, cross), true);
            number(size->substr(cross + 1), true);
        } else if (*size != "LETTER" && *size != "LEGAL" && *size != "TABLOID" && *size != "A4" && *size != "A3" && *size != "A5") {
            throw CoreError("unsupportedSyntax", "Unknown page size.");
        }
    }
    if (attrs.has_value("margins")) sectionSettingsParse({}, {{"margins", *attrs.get_value("margins")}});
}

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
    // Frontmatter lines before the body, so parsed lines count over the whole file
    int fmLines{0};
    if (markdown.ends_with(body)) {
        const auto prefix = markdown.substr(0, markdown.size() - body.size());
        fmLines = static_cast<int>(std::count(prefix.begin(), prefix.end(), '\n'));
    } else if (split->frontmatter) {
        fmLines = static_cast<int>(std::count(split->raw_frontmatter.begin(), split->raw_frontmatter.end(), '\n')) + 3;
    }

    std::istringstream ss{std::string(body)};
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    static const std::unordered_set<std::string> line_names = {"tab", "comment", "suggestion", "reply", "header", "footer"};

    std::vector<DocumentFileTab> tabs;
    std::vector<CommentBlock> comments;
    std::vector<SuggestionBlock> suggestions;

    auto trim = [](std::string_view s) -> std::string {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return std::string(s.substr(first, last - first + 1));
    };
    auto tabDirectiveLine = [](std::string_view text) {
        for (const std::string_view prefix : {"::tab", ":::tab"}) {
            if (text.starts_with(prefix) && (text.size() == prefix.size() ||
                text[prefix.size()] == '{' || text[prefix.size()] == '[' ||
                std::isspace(static_cast<unsigned char>(text[prefix.size()])))) return true;
        }
        return false;
    };

    size_t i = 0;
    while (i < lines.size()) {
        const auto& cur_line = lines[i];

        if (cur_line.find_first_not_of(" \t\r\n") == std::string::npos) {
            i++;
            continue;
        }

        // Check if tab directive
        if (tabDirectiveLine(cur_line)) {
            auto ld = lineDirectiveMatch(cur_line, line_names);
            if (ld && ld->name == "tab") {
                tabAttributesValidate(ld->attrs);
                DocumentFileTab tab;
                tab.line = fmLines + static_cast<int>(i + 1);
                tab.bodyLine = fmLines + static_cast<int>(i + 2);
                tab.markdownLine = tab.bodyLine;
                tab.id = ld->attrs.get_value("id").value_or("");
                tab.title = ld->attrs.get_value("title").value_or(
                    ld->attrs.get_value("name").value_or(file.frontmatter.title)
                );
                if (ld->attrs.has_flag("pageless")) tab.settings.pageless = true;
                if (const auto value = ld->attrs.get_value("background")) tab.settings.background = uppercaseHex(*value);
                if (const auto value = ld->attrs.get_value("footerMargin")) tab.settings.footerMargin = std::stod(*value);
                if (const auto value = ld->attrs.get_value("headerMargin")) tab.settings.headerMargin = std::stod(*value);
                if (const auto value = ld->attrs.get_value("pageNumberStart")) tab.settings.pageNumberStart = static_cast<int>(std::stod(*value));
                if (ld->attrs.has_flag("delete")) tab.isDelete = true;
                tab.parent = ld->attrs.get_value("parent");
                for (const auto& [key, value] : ld->attrs.values) {
                    if (key != "id" && key != "title" && key != "name" && key != "parent") {
                        tab.readonlyAttributes.values.emplace_back(key, value);
                    }
                }
                for (const auto& flag : ld->attrs.flags) {
                    if (flag != "delete") tab.readonlyAttributes.flags.push_back(flag);
                }
                tab.readonlyAttributes.classes = ld->attrs.classes;

                i++;
                std::string tab_body;
                while (i < lines.size()) {
                    const auto& sub_line = lines[i];
                    if (tabDirectiveLine(sub_line) ||
                        sub_line.starts_with("::comment") || sub_line.starts_with(":::comment") ||
                        sub_line.starts_with("::suggestion") || sub_line.starts_with(":::suggestion") ||
                        sub_line.starts_with("::header") || sub_line.starts_with("::footer")) {
                        break;
                    }
                    const bool bodyEmpty = tab_body.empty();
                    if (file.frontmatter.sourceVersion) {
                        if (!tab_body.empty() || !sub_line.empty()) tab_body += sub_line + "\n";
                    } else {
                        if (!tab_body.empty()) tab_body += "\n";
                        tab_body += sub_line;
                    }
                    if (bodyEmpty && !tab_body.empty()) tab.markdownLine = fmLines + static_cast<int>(i + 1);
                    i++;
                }
                if (!file.frontmatter.sourceVersion) {
                    while (!tab_body.empty() && tab_body.back() == '\n') tab_body.pop_back();
                    if (!tab_body.empty()) tab_body += "\n";
                }
                tab.markdown = std::move(tab_body);
                if (!tab.id.empty() && std::any_of(tabs.begin(), tabs.end(), [&](const auto& other) { return other.id == tab.id; })) {
                    throw CoreError("unsupportedSyntax", "A tab identity occurs more than once.");
                }
                tabs.push_back(std::move(tab));
                continue;
            }
        }

        if (cur_line.starts_with("::header") || cur_line.starts_with("::footer")) {
            const auto ld = lineDirectiveMatch(cur_line, line_names);
            if (!ld || tabs.empty()) throw CoreError("unsupportedSyntax", "Header/footer must follow a tab.");
            if (!ld->attrs.classes.empty() ||
                std::any_of(ld->attrs.flags.begin(), ld->attrs.flags.end(), [](const auto& flag) { return flag != "delete"; }) ||
                std::any_of(ld->attrs.values.begin(), ld->attrs.values.end(), [](const auto& pair) {
                    return pair.first != "kind" && pair.first != "section";
                })) throw CoreError("unsupportedSyntax", "Unsupported header/footer attributes.");
            DocumentFileSegment segment;
            segment.kind = ld->name;
            const auto type = ld->attrs.get_value("kind").value_or("default");
            segment.type = type == "firstPage" ? "first" : type == "evenPage" ? "even" : type;
            if (segment.type != "first" && segment.type != "even" && segment.type != "default") {
                throw CoreError("unsupportedSyntax", "Invalid header/footer kind.");
            }
            if (auto section = ld->attrs.get_value("section")) {
                try {
                    size_t consumed = 0;
                    segment.section = std::stoi(*section, &consumed);
                    if (consumed != section->size() || *segment.section < 2) {
                        throw CoreError("unsupportedSyntax", "Invalid segment section.");
                    }
                }
                catch (const std::exception&) { throw CoreError("unsupportedSyntax", "Invalid segment section."); }
            }
            segment.isDelete = ld->attrs.has_flag("delete");
            ++i;
            while (i < lines.size()) {
                const auto& text = lines[i];
                if (tabDirectiveLine(text) ||
                    text.starts_with("::header") || text.starts_with("::footer") ||
                    text.starts_with("::comment") || text.starts_with(":::comment") ||
                    text.starts_with("::suggestion") || text.starts_with(":::suggestion")) break;
                segment.markdown += text + "\n";
                ++i;
            }
            segment.markdown = suggestionSnippetTrim(segment.markdown);
            if (!segment.markdown.empty()) segment.markdown += "\n";
            tabs.back().segments.push_back(std::move(segment));
            continue;
        }

        // Check if comment block
        if (cur_line.starts_with(":::comment") || cur_line.starts_with("::comment")) {
            auto ld = lineDirectiveMatch(cur_line, line_names);
            if (ld && ld->name == "comment") {
                CommentBlock cb;
                cb.line = fmLines + static_cast<int>(i + 1);
                cb.id = ld->attrs.get_value("id").value_or("");
                cb.author = ld->attrs.get_value("author");
                cb.quote = ld->attrs.get_value("quote");
                cb.resolved = ld->attrs.has_flag("resolved");
                cb.orphaned = ld->attrs.has_flag("orphaned");
                cb.ambiguous = ld->attrs.has_flag("ambiguous");
                cb.mine = ld->attrs.has_flag("mine");
                cb.isDelete = ld->attrs.has_flag("delete");

                i++;
                std::string c_text;
                std::string quote_text;
                auto decoded = [&](std::string value) {
                    if (file.frontmatter.sourceVersion && value.starts_with('\\')) {
                        const auto first = value.find_first_not_of('\\');
                        if (first != std::string::npos && value.substr(first).starts_with("::")) value.erase(0, 1);
                    }
                    return value;
                };
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
                        rep.line = fmLines + static_cast<int>(i + 1);
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
                                lines[i].starts_with(":::comment") || lines[i].starts_with("::comment") ||
                                lines[i].starts_with(":::suggestion") || lines[i].starts_with("::suggestion")) {
                                break;
                            }
                            if (!rep_text.empty()) rep_text += "\n";
                            rep_text += decoded(lines[i]);
                            i++;
                        }
                        rep.text = trim(rep_text);
                        cb.replies.push_back(std::move(rep));
                        continue;
                    }
                    if (!file.frontmatter.sourceVersion && sub_line.starts_with("> ")) {
                        if (!quote_text.empty()) quote_text += "\n";
                        quote_text += sub_line.substr(2);
                    } else if (!file.frontmatter.sourceVersion && sub_line == ">") {
                        if (!quote_text.empty()) quote_text += "\n";
                    } else {
                        if (!c_text.empty()) c_text += "\n";
                        c_text += decoded(sub_line);
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
                sb.line = fmLines + static_cast<int>(i + 1);
                sb.id = ld->attrs.get_value("id").value_or("");
                sb.kind = ld->attrs.get_value("kind").value_or("text");
                sb.segment = ld->attrs.get_value("segment");
                sb.isReject = ld->attrs.has_flag("reject");

                i++;
                std::string before_text;
                std::string after_text;
                std::string notice;
                std::string* target = nullptr;
                while (i < lines.size()) {
                    const auto& sub_line = lines[i];
                    if (sub_line.starts_with("::suggestion") || sub_line.starts_with(":::suggestion")) break;
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
                    } else {
                        if (!notice.empty()) notice += "\n";
                        notice += sub_line;
                    }
                    i++;
                }
                if (!before_text.empty()) sb.before = suggestionSnippetTrim(before_text);
                if (!after_text.empty()) sb.after = suggestionSnippetTrim(after_text);
                if (!notice.empty()) sb.notice = trim(notice);
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
        tab.markdownLine = fmLines + 1;
        tabs.push_back(std::move(tab));
    }

    file.tabs = std::move(tabs);
    for (auto& tab : file.tabs) {
        const bool sourceEnvelope = file.frontmatter.sourceVersion && split->frontmatter &&
            split->frontmatter->contains("gdocsmith") && (*split->frontmatter)["gdocsmith"].is_string();
        const bool existing = file.frontmatter.doc.has_value() && !file.frontmatter.doc->empty() &&
            !tab.id.empty() && !tab.isDelete;
        if (sourceEnvelope && !tab.isDelete) {
            std::istringstream input(tab.markdown);
            std::string first;
            while (std::getline(input, first) && trim(first).empty()) {}
            static const std::unordered_set<std::string> sectionNames = {"section"};
            const auto opening = lineDirectiveMatch(first, sectionNames);
            if (!opening || opening->label || opening->attrs.get_value("id") != std::optional<std::string>{"s1"}) {
                if (existing) {
                    throw CoreError("unsupportedSyntax", "An existing tab starts with its opening ::section{id=s1} line.");
                }
            } else {
                if (!opening->attrs.classes.empty()) throw CoreError("unsupportedSyntax", "An opening section takes no style class.");
                for (const auto& flag : opening->attrs.flags) if (flag != "columnSeparator" && flag != "rtl") {
                    throw CoreError("unsupportedSyntax", "Unknown opening section flag: " + flag);
                }
                for (const auto& [key, value] : opening->attrs.values) {
                    (void)value;
                    if (key != "id" && key != "columns" && key != "columnGap") {
                        throw CoreError("unsupportedSyntax", "Unknown opening section attribute: " + key);
                    }
                }
                tab.openingSettings = sectionSettingsParse(opening->attrs.flags, opening->attrs.values);
                const auto openingAt = tab.markdown.find(first);
                tab.markdown.erase(openingAt, first.size());
                while (std::getline(input, first)) {
                    if (const auto directive = lineDirectiveMatch(first, sectionNames);
                        directive && directive->attrs.get_value("id") == std::optional<std::string>{"s1"}) {
                        throw CoreError("unsupportedSyntax", "The opening section must be the tab's first line, once.");
                    }
                }
            }
        }
        std::istringstream input(tab.markdown);
        std::vector<std::string> bodyLines;
        while (std::getline(input, line)) bodyLines.push_back(line);
        std::string body;
        for (size_t at = 0; at < bodyLines.size();) {
            const auto& text = bodyLines[at];
            const auto close = text.starts_with("[^") ? text.find("]:") : std::string::npos;
            if (close == std::string::npos) { body += text + "\n"; ++at; continue; }
            DocumentFileFootnote footnote;
            footnote.label = text.substr(2, close - 2);
            footnote.markdown = text.substr(close + 2);
            if (footnote.markdown.starts_with(' ')) footnote.markdown.erase(0, 1);
            footnote.markdown += "\n";
            ++at;
            while (at < bodyLines.size() && (bodyLines[at].empty() || bodyLines[at].starts_with("    "))) {
                footnote.markdown += (bodyLines[at].empty() ? "" : bodyLines[at].substr(4)) + "\n";
                ++at;
            }
            footnote.markdown = suggestionSnippetTrim(footnote.markdown);
            if (!footnote.markdown.empty()) footnote.markdown += "\n";
            tab.footnotes.push_back(std::move(footnote));
        }
        if (sourceEnvelope && tab.footnotes.empty()) tab.markdown = std::move(body);
        else {
            // Leading newlines trimmed here move the first markdown line down
            const auto firstText = body.find_first_not_of('\n');
            if (firstText != std::string::npos) tab.markdownLine += static_cast<int>(firstText);
            tab.markdown = suggestionSnippetTrim(body);
            if (!tab.markdown.empty()) tab.markdown += "\n";
        }
    }
    file.comments = std::move(comments);
    file.suggestions = std::move(suggestions);
    return file;
}

template<class T>
inline slimr::Json declarationJson(const T& value) {
    auto parsed = slimr::parse_json<slimr::Json>(slimr::write_json(value));
    if (!parsed) throw CoreError("internal", "Cannot serialize style declaration.");
    if (parsed->is_object()) {
        std::vector<std::string> empty;
        for (const auto& [key, entry] : parsed->template get<slimr::Json::object_t>()) {
            if (entry.is_null()) empty.push_back(key);
        }
        for (const auto& key : empty) parsed->template get<slimr::Json::object_t>().erase(key);
        parsed->template get<slimr::Json::object_t>().erase("resets");
        for (const auto& key : value.resets) (*parsed)[key] = nullptr;
    }
    return *parsed;
}

inline std::string documentFileExport(const DocumentFile& file) {
    slimr::Json fm = slimr::json_obj();
    fm["gdocsmith"] = file.frontmatter.sourceVersion
        ? slimr::Json(*file.frontmatter.sourceVersion) : slimr::Json(file.frontmatter.gdocsmith);
    if (file.frontmatter.doc) fm["doc"] = *file.frontmatter.doc;
    else fm["doc"] = slimr::Json(nullptr);
    if (file.frontmatter.revision) fm["revision"] = *file.frontmatter.revision;
    else fm["revision"] = slimr::Json(nullptr);
    fm["title"] = file.frontmatter.title;
    if (file.frontmatter.apiUser && !file.frontmatter.apiUser->empty()) {
        fm["apiUser"] = *file.frontmatter.apiUser;
    }
    for (const auto& [name, value] : file.frontmatter.textStyles) fm["textStyles"][name] = declarationJson(value);
    for (const auto& [name, value] : file.frontmatter.paragraphStyles) fm["paragraphStyles"][name] = declarationJson(value);
    for (const auto& [name, value] : file.frontmatter.cellStyles) fm["cellStyles"][name] = declarationJson(value);
    if (file.frontmatter.namedStyleSets.is_object() && !file.frontmatter.namedStyleSets.get<slimr::Json::object_t>().empty()) {
        fm["namedStyleSets"] = file.frontmatter.namedStyleSets;
    }
    if (file.frontmatter.dropdowns.is_object() && !file.frontmatter.dropdowns.get<slimr::Json::object_t>().empty()) {
        fm["dropdowns"] = file.frontmatter.dropdowns;
    }
    if (file.frontmatter.styleBaseline) fm["styleBaseline"] = *file.frontmatter.styleBaseline;
    if (file.frontmatter.guards) fm["guards"] = *file.frontmatter.guards;
    for (const auto& [key, value] : file.frontmatter.variables) fm["variables"][key] = value;

    std::string out = "---\n" + slimr::to_yaml(fm) + (file.frontmatter.sourceVersion ? "---\n" : "---\n\n");

    std::vector<std::string> sections;
    std::vector<int> sectionStarts;
    for (size_t i = 0; i < file.tabs.size(); i++) {
        const auto& tab = file.tabs[i];
        const auto sectionOffset = out.size();
        sectionStarts.push_back(static_cast<int>(std::count(out.begin(),out.end(),'\n')) + 1);
        out += "::tab{id=" + directiveValueRender(tab.id) + " title=" + directiveValueRender(tab.title);
        if (tab.parent) out += " parent=" + directiveValueRender(*tab.parent);
        if (tab.isDelete) out += " delete";
        if (tab.settings.pageless && !tab.readonlyAttributes.has_flag("pageless")) out += " pageless";
        const auto attributes = directiveAttrsRender(tab.readonlyAttributes);
        if (attributes.size() > 2) out += " " + attributes.substr(1, attributes.size() - 2);
        out += "}\n\n";
        if (tab.openingSettings) {
            auto opening = sectionSettingsAttrs(*tab.openingSettings);
            opening.values.emplace_back("id", "s1");
            out += "::section" + directiveAttrsRender(opening);
            if (!tab.markdown.starts_with('\n')) out += "\n\n";
        }
        out += tab.markdown;
        if (!tab.markdown.ends_with('\n')) out += "\n";
        if (!file.frontmatter.sourceVersion ||
            (!tab.openingSettings && (i + 1 < file.tabs.size() || !file.comments.empty() || !file.suggestions.empty())) ||
            !tab.footnotes.empty() || !tab.segments.empty()) out += "\n";
        for (const auto& footnote : tab.footnotes) {
            std::istringstream content(footnote.markdown);
            std::string text;
            bool first = true;
            out += "[^" + footnote.label + "]:";
            while (std::getline(content, text)) {
                out += first ? " " : (text.empty() ? "" : "    ");
                out += text + "\n";
                first = false;
            }
            if (first) out += "\n";
            out += "\n";
        }
        for (const auto& segment : tab.segments) {
            out += "::" + segment.kind + "{kind=" +
                (segment.type == "first" ? "firstPage" : segment.type == "even" ? "evenPage" : "default");
            if (segment.section) out += " section=" + std::to_string(*segment.section);
            if (segment.isDelete) out += " delete";
            out += "}\n\n" + segment.markdown;
            if (!segment.markdown.ends_with('\n')) out += "\n";
            out += "\n";
        }
        sections.push_back(out.substr(sectionOffset));
    }

    for (const auto& c : file.comments) {
        out += commentBlockRender(c, !file.frontmatter.sourceVersion) + "\n";
    }

    for (size_t i=0;i<file.suggestions.size();++i) {
        const auto& s=file.suggestions[i];
        if (!file.frontmatter.sourceVersion) {
            out += suggestionBlockRender(s,true) + "\n";
            continue;
        }
        const auto index=static_cast<size_t>(s.tabIndex);
        std::optional<int> hint;
        std::optional<std::string> range;
        std::string title;
        if (index<file.tabs.size()) {
            title=file.tabs[index].title;
            const auto marker=":suggestion{id="+s.id;
            const bool inlineKind=s.kind=="text" || s.kind=="textStyle" || s.kind=="object";
            const bool marked=sections[index].find(marker+" ")!=std::string::npos ||
                              sections[index].find(marker+"}")!=std::string::npos;
            if (!inlineKind || !marked) {
                std::vector<std::string> context;
                std::istringstream input(suggestionSnippetTrim(s.before.value_or("")));
                std::string line,probe;
                while (std::getline(input,line)) {
                    context.push_back(line);
                    line=std::regex_replace(line,std::regex(R"(:change\[((?:[^\]\\]|\\.)*)\])"),"$1");
                    if (line.starts_with('\\')) line.erase(0,1);
                    const auto first=line.find_first_not_of(" \t\r\n");
                    line=first==std::string::npos ? std::string{} : line.substr(first,line.find_last_not_of(" \t\r\n")-first+1);
                    if (probe.empty() && !line.empty()) probe=line;
                }
                int at=0,n=0;
                std::istringstream section(sections[index]);
                while (std::getline(section,line)) {
                    line=std::regex_replace(line,std::regex(R"(:(?:comment|namedrange|suggestion)\{[^}\n]*\})"),"");
                    if (n>0 && !probe.empty() && line.find(probe)!=std::string::npos) { at=n; break; }
                    ++n;
                }
                hint=sectionStarts[index]+at;
                if (context.size()>1 && at>0) range=std::to_string(*hint)+"-"+std::to_string(*hint+context.size()-1);
            }
        }
        if (i>0) out+="\n";
        out += suggestionBlockRender(s,false,title,hint,range);
    }

    return out;
}

inline std::string documentTabMarkdown(const DocumentFile& file, const DocumentFileTab& tab,
                                      const model::DocModel&, const std::string&) {
    slimr::Json declarations = slimr::json_obj();
    for (const auto& [name, value] : file.frontmatter.textStyles) declarations["textStyles"][name] = declarationJson(value);
    for (const auto& [name, value] : file.frontmatter.paragraphStyles) declarations["paragraphStyles"][name] = declarationJson(value);
    for (const auto& [name, value] : file.frontmatter.cellStyles) declarations["cellStyles"][name] = declarationJson(value);
    std::string body;
    if (tab.openingSettings) {
        auto opening = sectionSettingsAttrs(*tab.openingSettings);
        opening.values.emplace_back("id", "s1");
        body = "::section" + directiveAttrsRender(opening);
        if (!tab.markdown.starts_with('\n')) body += "\n\n";
    }
    body += tab.markdown;
    if (declarations.get<slimr::Json::object_t>().empty()) return body;
    return "---\n" + slimr::to_yaml(declarations) + "---\n\n" + body;
}

} // namespace gdocsmith::lens
