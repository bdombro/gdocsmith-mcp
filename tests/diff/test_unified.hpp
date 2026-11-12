#pragma once

#include "diff/unified.hpp"
#include "test.hpp"

#include <regex>
#include <string>
#include <vector>

namespace gdocsmith::diff::test {

TEST_CASE("unifiedDiffFormat - equal texts give an empty diff") {
    using namespace gdocsmith::diff;
    UnifiedDiffOptions paths{.newPath = "b/doc", .oldPath = "a/doc"};
    CHECK(unifiedDiffFormat("a\nb", "a\nb", paths) == "");
}

TEST_CASE("unifiedDiffFormat - one changed line with context") {
    using namespace gdocsmith::diff;
    UnifiedDiffOptions paths{.newPath = "b/doc", .oldPath = "a/doc", .context = 1};
    std::string old_text = "1\n2\n3\n4\n5\n6\n7";
    std::string next_text = "1\n2\n3\nX\n5\n6\n7";
    std::string expected = "--- a/doc\n+++ b/doc\n@@ -3,3 +3,3 @@\n 3\n-4\n+X\n 5";
    CHECK(unifiedDiffFormat(old_text, next_text, paths) == expected);
}

TEST_CASE("unifiedDiffFormat - distant changes make separate hunks; near ones merge") {
    using namespace gdocsmith::diff;
    UnifiedDiffOptions paths{.newPath = "b/doc", .oldPath = "a/doc", .context = 3};
    std::vector<std::string> old_lines;
    for (int i = 0; i < 20; ++i) old_lines.push_back("l" + std::to_string(i));

    auto join = [](const std::vector<std::string>& lines) {
        std::string s;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) s += "\n";
            s += lines[i];
        }
        return s;
    };

    auto countHunkHeaders = [](const std::string& diff) {
        int count = 0;
        size_t pos = 0;
        while ((pos = diff.find("@@", pos)) != std::string::npos) {
            count++;
            pos = diff.find('\n', pos);
            if (pos == std::string::npos) break;
        }
        return count;
    };

    auto far_lines = old_lines;
    far_lines[1] = "A";
    far_lines[18] = "B";
    std::string far_diff = unifiedDiffFormat(join(old_lines), join(far_lines), paths);
    CHECK(countHunkHeaders(far_diff) == 2);

    auto near_lines = old_lines;
    near_lines[5] = "A";
    near_lines[9] = "B";
    std::string near_diff = unifiedDiffFormat(join(old_lines), join(near_lines), paths);
    CHECK(countHunkHeaders(near_diff) == 1);
}

TEST_CASE("unifiedDiffFormat - insertion into an empty text") {
    using namespace gdocsmith::diff;
    UnifiedDiffOptions paths{.newPath = "b/doc", .oldPath = "a/doc"};
    std::string expected = "--- a/doc\n+++ b/doc\n@@ -0,0 +1,1 @@\n+a";
    CHECK(unifiedDiffFormat("", "a", paths) == expected);
}

} // namespace gdocsmith::diff::test
