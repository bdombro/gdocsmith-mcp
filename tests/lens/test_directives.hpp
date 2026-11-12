#pragma once

#include "lens/directives.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("directive_attrs_parse valid") {
    {
        auto attrs = directiveAttrsParse("id=t.0 title=\"Launch Plan\" pageful margins=54,54,54,54");
        CHECK(attrs.classes.size() == 0u);
        CHECK(attrs.flags.size() == 1u);
        CHECK(attrs.flags[0] == "pageful");
        CHECK(attrs.get_value("id") == std::optional<std::string>("t.0"));
        CHECK(attrs.get_value("title") == std::optional<std::string>("Launch Plan"));
        CHECK(attrs.get_value("margins") == std::optional<std::string>("54,54,54,54"));
    }
    {
        auto attrs = directiveAttrsParse(".size9 .color1967D2");
        CHECK(attrs.classes.size() == 2u);
        CHECK(attrs.classes[0] == "size9");
        CHECK(attrs.classes[1] == "color1967D2");
        CHECK(attrs.flags.size() == 0u);
        CHECK(attrs.values.size() == 0u);
    }
    {
        auto attrs = directiveAttrsParse("time=10:00 zone=America/New_York");
        CHECK(attrs.get_value("time") == std::optional<std::string>("10:00"));
        CHECK(attrs.get_value("zone") == std::optional<std::string>("America/New_York"));
    }
    {
        auto attrs = directiveAttrsParse("src=\"https://x.test/a.png?a=1&b=2\"");
        CHECK(attrs.get_value("src") == std::optional<std::string>("https://x.test/a.png?a=1&b=2"));
    }
    {
        auto attrs = directiveAttrsParse("emoji=👨‍👩‍👧 background=#FFF4CC");
        CHECK(attrs.get_value("emoji") == std::optional<std::string>("👨‍👩‍👧"));
        CHECK(attrs.get_value("background") == std::optional<std::string>("#FFF4CC"));
    }
    {
        auto attrs = directiveAttrsParse("quote=\"say \\\"hi\\\" } | here\"");
        CHECK(attrs.get_value("quote") == std::optional<std::string>("say \"hi\" } | here"));
    }
}

TEST_CASE("directive_attrs_parse invalid refuses with unsupportedSyntax") {
    const std::vector<std::string> bad_inputs = {
        "bad=\"unterminated",
        "glued=\"a\"b",
        "x=",
        "x=a x=b",
        "x x",
        ". a",
        "=v"
    };
    for (const auto& src : bad_inputs) {
        bool threw = false;
        try {
            directiveAttrsParse(src);
        } catch (const CoreError& err) {
            threw = true;
            CHECK(err.code == "unsupportedSyntax");
        }
        CHECK(threw);
    }
}

TEST_CASE("directive_attrs_render round-trips") {
    DirectiveAttrs attrs{
        .classes = {"size9"},
        .flags = {"pageful"},
        .values = {{"id", "t.0"}, {"title", "Launch Plan"}, {"url", "https://x.test/?a=1"}, {"empty", ""}}
    };
    std::string rendered = directiveAttrsRender(attrs);
    CHECK(rendered == "{.size9 id=t.0 title=\"Launch Plan\" url=\"https://x.test/?a=1\" empty=\"\" pageful}");
    auto parsed = directiveAttrsParse(rendered.substr(1, rendered.size() - 2));
    CHECK(parsed == attrs);
    CHECK(directiveAttrsRender(DirectiveAttrs{}) == "");
}

TEST_CASE("directive matchers inline forms") {
    std::unordered_set<std::string> names = {
        "style", "chip", "date", "image", "range", "tab", "section", "reply", "pagebreak"
    };
    auto m1 = inlineDirectiveMatch(":style[March 1]{.accent} rest", names);
    CHECK(m1.has_value());
    CHECK(m1->name == "style");
    CHECK(m1->label.value_or("") == "March 1");
    CHECK(m1->attrs.classes.size() == 1u);
    CHECK(m1->attrs.classes[0] == "accent");
    CHECK(m1->raw == ":style[March 1]{.accent}");

    auto m2 = inlineDirectiveMatch(":chip{kind=vote}", names);
    CHECK(m2.has_value());
    CHECK(m2->attrs.get_value("kind").value_or("") == "vote");

    auto m3 = inlineDirectiveMatch(":style[a [nested] \\] b]{.x}", names);
    CHECK(m3.has_value());
    CHECK(m3->label.value_or("") == "a [nested] \\] b");

    auto m4 = inlineDirectiveMatch(":image[Logo]{src=\"a}b\"}", names);
    CHECK(m4.has_value());
    CHECK(m4->attrs.get_value("src").value_or("") == "a}b");

    CHECK(!inlineDirectiveMatch(":chip", names).has_value());
    CHECK(!inlineDirectiveMatch(":unknown{a=b}", names).has_value());
    CHECK(!inlineDirectiveMatch(":style[unclosed", names).has_value());
}

TEST_CASE("inline_directive_start finds or ignores prose colons") {
    std::unordered_set<std::string> names = {
        "style", "chip", "date", "image", "range", "tab", "section", "reply", "pagebreak"
    };
    const std::vector<std::string> prose = {
        "Ratio 3:2", "time 10:00", "see note:this", "mailto:a@b.c", "a::style[x]{.b}", "plain :style text"
    };
    for (const auto& text : prose) {
        CHECK(inlineDirectiveStart(text, names) == -1);
    }
    CHECK(inlineDirectiveStart("Ship (:style[x]{.a})", names) == 6);
    CHECK(inlineDirectiveStart("\\:style[x]{.a} then :chip{kind=vote}", names) == 20);
}

TEST_CASE("directive matchers line forms") {
    std::unordered_set<std::string> names = {
        "style", "chip", "date", "image", "range", "tab", "section", "reply", "pagebreak"
    };
    auto l1 = lineDirectiveMatch("::tab{id=t.0 title=\"Plan\" pageful}", names);
    CHECK(l1.has_value());
    CHECK(l1->name == "tab");
    CHECK(l1->colons == 2);
    CHECK(l1->attrs.get_value("id").value_or("") == "t.0");
    CHECK(l1->attrs.get_value("title").value_or("") == "Plan");
    CHECK(l1->attrs.has_flag("pageful"));

    auto l2 = lineDirectiveMatch("::reply", names);
    CHECK(l2.has_value());
    CHECK(l2->name == "reply");

    auto l3 = lineDirectiveMatch(":::range{name=scope}", names);
    CHECK(l3.has_value());
    CHECK(l3->colons == 3);

    auto l4 = lineDirectiveMatch("::other{a=b}", names);
    CHECK(!l4.has_value());

    bool threw = false;
    try {
        lineDirectiveMatch("::tab{id=t.0} trailing", names);
    } catch (const CoreError& err) {
        threw = true;
        CHECK(err.code == "unsupportedSyntax");
    }
    CHECK(threw);
}

} // namespace gdocsmith::lens::test
