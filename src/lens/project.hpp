#pragma once

#include <string>
#include <vector>
#include <map>
#include "json.hpp"
#include "lens/json_helper.hpp"
#include "tokens.hpp"
#include "directives.hpp"
#include "frontmatter.hpp"
#include "paragraph_styles.hpp"
#include "cell_styles.hpp"

namespace gdocsmith::lens {

inline Projection projectionBuild(const slimr::Json& tab) {
    Projection proj;
    auto* blocks = json::get(tab, "blocks");
    if (!blocks || !blocks->is_array()) return proj;

    for (const auto& b : blocks->get<slimr::Json::array_t>()) {
        std::string kind = "paragraph";
        if (auto* k = json::get(b, "kind")) {
            if (k->is_string()) kind = k->get<std::string>();
        }
        if (kind == "paragraph") {
            ProjBlock pb;
            pb.kind = "paragraph";
            if (auto* inlines = json::get(b, "inlines")) {
                if (inlines->is_array()) {
                    for (const auto& inline_elem : inlines->get<slimr::Json::array_t>()) {
                        ProjSpan span;
                        std::string ik = "text";
                        if (auto* ik_val = json::get(inline_elem, "kind")) {
                            if (ik_val->is_string()) ik = ik_val->get<std::string>();
                        }
                        if (ik == "text") {
                            span.kind = "text";
                            if (auto* t_val = json::get(inline_elem, "text")) {
                                if (t_val->is_string()) span.text = t_val->get<std::string>();
                            }
                            if (auto* st = json::get(inline_elem, "style")) {
                                if (st->is_object()) {
                                    if (auto* bold = json::get(*st, "bold")) {
                                        if (bold->is_boolean() && bold->get<bool>()) span.marks.bold = true;
                                    }
                                    if (auto* italic = json::get(*st, "italic")) {
                                        if (italic->is_boolean() && italic->get<bool>()) span.marks.italic = true;
                                    }
                                    if (auto* underline = json::get(*st, "underline")) {
                                        if (underline->is_boolean() && underline->get<bool>()) span.marks.underline = true;
                                    }
                                    if (auto* strikethrough = json::get(*st, "strikethrough")) {
                                        if (strikethrough->is_boolean() && strikethrough->get<bool>()) span.marks.strikethrough = true;
                                    }
                                }
                            }
                        } else if (ik == "atom") {
                            span.kind = "token";
                            TokenRef tr;
                            if (auto* type_val = json::get(inline_elem, "type")) {
                                if (type_val->is_string()) tr.kind = type_val->get<std::string>();
                            }
                            span.token = std::move(tr);
                        }
                        pb.spans.push_back(std::move(span));
                    }
                }
            }
            proj.blocks.push_back(std::move(pb));
        } else if (kind == "table") {
            ProjBlock tb;
            tb.kind = "table";
            ProjTable pt;
            if (auto* rows = json::get(b, "rows")) {
                if (rows->is_array()) {
                    for (const auto& r : rows->get<slimr::Json::array_t>()) {
                        std::vector<std::vector<ProjSpan>> row_spans;
                        if (auto* cells = json::get(r, "cells")) {
                            if (cells->is_array()) {
                                for (const auto& c : cells->get<slimr::Json::array_t>()) {
                                    std::vector<ProjSpan> cell_spans;
                                    if (auto* cell_blocks = json::get(c, "blocks")) {
                                        if (cell_blocks->is_array()) {
                                            for (const auto& cb : cell_blocks->get<slimr::Json::array_t>()) {
                                                if (auto* inlines = json::get(cb, "inlines")) {
                                                    if (inlines->is_array()) {
                                                        for (const auto& inline_elem : inlines->get<slimr::Json::array_t>()) {
                                                            ProjSpan span;
                                                            span.kind = "text";
                                                            if (auto* t_val = json::get(inline_elem, "text")) {
                                                                if (t_val->is_string()) span.text = t_val->get<std::string>();
                                                            }
                                                            cell_spans.push_back(std::move(span));
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    row_spans.push_back(std::move(cell_spans));
                                }
                            }
                        }
                        pt.rows.push_back(std::move(row_spans));
                    }
                }
            }
            tb.table = std::move(pt);
            proj.blocks.push_back(std::move(tb));
        }
    }
    return proj;
}

} // namespace gdocsmith::lens
