#pragma once

#include "reconcile/lists.hpp"
#include "reconcile/test_harness.hpp"
#include "test.hpp"

namespace gdocsmith::reconcile::test {

inline JsonObject docWithLists(const std::vector<BlockSpec>& blocks) {
    DocSpec spec;
    TabSpec tspec;
    tspec.blocks = blocks;

    JsonObject lmap = slimr::Json::object_t{};
    slimr::Json::array_t cNesting;
    for (int i = 0; i < 9; ++i) {
        cNesting.push_back(slimr::Json::object_t{{"glyphSymbol", "✱"}});
    }
    lmap["kix.c"] = slimr::Json::object_t{
        {"listProperties", slimr::Json::object_t{{"nestingLevels", std::move(cNesting)}}}
    };

    slimr::Json::array_t pNesting;
    auto preset = listPresetTable().find("BULLET_DISC_CIRCLE_SQUARE");
    if (preset != listPresetTable().end()) {
        for (const auto& item : preset->second) pNesting.push_back(item);
    }
    lmap["kix.p"] = slimr::Json::object_t{
        {"listProperties", slimr::Json::object_t{{"nestingLevels", std::move(pNesting)}}}
    };

    tspec.lists = std::move(lmap);
    spec.tabs = {std::move(tspec)};
    return docJsonBuild(spec);
}

TEST_CASE("bulletsReconcile - paragraph after same-preset list item joins it") {
    auto json = docWithLists({itemSpec("a"), paraSpec({"b"}), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        bulletsSet(t, {k[1]}, BulletIntent{.kind = "bullet"});
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{"createParagraphBullets", "updateParagraphStyle"});
    CHECK(r.ctx.listRebuilds.empty());
}

TEST_CASE("bulletsReconcile - nesting change keeps text and rebuilds run") {
    auto json = docWithLists({itemSpec("a"), itemSpec("b"), itemSpec("c"), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        nestingSet(t, k[1], 1);
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(std::find(kinds.begin(), kinds.end(), "deleteContentRange") == kinds.end());
    CHECK(!r.ctx.listRebuilds.empty());
    CHECK(!r.ctx.listRebuilds[0].lossy);
}

TEST_CASE("bulletsReconcile - kind change splits item into new list") {
    auto json = docWithLists({itemSpec("a"), itemSpec("b"), itemSpec("c"), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        listKindSet(t, {k[1]}, "number");
    });
    CHECK(r.compare.diffs.empty());
    const auto& a = r.final.tabs[0].blocks[0].as_paragraph();
    const auto& b = r.final.tabs[0].blocks[1].as_paragraph();
    const auto& c = r.final.tabs[0].blocks[2].as_paragraph();
    CHECK(b.bullet->listId != a.bullet->listId);
    CHECK(c.bullet->listId == "kix.p");
}

TEST_CASE("bulletsReconcile - numbered list after bullet list stays separate") {
    auto json = docWithLists({itemSpec("a"), paraSpec({"x"}), paraSpec({"y"}), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        bulletsSet(t, {k[1], k[2]}, BulletIntent{.kind = "number"});
    });
    CHECK(r.compare.diffs.empty());
    auto kinds = requestKinds(r.requests);
    CHECK(kinds == std::vector<std::string>{"createParagraphBullets", "updateParagraphStyle", "updateParagraphStyle"});
}

TEST_CASE("bulletsReconcile - rebuilding list restores explicit paragraph spacing mode") {
    auto json = docWithLists({itemSpec("parent"), itemSpec("child", 1), itemSpec("next", 1)});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        for (auto& b : t.tab.blocks) {
            if (b.is_paragraph()) b.as_paragraph().style["spacingMode"] = "NEVER_COLLAPSE";
        }
        nestingSet(t, k[2], 0);
    });
    CHECK(r.compare.diffs.empty());
    CHECK(!r.requests.empty());
    const auto& lastReq = r.requests.back();
    CHECK(lastReq.contains("updateParagraphStyle"));
    CHECK(lastReq["updateParagraphStyle"]["fields"].get<std::string>().find("spacingMode") != std::string::npos);
}

TEST_CASE("bulletsReconcile - top-level item after nested predecessor rebuilds run (insert=false)") {
    auto json = docWithLists({itemSpec("parent"), itemSpec("child", 1), itemSpec("next", 1), paraSpec({"after"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        nestingSet(t, k[2], 0);
    });
    CHECK(r.compare.diffs.empty());
    CHECK(!r.ctx.listRebuilds.empty());
    CHECK(!r.ctx.listRebuilds[0].lossy);
}

TEST_CASE("bulletsReconcile - top-level item after nested predecessor rebuilds run (insert=true)") {
    auto json = docWithLists({itemSpec("parent"), itemSpec("child", 1), paraSpec({"after"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditParagraphSpec eps;
        eps.bullet = BulletRef{.listId = "kix.p", .nestingLevel = 0};
        eps.style = slimr::Json::object_t{
            {"indentFirstLine", dimensionFromPt(18)},
            {"indentStart", dimensionFromPt(36)}
        };
        for (char c : std::string("next")) eps.syms.push_back(SymSpec{.ch = std::string(1, c)});
        paragraphsInsert(t, ContainerRef{.kind = "body"}, 2, {eps});
    });
    CHECK(r.compare.diffs.empty());
    CHECK(!r.ctx.listRebuilds.empty());
}

TEST_CASE("bulletsReconcile - removing bullets restores final indents") {
    auto json = docWithLists({itemSpec("a"), itemSpec("b"), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
        bulletsSet(t, {k[0], k[1]}, std::nullopt);
    });
    CHECK(r.compare.diffs.empty());
    CHECK(requestKinds(r.requests)[0] == "deleteParagraphBullets");
}

TEST_CASE("bulletsReconcile - new nested item after existing ones rebuilds run") {
    auto json = docWithLists({itemSpec("a"), itemSpec("b"), paraSpec({"z"})});
    auto r = differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
        EditParagraphSpec eps;
        eps.bullet = BulletRef{.listId = "kix.p", .nestingLevel = 1};
        eps.style = slimr::Json::object_t{
            {"indentFirstLine", dimensionFromPt(54)},
            {"indentStart", dimensionFromPt(72)}
        };
        eps.syms = {SymSpec{.ch = "c"}};
        paragraphsInsert(t, ContainerRef{.kind = "body"}, 2, {eps});
    });
    CHECK(r.compare.diffs.empty());
    CHECK(r.ctx.listRebuilds[0].keys.size() == 3);
}

TEST_CASE("bulletsReconcile - new list right after same-preset list is refused") {
    auto json = docWithLists({itemSpec("a"), paraSpec({"x"}), paraSpec({"z"})});
    bool threw = false;
    try {
        differentialRun(json, [](EditTarget& t, const std::vector<std::string>&) {
            auto& x = t.tab.blocks[1].as_paragraph();
            x.bullet = BulletRef{.listId = "new:list:x", .nestingLevel = 0};
            ListDef def;
            def.isNew = true;
            auto preset = listPresetTable().find("BULLET_DISC_CIRCLE_SQUARE");
            if (preset != listPresetTable().end()) def.nestingLevels = preset->second;
            def.preset = "BULLET_DISC_CIRCLE_SQUARE";
            t.tab.lists["new:list:x"] = std::move(def);
        });
    } catch (const CoreError& err) {
        threw = true;
        CHECK(std::string(err.what()).find("merge them") != std::string::npos);
    }
    CHECK(threw);
}

TEST_CASE("bulletsReconcile - nesting item of custom list is refused") {
    auto json = docWithLists({itemSpec("a", 0, "kix.c"), itemSpec("b", 0, "kix.c"), paraSpec({"z"})});
    bool threw = false;
    try {
        differentialRun(json, [](EditTarget& t, const std::vector<std::string>& k) {
            nestingSet(t, k[1], 1);
        });
    } catch (const CoreError& err) {
        threw = true;
        CHECK(std::string(err.what()).find("can't start a new list at a nested level") != std::string::npos);
    }
    CHECK(threw);
}

} // namespace gdocsmith::reconcile::test
