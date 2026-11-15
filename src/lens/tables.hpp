#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <map>
#include <optional>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "json.hpp"
#include "cell_styles.hpp"
#include "directives.hpp"
#include "diff/myers.hpp"
#include "model/errors.hpp"
#include <functional>

namespace gdocsmith::lens {

inline constexpr std::string_view MERGE_LEFT = ":merge{left}";
inline constexpr std::string_view MERGE_UP = ":merge{up}";
inline constexpr std::string_view MERGE_DOWN = ":merge{down}";

struct TableMerge {
    int column{0};
    int columnSpan{1};
    int row{0};
    int rowSpan{1};

    bool operator==(const TableMerge& other) const = default;
};

inline std::set<std::pair<int, int>> mergeCovered(const std::vector<TableMerge>& merges) {
    std::set<std::pair<int, int>> covered;
    for (const auto& m : merges) {
        for (int r = m.row; r < m.row + m.rowSpan; r++) {
            for (int c = m.column; c < m.column + m.columnSpan; c++) {
                if (r != m.row || c != m.column) {
                    covered.insert({r, c});
                }
            }
        }
    }
    return covered;
}

inline double dice2(std::string_view a, std::string_view b) {
    auto norm = [](std::string_view s) {
        std::string out;
        bool in_space = false;
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start != std::string_view::npos) {
            size_t end = s.find_last_not_of(" \t\r\n");
            for (size_t i = start; i <= end; i++) {
                char c = s[i];
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (!in_space) {
                        out += ' ';
                        in_space = true;
                    }
                } else {
                    out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    in_space = false;
                }
            }
        }
        return out;
    };

    std::string x = norm(a);
    std::string y = norm(b);
    if (x.empty() && y.empty()) return 1.0;
    if (x.empty() || y.empty()) return 0.0;

    auto grams = [](const std::string& s) {
        std::vector<std::string> res;
        if (s.size() < 2) {
            res.push_back(s);
            return res;
        }
        for (size_t i = 0; i + 1 < s.size(); i++) {
            res.push_back(s.substr(i, 2));
        }
        return res;
    };

    auto gx = grams(x);
    auto gy = grams(y);
    if (gx.empty() || gy.empty()) return 0.0;

    std::map<std::string, int> counts;
    for (const auto& g : gx) counts[g]++;

    int common = 0;
    for (const auto& g : gy) {
        auto it = counts.find(g);
        if (it != counts.end() && it->second > 0) {
            common++;
            it->second--;
        }
    }
    return (2.0 * common) / (gx.size() + gy.size());
}

struct AlignStep {
    std::optional<int> n;
    std::optional<int> o;

    bool operator==(const AlignStep& other) const = default;
};

struct TableAlignment {
    std::vector<AlignStep> columns;
    std::vector<AlignStep> rows;

    bool operator==(const TableAlignment& other) const = default;
};

inline std::vector<AlignStep> tableLinesAlign(const std::vector<std::string>& old,
                                             const std::vector<std::string>& next,
                                             const std::function<double(int, int)>& similarity) {
    auto matches = diff::matchesMyers<std::string>(old, next);
    matches.emplace_back(static_cast<int>(old.size()), static_cast<int>(next.size()));
    std::vector<AlignStep> result;
    int fromOld = 0, fromNext = 0;
    for (const auto& [endOld, endNext] : matches) {
        const int rows = endOld - fromOld, cols = endNext - fromNext;
        std::vector<std::vector<double>> scores(rows + 1, std::vector<double>(cols + 1));
        const auto score = [&](int row, int col) {
            const double value = similarity(fromOld + row, fromNext + col);
            return value >= 0.5 ? value : -1.0;
        };
        for (int r = 1; r <= rows; ++r) for (int c = 1; c <= cols; ++c) {
            scores[r][c] = std::max({scores[r - 1][c], scores[r][c - 1],
                                    scores[r - 1][c - 1] + score(r - 1, c - 1)});
        }
        std::vector<AlignStep> gap;
        int r = rows, c = cols;
        while (r || c) {
            if (r && c && scores[r][c] == scores[r - 1][c - 1] + score(r - 1, c - 1)) {
                gap.push_back({.n = fromNext + --c, .o = fromOld + --r});
            } else if (c && (!r || scores[r][c - 1] >= scores[r - 1][c])) {
                gap.push_back({.n = fromNext + --c});
            } else gap.push_back({.o = fromOld + --r});
        }
        result.insert(result.end(), gap.rbegin(), gap.rend());
        if (endOld < static_cast<int>(old.size())) result.push_back({.n = endNext, .o = endOld});
        fromOld = endOld + 1;
        fromNext = endNext + 1;
    }
    return result;
}

