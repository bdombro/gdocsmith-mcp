#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "json.hpp"
#include "directives.hpp"

namespace gdocsmith::lens {

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

inline std::string suggestionBlockRender(const SuggestionBlock& block) {
    std::string out = ":::suggestion{id=" + directiveValueRender(block.id) + " kind=" + block.kind;
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
    out += ":::\n";
    return out;
}

} // namespace gdocsmith::lens
