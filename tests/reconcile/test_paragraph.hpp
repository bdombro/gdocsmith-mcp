#pragma once

#include "reconcile/paragraph.hpp"
#include "reconcile/test_harness.hpp"
#include "test.hpp"

namespace gdocsmith::reconcile::test {

inline std::vector<std::string> requestKinds(const std::vector<JsonObject>& reqs) {
    std::vector<std::string> res;
    for (const auto& r : reqs) {
        if (r.holds<slimr::Json::object_t>() && !r.get<slimr::Json::object_t>().empty()) {
            res.push_back(r.get<slimr::Json::object_t>().begin()->first);
        }
    }
    return res;
}

inline DifferentialResult roundTripParagraph(
    const std::vector<BlockSpec>& blocks,
    std::function<void(EditTarget& target, const std::vector<std::string>& keys)> mutate
) {
    DocSpec spec;
    TabSpec tspec;
    tspec.blocks = blocks;
    spec.tabs = {std::move(tspec)};
    JsonObject json = docJsonBuild(spec);

    KeyAllocator keys;
    DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    DocModel final = original;
    TabModel& tab = final.tabs[0];

    EditContext editCtx{
        .keys = keys,
        .stamp = EditStamp{.force = false, .tabIndex = 0},
        .tombstones = {}
    };
    EditTarget target{.ctx = editCtx, .tab = tab};

    std::vector<std::string> origKeys;
    for (const auto& b : tab.blocks) origKeys.push_back(b.key);
    mutate(target, origKeys);

    ReconcileContext ctx = reconcileContextCreate("t.0");
    std::vector<ParagraphBlock> originals;
    for (const auto& b : original.tabs[0].blocks) {
        if (b.is_paragraph()) originals.push_back(b.as_paragraph());
    }
    std::reverse(originals.begin(), originals.end());
    for (const auto& o : originals) {
        for (const auto& b : final.tabs[0].blocks) {
            if (b.key == o.key && b.is_paragraph()) {
                paragraphReconcile(o, b.as_paragraph(), ctx);
                break;
            }
        }
    }

    auto emulated = emulator::requestsEmulate(json, ctx.requests).json;
    CompareResult comp = docModelsCompare(final, docModelParse(emulated, DocModelParseOptions{.docId = "d", .keys = KeyAllocator{}}));

    return DifferentialResult{
        .compare = std::move(comp),
        .ctx = std::move(ctx),
        .final = std::move(final),
        .json = std::move(emulated),
        .requests = ctx.requests
    };
}

TEST_CASE("paragraphReconcile - unchanged -> no requests") {
    auto r = roundTripParagraph({paraSpec({"hello"})}, [](EditTarget&, const std::vector<std::string>&) {});
    CHECK(r.requests.empty());
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - a word change is one delete and one insert, restyled with the full mask") {
    auto r = roundTripParagraph({paraSpec({"the cat sat"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        paragraphSplice(t, k[0], 4, 3, {SymSpec{.ch = "dog"}});
    });
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{"deleteContentRange", "insertText", "updateTextStyle"});
    CHECK(r.requests[0]["deleteContentRange"]["range"]["startIndex"].as<int>() == 5);
    CHECK(r.requests[0]["deleteContentRange"]["range"]["endIndex"].as<int>() == 8);
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - a bold toggle sends only the changed field") {
    auto r = roundTripParagraph({paraSpec({"ab", "cd"}), paraSpec({"z"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        textStyleUpdate(t, k[0], 1, 3, slimr::Json::object_t{{"bold", true}});
    });
    CHECK(r.requests.size() == 1);
    CHECK(r.requests[0].contains("updateTextStyle"));
    CHECK(r.requests[0]["updateTextStyle"]["fields"].get<std::string>() == "bold");
    CHECK(r.requests[0]["updateTextStyle"]["range"]["startIndex"].as<int>() == 2);
    CHECK(r.requests[0]["updateTextStyle"]["range"]["endIndex"].as<int>() == 4);
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - unsetting a field is a mask entry without a value") {
    ParagraphTextSpec textSpec;
    textSpec.text = "ab";
    textSpec.style = slimr::Json::object_t{{"italic", true}};
    auto r = roundTripParagraph({paraSpec({textSpec})}, [](EditTarget& t, const std::vector<std::string>& k) {
        textStyleUpdate(t, k[0], 0, 2, slimr::Json::object_t{{"italic", slimr::Json()}});
    });
    CHECK(!r.requests.empty());
    CHECK(r.requests[0]["updateTextStyle"]["fields"].get<std::string>() == "italic");
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - a partial restyle leaves the newline alone") {
    auto r = roundTripParagraph({paraSpec({"abc"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        textStyleUpdate(t, k[0], 0, 3, slimr::Json::object_t{{"bold", true}});
        textStyleUpdate(t, k[0], 1, 2, slimr::Json::object_t{{"bold", slimr::Json()}});
    });
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - deleting an atom and inserting a person chip") {
    ParagraphAtomSpec atomSpec;
    atomSpec.type = "richLink";
    auto r = roundTripParagraph({paraSpec({"a", atomSpec, "b"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        paragraphSplice(t, k[0], 1, 1, {SymSpec{
            .create = AtomCreate{.type = "person", .email = "p@x.test"}
        }});
    });
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{"deleteContentRange", "insertPerson"});
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - middle paragraph newline and paragraph style in place") {
    auto r = roundTripParagraph({paraSpec({"a"}), paraSpec({"b"}), paraSpec({"c"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        auto& p = t.tab.blocks[1].as_paragraph();
        p.newline.style = slimr::Json::object_t{{"bold", true}};
        paragraphStyleUpdate(t, k[1], slimr::Json::object_t{{"alignment", "CENTER"}});
    });
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{"updateParagraphStyle", "updateTextStyle"});
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - empty last body paragraph restyles directly") {
    auto r = roundTripParagraph({paraSpec({"a"}), paraSpec({})}, [](EditTarget& t, const std::vector<std::string>& k) {
        paragraphStyleUpdate(t, k[1], slimr::Json::object_t{{"namedStyleType", "HEADING_2"}});
    });
    CHECK(r.requests.size() == 1);
    CHECK(r.requests[0].contains("updateParagraphStyle"));
    CHECK(r.requests[0]["updateParagraphStyle"]["range"]["startIndex"].as<int>() == 3);
    CHECK(r.requests[0]["updateParagraphStyle"]["range"]["endIndex"].as<int>() == 4);
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - emoji edit replaces whole code points") {
    auto r = roundTripParagraph({paraSpec({"a😀b"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        paragraphSplice(t, k[0], 1, 1, {SymSpec{.ch = "😎"}});
    });
    CHECK(r.requests[0]["deleteContentRange"]["range"]["startIndex"].as<int>() == 2);
    CHECK(r.requests[0]["deleteContentRange"]["range"]["endIndex"].as<int>() == 4);
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - heading change keeps text") {
    auto r = roundTripParagraph({paraSpec({"Title"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        paragraphStyleUpdate(t, k[0], slimr::Json::object_t{{"namedStyleType", "HEADING_1"}});
    });
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{"updateParagraphStyle"});
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - new link lists underline and foregroundColor") {
    auto r = roundTripParagraph({paraSpec({"link me"}), paraSpec({"z"})}, [](EditTarget& t, const std::vector<std::string>& k) {
        textStyleUpdate(t, k[0], 0, 4, slimr::Json::object_t{
            {"link", slimr::Json::object_t{{"url", "https://x.test"}}}
        });
    });
    CHECK(r.requests[0]["updateTextStyle"]["fields"].get<std::string>() == "foregroundColor,link,underline");
    CHECK(r.compare.diffs.empty());
}

TEST_CASE("paragraphReconcile - pending link recorded") {
    std::string paragraphKey;
    auto r = roundTripParagraph({paraSpec({"see here"})}, [&](EditTarget& t, const std::vector<std::string>& k) {
        paragraphKey = k[0];
        textStyleUpdate(t, k[0], 4, 8, slimr::Json::object_t{
            {"link", slimr::Json::object_t{
                {"heading", slimr::Json::object_t{
                    {"key", "n9"},
                    {"tabId", "t.0"}
                }}
            }}
        });
    });
    CHECK(!r.ctx.pendingLinks.empty());
    CHECK(r.ctx.pendingLinks[0].headingKey == "n9");
    CHECK(r.ctx.pendingLinks[0].paragraphKey == paragraphKey);
    CHECK(r.ctx.pendingLinks[0].offset == 4);
    CHECK(r.ctx.pendingLinks[0].length == 4);
}

} // namespace gdocsmith::reconcile::test
