#pragma once

#include <string>
#include <string_view>
#include <regex>

namespace gdocsmith::lens {

inline constexpr std::string_view HIDDEN_CHIP = "\uE907";

inline std::string hiddenChipKindOf(std::string_view rendered) {
    auto trim = [](std::string_view s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return std::string_view("");
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    };

    std::string_view text = trim(rendered);
    if (text.starts_with("⏱")) return "stopwatch";
    if (text.starts_with("⌛")) return "timer";
    if (text == "Person" || text == "Date" || text == "Place" || text == "File" || text == "Calendar event") {
        return "placeholder";
    }
    if (text == "Listen to this tab") return "listen";

    // Vote regex: ^(?:\(\S+ \d+\)\s*)+$
    static const std::regex vote_re(R"(^(?:\(\S+ \d+\)\s*)+$)");
    if (std::regex_match(std::string(text), vote_re)) return "vote";

    return "unknown";
}

} // namespace gdocsmith::lens
