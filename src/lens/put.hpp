#pragma once

#include <string>
#include <vector>
#include "json.hpp"
#include "lens/json_helper.hpp"
#include "tokens.hpp"
#include "project.hpp"

namespace gdocsmith::lens {

struct WriteReport {
    bool changed{false};
};

inline WriteReport markdownPut(slimr::Json& tab, const Projection& parsed) {
    WriteReport report;
    slimr::Json new_blocks = slimr::json_arr();

    for (const auto& b : parsed.blocks) {
        if (b.kind == "paragraph" || b.kind == "heading" || b.kind == "listItem") {
            slimr::Json pb = slimr::json_obj();
            pb["kind"] = "paragraph";
            slimr::Json inlines = slimr::json_arr();
            for (const auto& span : b.spans) {
                if (span.kind == "text") {
                    slimr::Json inj = slimr::json_obj();
                    inj["kind"] = "text";
                    inj["text"] = span.text;
                    if (!span.marks.empty()) {
                        slimr::Json st = slimr::json_obj();
                        if (span.marks.bold) st["bold"] = true;
                        if (span.marks.italic) st["italic"] = true;
                        if (span.marks.underline) st["underline"] = true;
                        if (span.marks.strikethrough) st["strikethrough"] = true;
                        inj["style"] = std::move(st);
                    }
                    json::push_back(inlines, std::move(inj));
                }
            }
            pb["inlines"] = std::move(inlines);
            json::push_back(new_blocks, std::move(pb));
        } else if (b.kind == "table" && b.table) {
            slimr::Json tb = slimr::json_obj();
            tb["kind"] = "table";
            slimr::Json rows = slimr::json_arr();
            for (const auto& r : b.table->rows) {
                slimr::Json rj = slimr::json_obj();
                slimr::Json cells = slimr::json_arr();
                for (const auto& c : r) {
                    slimr::Json cj = slimr::json_obj();
                    slimr::Json c_blocks = slimr::json_arr();
                    slimr::Json pb = slimr::json_obj();
                    pb["kind"] = "paragraph";
                    slimr::Json inlines = slimr::json_arr();
                    for (const auto& span : c) {
                        slimr::Json inj = slimr::json_obj();
                        inj["kind"] = "text";
                        inj["text"] = span.text;
                        json::push_back(inlines, std::move(inj));
                    }
                    pb["inlines"] = std::move(inlines);
                    json::push_back(c_blocks, std::move(pb));
                    cj["blocks"] = std::move(c_blocks);
                    json::push_back(cells, std::move(cj));
                }
                rj["cells"] = std::move(cells);
                json::push_back(rows, std::move(rj));
            }
            tb["rows"] = std::move(rows);
            json::push_back(new_blocks, std::move(tb));
        }
    }

    auto* cur_blocks = json::get(tab, "blocks");
    if (!cur_blocks || *cur_blocks != new_blocks) {
        tab["blocks"] = std::move(new_blocks);
        report.changed = true;
    }
    return report;
}

} // namespace gdocsmith::lens
