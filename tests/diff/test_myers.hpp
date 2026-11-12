#pragma once

#include "diff/myers.hpp"
#include "diff/patch.hpp"
#include "test.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gdocsmith::diff::test {

inline auto rngCreate(uint32_t seed) {
    uint32_t state = seed;
    return [state]() mutable -> double {
        state = (state + 0x6d2b79f5u);
        uint32_t t = state;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return static_cast<double>((t ^ (t >> 14))) / 4294967296.0;
    };
}

inline int lcsLength(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            dp[i][j] = (a[i - 1] == b[j - 1]) ? dp[i - 1][j - 1] + 1 : std::max(dp[i - 1][j], dp[i][j - 1]);
        }
    }
    return dp[n][m];
}

inline std::vector<std::string> to_chars(std::string_view s) {
    std::vector<std::string> res;
    for (char c : s) res.push_back(std::string(1, c));
    return res;
}

TEST_CASE("diffHunks - known scripts") {
    using namespace gdocsmith::diff;
    auto hunks1 = diffHunks(to_chars("abc"), to_chars("abc"));
    CHECK(hunks1.empty());

    auto hunks2 = diffHunks(to_chars(""), to_chars("ab"));
    REQUIRE(hunks2.size() == 1);
    CHECK(hunks2[0] == Hunk{.aStart = 0, .aEnd = 0, .bStart = 0, .bEnd = 2});

    auto hunks3 = diffHunks(to_chars("ab"), to_chars(""));
    REQUIRE(hunks3.size() == 1);
    CHECK(hunks3[0] == Hunk{.aStart = 0, .aEnd = 2, .bStart = 0, .bEnd = 0});

    auto hunks4 = diffHunks(to_chars("the cat"), to_chars("the bat"));
    REQUIRE(hunks4.size() == 1);
    CHECK(hunks4[0] == Hunk{.aStart = 4, .aEnd = 5, .bStart = 4, .bEnd = 5});
}

TEST_CASE("diffHunks - random inputs: hunks reproduce b with a minimal number of edits, deterministically") {
    using namespace gdocsmith::diff;
    auto rng = rngCreate(7);
    const std::string alphabet = "abc";
    for (int t = 0; t < 500; ++t) {
        int a_len = static_cast<int>(rng() * 14);
        int b_len = static_cast<int>(rng() * 14);
        std::vector<std::string> a;
        std::vector<std::string> b;
        for (int i = 0; i < a_len; ++i) a.push_back(std::string(1, alphabet[static_cast<int>(rng() * 3)]));
        for (int i = 0; i < b_len; ++i) b.push_back(std::string(1, alphabet[static_cast<int>(rng() * 3)]));

        auto hunks = diffHunks(a, b);
        auto applied = hunksApply(a, b, hunks);
        CHECK(applied == b);

        int edits = 0;
        for (const auto& h : hunks) {
            edits += (h.aEnd - h.aStart) + (h.bEnd - h.bStart);
        }
        CHECK(edits == a_len + b_len - 2 * lcsLength(a, b));
        auto hunks_again = diffHunks(a, b);
        CHECK(hunks_again == hunks);
    }
}

TEST_CASE("diffHunks - custom equality") {
    using namespace gdocsmith::diff;
    std::vector<std::string> a = {"A"};
    std::vector<std::string> b = {"a"};
    auto eq = [](const std::string& x, const std::string& y) {
        return std::tolower(static_cast<unsigned char>(x[0])) == std::tolower(static_cast<unsigned char>(y[0]));
    };
    auto hunks = diffHunks(a, b, eq);
    CHECK(hunks.empty());
}

TEST_CASE("lcsPairs - pairs form a longest common subsequence") {
    using namespace gdocsmith::diff;
    auto rng = rngCreate(11);
    const std::string alphabet = "xyz";
    for (int t = 0; t < 300; ++t) {
        int a_len = static_cast<int>(rng() * 10);
        int b_len = static_cast<int>(rng() * 10);
        std::vector<std::string> a;
        std::vector<std::string> b;
        for (int i = 0; i < a_len; ++i) a.push_back(std::string(1, alphabet[static_cast<int>(rng() * 3)]));
        for (int i = 0; i < b_len; ++i) b.push_back(std::string(1, alphabet[static_cast<int>(rng() * 3)]));

        auto pairs = lcsPairs(a, b);
        CHECK(static_cast<int>(pairs.size()) == lcsLength(a, b));
        for (size_t k = 0; k < pairs.size(); ++k) {
            auto [i, j] = pairs[k];
            CHECK(a[i] == b[j]);
            if (k > 0) {
                CHECK(i > pairs[k - 1].first);
                CHECK(j > pairs[k - 1].second);
            }
        }
    }
}

TEST_CASE("lcsPairs - ties go to the heavier element") {
    using namespace gdocsmith::diff;
    std::vector<std::string> a = {"X", "X"};
    std::vector<std::string> b = {"X"};
    auto weightA = [](int i) -> double { return (i == 1) ? 5.0 : 0.0; };
    auto pairs = lcsPairs(a, b, weightA);
    REQUIRE(pairs.size() == 1);
    CHECK(pairs[0] == std::pair<int, int>{1, 0});
    CHECK(lcsPairs(a, b).size() == 1);
}

TEST_CASE("lcsPairs - a valued element is kept over a longer run of plain ones") {
    using namespace gdocsmith::diff;
    std::vector<std::string> a = {"H", "A", "C"};
    std::vector<std::string> b = {"C", "H", "A"};
    auto pairs1 = lcsPairs(a, b);
    std::vector<std::pair<int, int>> expected1 = {{0, 1}, {1, 2}};
    CHECK(pairs1 == expected1);

    auto valueA = [](int i) -> double { return (i == 2) ? 4.0 : 1.0; };
    auto pairs2 = lcsPairs(a, b, nullptr, nullptr, valueA);
    std::vector<std::pair<int, int>> expected2 = {{2, 0}};
    CHECK(pairs2 == expected2);
}

} // namespace gdocsmith::diff::test
