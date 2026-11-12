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
        "person", "date", "richlink", "richLink", "chip", "footnoteRef", "pagebreak", "section", "toc"
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
                    std::string url(text.substr(close_b + 2, close_p - (close_p + 2)));
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
            if (match && match->label.has_value() && !match->attrs.classes.empty()) {
                flush_text();
                ProjSpan s;
                s.kind = "text";
                s.text = *match->label;
                s.directive = match->attrs.classes[0];
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

inline Projection markdownParse(std::string_view markdown) {
    std::vector<FileProblem> problems;
    auto stripped = reviewMarkersStrip(markdown, problems, 1);

    Projection proj;
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
                referenced_cell_blocks[cell_id] = std::move(sub_proj.blocks);
            }
        }
    }

    size_t i = 0;
    while (i < lines.size()) {
        const auto& cur_line = lines[i];

        // Empty line
        if (cur_line.find_first_not_of(" \t\r\n") == std::string::npos) {
            i++;
            continue;
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
            lb.paragraphStyle = style;
            lb.spans = parseInlines(text);
            proj.blocks.push_back(std::move(lb));
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
                pt.rows.resize(raw_cells.size());
                pt.cellStyles.resize(raw_cells.size());
                pt.cells.resize(raw_cells.size());

                for (size_t r = 0; r < raw_cells.size(); r++) {
                    pt.rows[r].resize(raw_cells[r].size());
                    pt.cellStyles[r].resize(raw_cells[r].size());
                    pt.cells[r].resize(raw_cells[r].size());

                    for (size_t c = 0; c < raw_cells[r].size(); c++) {
                        std::string cell_str = raw_cells[r][c];

                        if (cell_str == MERGE_LEFT) {
                            // Find merge region to left
                            for (auto& m : pt.merges) {
                                if (m.row == static_cast<int>(r) && m.column + m.columnSpan == static_cast<int>(c)) {
                                    m.columnSpan++;
                                    break;
                                }
                            }
                            // Or start new merge with left cell
                            if (c > 0) {
                                bool found = false;
                                for (const auto& m : pt.merges) {
                                    if (m.row == static_cast<int>(r) && m.column <= static_cast<int>(c) &&
                                        static_cast<int>(c) < m.column + m.columnSpan) {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    pt.merges.push_back(TableMerge{
                                        .column = static_cast<int>(c - 1),
                                        .columnSpan = 2,
                                        .row = static_cast<int>(r),
                                        .rowSpan = 1
                                    });
                                }
                            }
                        } else if (cell_str == MERGE_UP) {
                            // Find merge region above
                            for (auto& m : pt.merges) {
                                if (m.column == static_cast<int>(c) && m.row + m.rowSpan == static_cast<int>(r)) {
                                    m.rowSpan++;
                                    break;
                                }
                            }
                        } else if (cell_str.starts_with(":cell{ref=")) {
                            size_t ref_start = 10;
                            size_t ref_end = cell_str.find('}', ref_start);
                            if (ref_end != std::string::npos) {
                                std::string ref_id = cell_str.substr(ref_start, ref_end - ref_start);
                                auto it = referenced_cell_blocks.find(ref_id);
                                if (it != referenced_cell_blocks.end()) {
                                    pt.cells[r][c] = it->second;
                                }
                            }
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

                ProjBlock tb;
                tb.kind = "table";
                tb.table = std::move(pt);
                proj.blocks.push_back(std::move(tb));
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
    }

    return proj;
}

} // namespace gdocsmith::lens
