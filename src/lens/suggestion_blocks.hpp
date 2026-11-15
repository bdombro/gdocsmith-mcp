#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "json.hpp"
#include "directives.hpp"

namespace gdocsmith::lens {

inline std::string suggestionSnippetTrim(std::string text) {
    const auto first = text.find_first_not_of("\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of("\r\n");
    return text.substr(first, last - first + 1);
}

struct SuggestionBlock {
    std::string id;
    std::string kind; // "cellStyle", "list", "namedStyle", "object", "pageLayout", "paragraphStyle", "table", "text", "textStyle"
    int tabIndex{0};
    std::optional<std::string> segment;
    std::optional<std::string> before;
    std::optional<std::string> after;
    std::optional<std::string> notice;
    bool isReject{false};
    int line{1};

    bool operator==(const SuggestionBlock& other) const = default;
};

inline std::string suggestionBlockRender(const SuggestionBlock& block, bool legacy = true,
                                         const std::string& tab = {}, std::optional<int> exportLine = {},
                                         const std::optional<std::string>& exportLines = {}) {
    std::string out = (legacy ? ":::suggestion{id=" : "::suggestion{id=") + directiveValueRender(block.id) + " kind=" + block.kind;
    if (!legacy && !tab.empty()) out += " tab=" + directiveValueRender(tab);
    if (!legacy && exportLines) out += " exportLines=" + *exportLines;
    else if (!legacy && exportLine) out += " exportLine=" + std::to_string(*exportLine);
    if (block.segment) {
        out += " segment=" + directiveValueRender(*block.segment);
    }
    if (block.isReject) {
        out += " reject";
    }
    out += "}\n";
    if (block.notice) {
        out += *block.notice + "\n";
    } else {
        if (block.before) {
            out += "::before\n" + *block.before;
            if (!block.before->ends_with('\n')) out += "\n";
        }
        if (block.after) {
            out += "::after\n" + *block.after;
            if (!block.after->ends_with('\n')) out += "\n";
        }
    }
    if (legacy) out += ":::\n";
    return out;
}

} // namespace gdocsmith::lens
