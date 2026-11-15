#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <regex>
#include <limits>
#include <memory>
#include "directives.hpp"
#include "model/symbols.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace gdocsmith::lens {

inline std::string linkUnicodeNormalize(std::string_view text) {
#ifdef __APPLE__
    struct Release { void operator()(CFTypeRef value) const { CFRelease(value); } };
    using String = std::unique_ptr<std::remove_pointer_t<CFStringRef>, Release>;
    using MutableString = std::unique_ptr<std::remove_pointer_t<CFMutableStringRef>, Release>;
    if (text.size() > static_cast<size_t>(std::numeric_limits<CFIndex>::max())) {
        throw model::CoreError("unsupportedSyntax", "link text is too long");
    }
    String source(CFStringCreateWithBytes(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.data()), static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
    if (!source) throw model::CoreError("unsupportedSyntax", "link text is not valid UTF-8");
    MutableString normalized(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, source.get()));
    if (!normalized) throw model::CoreError("unsupportedSyntax", "cannot normalize link text");
    CFStringNormalize(normalized.get(), kCFStringNormalizationFormC);
    CFStringLowercase(normalized.get(), nullptr);
    const auto maximum = CFStringGetMaximumSizeForEncoding(CFStringGetLength(normalized.get()), kCFStringEncodingUTF8);
    if (maximum < 0 || maximum == std::numeric_limits<CFIndex>::max()) {
        throw model::CoreError("unsupportedSyntax", "link text is too long");
    }
    std::vector<char> buffer(static_cast<size_t>(maximum) + 1);
    if (!CFStringGetCString(normalized.get(), buffer.data(), maximum + 1, kCFStringEncodingUTF8)) {
        throw model::CoreError("unsupportedSyntax", "cannot normalize link text");
    }
    return std::string(buffer.data());
#else
    std::string result(text);
    for (auto& ch : result) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return result;
#endif
}

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
    std::string normalized;
    bool in_space = false;
    for (const auto& cp : model::utf8CodePoints(linkUnicodeNormalize(text))) {
        const bool whitespace = (cp.size() == 1 && (cp[0] == ' ' || (cp[0] >= '\t' && cp[0] <= '\r'))) ||
            cp == "\u00A0" || cp == "\u1680" || (cp >= "\u2000" && cp <= "\u200A") ||
            cp == "\u2028" || cp == "\u2029" || cp == "\u202F" || cp == "\u205F" ||
            cp == "\u3000" || cp == "\uFEFF";
        if (whitespace) {
            in_space = !normalized.empty();
        } else {
            if (in_space) normalized += '-';
            normalized += cp;
            in_space = false;
        }
    }
    auto encoded = urlEncode(normalized);
    for (const auto& [escape, character] : {std::pair{"%21", "!"}, {"%27", "'"},
                                           {"%28", "("}, {"%29", ")"}, {"%2A", "*"}}) {
        size_t at = 0;
        while ((at = encoded.find(escape, at)) != std::string::npos) {
            encoded.replace(at, 3, character);
            ++at;
        }
    }
    return encoded;
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
    static const std::regex slugUrl(R"(^https://docs\.google\.com/document/d/[^#]*#[^=]+$)");
    if (!is_gdoc && !is_hash && !std::regex_match(std::string(href), slugUrl)) return std::nullopt;

    return LinkTarget{
        .kind = "pending",
        .tabId = std::string(tabId),
        .key = "file-link:" + std::string(href)
    };
}

inline model::JsonObject documentLinkStyle(std::string_view href, std::string_view tabId) {
    if (const auto pending = documentLinkParse(href, tabId)) {
        return slimr::Json::object_t{{"heading", slimr::Json::object_t{
            {"key", pending->key.value_or("")}, {"tabId", std::string(tabId)}}}};
    }
    if (href.starts_with("#bookmark=")) return slimr::Json::object_t{{"bookmarkId", std::string(href.substr(10))}};
    return slimr::Json::object_t{{"url", std::string(href)}};
}

namespace link_detail {

inline std::string text(const model::ParagraphBlock& paragraph) {
    std::string result;
    for (const auto& item : paragraph.inlines) result += item.is_text() ? item.text : "\uFFFC";
    return result;
}

inline std::string normalized(std::string_view fragment) {
    std::string result;
    auto digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < fragment.size(); ++i) {
        auto ch = static_cast<unsigned char>(fragment[i]);
        if (ch == '%') {
            if (i + 2 >= fragment.size() || digit(fragment[i + 1]) < 0 || digit(fragment[i + 2]) < 0) {
                throw model::CoreError("unsupportedSyntax", "invalid encoded link fragment");
            }
            ch = static_cast<unsigned char>(digit(fragment[i + 1]) * 16 + digit(fragment[i + 2]));
            i += 2;
        }
        result += static_cast<char>(std::tolower(ch));
    }
    return linkUnicodeNormalize(result);
}

