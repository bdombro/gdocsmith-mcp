#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdlib>
#include <filesystem>
#include "directives.hpp"

namespace gdocsmith::lens {

inline constexpr std::string_view IMAGE_UPLOAD_SCHEME = "upload:";
inline constexpr std::string_view IMAGE_DOC_SCHEME = "gdoc:";

inline std::string imageUriResolve(std::string_view href, const std::optional<std::string>& baseDir) {
    if (href.starts_with("https://") || href.starts_with("HTTPS://")) {
        return std::string(href);
    }
    if (href.starts_with("http://") || href.starts_with("HTTP://")) {
        return std::string(IMAGE_UPLOAD_SCHEME) + std::string(href);
    }

    std::string path(href);
    if (path.starts_with("file://") || path.starts_with("FILE://")) {
        path = path.substr(7);
    } else if (path == "~" || path.starts_with("~/")) {
        const char* home = std::getenv("HOME");
        std::string home_str = home ? home : "";
        path = home_str + path.substr(1);
    } else {
        size_t colon = path.find(':');
        if (colon != std::string::npos && colon > 0 && std::isalpha(static_cast<unsigned char>(path[0]))) {
            bool is_scheme = true;
            for (size_t i = 1; i < colon; i++) {
                char c = path[i];
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '.' && c != '-') {
                    is_scheme = false;
                    break;
                }
            }
            if (is_scheme) {
                throw CoreError("unsupportedSyntax", "image \"" + std::string(href) + "\" is neither a URL nor a local file path");
            }
        }
    }

    std::filesystem::path p(path);
    if (!p.is_absolute()) {
        if (!baseDir.has_value()) {
            throw CoreError("unsupportedSyntax", "a relative image path (\"" + std::string(href) + "\") needs an imported file to resolve against");
        }
        p = std::filesystem::path(*baseDir) / p;
    }
    return std::string(IMAGE_UPLOAD_SCHEME) + p.lexically_normal().string();
}

struct ImageSource {
    std::optional<std::string> label;
    std::vector<std::string> lost;
    struct SourceLoc {
        std::string docId;
        std::string key;
        std::string tabId;
        bool operator==(const SourceLoc& other) const = default;
    } source;

    bool operator==(const ImageSource& other) const = default;
};

} // namespace gdocsmith::lens
