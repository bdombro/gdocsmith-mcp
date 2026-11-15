#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include "tokens.hpp"
#include "directives.hpp"
#include "tables.hpp"
#include "cell_refs.hpp"
#include "review_markers.hpp"
#include "text_styles.hpp"
#include "frontmatter.hpp"
#include "model/lists.hpp"

namespace gdocsmith::lens {

inline std::vector<ProjSpan> parseInlines(std::string_view text) {
    std::vector<ProjSpan> spans;
    size_t i = 0;
    std::string current;

    auto flush_text = [&]() {
        if (!current.empty()) {
            ProjSpan s;
            s.kind = "text";
            s.text = std::move(current);
            spans.push_back(std::move(s));
            current.clear();
        }
    };

    static const std::unordered_set<std::string> token_names = {
        "image", "person", "date", "richlink", "richLink", "chip", "footnoteRef",
        "pagebreak", "section", "toc", "horizontalRule", "columnBreak", "equation",
        "dropdown", "autoText", "namedrange"
    };
    static const std::unordered_set<std::string> style_names = {"style"};

    while (i < text.size()) {
        // Escapes
        if (text[i] == '\\' && i + 1 < text.size()) {
            current += text[i + 1];
            i += 2;
            continue;
        }

        // Inline image: ![alt](url)
        if (text[i] == '!' && i + 1 < text.size() && text[i + 1] == '[') {
            size_t close_b = text.find(']', i + 2);
            if (close_b != std::string_view::npos && close_b + 1 < text.size() && text[close_b + 1] == '(') {
                size_t close_p = text.find(')', close_b + 2);
                if (close_p != std::string_view::npos) {
                    flush_text();
                    std::string alt(text.substr(i + 2, close_b - (i + 2)));
                    std::string url(text.substr(close_b + 2, close_p - (close_b + 2)));
                    TokenRef tr;
                    tr.kind = "image";
                    tr.label = alt;
                    tr.attrs.values.push_back({"src", url});

                    size_t next = close_p + 1;
                    if (next < text.size() && text[next] == '{') {
                        int end = braceEnd(text, next);
                        if (end > 0) {
                            try {
                                auto parsed_attrs = directiveAttrsParse(text.substr(next + 1, end - (next + 1)));
                                for (const auto& [k, v] : parsed_attrs.values) tr.attrs.values.push_back({k, v});
                                for (const auto& f : parsed_attrs.flags) tr.attrs.flags.push_back(f);
                                next = end + 1;
                            } catch (...) {}
                        }
                    }

                    ProjSpan s;
                    s.kind = "token";
                    s.token = std::move(tr);
                    spans.push_back(std::move(s));
                    i = next;
                    continue;
                }
            }
        }

        // Style directive: :style[text]{.name}
        if (text[i] == ':' && text.substr(i).starts_with(":style[")) {
            auto match = inlineDirectiveMatch(text.substr(i), style_names);
            if (match && match->label.has_value()) {
                flush_text();
                ProjSpan s;
                s.kind = "text";
                for (const auto& part : parseInlines(*match->label)) {
                    if (part.kind != "text" || !part.marks.empty() || part.directive) {
                        throw CoreError("unsupportedSyntax", ":style labels must contain plain text.");
                    }
                    s.text += part.text;
                }
                if (!match->attrs.classes.empty()) s.directive = match->attrs.classes[0];
                for (const auto& [name, value] : match->attrs.values) {
                    if (name == "fontFamily" || name == "fontWeight" || name == "fontSize" ||
                        name == "foregroundColor" || name == "backgroundColor" || name == "baselineOffset") {
                        s.formatting.values.emplace_back(name, value);
                        continue;
                    }
                    if (name == "link") {
                        s.marks.link = value;
                        continue;
                    }
                    auto checkedStyle = model::make_json_object();
                    textStyleApply(checkedStyle, s.formatting);
                    if (value != "true" && value != "false") {
                        throw CoreError("unsupportedSyntax", ":style marks must be true or false.");
                    }
                    const bool enabled = value == "true";
                    if (name == "bold") s.marks.bold = enabled;
                    else if (name == "italic") s.marks.italic = enabled;
                    else if (name == "underline") s.marks.underline = enabled;
                    else if (name == "strikethrough") s.marks.strikethrough = enabled;
                    else if (name == "smallCaps") s.marks.smallCaps = enabled;
                    else if (name == "code") s.marks.code = enabled;
                    else throw CoreError("unsupportedSyntax", "Unknown :style mark: " + name);
                }
                spans.push_back(std::move(s));
                i += match->raw.size();
                continue;
            }
        }

        // Other inline directives / tokens: :name[label]{attrs} or :name{attrs}
        if (text[i] == ':') {
            auto match = inlineDirectiveMatch(text.substr(i), token_names);
            if (match) {
                flush_text();
                TokenRef tr;
                tr.kind = match->name;
                tr.label = match->label;
                tr.attrs = match->attrs;
                ProjSpan s;
                s.kind = "token";
                s.token = std::move(tr);
                spans.push_back(std::move(s));
                i += match->raw.size();
                continue;
            }
        }

        // Footnote ref: [^1]
        if (text[i] == '[' && i + 2 < text.size() && text[i + 1] == '^') {
            size_t close = text.find(']', i + 2);
            if (close != std::string_view::npos) {
                flush_text();
                std::string label(text.substr(i + 2, close - (i + 2)));
                TokenRef tr;
                tr.kind = "footnoteRef";
                tr.label = label;
                ProjSpan s;
                s.kind = "token";
                s.token = std::move(tr);
                spans.push_back(std::move(s));
                i = close + 1;
                continue;
            }
        }

        // Code span: `code`
        if (text[i] == '`') {
            size_t close = text.find('`', i + 1);
            if (close != std::string_view::npos) {
                flush_text();
                ProjSpan s;
                s.kind = "text";
                s.text = std::string(text.substr(i + 1, close - (i + 1)));
                s.marks.code = true;
                spans.push_back(std::move(s));
                i = close + 1;
                continue;
            }
        }

        // Bold: **text**
        if (text[i] == '*' && i + 1 < text.size() && text[i + 1] == '*') {
            size_t close = text.find("**", i + 2);
            if (close != std::string_view::npos) {
                flush_text();
                ProjSpan s;
                s.kind = "text";
                s.text = std::string(text.substr(i + 2, close - (i + 2)));
                s.marks.bold = true;
                spans.push_back(std::move(s));
                i = close + 2;
                continue;
            }
        }

        // Italic: *text*
        if (text[i] == '*' && (i + 1 >= text.size() || text[i + 1] != '*')) {
            size_t close = text.find('*', i + 1);
            if (close != std::string_view::npos) {
                flush_text();
                ProjSpan s;
                s.kind = "text";
                s.text = std::string(text.substr(i + 1, close - (i + 1)));
                s.marks.italic = true;
                spans.push_back(std::move(s));
                i = close + 1;
                continue;
            }
        }

        // Strikethrough: ~~text~~
        if (text[i] == '~' && i + 1 < text.size() && text[i + 1] == '~') {
            size_t close = text.find("~~", i + 2);
            if (close != std::string_view::npos) {
                flush_text();
                ProjSpan s;
                s.kind = "text";
                s.text = std::string(text.substr(i + 2, close - (i + 2)));
                s.marks.strikethrough = true;
                spans.push_back(std::move(s));
                i = close + 2;
                continue;
            }
        }

        // Link: [text](url)
        if (text[i] == '[') {
            size_t close_b = text.find(']', i + 1);
            if (close_b != std::string_view::npos && close_b + 1 < text.size() && text[close_b + 1] == '(') {
                size_t close_p = text.find(')', close_b + 2);
                if (close_p != std::string_view::npos) {
                    flush_text();
                    std::string link_text(text.substr(i + 1, close_b - (i + 1)));
                    std::string link_url(text.substr(close_b + 2, close_p - (close_b + 2)));
                    ProjSpan s;
                    s.kind = "text";
                    s.text = std::move(link_text);
                    s.marks.link = std::move(link_url);
                    spans.push_back(std::move(s));
                    i = close_p + 1;
                    continue;
                }
            }
        }

        current += text[i];
        i++;
    }

    flush_text();
    return spans;
}

inline std::pair<std::string, std::optional<std::string>> extractParagraphStyleMarker(std::string_view text) {
    // Check if ends with :p{.styleName}
    static const std::regex p_re(R"(\s*:p\{\.([A-Za-z0-9_+.-]+)\}\s*$)");
    std::string s(text);
    std::smatch m;
    if (std::regex_search(s, m, p_re)) {
        std::string cleaned = s.substr(0, m.position());
        std::string style = m[1].str();
        return {cleaned, style};
    }
    return {s, std::nullopt};
}

inline void tableMergeRegionsResolve(ProjTable& table, const std::map<std::pair<int, int>, std::string>& markers) {
    if (markers.empty()) return;
    const int rows = static_cast<int>(table.rows.size());
    const int columns = rows ? static_cast<int>(table.rows[0].size()) : 0;
    std::map<std::pair<int, int>, std::pair<int, int>> parent;
    for (int r = 0; r < rows; ++r) for (int c = 0; c < columns; ++c) parent[{r, c}] = {r, c};
    auto root = [&](std::pair<int, int> at) {
        while (parent.at(at) != at) {
            parent.at(at) = parent.at(parent.at(at));
            at = parent.at(at);
        }
        return at;
    };
    for (const auto& [at, marker] : markers) {
        auto neighbor = at;
        if (marker == MERGE_LEFT) --neighbor.second;
        else if (marker == MERGE_UP) --neighbor.first;
        else ++neighbor.first;
        if (!parent.contains(neighbor)) throw CoreError("unsupportedSyntax", "A merge marker points outside the table.");
        if (table.cellStyles[at.first][at.second] || table.paragraphStyles[at.first][at.second])
            throw CoreError("unsupportedSyntax", "A covered merge cell cannot declare styles.");
        parent.at(root(at)) = root(neighbor);
    }
    std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> regions;
    for (const auto& [at, unused] : parent) {
        (void)unused;
        regions[root(at)].push_back(at);
    }
    table.merges.clear();
    for (const auto& [unused, cells] : regions) {
        (void)unused;
        if (cells.size() < 2) continue;
        std::optional<std::pair<int, int>> content;
        int top = rows, left = columns, bottom = 0, right = 0;
        for (const auto& at : cells) {
            if (!markers.contains(at)) {
                if (content) throw CoreError("unsupportedSyntax", "A merged region must have exactly one content cell.");
                content = at;
            }
            top = std::min(top, at.first); left = std::min(left, at.second);
            bottom = std::max(bottom, at.first); right = std::max(right, at.second);
        }
        if (!content) throw CoreError("unsupportedSyntax", "Merge markers form a cycle without a content cell.");
        if (static_cast<size_t>(bottom - top + 1) * static_cast<size_t>(right - left + 1) != cells.size())
            throw CoreError("unsupportedSyntax", "A merged region must form a rectangle.");
        if (*content != std::pair{top, left}) {
            const auto [r, c] = *content;
            table.rows[top][left] = std::move(table.rows[r][c]);
            table.cells[top][left] = std::move(table.cells[r][c]);
            table.cellStyles[top][left] = std::move(table.cellStyles[r][c]);
            table.paragraphStyles[top][left] = std::move(table.paragraphStyles[r][c]);
            table.rows[r][c].clear(); table.cells[r][c].clear();
            table.cellStyles[r][c].reset(); table.paragraphStyles[r][c].reset();
        }
        table.merges.push_back({.column = left, .columnSpan = right - left + 1, .row = top, .rowSpan = bottom - top + 1});
    }
    std::sort(table.merges.begin(), table.merges.end(), [](const auto& a, const auto& b) {
        return std::pair{a.row, a.column} < std::pair{b.row, b.column};
    });
}

inline Projection markdownParse(std::string_view markdown) {
    auto envelope = slimr::parse_frontmatter(markdown);
    if (!envelope) throw CoreError("unsupportedSyntax", "Invalid Markdown frontmatter.");
    std::vector<FileProblem> problems;
    auto body = std::string(envelope->body);
    const auto firstBody = body.find_first_not_of("\r\n");
    // Stripped leading newlines still count toward block source lines
    int leadNewlines{0};
    if (firstBody != std::string::npos) {
        leadNewlines = static_cast<int>(std::count(body.begin(), body.begin() + static_cast<std::ptrdiff_t>(firstBody), '\n'));
        body.erase(0, firstBody);
    }
    auto stripped = reviewMarkersStrip(body, problems, 1);

    Projection proj;
    if (envelope->frontmatter) {
        const auto declarations = frontmatterValidate(*envelope->frontmatter, "tab");
        for (const auto& [name, declaration] : declarations.textStyles) {
            proj.styles[name] = styleDeclarationAttrs(declaration);
        }
        proj.paragraphStyles.insert(declarations.paragraphStyles.begin(), declarations.paragraphStyles.end());
        proj.cellStyles.insert(declarations.cellStyles.begin(), declarations.cellStyles.end());
    }
    DirectiveAttrs pendingTableSettings;
    bool tableSettingsPending = false;
    std::optional<std::string> activeListPreset;
    bool listPresetAttached = false;
    proj.unsupportedReview = !problems.empty();
    proj.reviewMarkers = std::move(stripped.markers);
    std::istringstream ss{std::string(stripped.text)};
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    std::map<std::string, std::vector<ProjBlock>> referenced_cell_blocks;
    for (size_t li = 0; li < lines.size(); li++) {
        const auto& l = lines[li];
        if (l.starts_with("::cell")) {
            static const std::unordered_set<std::string> cell_set = {"cell"};
            auto ld = lineDirectiveMatch(l, cell_set);
            if (ld && ld->attrs.has_flag("start") && ld->attrs.get_value("id")) {
                std::string cell_id = *ld->attrs.get_value("id");
                li++;
                std::string cell_content;
                while (li < lines.size()) {
                    const auto& sub = lines[li];
                    if (sub.starts_with("::cell")) {
                        auto sub_ld = lineDirectiveMatch(sub, cell_set);
                        if (sub_ld && sub_ld->attrs.has_flag("end") &&
                            sub_ld->attrs.get_value("id") == cell_id) {
                            break;
                        }
                    }
                    if (!cell_content.empty()) cell_content += "\n";
                    cell_content += sub;
                    li++;
                }
                auto sub_proj = markdownParse(cell_content);
                proj.unsupportedReview |= sub_proj.unsupportedReview;
                referenced_cell_blocks[cell_id] = std::move(sub_proj.blocks);
            }
        }
    }

    size_t i = 0;
    auto lineAt = [&](size_t at) { proj.blockLines.lines.push_back(leadNewlines + static_cast<int>(at) + 1); };
    while (i < lines.size()) {
        const auto& cur_line = lines[i];
        const size_t blockStart = i;
        if (cur_line.starts_with("::comment") || cur_line.starts_with(":::comment") ||
            cur_line.starts_with("::suggestion") || cur_line.starts_with(":::suggestion") ||
            cur_line.starts_with("::header") || cur_line.starts_with("::footer") ||
            cur_line.starts_with("::footnote") || cur_line.starts_with(":::header") ||
            cur_line.starts_with(":::footer") || cur_line.starts_with(":::footnote")) proj.unsupportedReview = true;

        // Empty line
        if (cur_line.find_first_not_of(" \t\r\n") == std::string::npos) {
            i++;
            continue;
        }
        static const std::regex listLine(R"(^ *([-*] |[0-9]+\. ))");
        const bool listItemLine = std::regex_search(cur_line, listLine);
        if (tableSettingsPending && !cur_line.starts_with("|") && !cur_line.starts_with("::table")) {
            throw CoreError("unsupportedSyntax", "A ::table line must come directly before its table.");
        }
        if (activeListPreset && !listItemLine && !cur_line.starts_with("::list")) {
            if (!listPresetAttached) throw CoreError("unsupportedSyntax", "A ::list line must come directly before its list.");
            activeListPreset.reset();
            listPresetAttached = false;
        }

        // Code fence: ``` or ~~~
        size_t non_ws = cur_line.find_first_not_of(" \t");
        if (non_ws != std::string::npos && (cur_line.substr(non_ws, 3) == "```" || cur_line.substr(non_ws, 3) == "~~~")) {
            std::string fence = cur_line.substr(non_ws, 3);
            i++;
            while (i < lines.size()) {
                const auto& sub = lines[i];
                size_t sub_ws = sub.find_first_not_of(" \t");
                if (sub_ws != std::string::npos && sub.substr(sub_ws, 3) == fence) {
                    i++;
                    break;
                }
                ProjBlock cb;
                cb.kind = "codeLine";
                ProjSpan s;
                s.kind = "text";
                s.text = sub;
                cb.spans.push_back(std::move(s));
                proj.blocks.push_back(std::move(cb));
                lineAt(i);
                i++;
            }
            continue;
        }

        // Skip referenced cell block (parsed in pre-pass): ::cell{id=... start}
        if (cur_line.starts_with("::cell")) {
            static const std::unordered_set<std::string> cell_set = {"cell"};
            auto ld = lineDirectiveMatch(cur_line, cell_set);
            if (ld && ld->attrs.has_flag("start") && ld->attrs.get_value("id")) {
                std::string cell_id = *ld->attrs.get_value("id");
                i++;
                while (i < lines.size()) {
                    const auto& sub = lines[i];
                    if (sub.starts_with("::cell")) {
                        auto sub_ld = lineDirectiveMatch(sub, cell_set);
                        if (sub_ld && sub_ld->attrs.has_flag("end") &&
                            sub_ld->attrs.get_value("id") == cell_id) {
                            i++;
                            break;
                        }
                    }
                    i++;
                }
                continue;
            }
        }

        // Block directive: ::pagebreak, ::section, ::toc
        if (cur_line.starts_with("::table")) {
            static const std::unordered_set<std::string> tableNames = {"table"};
            const auto directive = lineDirectiveMatch(cur_line, tableNames);
            if (!directive || directive->label || tableSettingsPending) proj.unsupportedReview = true;
            else {
                pendingTableSettings = directive->attrs;
                tableSettingsPending = true;
            }
            ++i;
            continue;
        }
        if (cur_line.starts_with("::list")) {
            static const std::unordered_set<std::string> names = {"list"};
            const auto directive = lineDirectiveMatch(cur_line, names);
            if (!directive || directive->label || !directive->attrs.classes.empty() ||
                !directive->attrs.flags.empty() || directive->attrs.values.size() != 1 ||
                !directive->attrs.get_value("preset") ||
                !model::listPresetTable().contains(*directive->attrs.get_value("preset"))) {
                throw CoreError("unsupportedSyntax", "Unknown list preset.");
            }
            activeListPreset = directive->attrs.get_value("preset");
            listPresetAttached = false;
            ++i;
            continue;
        }
        if (cur_line.starts_with("::") && !cur_line.starts_with(":::")) {
            static const std::unordered_set<std::string> block_objs = {"pagebreak", "section", "toc"};
            auto ld = lineDirectiveMatch(cur_line, block_objs);
            if (ld) {
                ProjBlock pb;
                pb.kind = "token";
                TokenRef tr;
                tr.kind = ld->name;
                tr.label = ld->label;
                tr.attrs = ld->attrs;
                pb.token = std::move(tr);
                proj.blocks.push_back(std::move(pb));
                lineAt(blockStart);
                i++;
                continue;
            }
        }

        // Heading: # H1, ## H2, etc.
        if (cur_line.starts_with("#")) {
            size_t hashes = 0;
            while (hashes < cur_line.size() && cur_line[hashes] == '#') hashes++;
            if (hashes <= 6 && hashes < cur_line.size() && cur_line[hashes] == ' ') {
                auto [text, style] = extractParagraphStyleMarker(cur_line.substr(hashes + 1));
                ProjBlock hb;
                hb.kind = "heading";
                hb.headingLevel = static_cast<int>(hashes);
                hb.paragraphStyle = style;
                hb.spans = parseInlines(text);
                proj.blocks.push_back(std::move(hb));
                lineAt(blockStart);
                i++;
                continue;
            }
        }

        // List item: - item, * item, 1. item
        size_t indent = 0;
        while (indent < cur_line.size() && cur_line[indent] == ' ') indent++;
        std::string_view trimmed_line = std::string_view(cur_line).substr(indent);
        bool is_bullet = (trimmed_line.starts_with("- ") || trimmed_line.starts_with("* "));
        bool is_numbered = false;
        size_t num_prefix = 0;
        if (!is_bullet && std::isdigit(static_cast<unsigned char>(trimmed_line[0]))) {
            size_t d = 0;
            while (d < trimmed_line.size() && std::isdigit(static_cast<unsigned char>(trimmed_line[d]))) d++;
            if (d < trimmed_line.size() && trimmed_line[d] == '.' && d + 1 < trimmed_line.size() && trimmed_line[d + 1] == ' ') {
                is_numbered = true;
                num_prefix = d + 2;
            }
        }

        if (is_bullet || is_numbered) {
            std::string_view content = is_bullet ? trimmed_line.substr(2) : trimmed_line.substr(num_prefix);
            auto [text, style] = extractParagraphStyleMarker(content);
            ProjBlock lb;
            lb.kind = "listItem";
            lb.listKind = is_numbered ? "numbered" : "bullet";
            lb.listDepth = static_cast<int>(indent / 2);
            if (activeListPreset) {
                const bool number = activeListPreset->starts_with("NUMBERED_");
                if (number != is_numbered || *activeListPreset == "BULLET_CHECKBOX") {
                    throw CoreError("unsupportedSyntax", "The list preset does not match the list kind.");
                }
                lb.listPreset = activeListPreset;
                listPresetAttached = true;
            }
            lb.paragraphStyle = style;
            lb.spans = parseInlines(text);
            proj.blocks.push_back(std::move(lb));
            lineAt(blockStart);
            i++;
            continue;
        }

        // Table: starts with '|'
        if (cur_line.starts_with("|")) {
            std::vector<std::string> table_lines;
            while (i < lines.size() && lines[i].starts_with("|")) {
                table_lines.push_back(lines[i]);
                i++;
            }

            if (table_lines.size() >= 2) {
                ProjTable pt;
                pt.settings = std::move(pendingTableSettings);
                pendingTableSettings = {};
                tableSettingsPending = false;
                // Parse rows
                std::vector<std::vector<std::string>> raw_cells;
                for (size_t r = 0; r < table_lines.size(); r++) {
                    if (r == 1 && table_lines[1].find("---") != std::string::npos) {
                        // Header delimiter line: parse alignments
                        std::stringstream ss_del(table_lines[1]);
                        std::string seg;
                        std::vector<std::string> aligns;
                        while (std::getline(ss_del, seg, '|')) {
                            size_t s_pos = seg.find_first_not_of(" \t");
                            if (s_pos == std::string::npos) continue;
                            size_t e_pos = seg.find_last_not_of(" \t");
                            std::string part = seg.substr(s_pos, e_pos - s_pos + 1);
                            if (part.starts_with(":") && part.ends_with(":")) aligns.push_back("CENTER");
                            else if (part.ends_with(":")) aligns.push_back("RIGHT");
                            else aligns.push_back("LEFT");
                        }
                        pt.alignments = std::move(aligns);
                        continue;
                    }

                    std::stringstream ss_row(table_lines[r]);
                    std::string seg;
                    std::vector<std::string> cells;
                    bool first = true;
                    while (std::getline(ss_row, seg, '|')) {
                        if (first) { first = false; continue; } // Before first '|'
                        size_t s_pos = seg.find_first_not_of(" \t");
                        if (s_pos == std::string::npos) {
                            cells.push_back("");
                        } else {
                            size_t e_pos = seg.find_last_not_of(" \t");
                            cells.push_back(seg.substr(s_pos, e_pos - s_pos + 1));
                        }
                    }
                    if (!cells.empty()) raw_cells.push_back(std::move(cells));
                }

                // Process rows, merges, and cell references
                if (raw_cells.empty() || raw_cells.front().empty() || pt.alignments.size() != raw_cells.front().size()) {
                    throw CoreError("unsupportedSyntax", "A table's header and delimiter must have the same columns.");
                }
                for (auto& row : raw_cells) row.resize(raw_cells.front().size());
                std::map<std::pair<int, int>, std::string> mergeTokens;
                pt.rows.resize(raw_cells.size());
                pt.cellStyles.resize(raw_cells.size());
                pt.paragraphStyles.resize(raw_cells.size());
                pt.cells.resize(raw_cells.size());

                for (size_t r = 0; r < raw_cells.size(); r++) {
                    pt.rows[r].resize(raw_cells[r].size());
                    pt.cellStyles[r].resize(raw_cells[r].size());
                    pt.paragraphStyles[r].resize(raw_cells[r].size());
                    pt.cells[r].resize(raw_cells[r].size());

                    for (size_t c = 0; c < raw_cells[r].size(); c++) {
                        std::string cell_str = raw_cells[r][c];
                        static const std::regex stylePattern(R"(\s*:cell\{\.([A-Za-z0-9_+.-]+)\}\s*$)");
                        std::smatch styleMatch;
                        if (std::regex_search(cell_str, styleMatch, stylePattern)) {
                            pt.cellStyles[r][c] = styleMatch[1].str();
                            cell_str = cell_str.substr(0, styleMatch.position());
                        }
                        auto [cellText, paragraphStyle] = extractParagraphStyleMarker(cell_str);
                        cell_str = std::move(cellText);
                        pt.paragraphStyles[r][c] = std::move(paragraphStyle);

                        static const std::regex mixedMerge(R"((^|[^\\A-Za-z0-9]):merge\{(left|up|down)\})");
                        if (cell_str != MERGE_LEFT && cell_str != MERGE_UP && cell_str != MERGE_DOWN &&
                            std::regex_search(cell_str, mixedMerge))
                            throw CoreError("unsupportedSyntax", "A merge marker must be its cell's only content.");
                        if (cell_str == MERGE_LEFT || cell_str == MERGE_UP || cell_str == MERGE_DOWN) {
                            mergeTokens[{static_cast<int>(r), static_cast<int>(c)}] = cell_str;
                        } else if (cell_str.starts_with(":cell{ref=")) {
                            size_t ref_start = 10;
                            size_t ref_end = cell_str.find('}', ref_start);
                            if (ref_end != std::string::npos) {
                                std::string ref_id = cell_str.substr(ref_start, ref_end - ref_start);
                                auto it = referenced_cell_blocks.find(ref_id);
                                if (it != referenced_cell_blocks.end()) {
                                    pt.cells[r][c] = it->second;
                                } else throw CoreError("unsupportedSyntax", "A referenced table cell has no definition.");
                            } else throw CoreError("unsupportedSyntax", "Invalid table cell reference.");
                        } else {
                            // Check cell style marker :cell{.name}
                            static const std::regex c_re(R"(\s*:cell\{\.([A-Za-z0-9_+.-]+)\}\s*$)");
                            std::smatch m;
                            if (std::regex_search(cell_str, m, c_re)) {
                                pt.cellStyles[r][c] = m[1].str();
                                cell_str = cell_str.substr(0, m.position());
                            }
                            pt.rows[r][c] = parseInlines(cell_str);
                        }
                    }
                }
                tableMergeRegionsResolve(pt, mergeTokens);

                ProjBlock tb;
                tb.kind = "table";
                tb.table = std::move(pt);
                proj.blocks.push_back(std::move(tb));
                lineAt(blockStart);
                continue;
            }
        }

        // Paragraph: collect lines until blank line or next block
        std::string p_text = cur_line;
        i++;
        while (i < lines.size()) {
            const auto& next_line = lines[i];
            if (next_line.find_first_not_of(" \t\r\n") == std::string::npos ||
                next_line.starts_with("#") || next_line.starts_with("```") ||
                next_line.starts_with("~~~") || next_line.starts_with("|") ||
                next_line.starts_with("- ") || next_line.starts_with("* ") ||
                next_line.starts_with("::")) {
                break;
            }
            p_text += " " + next_line;
            i++;
        }

        auto [cleaned, style] = extractParagraphStyleMarker(p_text);
        ProjBlock pb;
        pb.kind = "paragraph";
        pb.paragraphStyle = style;
        pb.spans = parseInlines(cleaned);
        proj.blocks.push_back(std::move(pb));
        lineAt(blockStart);
    }

    if (!pendingTableSettings.values.empty() || !pendingTableSettings.flags.empty()) proj.unsupportedReview = true;
    if (tableSettingsPending || (activeListPreset && !listPresetAttached)) {
        throw CoreError("unsupportedSyntax", "A settings directive needs its table or list.");
    }
    return proj;
}

} // namespace gdocsmith::lens
