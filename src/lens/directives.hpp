#pragma once

#include "model/errors.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace gdocsmith::lens {

using model::CoreError;

struct DirectiveAttrs {
    std::vector<std::string> classes;
    std::vector<std::string> flags;
    std::vector<std::pair<std::string, std::string>> values;

    std::optional<std::string> get_value(const std::string& key) const {
        for (const auto& [k, v] : values) {
            if (k == key) return v;
        }
        return std::nullopt;
    }

    bool has_value(const std::string& key) const {
        for (const auto& [k, v] : values) {
            if (k == key) return true;
        }
        return false;
    }

    bool has_flag(const std::string& flag) const {
        return std::find(flags.begin(), flags.end(), flag) != flags.end();
    }

    bool has_class(const std::string& cls) const {
        return std::find(classes.begin(), classes.end(), cls) != classes.end();
    }

    bool operator==(const DirectiveAttrs& other) const = default;
};

struct InlineDirective {
    DirectiveAttrs attrs;
    std::optional<std::string> label;
    std::string name;
    std::string raw;

    bool operator==(const InlineDirective& other) const = default;
};

struct LineDirective {
    DirectiveAttrs attrs;
    int colons{0};
    std::optional<std::string> label;
    std::string name;

    bool operator==(const LineDirective& other) const = default;
};

inline DirectiveAttrs directiveAttrsParse(std::string_view src) {
    DirectiveAttrs attrs;
    size_t at = 0;
    auto fail = [&](std::string_view why) {
        throw CoreError("unsupportedSyntax", "invalid directive attributes {" + std::string(src) + "}: " + std::string(why));
    };

    while (at < src.size()) {
        if (std::isspace(static_cast<unsigned char>(src[at]))) {
            at++;
            continue;
        }
        if (src[at] == '.') {
            size_t start = at + 1;
            size_t cur = start;
            if (cur >= src.size() || !std::isalnum(static_cast<unsigned char>(src[cur]))) {
                fail("\".\" must start a style name");
            }
            while (cur < src.size() && (std::isalnum(static_cast<unsigned char>(src[cur])) ||
                   src[cur] == '_' || src[cur] == '+' || src[cur] == '.' || src[cur] == '-')) {
                cur++;
            }
            attrs.classes.push_back(std::string(src.substr(start, cur - start)));
            at = cur;
        } else {
            size_t start = at;
            if (!std::isalpha(static_cast<unsigned char>(src[start]))) {
                fail(std::string("unexpected \"") + src[start] + "\"");
            }
            size_t cur = start + 1;
            while (cur < src.size() && std::isalnum(static_cast<unsigned char>(src[cur]))) {
                cur++;
            }
            std::string name(src.substr(start, cur - start));
            at = cur;
            if (at >= src.size() || src[at] != '=') {
                if (attrs.has_value(name) || attrs.has_flag(name)) {
                    fail("duplicate \"" + name + "\"");
                }
                attrs.flags.push_back(name);
            } else {
                at++; // Skip '='
                std::string value;
                if (at < src.size() && src[at] == '"') {
                    size_t q_cur = at + 1;
                    bool closed = false;
                    std::string unescaped;
                    while (q_cur < src.size()) {
                        if (src[q_cur] == '\\') {
                            q_cur++;
                            if (q_cur >= src.size()) break;
                            char c = src[q_cur];
                            if (c == '"') unescaped += '"';
                            else if (c == '\\') unescaped += '\\';
                            else if (c == 'n') unescaped += '\n';
                            else if (c == 'r') unescaped += '\r';
                            else if (c == 't') unescaped += '\t';
                            else unescaped += c;
                            q_cur++;
                        } else if (src[q_cur] == '"') {
                            closed = true;
                            q_cur++;
                            break;
                        } else {
                            unescaped += src[q_cur];
                            q_cur++;
                        }
                    }
                    if (!closed) {
                        fail("unterminated quote in \"" + name + "\"");
                    }
                    value = std::move(unescaped);
                    at = q_cur;
                } else {
                    size_t v_start = at;
                    size_t v_cur = v_start;
                    while (v_cur < src.size()) {
                        char c = src[v_cur];
                        if (std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '{' || c == '}' ||
                            c == '[' || c == ']' || c == '=') {
                            break;
                        }
                        v_cur++;
                    }
                    if (v_cur == v_start) {
                        fail("\"" + name + "=\" needs a value (quote an empty one as \"\")");
                    }
                    value = std::string(src.substr(v_start, v_cur - v_start));
                    at = v_cur;
                }
                if (attrs.has_value(name) || attrs.has_flag(name)) {
                    fail("duplicate \"" + name + "\"");
                }
                attrs.values.push_back({name, value});
            }
        }
        if (at < src.size() && !std::isspace(static_cast<unsigned char>(src[at]))) {
            fail("attributes must be separated by spaces");
        }
    }
    return attrs;
}

inline std::string directiveValueRender(std::string_view value) {
    if (value.empty()) return "\"\"";
    bool needs_quote = false;
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == '=' || c == '\\') {
            needs_quote = true;
            break;
        }
    }
    if (!needs_quote) return std::string(value);
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    out += '"';
    return out;
}

inline std::string directiveAttrsRender(const DirectiveAttrs& attrs) {
    std::vector<std::string> parts;
    for (const auto& cls : attrs.classes) {
        parts.push_back("." + cls);
    }
    for (const auto& [name, val] : attrs.values) {
        parts.push_back(name + "=" + directiveValueRender(val));
    }
    for (const auto& flag : attrs.flags) {
        parts.push_back(flag);
    }
    if (parts.empty()) return "";
    std::string res = "{";
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) res += " ";
        res += parts[i];
    }
    res += "}";
    return res;
}