inline const model::TabModel& tab(const model::DocModel& document, const model::TabModel& source,
                                  const std::string& fragment, std::string& title) {
    const auto separator = fragment.find("__");
    if (separator == std::string::npos) { title = fragment; return source; }
    const auto requested = normalized(fragment.substr(0, separator));
    const model::TabModel* result = nullptr;
    for (const auto& candidate : document.tabs) {
        if (normalized(headingSlug(candidate.title)) != requested) continue;
        if (result) throw model::CoreError("anchorAmbiguous", "tab link is ambiguous; use a unique tab title");
        result = &candidate;
    }
    if (!result) throw model::CoreError("linkTargetNotFound", "tab link has no target; use a unique tab title");
    title = fragment.substr(separator + 2);
    return *result;
}

inline const model::ParagraphBlock& heading(const model::TabModel& tab, const std::string& title) {
    const model::ParagraphBlock* result = nullptr;
    for (const auto& block : tab.blocks) {
        if (!block.is_paragraph()) continue;
        const auto& paragraph = block.as_paragraph();
        if (!paragraph.style.contains("namedStyleType") || !paragraph.style["namedStyleType"].is_string() ||
            !paragraph.style["namedStyleType"].get<std::string>().starts_with("HEADING_") ||
            normalized(headingSlug(text(paragraph))) != normalized(title)) continue;
        if (result) throw model::CoreError("anchorAmbiguous", "heading link is ambiguous; use a unique heading title");
        result = &paragraph;
    }
    if (!result) throw model::CoreError("linkTargetNotFound", "heading link has no target; use a unique heading title");
    return *result;
}

inline model::JsonObject resolve(const model::DocModel& document, const model::TabModel& source,
                                 const std::vector<model::DocModel>& others, const std::string& href) {
    if (href.starts_with("https:")) throw model::CoreError("unsupportedSyntax", "new Google Docs slug URLs must use the gdoc: link grammar");
    const model::DocModel* target = &document;
    const model::TabModel* initial = &source;
    std::string fragment;
    bool external = false;
    if (href.starts_with("gdoc:")) {
        const auto hash = href.find('#');
        const auto docId = href.substr(5, hash == std::string::npos ? std::string::npos : hash - 5);
        static const std::regex docPattern(R"(^[A-Za-z0-9_-]+$)");
        if (!std::regex_match(docId, docPattern)) throw model::CoreError("unsupportedSyntax", "gdoc: link needs a document ID");
        if (docId != document.docId) {
            const auto found = std::find_if(others.begin(), others.end(), [&](const auto& other) { return other.docId == docId; });
            if (found == others.end()) throw model::CoreError("linkTargetNotFound", "gdoc: link names no readable document");
            target = &*found;
            external = true;
        }
        if (target->tabs.empty()) throw model::CoreError("linkTargetNotFound", "gdoc: link names no readable document");
        initial = &target->tabs.front();
        if (hash == std::string::npos) {
            if (external) return slimr::Json::object_t{{"url", "https://docs.google.com/document/d/" + docId + "/edit"}};
            return slimr::Json::object_t{{"tabId", initial->tabId}};
        }
        fragment = href.substr(hash + 1);
    } else fragment = href.substr(1);
    std::string title;
    const auto& selected = tab(*target, *initial, fragment, title);
    if (fragment.find("__") != std::string::npos && title.empty()) {
        if (external) return slimr::Json::object_t{{"url", "https://docs.google.com/document/d/" + target->docId + "/edit?tab=" + selected.tabId}};
        return slimr::Json::object_t{{"tabId", selected.tabId}};
    }
    const auto& found = heading(selected, title);
    if (external) {
        if (!found.headingId) throw model::CoreError("linkTargetNotFound", "external heading has no provider-assigned ID");
        return slimr::Json::object_t{{"url", "https://docs.google.com/document/d/" + target->docId + "/edit?tab=" + selected.tabId + "#heading=" + *found.headingId}};
    }
    return slimr::Json::object_t{{"heading", slimr::Json::object_t{
        {found.headingId ? "id" : "key", found.headingId.value_or(found.key)}, {"tabId", selected.tabId}}}};
}

