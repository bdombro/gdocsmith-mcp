#pragma once

#include "model/equivalence.hpp"
#include "model/types.hpp"
#include "test.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model::test {

inline TabModel makeTab(std::vector<Block> blocks, JsonObject namedStyles = make_json_object()) {
    TabModel t;
    t.tabId = "t.0";
    t.title = "Tab";
    t.isNew = false;
    t.blocks = std::move(blocks);
    t.documentStyle = make_json_object();
    t.leadingSectionStyle = make_json_object();
    t.namedStyles = std::move(namedStyles);
    t.headersFooters = make_json_object();
    return t;
}

inline DocModel makeDoc(std::vector<TabModel> tabs) {
    DocModel d;
    d.docId = "d1";
    d.title = "Doc";
    d.isNew = false;
    d.tabs = std::move(tabs);
    return d;
}

inline ParagraphBlock makeParagraph(
    std::string key,
    std::vector<Inline> inlines = {},
    JsonObject style = make_json_object(),
    std::optional<BulletRef> bullet = std::nullopt,
    std::optional<std::string> headingId = std::nullopt
) {
    ParagraphBlock p;
    p.key = std::move(key);
    p.inlines = std::move(inlines);
    p.style = std::move(style);
    p.bullet = std::move(bullet);
    p.headingId = std::move(headingId);
    p.newline = NewlineMark{.style = make_json_object()};
    p.is_protected = false;
    return p;
}

TEST_CASE("docModelsCompare - equal despite run splits (merged by canonical style)") {
    auto bold = slimr::Json::object_t{{"bold", true}};
    auto notBold = slimr::Json::object_t{{"bold", false}};

    auto expected = makeDoc({makeTab({makeParagraph("o1", {Inline::from_text("Hello world", bold)})})});
    auto actual = makeDoc({makeTab({makeParagraph("o1", {Inline::from_text("Hello ", bold), Inline::from_text("world", bold)})})});

    auto result = docModelsCompare(expected, actual);
    CHECK(result.diffs.empty());
    CHECK(result.equal == true);

    auto changed = actual;
    changed.tabs[0].blocks[0].as_paragraph().inlines[1] = Inline::from_text("world", notBold);
    CHECK(docModelsCompare(expected, changed).equal == false);

    changed.tabs[0].blocks[0].as_paragraph().inlines[1] = Inline::from_text("World", bold);
    CHECK(docModelsCompare(expected, changed).equal == false);
}

TEST_CASE("docModelsCompare - listIds match exactly for kept lists, but not for two newly created lists") {
    auto bulletAbc = BulletRef{.listId = "kix.abc", .nestingLevel = 0};
    auto bulletXyz = BulletRef{.listId = "kix.xyz", .nestingLevel = 0};
    auto keptExpected = makeDoc({makeTab({makeParagraph("o1", {}, make_json_object(), bulletAbc)})});
    auto keptActualSame = makeDoc({makeTab({makeParagraph("o1", {}, make_json_object(), bulletAbc)})});
    CHECK(docModelsCompare(keptExpected, keptActualSame).equal == true);

    auto keptActualDiff = makeDoc({makeTab({makeParagraph("o1", {}, make_json_object(), bulletXyz)})});
    CHECK(docModelsCompare(keptExpected, keptActualDiff).equal == false);

    auto bulletNew1 = BulletRef{.listId = "new:list:1", .nestingLevel = 0};
    auto bulletReal = BulletRef{.listId = "kix.realid", .nestingLevel = 0};
    auto newExpected = makeDoc({makeTab({makeParagraph("n1", {}, make_json_object(), bulletNew1)})});
    auto newActual = makeDoc({makeTab({makeParagraph("n1", {}, make_json_object(), bulletReal)})});
    CHECK(docModelsCompare(newExpected, newActual).equal == true);
}

