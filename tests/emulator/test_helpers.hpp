#pragma once

#include "emulator/types.hpp"
#include "emulator/document_state.hpp"
#include "emulator/document_emulator.hpp"
#include "emulator/google_docs_emulator.hpp"
#include "emulator/conformance.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::emulator::test_util {

inline slimr::Json make_simple_doc(const std::vector<std::string>& paras, std::string_view tab_id = "t.0") {
    slimr::Json doc = blank_doc_template();
    slimr::Json::array_t content;
    // Section break at 0..1
    content.push_back(slimr::Json::object_t{
        {"startIndex", static_cast<int64_t>(0)},
        {"endIndex", static_cast<int64_t>(1)},
        {"sectionBreak", slimr::Json::object_t{
            {"sectionStyle", slimr::Json::object_t{{"sectionType", "CONTINUOUS"}}}
        }}
    });
    int cur_idx = 1;
    for (const auto& text : paras) {
        int text_len = static_cast<int>(text.size()) + 1; // including '\n'
        content.push_back(slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(cur_idx)},
            {"endIndex", static_cast<int64_t>(cur_idx + text_len)},
            {"paragraph", slimr::Json::object_t{
                {"elements", slimr::Json::array_t{
                    slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(cur_idx)},
                        {"endIndex", static_cast<int64_t>(cur_idx + text_len)},
                        {"textRun", slimr::Json::object_t{
                            {"content", text + "\n"},
                            {"textStyle", slimr::Json::object_t{}}
                        }}
                    }
                }},
                {"paragraphStyle", slimr::Json::object_t{{"namedStyleType", "NORMAL_TEXT"}}}
            }}
        });
        cur_idx += text_len;
    }
    doc["tabs"].get<slimr::Json::array_t>()[0]["documentTab"]["body"]["content"] = std::move(content);
    return doc;
}

inline std::string doc_plain_text(const slimr::Json& doc, size_t tab_idx = 0) {
    std::string out;
    if (!doc.is_object() || !doc.contains("tabs") || !doc["tabs"].is_array()) return out;
    const auto& tabs = doc["tabs"].get<slimr::Json::array_t>();
    if (tab_idx >= tabs.size()) return out;
    const auto& dt = tabs[tab_idx]["documentTab"];
    if (!dt.is_object() || !dt.contains("body") || !dt["body"].is_object()) return out;
    const auto& content = dt["body"]["content"];
    if (!content.is_array()) return out;
    for (const auto& el : content.get<slimr::Json::array_t>()) {
        if (el.contains("paragraph") && el["paragraph"].contains("elements") &&
            el["paragraph"]["elements"].is_array()) {
            for (const auto& pe : el["paragraph"]["elements"].get<slimr::Json::array_t>()) {
                if (pe.contains("textRun") && pe["textRun"].contains("content")) {
                    out += pe["textRun"]["content"].get<std::string>();
                }
            }
        }
    }
    return out;
}

} // namespace gdocsmith::emulator::test_util
