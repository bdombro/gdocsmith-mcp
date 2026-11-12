#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <sstream>
#include <cctype>

namespace gdocsmith::lens {

inline std::string cellRefRender(int tabIndex, int row, int col) {
    return "t" + std::to_string(tabIndex) + "r" + std::to_string(row + 1) + "c" + std::to_string(col + 1);
}

inline std::string cellDefinitionLine(std::string_view id, std::string_view edge) {
    return "::cell{id=" + std::string(id) + " " + std::string(edge) + "}";
}

inline std::string toA1(int row, int col) {
    std::string col_str;
    int c = col;
    while (c >= 0) {
        col_str.insert(col_str.begin(), static_cast<char>('A' + (c % 26)));
        c = (c / 26) - 1;
    }
    return col_str + std::to_string(row + 1);
}

inline std::optional<std::pair<int, int>> fromA1(std::string_view a1) {
    size_t i = 0;
    int col = 0;
    while (i < a1.size() && std::isalpha(static_cast<unsigned char>(a1[i]))) {
        col = col * 26 + (std::toupper(static_cast<unsigned char>(a1[i])) - 'A' + 1);
        i++;
    }
    if (i == 0 || i == a1.size()) return std::nullopt;
    col -= 1; // 0-based

    int row = 0;
    while (i < a1.size() && std::isdigit(static_cast<unsigned char>(a1[i]))) {
        row = row * 10 + (a1[i] - '0');
        i++;
    }
    if (i != a1.size() || row <= 0) return std::nullopt;
    return std::make_pair(row - 1, col);
}

} // namespace gdocsmith::lens
