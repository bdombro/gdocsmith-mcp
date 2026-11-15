#pragma once

#include "lens/images.hpp"
#include "lens/atom_put.hpp"
#include "reconcile/paragraph.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("image recreation reports crop and alt text without treating neutral adjustments as loss") {
    model::JsonObject image = slimr::Json::object_t{
        {"description", "alt text"},
        {"imageProperties", slimr::Json::object_t{
            {"contentUri", "https://example.invalid/bearer"},
            {"brightness", 0.0},
            {"cropProperties", slimr::Json::object_t{{"offsetLeft", 0.2}}}
        }}
    };
    const auto losses = imageRecreationLosses(image);
    CHECK(losses == std::vector<std::string>({"description", "cropProperties"}));
    image = slimr::Json::object_t{{"imageProperties", slimr::Json::object_t{
        {"brightness", 0.0}, {"cropProperties", slimr::Json::object_t{{"offsetLeft", 0.0}}}
    }}};
    CHECK(imageRecreationLosses(image).empty());
}

TEST_CASE("imageUriResolve for remote, upload and local paths") {
    CHECK(imageUriResolve("https://example.com/pic.png", std::nullopt) == "https://example.com/pic.png");
    CHECK(imageUriResolve("http://example.com/pic.png", std::nullopt) == "gdocsmith-upload:http://example.com/pic.png");

    std::string rel_res = imageUriResolve("images/pic.png", "/tmp");
    CHECK(rel_res == "gdocsmith-upload:/tmp/images/pic.png");

    bool threw = false;
    try {
        imageUriResolve("images/pic.png", std::nullopt);
    } catch (const CoreError& err) {
        threw = true;
        CHECK(err.code == "unsupportedSyntax");
    }
    CHECK(threw);

    bool scheme_threw = false;
    try {
        imageUriResolve("ftp://example.com/pic.png", std::nullopt);
    } catch (const CoreError& err) {
        scheme_threw = true;
        CHECK(err.code == "unsupportedSyntax");
    }
    CHECK(scheme_threw);
}

TEST_CASE("source image recreation uses fresh content URI dimensions and reports loss without retaining identity") {
    model::DocModel doc;
    doc.docId = "doc";
    model::TabModel tab;
    tab.tabId = "tab";
    model::Atom original{.key = "old-image", .type = "image", .raw = slimr::Json::object_t{
        {"inlineObjectElement", slimr::Json::object_t{{"inlineObjectId", "provider-image"}, {"suggestedDeletionIds", slimr::Json::array_t{"s1"}}}}
    }};
    tab.inlineObjects["provider-image"] = slimr::Json::object_t{
        {"inlineObjectProperties", slimr::Json::object_t{{"embeddedObject", slimr::Json::object_t{
            {"title", "Logo"},
            {"size", slimr::Json::object_t{{"height", slimr::Json::object_t{{"magnitude", 90.5}}},
                                          {"width", slimr::Json::object_t{{"magnitude", 240}}}}},
            {"imageProperties", slimr::Json::object_t{
                {"contentUri", "https://example.invalid/fresh-image"},
                {"sourceUri", "https://example.invalid/original-image"},
                {"cropProperties", slimr::Json::object_t{{"offsetLeft", 0.1}}},
                {"brightness", 0}}}
        }}}}
    };
    model::EditContext context;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    const auto recreated = atomRecreate(target, original, doc, report);
    REQUIRE(recreated.create);
    CHECK(recreated.key != original.key);
    CHECK_FALSE(recreated.raw);
    CHECK(recreated.create->uri == "https://example.invalid/fresh-image");
    CHECK(recreated.create->heightPt == 90.5);
    CHECK(recreated.create->widthPt == 240);
    REQUIRE(report.recreated.size() == 1);
    CHECK(report.recreated[0].lost == std::vector<std::string>{"crop", "alt text"});
    CHECK(report.recreated[0].source.key == original.key);
    CHECK(report.recreated[0].source.docId == "doc");
    tab.inlineObjects["provider-image"]["inlineObjectProperties"]["embeddedObject"]["linkedContentReference"] = slimr::Json::object_t{};
    CHECK_THROWS_AS(atomRecreate(target, original, doc, report), model::CoreError);
    tab.inlineObjects.clear();
    CHECK_THROWS_AS(atomRecreate(target, original, doc, report), model::CoreError);
}

