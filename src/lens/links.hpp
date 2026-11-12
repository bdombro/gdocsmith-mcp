#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include "directives.hpp"

namespace gdocsmith::lens {

inline std::string urlEncode(std::string_view value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == '_') {
            escaped << "%5F";
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

inline std::string headingSlug(std::string_view text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    size_t end = text.find_last_not_of(" \t\r\n");
    std::string_view trimmed = text.substr(start, end - start + 1);

    std::string normalized;
    bool in_space = false;
    for (char c : trimmed) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) {
                normalized += '-';
                in_space = true;
            }
        } else {
            normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            in_space = false;
        }
    }
    return urlEncode(normalized);
}

struct LinkTarget {
    std::string kind; // "url", "heading", "tab", "bookmark", "pending"
    std::optional<std::string> url;
    std::optional<std::string> headingId;
    std::optional<std::string> tabId;
    std::optional<std::string> key;

    bool operator==(const LinkTarget& other) const = default;
};

inline std::optional<LinkTarget> documentLinkParse(std::string_view href, std::string_view tabId) {
    bool is_gdoc = href.starts_with("gdoc:");
    bool is_hash = href.starts_with("#") && !href.starts_with("#bookmark=");
    if (!is_gdoc && !is_hash) return std::nullopt;

    return LinkTarget{
        .kind = "pending",
        .tabId = std::string(tabId),
        .key = "file-link:" + std::string(href)
    };
}

} // namespace gdocsmith::lens
