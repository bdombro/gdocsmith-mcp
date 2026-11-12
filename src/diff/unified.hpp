#pragma once

#include "diff/myers.hpp"

#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::diff {

struct UnifiedDiffOptions {
    std::string newPath;
    std::string oldPath;
    int context = 3;
};

/** start,count with 1-based start, per the unified format (an empty range names the line before it). */
inline std::string rangeLabel(int start, int count) {
    return std::format("{},{}", count == 0 ? start : start + 1, count);
}

/** Groups hunks whose context windows would touch (fewer than 2 * context unchanged lines apart). */
inline std::vector<std::vector<Hunk>> hunksGroup(const std::vector<Hunk>& hunks, int context) {
    std::vector<std::vector<Hunk>> groups;
    for (const auto& hunk : hunks) {
        if (!groups.empty() && !groups.back().empty()) {
            const auto& last = groups.back().back();
            if (hunk.aStart - last.aEnd <= 2 * context) {
                groups.back().push_back(hunk);
                continue;
            }
        }
        groups.push_back({hunk});
    }
    return groups;
}

/** Formats a unified diff from `oldText` to `newText`; `""` when they're equal. */
inline std::string unifiedDiffFormat(
    std::string_view oldText,
    std::string_view newText,
    const UnifiedDiffOptions& opts
) {
    if (oldText == newText) return "";

    auto splitLines = [](std::string_view s) {
        std::vector<std::string> lines;
        if (s.empty()) return lines;
        size_t start = 0;
        while (start <= s.size()) {
            size_t pos = s.find('\n', start);
            if (pos == std::string_view::npos) {
                lines.emplace_back(s.substr(start));
                break;
            }
            lines.emplace_back(s.substr(start, pos - start));
            start = pos + 1;
        }
        return lines;
    };

    auto a = splitLines(oldText);
    auto b = splitLines(newText);
    int context = opts.context;
    auto groups = hunksGroup(diffHunks(std::span<const std::string>(a), std::span<const std::string>(b)), context);

    std::vector<std::string> out;
    out.push_back(std::format("--- {}", opts.oldPath));
    out.push_back(std::format("+++ {}", opts.newPath));

    for (const auto& group : groups) {
        int aStart = std::max(0, group.front().aStart - context);
        int bStart = std::max(0, group.front().bStart - context);
        const auto& last = group.back();
        int aEnd = std::min(static_cast<int>(a.size()), last.aEnd + context);
        int bEnd = std::min(static_cast<int>(b.size()), last.bEnd + context);

        out.push_back(std::format("@@ -{} +{} @@", rangeLabel(aStart, aEnd - aStart), rangeLabel(bStart, bEnd - bStart)));

        int ai = aStart;
        for (const auto& hunk : group) {
            while (ai < hunk.aStart) {
                out.push_back(std::format(" {}", a[ai++]));
            }
            for (int i = hunk.aStart; i < hunk.aEnd; ++i) {
                out.push_back(std::format("-{}", a[i]));
            }
            for (int j = hunk.bStart; j < hunk.bEnd; ++j) {
                out.push_back(std::format("+{}", b[j]));
            }
            ai = hunk.aEnd;
        }
        while (ai < aEnd) {
            out.push_back(std::format(" {}", a[ai++]));
        }
    }

    std::string res;
    for (size_t i = 0; i < out.size(); ++i) {
        if (i > 0) res += "\n";
        res += out[i];
    }
    return res;
}

} // namespace gdocsmith::diff
