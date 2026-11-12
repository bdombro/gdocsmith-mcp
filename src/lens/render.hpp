#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include "tokens.hpp"
#include "directives.hpp"
#include "tables.hpp"
#include "cell_refs.hpp"

namespace gdocsmith::lens {

inline std::string renderSpans(const std::vector<ProjSpan>& spans) {
    std::string out;
    for (const auto& span : spans) {
        if (span.kind == "text") {
            std::string piece = span.text;
            if (span.marks.code) piece = "`" + piece + "`";
            if (span.marks.bold) piece = "**" + piece + "**";
            if (span.marks.italic) piece = "*" + piece + "*";
            if (span.marks.strikethrough) piece = "~~" + piece + "~~";
            if (span.directive) {
                piece = ":style[" + piece + "]{." + *span.directive + "}";
            }
            if (span.marks.link) {
                piece = "[" + piece + "](" + *span.marks.link + ")";
            }
            out += piece;
        } else if (span.kind == "token" && span.token) {
            const auto& t = *span.token;
            std::string dir = ":" + t.kind;
            if (t.label) dir += "[" + *t.label + "]";
            dir += directiveAttrsRender(t.attrs);
            out += dir;
        }
    }
    return out;
}

inline std::string markdownRender(const Projection& projection) {
    std::string out;
    bool in_code_fence = false;

    auto end_code_fence = [&]() {
        if (in_code_fence) {
            out += "```\n\n";
            in_code_fence = false;
        }
    };

    for (size_t b_idx = 0; b_idx < projection.blocks.size(); b_idx++) {
        const auto& b = projection.blocks[b_idx];

        if (b.kind == "codeLine") {
            if (!in_code_fence) {
                out += "```\n";
                in_code_fence = true;
            }
            std::string line;
            for (const auto& s : b.spans) if (s.kind == "text") line += s.text;
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
            out += renderSpans(b.spans);
            if (b.paragraphStyle) {
                out += " :p{." + *b.paragraphStyle + "}";
            }
            out += "\n\n";
        } else if (b.kind == "token" && b.token) {
            const auto& t = *b.token;
            out += "::" + t.kind;
            if (t.label) out += "[" + *t.label + "]";
            out += directiveAttrsRender(t.attrs) + "\n\n";
        } else if (b.kind == "table" && b.table) {
            const auto& t = *b.table;
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
                        for (const auto& m : t.merges) {
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
                        std::string ref_id = cellRefRender(1, static_cast<int>(r), static_cast<int>(c));
                        out += ":cell{ref=" + ref_id + "}";
                        std::string def_content;
                        Projection cell_proj;
                        cell_proj.blocks = t.cells[r][c];
                        def_content = markdownRender(cell_proj);
                        while (!def_content.empty() && def_content.back() == '\n') def_content.pop_back();
                        definitions.push_back({ref_id, def_content});
                    } else {
                        std::string cell_text = renderSpans(t.rows[r][c]);
                        out += cell_text;
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
                        if (align == "CENTER") out += " :---: |";
                        else if (align == "RIGHT") out += " ---: |";
                        else out += " --- |";
                    }
                    out += "\n";
                }
            }
            out += "\n";

            for (const auto& def : definitions) {
                out += cellDefinitionLine(def.id, "start") + "\n";
                out += def.content + "\n";
                out += cellDefinitionLine(def.id, "end") + "\n\n";
            }
        }
    }

    end_code_fence();
    return out;
}

} // namespace gdocsmith::lens
