#pragma once

#include "directives.hpp"
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::lens {

struct CommentReply {
    std::optional<std::string> author;
    std::optional<std::string> id;
    bool mine{false};
    bool isDelete{false};
    int line{1};
    std::string text;

    bool operator==(const CommentReply &other) const = default;
};

struct CommentBlock {
    std::string id;
    std::optional<std::string> author;
    std::optional<std::string> quote;
    std::string text;
    bool resolved{false};
    bool mine{false};
    bool orphaned{false};
    bool ambiguous{false};
    bool isDelete{false};
    int line{1};
    std::vector<CommentReply> replies;

    bool operator==(const CommentBlock &other) const = default;
};

inline std::string commentTextEscape(std::string_view text) {
    std::istringstream input{std::string(text)};
    std::string out, line;
    while (std::getline(input, line)) {
        const auto first = line.find_first_not_of('\\');
        if (first != std::string::npos && line.substr(first).starts_with("::"))
            out += '\\';
        out += line + "\n";
    }
    return out;
}

inline std::string commentBlockRender(const CommentBlock &cb, bool legacy = true) {
    if (!legacy) {
        DirectiveAttrs attrs;
        if (!cb.id.empty())
            attrs.values.emplace_back("id", cb.id);
        if (cb.quote)
            attrs.values.emplace_back("quote", *cb.quote);
        if (cb.author)
            attrs.values.emplace_back("author", *cb.author);
        if (cb.resolved)
            attrs.flags.push_back("resolved");
        if (cb.orphaned)
            attrs.flags.push_back("orphaned");
        if (cb.ambiguous)
            attrs.flags.push_back("ambiguous");
        if (cb.mine)
            attrs.flags.push_back("mine");
        if (cb.isDelete)
            attrs.flags.push_back("delete");
        auto out = "::comment" + directiveAttrsRender(attrs) + "\n" + commentTextEscape(cb.text);
        if (cb.text.empty())
            out += "\n";
        for (const auto &reply : cb.replies) {
            DirectiveAttrs replyAttrs;
            if (reply.id)
                replyAttrs.values.emplace_back("id", *reply.id);
            if (reply.author)
                replyAttrs.values.emplace_back("author", *reply.author);
            if (reply.mine)
                replyAttrs.flags.push_back("mine");
            if (reply.isDelete)
                replyAttrs.flags.push_back("delete");
            out += "::reply" + directiveAttrsRender(replyAttrs) + "\n" + commentTextEscape(reply.text);
            if (reply.text.empty())
                out += "\n";
        }
        return out;
    }
    std::string out = ":::comment{id=" + directiveValueRender(cb.id);
    if (cb.author)
        out += " author=" + directiveValueRender(*cb.author);
    if (cb.resolved)
        out += " resolved";
    if (cb.orphaned)
        out += " orphaned";
    if (cb.ambiguous)
        out += " ambiguous";
    if (cb.mine)
        out += " mine";
    if (cb.isDelete)
        out += " delete";
    out += "}\n";

    if (cb.quote && !cb.quote->empty()) {
        std::stringstream ss(*cb.quote);
        std::string ql;
        while (std::getline(ss, ql)) {
            out += "> " + ql + "\n";
        }
        out += "\n";
    }

    out += cb.text;
    if (!cb.text.ends_with('\n'))
        out += "\n";

    for (const auto &rep : cb.replies) {
        out += "\n::reply";
        bool has_attrs = rep.author.has_value() || rep.id.has_value() || rep.mine || rep.isDelete;
        if (has_attrs) {
            out += "{";
            bool first = true;
            if (rep.author) {
                out += "author=" + directiveValueRender(*rep.author);
                first = false;
            }
            if (rep.id) {
                if (!first)
                    out += " ";
                out += "id=" + directiveValueRender(*rep.id);
                first = false;
            }
            if (rep.mine) {
                if (!first)
                    out += " ";
                out += "mine";
                first = false;
            }
            if (rep.isDelete) {
                if (!first)
                    out += " ";
                out += "delete";
            }
            out += "}";
        }
        out += "\n" + rep.text;
        if (!rep.text.ends_with('\n'))
            out += "\n";
    }

    out += ":::\n";
    return out;
}

} // namespace gdocsmith::lens
