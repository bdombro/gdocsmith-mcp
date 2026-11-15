#pragma once

#include "cell_refs.hpp"
#include "directives.hpp"
#include "tables.hpp"
#include "tokens.hpp"
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::lens {

inline std::string markdownTextEscape(std::string_view text) {
    std::string escaped;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (std::string_view("\\`*~[]!#|{}").find(c) != std::string_view::npos) {
            escaped += '\\';
        } else if (c == ':' && (i == 0 || !(std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '\\'))) {
            const auto rest = text.substr(i + 1);
            for (const std::string_view kw : {":pagebreak", ":section", ":toc"}) {
                if (rest.starts_with(kw) &&
                    (i + 1 + kw.size() >= text.size() || !std::isalnum(static_cast<unsigned char>(text[i + 1 + kw.size()])))) {
                    escaped += '\\';
                    break;
                }
            }
        }
        escaped += c;
    }
    return escaped;
}

inline std::string renderSpans(const std::vector<ProjSpan> &spans) {
    std::string out;
    for (const auto &span : spans) {
        if (span.kind == "reviewStart") {
            out += ":change[";
        } else if (span.kind == "reviewEnd") {
            out += "]";
        } else if (span.kind == "text") {
            std::string piece = markdownTextEscape(span.text);
            const auto &marks = span.marks;
            const int count = marks.bold + marks.italic + marks.strikethrough + marks.code + marks.link.has_value();
            if (span.directive || marks.underline || marks.smallCaps || count > 1 || !span.formatting.values.empty() ||
                (count > 0 && (piece != span.text || span.text.find('`') != std::string::npos ||
                               (marks.link && marks.link->find(')') != std::string::npos)))) {
                DirectiveAttrs attrs = span.formatting;
                if (span.directive)
                    attrs.classes.push_back(*span.directive);
                for (const auto &[name, enabled] : {
                         std::pair{"bold", marks.bold}, {"italic", marks.italic}, {"underline", marks.underline}, {"strikethrough", marks.strikethrough}, {"smallCaps", marks.smallCaps}, {"code", marks.code}}) {
                    if (enabled)
                        attrs.values.emplace_back(name, "true");
                }
                if (marks.link)
                    attrs.values.emplace_back("link", *marks.link);
                piece = ":style[" + piece + "]" + directiveAttrsRender(attrs);
            } else {
                if (marks.code)
                    piece = "`" + span.text + "`";
                if (marks.bold)
                    piece = "**" + piece + "**";
                if (marks.italic)
                    piece = "*" + piece + "*";
                if (marks.strikethrough)
                    piece = "~~" + piece + "~~";
                if (marks.link)
                    piece = "[" + piece + "](" + *marks.link + ")";
            }
            out += piece;
        } else if (span.kind == "token" && span.token) {
            const auto &t = *span.token;
            if (t.kind == "footnoteRef" && t.label) {
                out += "[^" + *t.label + "]";
                continue;
            }
            std::string dir = ":" + t.kind;
            if (t.label)
                dir += "[" + *t.label + "]";
            dir += directiveAttrsRender(t.attrs);
            out += dir;
        }
    }
    return out;
}

