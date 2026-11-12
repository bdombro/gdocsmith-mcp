#pragma once

#include "diff/myers.hpp"
#include <span>
#include <vector>

namespace gdocsmith::diff {

/** Applies hunks to `a`, producing what should equal `b`. */
template <typename T>
inline std::vector<T> hunksApply(
    std::span<const T> a,
    std::span<const T> b,
    const std::vector<Hunk>& hunks
) {
    std::vector<T> out;
    int ai = 0;
    for (const auto& h : hunks) {
        while (ai < h.aStart) {
            out.push_back(a[ai++]);
        }
        for (int bi = h.bStart; bi < h.bEnd; ++bi) {
            out.push_back(b[bi]);
        }
        ai = h.aEnd;
    }
    while (ai < static_cast<int>(a.size())) {
        out.push_back(a[ai++]);
    }
    return out;
}

template <typename T>
inline std::vector<T> hunksApply(
    const std::vector<T>& a,
    const std::vector<T>& b,
    const std::vector<Hunk>& hunks
) {
    return hunksApply(std::span<const T>(a), std::span<const T>(b), hunks);
}

} // namespace gdocsmith::diff
