#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include "directives.hpp"

namespace gdocsmith::lens {

struct CommentReply {
    std::optional<std::string> author;
    std::optional<std::string> id;
    bool mine{false};
    bool isDelete{false};
    int line{1};
    std::string text;

    bool operator==(const CommentReply& other) const = default;
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

    bool operator==(const CommentBlock& other) const = default;
};

inline std::string commentBlockRender(const CommentBlock& cb) {
    std::string out = ":::comment{id=" + directiveValueRender(cb.id);
    if (cb.author) out += " author=" + directiveValueRender(*cb.author);
    if (cb.resolved) out += " resolved";
    if (cb.orphaned) out += " orphaned";
    if (cb.ambiguous) out += " ambiguous";
    if (cb.mine) out += " mine";
    if (cb.isDelete) out += " delete";
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
    if (!cb.text.ends_with('\n')) out += "\n";

    for (const auto& rep : cb.replies) {
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
                if (!first) out += " ";
                out += "id=" + directiveValueRender(*rep.id);
                first = false;
            }
            if (rep.mine) {
                if (!first) out += " ";
                out += "mine";
                first = false;
            }
            if (rep.isDelete) {
                if (!first) out += " ";
                out += "delete";
            }
            out += "}";
        }
        out += "\n" + rep.text;
        if (!rep.text.ends_with('\n')) out += "\n";
    }

    out += ":::\n";
    return out;
}

} // namespace gdocsmith::lens