TEST_CASE("docModelsCompare - a new heading needs some headingId; a kept heading needs the same one") {
    auto h1Style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    auto keptExpected = makeDoc({makeTab({makeParagraph("o1", {}, h1Style, std::nullopt, "h.1")})});
    auto keptActualMissing = makeDoc({makeTab({makeParagraph("o1", {}, h1Style)})});
    CHECK(docModelsCompare(keptExpected, keptActualMissing).equal == false);

    auto keptActualSame = makeDoc({makeTab({makeParagraph("o1", {}, h1Style, std::nullopt, "h.1")})});
    CHECK(docModelsCompare(keptExpected, keptActualSame).equal == true);

    auto newExpected = makeDoc({makeTab({makeParagraph("n1", {}, h1Style)})});
    auto newActualWithId = makeDoc({makeTab({makeParagraph("n1", {}, h1Style, std::nullopt, "h.new")})});
    CHECK(docModelsCompare(newExpected, newActualWithId).equal == true);

    auto newActualMissing = makeDoc({makeTab({makeParagraph("n1", {}, h1Style)})});
    CHECK(docModelsCompare(newExpected, newActualMissing).equal == false);
}

TEST_CASE("docModelsCompare - identity transfer moves the expected headingId onto a different key") {
    auto h1Style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}};
    auto expected = makeDoc({makeTab({makeParagraph("o1", {}, h1Style, std::nullopt, "h.1")})});
    auto actual = makeDoc({makeTab({makeParagraph("o1", {}, h1Style, std::nullopt, "h.1")})});
    auto transferred = makeDoc({makeTab({makeParagraph("o1", {}, h1Style, std::nullopt, "h.moved")})});

    CHECK(docModelsCompare(expected, actual).equal == true);
    CHECK(docModelsCompare(expected, transferred).equal == false);

    CompareOptions opts;
    opts.identityTransfers = {IdentityTransfer{.fromKey = "o1", .headingId = "h.moved", .tabId = "t.0", .toKey = "o1"}};
    CHECK(docModelsCompare(expected, transferred, opts).equal == true);
}

TEST_CASE("docModelsCompare - a new image atom compares by its create fields only, ignoring style") {
    Atom expAtom;
    expAtom.key = "n2";
    expAtom.type = "image";
    expAtom.length = 1;
    expAtom.create = AtomCreate{.type = "image", .uri = "https://x/img.png"};

    Atom actAtom = expAtom;
    actAtom.style = slimr::Json::object_t{
        {"backgroundColor", slimr::Json::object_t{
            {"color", slimr::Json::object_t{
                {"rgbColor", slimr::Json::object_t{}}
            }}
        }}
    };

    auto expected = makeDoc({makeTab({makeParagraph("n1", {Inline::from_atom(expAtom)})})});
    auto actual = makeDoc({makeTab({makeParagraph("n1", {Inline::from_atom(actAtom)})})});

    CHECK(docModelsCompare(expected, actual).equal == true);
}

TEST_CASE("docModelsCompare - a paragraph-style value Google omits matches only when the named style has it") {
    JsonObject namedStyles = slimr::Json::object_t{
        {"styles", slimr::Json::array_t{
            slimr::Json::object_t{
                {"namedStyleType", "NORMAL_TEXT"},
                {"paragraphStyle", slimr::Json::object_t{
                    {"indentStart", slimr::Json::object_t{{"unit", "PT"}}}
                }}
            }
        }}
    };

    auto unbulleted = [&](double magnitude) {
        JsonObject style = slimr::Json::object_t{
            {"indentStart", slimr::Json::object_t{
                {"magnitude", magnitude},
                {"unit", "PT"}
            }}
        };
        return makeDoc({makeTab({makeParagraph("o1", {}, style)}, namedStyles)});
    };

    auto omitted = makeDoc({makeTab({makeParagraph("o1")}, namedStyles)});

    auto res0 = docModelsCompare(unbulleted(0), omitted);
    CHECK(res0.diffs.empty());
    CHECK(res0.equal == true);

    auto res36 = docModelsCompare(unbulleted(36), omitted);
    CHECK(res36.equal == false);
}

TEST_CASE("docModelsCompare - diffs are human-readable and name the field") {
    auto expected = makeDoc({makeTab({makeParagraph("o1", {Inline::from_text("a")})})});
    auto actual = makeDoc({makeTab({makeParagraph("o1", {Inline::from_text("b")})})});
    auto result = docModelsCompare(expected, actual);
    CHECK(result.equal == false);
    REQUIRE(!result.diffs.empty());
    CHECK(result.diffs[0].find("text") != std::string::npos);
    CHECK(result.diffs[0].find("expected") != std::string::npos);
}

} // namespace gdocsmith::model::test
