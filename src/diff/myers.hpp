#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gdocsmith::diff {

/** One changed region: a[aStart, aEnd) is replaced by b[bStart, bEnd) (either side may be empty). */
struct Hunk {
    int aStart = 0;
    int aEnd = 0;
    int bStart = 0;
    int bEnd = 0;

    bool operator==(const Hunk&) const = default;
};

inline constexpr int LCS_DP_CELLS_MAX = 4'000'000;
inline constexpr double LCS_WEIGHT_BONUS = 1e-3;
inline constexpr int KEPT_CHARS_MAX = 4'000;

/** Matched index pairs of a Myers shortest edit script, in order. */
template <typename T, typename Eq = std::equal_to<T>>
inline std::vector<std::pair<int, int>> matchesMyers(
    std::span<const T> a,
    std::span<const T> b,
    Eq eq = Eq{}
) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    const int max_d = n + m;
    const int offset = max_d + 1;
    std::vector<int> v(2 * max_d + 3, 0);
    std::vector<std::vector<int>> trace;
    bool found = (n == 0 && m == 0);
    for (int d = 0; d <= max_d && !found; ++d) {
        trace.push_back(v);
        for (int k = -d; k <= d; k += 2) {
            int x = (k == -d || (k != d && v[offset + k - 1] < v[offset + k + 1]))
                        ? v[offset + k + 1]
                        : v[offset + k - 1] + 1;
            int y = x - k;
            while (x < n && y < m && eq(a[x], b[y])) {
                x++;
                y++;
            }
            v[offset + k] = x;
            if (x >= n && y >= m) {
                found = true;
                break;
            }
        }
    }
    std::vector<std::pair<int, int>> pairs;
    int x = n;
    int y = m;
    for (int d = static_cast<int>(trace.size()) - 1; d >= 0 && (x > 0 || y > 0); --d) {
        const auto& vd = trace[d];
        const int k = x - y;
        const int prevK = (k == -d || (k != d && vd[offset + k - 1] < vd[offset + k + 1]))
                              ? k + 1
                              : k - 1;
        const int prevX = (d == 0) ? 0 : vd[offset + prevK];
        const int prevY = (d == 0) ? 0 : prevX - prevK;
        while (x > prevX && y > prevY) {
            x--;
            y--;
            pairs.push_back({x, y});
        }
        if (d > 0) {
            x = prevX;
            y = prevY;
        }
    }
    std::reverse(pairs.begin(), pairs.end());
    return pairs;
}

/** Turns ordered matched pairs into the changed regions between them. */
inline std::vector<Hunk> hunksFromMatches(
    const std::vector<std::pair<int, int>>& pairs,
    int n,
    int m
) {
    std::vector<Hunk> hunks;
    int ai = 0;
    int bi = 0;
    auto all_pairs = pairs;
    all_pairs.push_back({n, m});
    for (const auto& [i, j] : all_pairs) {
        if (i > ai || j > bi) {
            hunks.push_back(Hunk{
                .aStart = ai,
                .aEnd = i,
                .bStart = bi,
                .bEnd = j
            });
        }
        ai = i + 1;
        bi = j + 1;
    }
    return hunks;
}

/**
 * Computes the minimal edit script turning `a` into `b` (Myers O(ND)) as changed regions in order.
 * Deterministic for equal inputs.
 */
template <typename T, typename Eq = std::equal_to<T>>
inline std::vector<Hunk> diffHunks(
    std::span<const T> a,
    std::span<const T> b,
    Eq eq = Eq{}
) {
    return hunksFromMatches(matchesMyers(a, b, eq), static_cast<int>(a.size()), static_cast<int>(b.size()));
}

template <typename T, typename Eq = std::equal_to<T>>
inline std::vector<Hunk> diffHunks(
    const std::vector<T>& a,
    const std::vector<T>& b,
    Eq eq = Eq{}
) {
    return diffHunks(std::span<const T>(a), std::span<const T>(b), eq);
}

inline std::vector<Hunk> diffHunks(
    std::string_view a,
    std::string_view b
) {
    return diffHunks(std::span<const char>(a.data(), a.size()), std::span<const char>(b.data(), b.size()));
}

