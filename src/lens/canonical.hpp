#pragma once

#include <string>
#include <vector>
#include <map>
#include "json.hpp"
#include "lens/json_helper.hpp"
#include "tokens.hpp"
#include "directives.hpp"

namespace gdocsmith::lens {

inline slimr::Json spansCanonical(const std::vector<ProjSpan>& spans) {
    slimr::Json out = slimr::json_arr();
    for (const auto& span : spans) {
        if (span.kind == "text") {
            if (span.text.empty()) continue;
            // Merge with previous if same marks and directive
            auto* arr = out.get_if<slimr::Json::array_t>();
            if (arr && !arr->empty()) {
                auto& last = arr->back();
                auto* last_k = json::get(last, "kind");
                if (last_k && last_k->is_string() && last_k->get<std::string>() == "text") {
                    auto* b_val = json::get(last, "bold");
                    bool last_b = b_val && b_val->is_boolean() && b_val->get<bool>();
                    auto* i_val = json::get(last, "italic");
                    bool last_i = i_val && i_val->is_boolean() && i_val->get<bool>();
                    auto* u_val = json::get(last, "underline");
                    bool last_u = u_val && u_val->is_boolean() && u_val->get<bool>();
                    auto* s_val = json::get(last, "strikethrough");
                    bool last_s = s_val && s_val->is_boolean() && s_val->get<bool>();
                    auto* c_val = json::get(last, "code");
                    bool last_c = c_val && c_val->is_boolean() && c_val->get<bool>();
                    auto* d_val = json::get(last, "directive");
                    std::string last_dir = (d_val && d_val->is_string()) ? d_val->get<std::string>() : "";
                    auto* l_val = json::get(last, "link");
                    std::string last_link = (l_val && l_val->is_string()) ? l_val->get<std::string>() : "";

                    bool same_b = (last_b == span.marks.bold);
                    bool same_i = (last_i == span.marks.italic);
                    bool same_u = (last_u == span.marks.underline);
                    bool same_s = (last_s == span.marks.strikethrough);
                    bool same_c = (last_c == span.marks.code);
                    bool same_dir = (last_dir == span.directive.value_or(""));
                    bool same_link = (last_link == span.marks.link.value_or(""));

                    if (same_b && same_i && same_u && same_s && same_c && same_dir && same_link) {
                        auto* text_val = json::get_mut(last, "text");
                        if (text_val && text_val->is_string()) {
                            text_val->get<std::string>() += span.text;
                        }
                        continue;
                    }
                }
            }
            slimr::Json sj = slimr::json_obj();
            sj["kind"] = "text";
            sj["text"] = span.text;
            if (span.marks.bold) sj["bold"] = true;
            if (span.marks.italic) sj["italic"] = true;
            if (span.marks.underline) sj["underline"] = true;
            if (span.marks.strikethrough) sj["strikethrough"] = true;
            if (span.marks.code) sj["code"] = true;
            if (span.marks.link) sj["link"] = *span.marks.link;
            if (span.directive) sj["directive"] = *span.directive;
            json::push_back(out, std::move(sj));
        } else {
            slimr::Json tj = slimr::json_obj();
            tj["kind"] = "token";
            if (span.token) {
                tj["tokenKind"] = span.token->kind;
                tj["ordinal"] = span.token->ordinal;
            }
            json::push_back(out, std::move(tj));
        }
    }
    return out;
}

inline slimr::Json parsedCanonical(const std::vector<ProjBlock>& blocks) {
    slimr::Json out = slimr::json_arr();
    int group = 0;
    std::string prev_list;

    for (const auto& block : blocks) {
        slimr::Json bj = slimr::json_obj();
        bj["kind"] = block.kind;
        if (block.paragraphStyle) bj["paragraphStyle"] = *block.paragraphStyle;

        if (block.kind == "listItem" && block.listKind) {
            std::string list_look = *block.listKind + ":" + block.listPreset.value_or("");
            if (prev_list != list_look) group++;
            prev_list = list_look;
            bj["group"] = group;
            bj["depth"] = block.listDepth;
            bj["listKind"] = *block.listKind;
            if (block.listPreset) bj["preset"] = *block.listPreset;
            bj["spans"] = spansCanonical(block.spans);
            json::push_back(out, std::move(bj));
            continue;
        }

        prev_list.clear();
        if (block.kind == "codeLine") {
            std::string text;
            for (const auto& s : block.spans) if (s.kind == "text") text += s.text;
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.pop_back();
            bj["text"] = text;
        } else if (block.kind == "heading") {
            bj["headingLevel"] = block.headingLevel.value_or(1);
            bj["spans"] = spansCanonical(block.spans);
        } else if (block.kind == "table" && block.table) {
            slimr::Json tj = slimr::json_obj();
            slimr::Json rows = slimr::json_arr();
            for (const auto& row : block.table->rows) {
                slimr::Json rj = slimr::json_arr();
                for (const auto& cell : row) {
                    json::push_back(rj, spansCanonical(cell));
                }
                json::push_back(rows, std::move(rj));
            }
            tj["rows"] = std::move(rows);
            bj["table"] = std::move(tj);
        } else if (block.kind == "token" && block.token) {
            bj["tokenKind"] = block.token->kind;
            bj["ordinal"] = block.token->ordinal;
        } else {
            bj["spans"] = spansCanonical(block.spans);
        }

        json::push_back(out, std::move(bj));
    }
    return out;
}

} // namespace gdocsmith::lens