TEST_CASE("source sized image creation replacement and invalid dimensions reach atom helper") {
    model::EditContext context;
    model::TabModel tab;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    TokenRef token{.kind = "image", .attrs = directiveAttrsParse("src=https://example.invalid/image width=240 height=90.5"), .label = "Logo"};
    auto image = atomTokenCreate(target, token, {}, report);
    REQUIRE(image.create);
    CHECK(image.create->heightPt == 90.5);
    CHECK(image.create->widthPt == 240);
    CHECK(report.notes.size() == 1);
    CHECK(report.notes.front() == "alt text \"Logo\" was dropped: Google can't set an inserted image's alt text");
    CHECK_NOTHROW(atomTokenCreate(target, token, {}, report));
    CHECK(report.notes.size() == 1);
    auto kept = token;
    kept.attrs = directiveAttrsParse("src=http://example.invalid/replacement");
    atomTokenKeep(image, token, kept, tab, {});
    CHECK(image.replaceUri == "gdocsmith-upload:http://example.invalid/replacement");
    for (const auto* size : {"0", "-1", "nan", "inf", "2px"}) {
        token.attrs = directiveAttrsParse(std::string("src=https://example.invalid/image width=") + size);
        CHECK_THROWS_AS(atomTokenCreate(target, token, {}, report), model::CoreError);
    }
}

TEST_CASE("source image resolver copy cannot override source dimensions and unknown sources refuse") {
    model::EditContext context;
    model::TabModel tab;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    AtomPutOptions options;
    options.images = [](const std::string& reference) -> std::optional<ImageSource> {
        if (reference != "gdoc:source/picture") return std::nullopt;
        ImageSource source;
        source.create = {.type = "image", .heightPt = 50, .uri = "https://example.invalid/fresh", .widthPt = 100};
        return source;
    };
    TokenRef token{.kind = "image", .attrs = directiveAttrsParse("src=gdoc:source/picture")};
    const auto image = atomTokenCreate(target, token, options, report);
    REQUIRE(image.create);
    CHECK(image.create->widthPt == 100);
    REQUIRE(report.recreated.size() == 1);
    token.attrs.values.emplace_back("width", "20");
    CHECK_THROWS_AS(atomTokenCreate(target, token, options, report), model::CoreError);
    token.attrs = directiveAttrsParse("src=gdoc:source/unknown");
    CHECK_THROWS_AS(atomTokenCreate(target, token, options, report), model::CoreError);
}

TEST_CASE("created atom intents reach real back-to-front request emission without invented provider IDs") {
    model::EditContext context;
    model::TabModel tab;
    tab.tabId = "tab";
    model::EditTarget target{context, tab};
    AtomPutReport report;
    const auto image = atomTokenCreate(target, TokenRef{
        .kind = "image", .attrs = directiveAttrsParse("src=https://example.invalid/image width=240 height=90.5")
    }, {}, report);
    const auto person = atomTokenCreate(target, TokenRef{
        .kind = "person", .attrs = directiveAttrsParse("email=person@example.invalid")
    }, {}, report);
    reconcile::ReconcileContext requests;
    requests.tabId = "tab";
    reconcile::symbolsInsertEmit(6, {
        model::Sym(model::CharSym{.ch = "a"}), model::Sym(image),
        model::Sym(model::CharSym{.ch = "b"}), model::Sym(person)
    }, requests, {});
    REQUIRE(requests.requests.size() == 4);
    CHECK(requests.requests[0]["insertPerson"]["personProperties"]["email"] == slimr::Json("person@example.invalid"));
    CHECK(requests.requests[1]["insertText"]["text"] == slimr::Json("b"));
    CHECK(requests.requests[2]["insertInlineImage"]["uri"] == slimr::Json("https://example.invalid/image"));
    CHECK(requests.requests[2]["insertInlineImage"]["objectSize"]["width"]["magnitude"].as<double>() == 240);
    CHECK(requests.requests[2]["insertInlineImage"]["objectSize"]["height"]["magnitude"].as<double>() == 90.5);
    CHECK(requests.requests[3]["insertText"]["text"] == slimr::Json("a"));
    for (const auto& request : requests.requests) {
        const auto& body = request.get<slimr::Json::object_t>().begin()->second;
        CHECK(body["location"]["index"].as<int>() == 6);
        CHECK(body["location"]["tabId"] == slimr::Json("tab"));
        CHECK_FALSE(body.contains("inlineObjectId"));
        CHECK_FALSE(body.contains("personId"));
    }
}

