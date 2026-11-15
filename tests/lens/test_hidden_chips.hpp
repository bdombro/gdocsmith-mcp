#pragma once

#include "lens/hidden_chips.hpp"
#include "lens/atom_put.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("source person email criterion refuses before allocating or mutating intent") {
    model::EditContext context;
    model::TabModel tab;
    tab.tabId = "main";
    model::EditTarget target{context, tab};
    AtomPutReport report;
    for (const auto* email : {"not-an-email", "a@localhost", "a b@host.test", "a@@b.test", "@host.test", "a@\u00A0host.test"}) {
        TokenRef token{.kind = "person"};
        token.attrs.values.emplace_back("email", email);
        try { (void)atomTokenCreate(target, token, {}, report); FAIL("expected invalid person refusal"); }
        catch (const model::CoreError& error) { CHECK(error.code == "unsupportedSyntax"); }
    }
    CHECK(tab.blocks.empty());
    CHECK(context.tombstones.empty());
    CHECK(report.notes.empty());
    for (const auto* email : {"a@b.test", "person+tag@example.invalid", "a..b@host.example"}) {
        TokenRef token{.kind = "person"};
        token.attrs.values.emplace_back("email", email);
        const auto created = atomTokenCreate(target, token, {}, report);
        REQUIRE(created.create);
        CHECK(created.create->email == email);
        if (std::string_view(email) == "a@b.test") CHECK(created.key == "n1");
    }
}

TEST_CASE("hiddenChipKindOf identifies kinds") {
    CHECK(hiddenChipKindOf("⏱ 01:23") == "stopwatch");
    CHECK(hiddenChipKindOf("⌛ 05:00") == "timer");
    CHECK(hiddenChipKindOf("Person") == "placeholder");
    CHECK(hiddenChipKindOf("Date") == "placeholder");
    CHECK(hiddenChipKindOf("Place") == "placeholder");
    CHECK(hiddenChipKindOf("File") == "placeholder");
    CHECK(hiddenChipKindOf("Calendar event") == "placeholder");
    CHECK(hiddenChipKindOf("Listen to this tab") == "listen");
    CHECK(hiddenChipKindOf("(👍 3) (👎 1)") == "vote");
    CHECK(hiddenChipKindOf("random prose") == "unknown");
}

TEST_CASE("source person and date recreation preserve supported metadata but not old provider identity") {
    model::EditContext context;
    model::TabModel tab;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    model::Atom person{.key = "person", .type = "person", .raw = slimr::Json::object_t{
        {"person", slimr::Json::object_t{{"personId", "provider-person"},
            {"personProperties", slimr::Json::object_t{{"email", "person@example.invalid"}, {"name", "Person"}}}}}
    }};
    auto moved = atomRecreate(target, person, report);
    REQUIRE(moved.create);
    CHECK(moved.create->email == "person@example.invalid");
    CHECK_FALSE(moved.raw);
    CHECK(moved.key != person.key);
    model::Atom date{.key = "date", .type = "date", .raw = slimr::Json::object_t{
        {"dateElement", slimr::Json::object_t{{"dateElementProperties", slimr::Json::object_t{
            {"timestamp", "2026-03-01T15:00:00Z"}, {"dateFormat", "DATE_FORMAT_ISO8601"},
            {"timeFormat", "TIME_FORMAT_HOUR_MINUTE_TIMEZONE"}, {"timeZoneId", "America/New_York"}, {"locale", "pt-BR"}
        }}}}
    }};
    moved = atomRecreate(target, date, report);
    REQUIRE(moved.create);
    CHECK(moved.create->timestamp == "2026-03-01T15:00:00Z");
    CHECK(moved.create->timeFormat == "TIME_FORMAT_HOUR_MINUTE_TIMEZONE");
    CHECK(moved.create->timeZoneId == "America/New_York");
    CHECK(moved.create->locale == "pt-BR");
    CHECK_THROWS_AS(atomRecreate(target, model::Atom{.type = "equation"}, report), model::CoreError);
    CHECK_THROWS_AS(atomTokenCreate(target, TokenRef{.kind = "chip", .attrs = directiveAttrsParse("kind=timer")}, {}, report), model::CoreError);
    TokenRef before{.kind = "person", .attrs = directiveAttrsParse("id=person email=person@example.invalid"), .label = "Pat"};
    auto after = before;
    after.label = "Sam";
    try { atomTokenKeep(person, before, after, tab, {}); FAIL("expected relabel refusal"); }
    catch (const model::CoreError& error) {
        CHECK(error.code == "tokenLabelChanged");
        CHECK(error.what() == std::string("the person \"Pat\" can't be relabeled \"Sam\"; keep exported objects unchanged"));
    }
    after = before;
    std::reverse(after.attrs.values.begin(), after.attrs.values.end());
    CHECK(atomTokenIdentity(before) == atomTokenIdentity(after));
    after.attrs.values.front().second = "different@example.invalid";
    CHECK(atomTokenIdentity(before) != atomTokenIdentity(after));
}