/** Weighted LCS by dynamic programming (ties broken toward heavier `a` elements, then deterministically). */
inline std::vector<std::pair<int, int>> lcsWeighted(
    std::span<const std::string> a,
    std::span<const std::string> b,
    const std::function<double(int)>& weight,
    const std::function<double(int)>& value,
    const std::function<void()>& onAmbiguous = nullptr
) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    std::vector<double> score((n + 1) * (m + 1), 0.0);
    auto at = [m](int i, int j) { return i * (m + 1) + j; };
    for (int i = n - 1; i >= 0; --i) {
        for (int j = m - 1; j >= 0; --j) {
            double skip = std::max(score[at(i + 1, j)], score[at(i, j + 1)]);
            double take = (a[i] == b[j]) ? score[at(i + 1, j + 1)] + value(i) + LCS_WEIGHT_BONUS * weight(i) : -1.0;
            score[at(i, j)] = std::max(skip, take);
        }
    }
    std::vector<std::pair<int, int>> pairs;
    int i = 0;
    int j = 0;
    while (i < n && j < m) {
        if (a[i] == b[j] && score[at(i, j)] == score[at(i + 1, j + 1)] + value(i) + LCS_WEIGHT_BONUS * weight(i)) {
            if (weight(i) > 0 && (score[at(i, j)] == score[at(i + 1, j)] || score[at(i, j)] == score[at(i, j + 1)])) {
                if (onAmbiguous) onAmbiguous();
            }
            pairs.push_back({i, j});
            i++;
            j++;
        } else if (score[at(i + 1, j)] >= score[at(i, j + 1)]) {
            i++;
        } else {
            j++;
        }
    }
    return pairs;
}

/**
 * Pairs equal strings of `a` and `b` as a longest common subsequence, preferring pairs whose `a`
 * element weighs more when several subsequences are equally long.
 */
inline std::vector<std::pair<int, int>> lcsPairs(
    std::span<const std::string> a,
    std::span<const std::string> b,
    const std::function<double(int)>& weightA = nullptr,
    const std::function<void()>& onAmbiguous = nullptr,
    const std::function<double(int)>& valueA = nullptr
) {
    if (a.size() == b.size()) {
        bool all_equal = true;
        for (size_t k = 0; k < a.size(); ++k) {
            if (a[k] != b[k]) {
                all_equal = false;
                break;
            }
        }
        if (all_equal) {
            std::vector<std::pair<int, int>> res;
            res.reserve(a.size());
            for (int k = 0; k < static_cast<int>(a.size()); ++k) res.push_back({k, k});
            return res;
        }
    }
    int pre = 0;
    int suf = 0;
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    if (!weightA) {
        while (pre < n && pre < m && a[pre] == b[pre]) pre++;
        while (suf < n - pre && suf < m - pre && a[n - 1 - suf] == b[m - 1 - suf]) suf++;
    }
    std::vector<std::pair<int, int>> pairs;
    for (int i = 0; i < pre; ++i) pairs.push_back({i, i});

    auto aMid = a.subspan(pre, n - suf - pre);
    auto bMid = b.subspan(pre, m - suf - pre);
    int64_t cells = static_cast<int64_t>(aMid.size()) * static_cast<int64_t>(bMid.size());
    if (cells > LCS_DP_CELLS_MAX && weightA) {
        for (size_t idx = 0; idx < aMid.size(); ++idx) {
            if (weightA(static_cast<int>(idx) + pre) > 0) {
                if (onAmbiguous) onAmbiguous();
                break;
            }
        }
    }
    std::vector<std::pair<int, int>> middle;
    if (cells <= LCS_DP_CELLS_MAX) {
        auto w = [&](int i) -> double { return weightA ? weightA(i + pre) : 0.0; };
        auto v = [&](int i) -> double { return valueA ? valueA(i + pre) : 1.0; };
        middle = lcsWeighted(aMid, bMid, w, v, onAmbiguous);
    } else {
        middle = matchesMyers<std::string>(aMid, bMid);
    }
    for (const auto& [i, j] : middle) pairs.push_back({i + pre, j + pre});
    for (int k = suf; k > 0; --k) pairs.push_back({n - k, m - k});
    return pairs;
}

inline std::vector<std::pair<int, int>> lcsPairs(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b,
    const std::function<double(int)>& weightA = nullptr,
    const std::function<void()>& onAmbiguous = nullptr,
    const std::function<double(int)>& valueA = nullptr
) {
    return lcsPairs(std::span<const std::string>(a), std::span<const std::string>(b), weightA, onAmbiguous, valueA);
}

/** Changed regions from a weighted LCS: keeping a[i] is worth valueA(i). */
inline std::vector<Hunk> diffHunksValued(
    std::span<const std::string> a,
    std::span<const std::string> b,
    const std::function<double(int)>& valueA
) {
    return hunksFromMatches(lcsPairs(a, b, nullptr, nullptr, valueA), static_cast<int>(a.size()), static_cast<int>(b.size()));
}