inline std::string directiveLabelEscape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '\\' || c == '[' || c == ']') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

inline std::string directiveLabelUnescape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            out += text[i + 1];
            i++;
        } else {
            out += text[i];
        }
    }
    return out;
}

inline int bracketEnd(std::string_view src, size_t open) {
    int depth = 0;
    for (size_t i = open; i < src.size(); i++) {
        if (src[i] == '\\') {
            i++;
        } else if (src[i] == '[') {
            depth++;
        } else if (src[i] == ']') {
            if (--depth == 0) return static_cast<int>(i);
        }
    }
    return -1;
}

inline int braceEnd(std::string_view src, size_t open) {
    for (size_t i = open + 1; i < src.size(); i++) {
        if (src[i] == '"') {
            i++;
            while (i < src.size() && src[i] != '"') {
                if (src[i] == '\\') i++;
                i++;
            }
        } else if (src[i] == '{') {
            return -1;
        } else if (src[i] == '}') {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline std::optional<InlineDirective> inlineDirectiveMatch(std::string_view src, const std::unordered_set<std::string>& names) {
    if (src.empty() || src[0] != ':') return std::nullopt;
    size_t i = 1;
    if (i >= src.size() || !std::isalpha(static_cast<unsigned char>(src[i]))) return std::nullopt;
    size_t start_name = i;
    while (i < src.size() && std::isalnum(static_cast<unsigned char>(src[i]))) i++;
    std::string name(src.substr(start_name, i - start_name));
    if (!names.contains(name)) return std::nullopt;

    std::optional<std::string> label;
    if (i < src.size() && src[i] == '[') {
        int end = bracketEnd(src, i);
        if (end < 0) return std::nullopt;
        label = std::string(src.substr(i + 1, end - (i + 1)));
        i = end + 1;
    }
    DirectiveAttrs attrs;
    if (i < src.size() && src[i] == '{') {
        int end = braceEnd(src, i);
        if (end < 0) return std::nullopt;
        try {
            attrs = directiveAttrsParse(src.substr(i + 1, end - (i + 1)));
        } catch (...) {
            return std::nullopt;
        }
        i = end + 1;
    } else if (!label.has_value()) {
        return std::nullopt;
    }
    return InlineDirective{
        .attrs = std::move(attrs),
        .label = std::move(label),
        .name = std::move(name),
        .raw = std::string(src.substr(0, i))
    };
}

inline int inlineDirectiveStart(std::string_view src, const std::unordered_set<std::string>& names, bool afterColon = false) {
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] != ':') continue;
        if (i > 0 && src[i - 1] == '\\') continue;
        if (!afterColon && i > 0 && src[i - 1] == ':') continue;

        size_t j = i + 1;
        if (j >= src.size() || !std::isalpha(static_cast<unsigned char>(src[j]))) continue;
        size_t start_name = j;
        while (j < src.size() && std::isalnum(static_cast<unsigned char>(src[j]))) j++;
        if (j >= src.size() || (src[j] != '[' && src[j] != '{')) continue;
        std::string name(src.substr(start_name, j - start_name));
        if (names.contains(name)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline std::optional<LineDirective> lineDirectiveMatch(std::string_view line, const std::unordered_set<std::string>& names) {
    size_t end_idx = line.size();
    while (end_idx > 0 && (line[end_idx - 1] == ' ' || line[end_idx - 1] == '\t' ||
                          line[end_idx - 1] == '\r' || line[end_idx - 1] == '\n')) {
        end_idx--;
    }
    std::string_view text = line.substr(0, end_idx);

    size_t colons = 0;
    while (colons < text.size() && text[colons] == ':') colons++;
    if (colons < 2) return std::nullopt;

    size_t start_name = colons;
    if (start_name >= text.size() || !std::isalpha(static_cast<unsigned char>(text[start_name]))) return std::nullopt;
    size_t cur = start_name + 1;
    while (cur < text.size() && std::isalnum(static_cast<unsigned char>(text[cur]))) cur++;
    std::string name(text.substr(start_name, cur - start_name));
    if (!names.contains(name)) return std::nullopt;

    std::optional<std::string> label;
    if (cur < text.size() && text[cur] == '[') {
        int b_end = bracketEnd(text, cur);
        if (b_end < 0) {
            throw CoreError("unsupportedSyntax", "unclosed [ in \"" + std::string(text) + "\"");
        }
        label = std::string(text.substr(cur + 1, b_end - (cur + 1)));
        cur = b_end + 1;
    }

    DirectiveAttrs attrs;
    if (cur < text.size() && text[cur] == '{') {
        int b_end = braceEnd(text, cur);
        if (b_end < 0) {
            throw CoreError("unsupportedSyntax", "unclosed { in \"" + std::string(text) + "\"");
        }
        attrs = directiveAttrsParse(text.substr(cur + 1, b_end - (cur + 1)));
        cur = b_end + 1;
    }

    if (cur != text.size()) {
        throw CoreError("unsupportedSyntax", "unexpected text after directive in \"" + std::string(text) + "\"");
    }

    return LineDirective{
        .attrs = std::move(attrs),
        .colons = static_cast<int>(colons),
        .label = std::move(label),
        .name = std::move(name)
    };
}

} // namespace gdocsmith::lens
