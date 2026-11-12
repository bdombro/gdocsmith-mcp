#pragma once

#include "model/symbols.hpp"
#include "test.hpp"

namespace gdocsmith::model::test {

inline ParagraphBlock makeTestParagraph(std::vector<Inline> inlines) {
    ParagraphBlock p;
    p.inlines = std::move(inlines);
    p.key = "o1";
    p.kind = "paragraph";
    p.newline = NewlineMark{.style = make_json_object()};
    p.is_protected = false;
    p.style = make_json_object();
    return p;
}

TEST_CASE("symbols - emoji stays one symbol and keeps its UTF-16 length") {
    using namespace gdocsmith::model;
    auto p = makeTestParagraph({Inline::from_text("a😀b")});
    auto syms = paragraphSymbols(p);
    std::vector<std::string> ids;
    for (const auto& s : syms) ids.push_back(symbolId(s));
    std::vector<std::string> expected = {"a", "😀", "b"};
    CHECK(ids == expected);
    CHECK(symbolsUtf16Length(syms) == 4);
}

TEST_CASE("symbols - atoms are one symbol keyed by their key; lengths come from the atom") {
    using namespace gdocsmith::model;
    Atom eq;
    eq.kind = "atom";
    eq.key = "o9";
    eq.type = "equation";
    eq.length = 3;

    auto p = makeTestParagraph({Inline::from_text("x"), Inline(eq)});
    auto syms = paragraphSymbols(p);
    std::vector<std::string> ids;
    for (const auto& s : syms) ids.push_back(symbolId(s));
    std::vector<std::string> expected = {"x", std::string(1, '\0') + "o9"};
    CHECK(ids == expected);
    CHECK(symbolsUtf16Length(syms) == 4);
}

TEST_CASE("symbols - set merges equal-style characters into canonical runs") {
    using namespace gdocsmith::model;
    JsonObject boldStyle = slimr::Json::object_t{{"bold", true}};
    auto p = makeTestParagraph({
        Inline::from_text("a", boldStyle),
        Inline::from_text("b", boldStyle),
        Inline::from_text("c", make_json_object())
    });
    paragraphSymbolsSet(p, paragraphSymbols(p));
    REQUIRE(p.inlines.size() == 2);
    CHECK(p.inlines[0].text == "ab");
    CHECK(styleEqual(p.inlines[0].style, boldStyle));
    CHECK(p.inlines[1].text == "c");
}

TEST_CASE("symbols - suggestion metadata splits runs") {
    using namespace gdocsmith::model;
    TextRun tr1;
    tr1.kind = "text";
    tr1.text = "a";
    tr1.style = make_json_object();
    tr1.suggestedInsertionIds = std::vector<std::string>{"s1"};

    TextRun tr2;
    tr2.kind = "text";
    tr2.text = "b";
    tr2.style = make_json_object();

    auto p = makeTestParagraph({Inline(tr1), Inline(tr2)});
    paragraphSymbolsSet(p, paragraphSymbols(p));
    CHECK(p.inlines.size() == 2);
}

} // namespace gdocsmith::model::test
