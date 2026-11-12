#pragma once

#include <string>
#include <vector>
#include <optional>
#include "directives.hpp"

namespace gdocsmith::lens {

struct RangeMark {
    std::string edge; // "start" or "end"
    std::optional<std::string> key;
    std::string name;
    int offset{0};
    bool same{false};

    bool operator==(const RangeMark& other) const = default;
};

struct RangeCreate {
    struct Position {
        std::string key;
        int offset{0};
        bool operator==(const Position& other) const = default;
    };
    Position start;
    Position end;
    std::string name;

    bool operator==(const RangeCreate& other) const = default;
};

} // namespace gdocsmith::lens
