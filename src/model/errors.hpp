#pragma once

#include "model/raw_json.hpp"

#include <exception>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::model {

using CoreErrorCode = std::string;

namespace error_codes {
inline constexpr std::string_view AMBIGUOUS_TABLE_ALIGNMENT = "ambiguousTableAlignment";
inline constexpr std::string_view ANCHOR_AMBIGUOUS = "anchorAmbiguous";
inline constexpr std::string_view ANCHOR_NOT_FOUND = "anchorNotFound";
inline constexpr std::string_view ATOM_MOVE = "atomMove";
inline constexpr std::string_view CONCURRENT_EDITS = "concurrentEdits";
inline constexpr std::string_view FAKE_BULLET_PREFIX = "fakeBulletPrefix";
inline constexpr std::string_view GUARD_REFUSED = "guardRefused";
inline constexpr std::string_view HEADING_BULLET = "headingBullet";
inline constexpr std::string_view INTERNAL = "internal";
inline constexpr std::string_view INVALID_ATOM = "invalidAtom";
inline constexpr std::string_view INVALID_PLACEMENT = "invalidPlacement";
inline constexpr std::string_view LINK_TARGET_NOT_FOUND = "linkTargetNotFound";
inline constexpr std::string_view LIST_ITEM_EMPTY = "listItemEmpty";
inline constexpr std::string_view MERGE_NON_EMPTY = "mergeNonEmpty";
inline constexpr std::string_view STALE_BASE = "staleBase";
inline constexpr std::string_view SUGGESTION_CONFLICT = "suggestionConflict";
inline constexpr std::string_view TAB_NOT_FOUND = "tabNotFound";
inline constexpr std::string_view TAB_REQUIRED = "tabRequired";
inline constexpr std::string_view TAB_TITLE_TAKEN = "tabTitleTaken";
inline constexpr std::string_view TOKEN_LABEL_CHANGED = "tokenLabelChanged";
inline constexpr std::string_view UNREALIZABLE_LIST = "unrealizableList";
inline constexpr std::string_view UNRECREATABLE_COPY = "unrecreatableCopy";
inline constexpr std::string_view UNRECREATABLE_MOVE = "unrecreatableMove";
inline constexpr std::string_view UNSUPPORTED_SYNTAX = "unsupportedSyntax";
} // namespace error_codes

class CoreError : public std::exception {
public:
    CoreErrorCode code;
    std::string message;
    std::optional<JsonObject> details;

    explicit CoreError(CoreErrorCode c, std::string msg, std::optional<JsonObject> d = std::nullopt)
        : code(std::move(c)), message(std::move(msg)), details(std::move(d)) {}

    [[nodiscard]] const char* what() const noexcept override {
        return message.c_str();
    }
};

inline const std::set<std::string>& contentCodes() {
    static const std::set<std::string> codes = {
        std::string(error_codes::ANCHOR_NOT_FOUND),
        std::string(error_codes::INVALID_ATOM),
        std::string(error_codes::INVALID_PLACEMENT),
        std::string(error_codes::LINK_TARGET_NOT_FOUND),
        std::string(error_codes::SUGGESTION_CONFLICT),
        std::string(error_codes::TAB_NOT_FOUND),
        std::string(error_codes::TAB_REQUIRED),
        std::string(error_codes::TAB_TITLE_TAKEN),
        std::string(error_codes::UNSUPPORTED_SYNTAX),
    };
    return codes;
}

struct FileProblem {
    CoreErrorCode code;
    std::optional<int> line;
    std::string message;
    std::optional<std::string> tab;
};

class FileProblems : public CoreError {
public:
    std::vector<FileProblem> problems;

    explicit FileProblems(std::vector<FileProblem> probs)
        : CoreError(probs.empty() ? std::string(error_codes::INTERNAL) : probs.front().code,
                    probs.empty() ? "FileProblems" : probs.front().message),
          problems(std::move(probs)) {}
};

struct FileProblemWhere {
    std::optional<int> line;
    std::optional<std::string> tab;
};

inline bool problemCollect(
    std::vector<FileProblem>& problems,
    const FileProblemWhere& where,
    const std::function<void()>& step
) {
    try {
        step();
        return true;
    } catch (const FileProblems& fp) {
        for (auto p : fp.problems) {
            if (!p.line && where.line) p.line = where.line;
            if (!p.tab && where.tab) p.tab = where.tab;
            problems.push_back(std::move(p));
        }
        return false;
    } catch (const CoreError& ce) {
        if (contentCodes().contains(ce.code)) {
            problems.push_back(FileProblem{
                .code = ce.code,
                .line = where.line,
                .message = ce.message,
                .tab = where.tab,
            });
            return false;
        }
        throw;
    }
}

inline void problemsThrow(const std::vector<FileProblem>& problems) {
    if (!problems.empty()) {
        throw FileProblems(problems);
    }
}

} // namespace gdocsmith::model