inline void resolveBlocks(std::vector<model::Block>& blocks, const model::DocModel& document,
                          const model::TabModel& source, const std::vector<model::DocModel>& others) {
    for (auto& block : blocks) {
        if (block.is_table()) {
            for (auto& row : block.as_table().rows) for (auto& cell : row.cells) resolveBlocks(cell.blocks, document, source, others);
        } else if (block.is_paragraph()) {
            for (auto& item : block.as_paragraph().inlines) {
                if (!item.style.contains("link") || !item.style["link"].contains("heading")) continue;
                const auto& pending = item.style["link"]["heading"];
                if (!pending.contains("key") || !pending["key"].is_string()) continue;
                const auto key = pending["key"].get<std::string>();
                if (!key.starts_with("file-link:")) continue;
                item.style["link"] = resolve(document, source, others, key.substr(10));
            }
        }
    }
}

} // namespace link_detail

inline void documentLinksResolve(model::DocModel& document, const std::vector<model::DocModel>& others = {}) {
    const auto snapshot = document;
    for (auto& tab : document.tabs) link_detail::resolveBlocks(tab.blocks, snapshot, tab, others);
}

inline std::optional<std::string> documentLinkHref(const LinkTarget& target, const model::DocModel& document,
                                                 const model::TabModel& source) {
    if (target.kind == "bookmark") return std::nullopt;
    if (target.kind == "url") {
        if (!target.url) return std::nullopt;
        static const std::regex googleUrl(
            R"(^[A-Za-z][A-Za-z0-9+.-]*://docs\.google\.com(?::[0-9]+)?(/[^?#]*)(?:\?([^#]*))?(?:#(.*))?$)",
            std::regex::icase);
        std::smatch parsed;
        if (!std::regex_match(*target.url, parsed, googleUrl)) return std::nullopt;
        const auto path = parsed[1].str();
        const auto first = path.find('/', 1);
        const auto second = first == std::string::npos ? std::string::npos : path.find('/', first + 1);
        if (second == std::string::npos) return std::nullopt;
        const auto end = path.find('/', second + 1);
        if (path.substr(second + 1, end == std::string::npos ? std::string::npos : end - second - 1) != document.docId) {
            return std::nullopt;
        }
        auto decode = [](std::string value) {
            auto digit = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            std::string result;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '%' && i + 2 < value.size() && digit(value[i + 1]) >= 0 && digit(value[i + 2]) >= 0) {
                    result += static_cast<char>(digit(value[i + 1]) * 16 + digit(value[i + 2]));
                    i += 2;
                } else result += value[i] == '+' ? ' ' : value[i];
            }
            return result;
        };
        auto tabId = source.tabId;
        const auto query = parsed[2].str();
        size_t start = 0;
        while (start <= query.size()) {
            const auto next = query.find('&', start);
            const auto pair = query.substr(start, next == std::string::npos ? std::string::npos : next - start);
            const auto equals = pair.find('=');
            if (decode(pair.substr(0, equals)) == "tab") {
                tabId = equals == std::string::npos ? "" : decode(pair.substr(equals + 1));
                break;
            }
            if (next == std::string::npos) break;
            start = next + 1;
        }
        const auto fragment = parsed[3].str();
        return documentLinkHref(LinkTarget{
            .kind = fragment.starts_with("heading=") ? "heading" : "tab",
            .headingId = fragment.starts_with("heading=") ? std::optional(fragment.substr(8)) : std::nullopt,
            .tabId = tabId
        }, document, source);
    }
    if (target.kind == "pending" && target.key && target.key->starts_with("file-link:")) return target.key->substr(10);
    const auto tabId = target.tabId.value_or(source.tabId);
    const auto tab = std::find_if(document.tabs.begin(), document.tabs.end(), [&](const auto& candidate) { return candidate.tabId == tabId; });
    const auto tabSlug = headingSlug(tab == document.tabs.end() ? "missing-tab" : tab->title);
    if (target.kind == "tab") return "#" + tabSlug + "__";
    const model::ParagraphBlock* paragraph = nullptr;
    if (tab != document.tabs.end()) for (const auto& block : tab->blocks) {
        if (!block.is_paragraph()) continue;
        const auto& candidate = block.as_paragraph();
        if ((target.headingId && candidate.headingId == target.headingId) ||
            (target.key && candidate.key == *target.key)) { paragraph = &candidate; break; }
    }
    return "#" + (tabId == source.tabId ? "" : tabSlug + "__") +
        headingSlug(paragraph ? link_detail::text(*paragraph) : "missing-heading");
}

} // namespace gdocsmith::lens
