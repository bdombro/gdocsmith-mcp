#pragma once

#include "lens/suggestion_blocks.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("suggestionBlockRender renders block fences") {
    SuggestionBlock sb{
        .id = "s1",
        .kind = "text",
        .tabIndex = 0,
        .segment = "header default",
        .before = "before text",
        .after = "after text"
    };
    std::string rendered = suggestionBlockRender(sb);
    CHECK(rendered.find(":::suggestion{id=s1 kind=text segment=\"header default\"}") != std::string::npos);
    CHECK(rendered.find("::before\nbefore text\n") != std::string::npos);
    CHECK(rendered.find("::after\nafter text\n") != std::string::npos);
    CHECK(rendered.ends_with(":::\n"));
}

} // namespace gdocsmith::lens::test
