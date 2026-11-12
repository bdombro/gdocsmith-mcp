#pragma once

#include "lens/named_styles.hpp"
#include "test.hpp"
#include <algorithm>

namespace gdocsmith::lens::test {

TEST_CASE("defaultNamedStyles covers all 9 named styles") {
    const auto& defs = defaultNamedStyles();
    CHECK(defs.size() == 9u);
    std::vector<std::string> types;
    for (const auto& d : defs) types.push_back(d.namedStyleType);

    for (const auto& expected : NAMED_STYLE_TYPES) {
        CHECK(std::find(types.begin(), types.end(), expected) != types.end());
    }
}

TEST_CASE("namedStyleSetMerge overlays properties") {
    NamedStyleSet base;
    NamedStyleDeclaration nsd_base;
    nsd_base.text.fontSize = 12.0;
    nsd_base.text.fontFamily = "Arial";
    base["NORMAL_TEXT"] = nsd_base;

    NamedStyleSet overlay;
    NamedStyleDeclaration nsd_over;
    nsd_over.text.fontSize = 14.0;
    nsd_over.text.bold = true;
    overlay["NORMAL_TEXT"] = nsd_over;

    auto merged = namedStyleSetMerge(base, overlay);
    CHECK(merged.contains("NORMAL_TEXT"));
    const auto& m = merged["NORMAL_TEXT"];
    CHECK(m.text.fontSize == std::optional<double>(14.0));
    CHECK(m.text.fontFamily == std::optional<std::string>("Arial"));
    CHECK(m.text.bold == std::optional<bool>(true));
}

} // namespace gdocsmith::lens::test
