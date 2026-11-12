#pragma once

#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace gdocsmith::model {

struct CommentReply {
    std::string replyId;
    std::string author;
    std::string content;
    std::string timestamp;
    std::optional<std::string> action; // "resolve", etc.
};

struct CommentThread {
    std::string commentId;
    std::string author;
    std::string content;
    std::string timestamp;
    bool resolved = false;
    std::string quote;
    std::optional<std::string> anchorText;
    std::optional<Range> range;
    std::vector<CommentReply> replies;
};

} // namespace gdocsmith::model