TEST_CASE("source put fallback uses only authoritative HTTPS source URI without inventing size or copy support") {
    model::DocModel doc;
    doc.docId = "doc";
    model::TabModel tab;
    tab.tabId = "tab";
    model::Atom original{.key = "old-image", .type = "image", .raw = slimr::Json::object_t{
        {"inlineObjectElement", slimr::Json::object_t{{"inlineObjectId", "provider-image"}}}
    }};
    tab.inlineObjects["provider-image"] = slimr::Json::object_t{
        {"inlineObjectProperties", slimr::Json::object_t{{"embeddedObject", slimr::Json::object_t{
            {"title", "Logo"},
            {"size", slimr::Json::object_t{{"width", slimr::Json::object_t{{"magnitude", 240}}}}},
            {"imageProperties", slimr::Json::object_t{
                {"sourceUri", "https://example.invalid/original"},
                {"cropProperties", slimr::Json::object_t{{"offsetLeft", 0.1}}}}}
        }}}}
    };
    model::ParagraphBlock paragraph;
    paragraph.inlines = {model::Inline::from_atom(original)};
    tab.blocks.emplace_back(paragraph);
    doc.tabs = {tab};
    model::EditContext context;
    model::EditTarget target{context, tab};
    AtomPutReport report;
    const auto recreated = atomRecreate(target, original, doc, report);
    REQUIRE(recreated.create);
    CHECK(recreated.create->uri == "https://example.invalid/original");
    CHECK_FALSE(recreated.create->widthPt);
    CHECK_FALSE(recreated.create->heightPt);
    CHECK_FALSE(recreated.raw);
    REQUIRE(report.recreated.size() == 1);
    CHECK(report.recreated[0].sourceUriFallback);
    CHECK(report.recreated[0].source.key == original.key);
    CHECK(report.recreated[0].source.docId == "doc");
    CHECK(report.recreated[0].lost.empty());
    CHECK(report.notes.empty());
    CHECK(atomTokenProject(original, tab).label == "Logo");
    CHECK_FALSE(imageSourceOf(doc, tab, original));
    CHECK_FALSE(imageResolverBuild({doc})("provider-image"));
    auto& embedded = tab.inlineObjects["provider-image"]["inlineObjectProperties"]["embeddedObject"];
    embedded["imageProperties"]["contentUri"] = "https://example.invalid/fresh";
    const auto preferred = imageRecreationSourceOf(doc, tab, original);
    REQUIRE(preferred);
    CHECK(preferred->create.uri == "https://example.invalid/fresh");
    CHECK(preferred->create.widthPt == 240);
    CHECK_FALSE(preferred->sourceUriFallback);
    for (const auto* invalid : {"", "http://example.invalid/fresh"}) {
        embedded["imageProperties"]["contentUri"] = invalid;
        try { (void)atomRecreate(target, original, doc, report); FAIL("expected invalid fresh source refusal"); }
        catch (const model::CoreError& error) { CHECK(error.code == "invalidAtom"); }
    }
    embedded["imageProperties"] = slimr::Json::object_t{};
    CHECK_THROWS_AS(atomRecreate(target, original, doc, report), model::CoreError);
    embedded["imageProperties"]["sourceUri"] = "http://example.invalid/original";
    CHECK_THROWS_AS(atomRecreate(target, original, doc, report), model::CoreError);
    embedded["imageProperties"]["sourceUri"] = "https://example.invalid/original";
    for (const auto* field : {"embeddedDrawingProperties", "linkedContentReference"}) {
        embedded[field] = slimr::Json::object_t{};
        CHECK_THROWS_AS(atomRecreate(target, original, doc, report), model::CoreError);
        embedded.get<slimr::Json::object_t>().erase(field);
    }
}

} // namespace gdocsmith::lens::test