inline std::vector<Hunk> diffHunksValued(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b,
    const std::function<double(int)>& valueA
) {
    return diffHunksValued(std::span<const std::string>(a), std::span<const std::string>(b), valueA);
}

/** How many characters two strings have in common, as a longest common subsequence. */
inline int commonLength(std::string_view a, std::string_view b) {
    std::vector<int> row(b.size() + 1, 0);
    std::vector<int> next(b.size() + 1, 0);
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
        for (int j = static_cast<int>(b.size()) - 1; j >= 0; --j) {
            next[j] = (a[i] == b[j]) ? row[j + 1] + 1 : std::max(row[j], next[j + 1]);
        }
        std::swap(row, next);
    }
    return row[0];
}

struct PairAB {
    int a = 0;
    int b = 0;
};

/** The longest run of pairs increasing in `a`, from pairs sorted by `b` (patience sorting). */
inline std::vector<PairAB> increasingRun(const std::vector<PairAB>& pairs) {
    std::vector<int> tails;
    std::vector<int> previous(pairs.size(), -1);
    for (size_t i = 0; i < pairs.size(); ++i) {
        int low = 0;
        int high = static_cast<int>(tails.size());
        while (low < high) {
            int mid = (low + high) >> 1;
            if (pairs[tails[mid]].a < pairs[i].a) low = mid + 1;
            else high = mid;
        }
        if (low > 0) previous[i] = tails[low - 1];
        if (low == static_cast<int>(tails.size())) tails.push_back(static_cast<int>(i));
        else tails[low] = static_cast<int>(i);
    }
    std::vector<PairAB> out;
    int cur = tails.empty() ? -1 : tails.back();
    for (; cur >= 0; cur = previous[cur]) {
        out.push_back(pairs[cur]);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

/** Pairs lines in order so the pairs have the most characters in common (ties pair the earlier lines). */
inline std::vector<std::pair<int, int>> linePairs(std::span<const std::string> a, std::span<const std::string> b) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    std::vector<int> score((n + 1) * (m + 1), 0);
    auto at = [m](int i, int j) { return i * (m + 1) + j; };
    for (int i = n - 1; i >= 0; --i) {
        for (int j = m - 1; j >= 0; --j) {
            int common = commonLength(a[i], b[j]);
            score[at(i, j)] = std::max({
                score[at(i + 1, j)],
                score[at(i, j + 1)],
                score[at(i + 1, j + 1)] + common
            });
        }
    }
    std::vector<std::pair<int, int>> pairs;
    int i = 0;
    int j = 0;
    while (i < n && j < m) {
        int common = commonLength(a[i], b[j]);
        if (common && score[at(i, j)] == score[at(i + 1, j + 1)] + common) {
            pairs.push_back({i, j});
            i++;
            j++;
        } else if (score[at(i + 1, j)] >= score[at(i, j + 1)]) {
            i++;
        } else {
            j++;
        }
    }
    return pairs;
}

struct KeptOffsetsOptions {
    bool withinLines = false;
};

/**
 * For each character of `edited`, the offset of the same surviving character in `exported`, else -1.
 */
inline std::vector<int> keptOffsets(
    std::string_view exported,
    std::string_view edited,
    KeptOffsetsOptions opts = {}
) {
    std::vector<int> map(edited.size(), -1);
    auto splitLines = [](std::string_view s) {
        std::vector<std::string> lines;
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
    auto linesA = splitLines(exported);
    auto linesB = splitLines(edited);
    auto startsOf = [](const std::vector<std::string>& lines) {
        std::vector<int> starts;
        int at = 0;
        for (const auto& line : lines) {
            starts.push_back(at);
            at += static_cast<int>(line.size()) + 1;
        }
        starts.push_back(at);
        return starts;
    };
    auto startsA = startsOf(linesA);
    auto startsB = startsOf(linesB);
    auto lineSame = [&](int a, int b) {
        int length = std::min(static_cast<int>(linesA[a].size()) + 1, static_cast<int>(map.size()) - startsB[b]);
        for (int i = 0; i < length; ++i) {
            map[startsB[b] + i] = startsA[a] + i;
        }
    };
    struct CountEntry { int a = 0; int at = 0; int b = 0; int bt = -1; };
    std::unordered_map<std::string, CountEntry> counts;
    for (size_t i = 0; i < linesA.size(); ++i) {
        auto& entry = counts[linesA[i]];
        entry.a++;
        entry.at = static_cast<int>(i);
    }
    for (size_t i = 0; i < linesB.size(); ++i) {
        auto it = counts.find(linesB[i]);
        if (it != counts.end()) {
            it->second.b++;
            it->second.bt = static_cast<int>(i);
        }
    }
    std::vector<PairAB> pairs;
    for (const auto& [_, entry] : counts) {
        if (entry.a == 1 && entry.b == 1) {
            pairs.push_back(PairAB{.a = entry.at, .b = entry.bt});
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const PairAB& x, const PairAB& y) { return x.b < y.b; });
    auto anchors = increasingRun(pairs);

    std::vector<int> matched(linesB.size(), -1);
    int head = 0;
    while (head < static_cast<int>(linesA.size()) && head < static_cast<int>(linesB.size()) && linesA[head] == linesB[head]) {
        matched[head] = head;
        head++;
    }
    for (int tail = 1; tail <= static_cast<int>(linesA.size()) - head && tail <= static_cast<int>(linesB.size()) - head; ++tail) {
        if (linesA[linesA.size() - tail] != linesB[linesB.size() - tail]) break;
        matched[linesB.size() - tail] = static_cast<int>(linesA.size()) - tail;
    }
    for (const auto& [a, b] : anchors) {
        if (matched[b] < 0) matched[b] = a;
    }
    for (const auto& [a, b] : anchors) {
        for (int i = 1; b + i < static_cast<int>(linesB.size()) && a + i < static_cast<int>(linesA.size()) && matched[b + i] < 0; ++i) {
            if (linesA[a + i] != linesB[b + i]) break;
            matched[b + i] = a + i;
        }
        for (int i = 1; b - i >= 0 && a - i >= 0 && matched[b - i] < 0; ++i) {
            if (linesA[a - i] != linesB[b - i]) break;
            matched[b - i] = a - i;
        }
    }
    int lastA = -1;
    int lastB = -1;
    auto charsKeep = [&](int fromA, int toA, int fromB, int toB) {
        auto subA = exported.substr(fromA, toA - fromA);
        auto subB = edited.substr(fromB, toB - fromB);
        std::vector<char> charsA(subA.begin(), subA.end());
        std::vector<char> charsB(subB.begin(), subB.end());
        int i = 0;
        int j = 0;
        auto hunks = diffHunks(std::span<const char>(charsA), std::span<const char>(charsB));
        hunks.push_back(Hunk{
            .aStart = static_cast<int>(charsA.size()),
            .aEnd = static_cast<int>(charsA.size()),
            .bStart = static_cast<int>(charsB.size()),
            .bEnd = static_cast<int>(charsB.size())
        });
        for (const auto& inner : hunks) {
            for (int k = 0; k < inner.aStart - i && fromB + j + k < static_cast<int>(map.size()); ++k) {
                map[fromB + j + k] = fromA + i + k;
            }
            i = inner.aEnd;
            j = inner.bEnd;
        }
    };
    auto gap = [&](int aEnd, int bEnd) {
        int fromA = startsA[lastA + 1];
        int fromB = startsB[lastB + 1];
        int toA = std::min(startsA[aEnd], static_cast<int>(exported.size()));
        int toB = std::min(startsB[bEnd], static_cast<int>(edited.size()));
        if (toA <= fromA || toB <= fromB || toA - fromA + toB - fromB > KEPT_CHARS_MAX) return;
        if (!opts.withinLines) {
            charsKeep(fromA, toA, fromB, toB);
            return;
        }
        std::vector<std::string> subA(linesA.begin() + (lastA + 1), linesA.begin() + aEnd);
        std::vector<std::string> subB(linesB.begin() + (lastB + 1), linesB.begin() + bEnd);
        for (const auto& [i, j] : linePairs(subA, subB)) {
            int a = lastA + 1 + i;
            int b = lastB + 1 + j;
            charsKeep(
                startsA[a],
                std::min(startsA[a + 1], static_cast<int>(exported.size())),
                startsB[b],
                std::min(startsB[b + 1], static_cast<int>(edited.size()))
            );
        }
    };
    for (size_t b = 0; b < linesB.size(); ++b) {
        int a = matched[b];
        if (a < 0 || a <= lastA) continue;
        gap(a, static_cast<int>(b));
        lineSame(a, static_cast<int>(b));
        lastA = a;
        lastB = static_cast<int>(b);
    }
    gap(static_cast<int>(linesA.size()), static_cast<int>(linesB.size()));
    return map;
}

} // namespace gdocsmith::diff