TEST_CASE("source fresh date chips produce noon floating and UTC payloads and validate attributes") {
    model::EditContext context;
    model::TabModel tab;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    auto create = [&](const std::string& attrs) {
        return atomTokenCreate(target, TokenRef{.kind = "date", .attrs = directiveAttrsParse(attrs)}, {}, report);
    };
    const auto noon = create("value=2026-03-01 format=iso locale=pt-BR");
    REQUIRE(noon.create);
    CHECK(noon.create->timestamp == "2026-03-01T12:00:00Z");
    CHECK(noon.create->locale == "pt-BR");
    CHECK(noon.create->dateFormat == "DATE_FORMAT_ISO8601");
    const auto floating = create("value=2026-03-01 time=10:00");
    REQUIRE(floating.create);
    CHECK(floating.create->timestamp == "2026-03-01T10:00:00Z");
    CHECK(floating.create->timeFormat == "TIME_FORMAT_HOUR_MINUTE");
    CHECK_FALSE(floating.create->timeZoneId);
    const auto utc = create("value=2026-03-01 time=10:00 zone=UTC");
    REQUIRE(utc.create);
    CHECK(utc.create->timeFormat == "TIME_FORMAT_HOUR_MINUTE_TIMEZONE");
    CHECK(utc.create->timeZoneId == "UTC");
#if defined(__APPLE__) || (defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L)
    for (const auto& [attrs, expected] : {
        std::pair{"value=2026-03-01 time=10:00 zone=America/New_York format=long", "2026-03-01T15:00:00Z"},
        {"value=2026-07-01 time=10:00 zone=America/New_York", "2026-07-01T14:00:00Z"},
        {"value=2026-01-01 time=00:30 zone=Asia/Tokyo", "2025-12-31T15:30:00Z"},
        {"value=2026-03-08 time=03:30 zone=America/New_York", "2026-03-08T07:30:00Z"},
        {"value=2026-11-01 time=01:30 zone=America/New_York", "2026-11-01T05:30:00Z"}
    }) {
        const auto zoned = create(attrs);
        REQUIRE(zoned.create);
        CHECK(zoned.create->timestamp == expected);
        const auto token = atomTokenProject(zoned, tab);
        CHECK(token.attrs.get_value("zone") == zoned.create->timeZoneId);
        CHECK(token.attrs.get_value("time") == (std::string(attrs).find("00:30") != std::string::npos ? "00:30" :
            std::string(attrs).find("03:30") != std::string::npos ? "03:30" :
            std::string(attrs).find("01:30") != std::string::npos ? "01:30" : "10:00"));
    }
#endif
    for (const auto* attrs : {"value=2026-03-01 time=25:00", "value=2026-03-01 zone=UTC",
                              "value=2026-03-01 format=bogus", "value=2026-03-01 locale=English",
                              "value=2026-03-01 time=10:00 zone=EST",
                              "value=2026-03-01 time=10:00 zone=Not/AZone"}) {
        CHECK_THROWS_AS(create(attrs), model::CoreError);
    }
}

TEST_CASE("source moved footnote recreates original label with fresh identity") {
    model::EditContext context;
    model::TabModel tab;
    tab.tabId = "tab";
    model::Atom original{.key = "old-footnote", .type = "footnoteRef", .raw = slimr::Json::object_t{
        {"footnoteReference", slimr::Json::object_t{{"footnoteId", "provider-note"}}}
    }};
    model::ParagraphBlock paragraph;
    paragraph.inlines = {model::Inline::from_atom(original)};
    tab.blocks.emplace_back(paragraph);
    model::EditTarget target{context, tab};
    AtomPutReport report;
    const auto recreated = atomRecreate(target, original, report);
    REQUIRE(recreated.create);
    CHECK(recreated.create->type == "footnoteRef");
    CHECK(recreated.create->label == "1");
    CHECK(recreated.key != original.key);
    CHECK_FALSE(recreated.raw);
    tab.blocks.clear();
    original.create = model::AtomCreate{.type = "footnoteRef", .label = "7"};
    const auto snapshot = atomRecreate(target, original, report);
    REQUIRE(snapshot.create);
    CHECK(snapshot.create->label == "7");
}

TEST_CASE("source new footnotes are body-only while existing references remain preservable") {
    model::EditContext context;
    model::TabModel tab;
    tab.tabId = "main";
    model::EditTarget target{context, tab};
    AtomPutReport report;
    const TokenRef token{.kind = "footnoteRef", .label = "new"};
    CHECK_NOTHROW(atomTokenCreate(target, token, {}, report));
    for (const auto* segment : {"main::header", "main::footer", "main::footnote"}) {
        tab.tabId = segment;
        try { (void)atomTokenCreate(target, token, {}, report); FAIL("expected body-only footnote refusal"); }
        catch (const model::CoreError& error) {
            CHECK(error.code == "unsupportedSyntax");
            CHECK(error.message == "footnote [^new] can only be referenced from a tab body");
        }
        model::Atom existing{.key = "old", .type = "footnoteRef"};
        const TokenRef reference{.kind = "footnoteRef", .label = "1"};
        CHECK_NOTHROW(atomTokenKeep(existing, reference, reference, tab, {}));
    }
}

TEST_CASE("source default English date locale does not change exported token identity") {
    model::TabModel tab;
    model::Atom date{.key = "old-date", .type = "date", .raw = slimr::Json::object_t{
        {"dateElement", slimr::Json::object_t{{"dateId", "provider-date"}, {"dateElementProperties", slimr::Json::object_t{
            {"timestamp", "2026-03-01T12:00:00Z"}, {"locale", "en"}
        }}}}
    }};
    const TokenRef sourceToken{.kind = "date", .attrs = directiveAttrsParse("id=provider-date value=2026-03-01")};
    const auto projected = atomTokenProject(date, tab);
    CHECK_FALSE(projected.attrs.get_value("locale"));
    CHECK(atomTokenIdentity(projected) == atomTokenIdentity(sourceToken));
    (*date.raw)["dateElement"]["dateElementProperties"]["locale"] = "pt-BR";
    const auto localized = atomTokenProject(date, tab);
    CHECK(localized.attrs.get_value("locale") == "pt-BR");
    CHECK(atomTokenIdentity(localized) != atomTokenIdentity(sourceToken));
}

} // namespace gdocsmith::lens::test
