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

namespace gdocsmith::lens {

inline constexpr std::string_view MERGE_LEFT = ":merge{left}";
inline constexpr std::string_view MERGE_UP = ":merge{up}";

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

} // namespace gdocsmith::lens