inline std::string markdownRender(const Projection &projection, int *tableCounter = nullptr) {
    int localCounter = 0;
    if (!tableCounter)
        tableCounter = &localCounter;
    std::string out;
    bool in_code_fence = false;

    auto end_code_fence = [&]() {
        if (in_code_fence) {
            out += "```\n\n";
            in_code_fence = false;
        }
    };

    for (size_t b_idx = 0; b_idx < projection.blocks.size(); b_idx++) {
        const auto &b = projection.blocks[b_idx];

        if (b.kind == "codeLine") {
            if (!in_code_fence) {
                out += "```\n";
                in_code_fence = true;
            }
            std::string line;
            for (const auto &s : b.spans)
                if (s.kind == "text")
                    line += s.text;
            out += line + "\n";
            continue;
        }

        end_code_fence();

        if (b.kind == "heading") {
            int level = b.headingLevel.value_or(1);
            level = std::clamp(level, 1, 6);
            std::string prefix(level, '#');
            out += prefix + " " + renderSpans(b.spans);
            if (b.paragraphStyle) {
                out += " :p{." + *b.paragraphStyle + "}";
            }
            out += "\n\n";
        } else if (b.kind == "listItem") {
            if (b.listPreset && (b_idx == 0 || projection.blocks[b_idx - 1].kind != "listItem" ||
                                 projection.blocks[b_idx - 1].listPreset != b.listPreset)) {
                out += "::list{preset=" + *b.listPreset + "}\n";
            }
            std::string indent(b.listDepth * 2, ' ');
            std::string bullet = (b.listKind.value_or("bullet") == "numbered") ? "1. " : "- ";
            out += indent + bullet + renderSpans(b.spans);
            if (b.paragraphStyle) {
                out += " :p{." + *b.paragraphStyle + "}";
            }
            out += "\n";
            // Check if next is not a list item to put blank line
            if (b_idx + 1 >= projection.blocks.size() || projection.blocks[b_idx + 1].kind != "listItem") {
                out += "\n";
            }
        } else if (b.kind == "paragraph") {
            std::string text = renderSpans(b.spans);
            if (text.starts_with("- ") || text.starts_with("* "))
                text.insert(0, "\\");
            const auto digits = text.find_first_not_of("0123456789");
            if (digits != std::string::npos && digits > 0 && text.substr(digits, 2) == ". ") {
                text.insert(digits, "\\");
            }
            out += text;
            if (b.paragraphStyle) {
                out += " :p{." + *b.paragraphStyle + "}";
            }
            out += "\n\n";
        } else if (b.kind == "token" && b.token) {
            const auto &t = *b.token;
            out += "::" + t.kind;
            if (t.label)
                out += "[" + *t.label + "]";
            out += directiveAttrsRender(t.attrs) + "\n\n";
        } else if (b.kind == "table" && b.table) {
            const auto &t = *b.table;
            if (!t.settings.values.empty() || !t.settings.flags.empty()) {
                out += "::table" + directiveAttrsRender(t.settings) + "\n";
            }
            const int ordinal = ++*tableCounter;
            auto covered = mergeCovered(t.merges);

            struct CellDef {
                std::string id;
                std::string content;
            };
            std::vector<CellDef> definitions;

            for (size_t r = 0; r < t.rows.size(); r++) {
                out += "|";
                for (size_t c = 0; c < t.rows[r].size(); c++) {
                    out += " ";
                    if (covered.contains({static_cast<int>(r), static_cast<int>(c)})) {
                        // Check if in top row of its merge
                        bool is_top_row = false;
                        for (const auto &m : t.merges) {
                            if (static_cast<int>(r) == m.row &&
                                static_cast<int>(c) >= m.column &&
                                static_cast<int>(c) < m.column + m.columnSpan) {
                                is_top_row = true;
                                break;
                            }
                        }
                        out += is_top_row ? std::string(MERGE_LEFT) : std::string(MERGE_UP);
                    } else if (r < t.cells.size() && c < t.cells[r].size() && !t.cells[r][c].empty()) {
                        // Referenced cell
                        std::string ref_id = cellRefRender(ordinal, static_cast<int>(r), static_cast<int>(c));
                        out += ":cell{ref=" + ref_id + "}";
                        if (r < t.cellStyles.size() && c < t.cellStyles[r].size() && t.cellStyles[r][c]) {
                            out += " :cell{." + *t.cellStyles[r][c] + "}";
                        }
                        std::string def_content;
                        Projection cell_proj;
                        cell_proj.blocks = t.cells[r][c];
                        def_content = markdownRender(cell_proj, tableCounter);
                        while (!def_content.empty() && def_content.back() == '\n')
                            def_content.pop_back();
                        definitions.push_back({ref_id, def_content});
                    } else {
                        std::string cell_text = renderSpans(t.rows[r][c]);
                        out += cell_text;
                        if (r < t.paragraphStyles.size() && c < t.paragraphStyles[r].size() && t.paragraphStyles[r][c]) {
                            out += " :p{." + *t.paragraphStyles[r][c] + "}";
                        }
                        if (r < t.cellStyles.size() && c < t.cellStyles[r].size() && t.cellStyles[r][c]) {
                            out += " :cell{." + *t.cellStyles[r][c] + "}";
                        }
                    }
                    out += " |";
                }
                out += "\n";

                // Header delimiter line after row 0
                if (r == 0) {
                    out += "|";
                    for (size_t c = 0; c < t.rows[0].size(); c++) {
                        std::string align = (c < t.alignments.size()) ? t.alignments[c] : "LEFT";
                        if (align == "CENTER")
                            out += " :---: |";
                        else if (align == "RIGHT")
                            out += " ---: |";
                        else
                            out += " --- |";
                    }
                    out += "\n";
                }
            }
            out += "\n";

            for (const auto &def : definitions) {
                out += cellDefinitionLine(def.id, "start") + "\n";
                out += def.content + "\n";
                out += cellDefinitionLine(def.id, "end") + "\n\n";
            }
        }
    }

    end_code_fence();
    if (out.empty())
        return "\n";
    if (out.ends_with("\n\n"))
        out.pop_back();
    return out;
}

} // namespace gdocsmith::lens