inline TableAlignment tableAlign(const std::vector<std::vector<std::string>>& old,
                                 const std::vector<std::vector<std::string>>& next) {
    if (old.empty() || next.empty() || old.front().empty() || next.front().empty()) {
        throw model::CoreError("unsupportedSyntax", "A table needs at least one cell.");
    }
    TableAlignment result;
    const auto text = [](const auto& rows, int r, int c) -> std::string {
        return r >= 0 && c >= 0 && r < static_cast<int>(rows.size()) &&
               c < static_cast<int>(rows[r].size()) ? rows[r][c] : "";
    };
    if (old.front().size() == next.front().size()) {
        for (size_t c = 0; c < old.front().size(); ++c) result.columns.push_back({.n = static_cast<int>(c), .o = static_cast<int>(c)});
    } else {
        auto header = [](const auto& rows) {
            std::vector<std::string> values = rows.front();
            for (auto& value : values) {
                const auto first = value.find_first_not_of(" \t\r\n");
                value = first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
                for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return values;
        };
        const auto a = header(old), b = header(next);
        result.columns = tableLinesAlign(a, b, [&](int x, int y) {
            std::string left, right;
            for (size_t r = 0; r < old.size(); ++r) left += text(old, static_cast<int>(r), x) + "\n";
            for (size_t r = 0; r < next.size(); ++r) right += text(next, static_cast<int>(r), y) + "\n";
            return dice2(left, right);
        });
        for (const auto& step : result.columns) {
            if (step.o && !step.n && !a[*step.o].empty() && std::count(a.begin(), a.end(), a[*step.o]) > 1) {
                throw model::CoreError("ambiguousTableAlignment", "Duplicate column headers cannot be distinguished.");
            }
            if (step.n && !step.o && !b[*step.n].empty() && std::count(b.begin(), b.end(), b[*step.n]) > 1) {
                throw model::CoreError("ambiguousTableAlignment", "Duplicate column headers cannot be distinguished.");
            }
        }
    }
    std::vector<std::string> a, b;
    for (size_t r = 1; r < old.size(); ++r) {
        std::string value;
        for (const auto& col : result.columns) if (col.o && col.n) value += text(old, static_cast<int>(r), *col.o) + "\t";
        a.push_back(std::move(value));
    }
    for (size_t r = 1; r < next.size(); ++r) {
        std::string value;
        for (const auto& col : result.columns) if (col.o && col.n) value += text(next, static_cast<int>(r), *col.n) + "\t";
        b.push_back(std::move(value));
    }
    result.rows.push_back({.n = 0, .o = 0});
    const auto aligned = tableLinesAlign(a, b, [&](int x, int y) {
        double score = 0;
        size_t count = 0;
        for (const auto& col : result.columns) if (col.o && col.n) {
            score += dice2(text(old, x + 1, *col.o), text(next, y + 1, *col.n));
            ++count;
        }
        return count ? score / static_cast<double>(count) : 0;
    });
    for (auto step : aligned) {
        if (step.o) ++*step.o;
        if (step.n) ++*step.n;
        result.rows.push_back(step);
    }
    return result;
}

} // namespace gdocsmith::lens
