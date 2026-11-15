#pragma once

#include "engine/session.hpp"
#include "engine/suggestion_rule.hpp"
#include "engine/test_import.hpp"
#include "lens/parse.hpp"
#include "lens/put.hpp"
#include "reconcile/reconcile.hpp"

#include <string>
#include <vector>

namespace gdocsmith::engine::test {

// Fault controls C-P1-a/b/c; flip one temporarily to prove the assertions fire, then restore None.
enum class ReviewWritesControl { None, ExtraParagraphStyle, ExtraTextStyle, StoredDirection, StoredBold, DropComment };
inline constexpr auto reviewWritesControl = ReviewWritesControl::None;

inline const slimr::Json& reviewWritesParagraph(RemoteImportFixture& fixture, size_t index) {
    return fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["body"]["content"][index]["paragraph"];
}

inline slimr::Json reviewWritesNewlineStyle(const slimr::Json& paragraph) {
    const auto& elements = paragraph["elements"].get<slimr::Json::array_t>();
    const auto& run = elements.back()["textRun"];
    return run.contains("textStyle") ? run["textStyle"] : slimr::Json(slimr::Json::object_t{});
}

inline slimr::Json reviewWritesParagraphStyle(const slimr::Json& paragraph) {
    return paragraph.contains("paragraphStyle") ? paragraph["paragraphStyle"] : slimr::Json(slimr::Json::object_t{});
}

// Key-order-insensitive object comparison; the emulator may reorder stored style keys.
inline bool reviewWritesSameObject(const slimr::Json& a, const slimr::Json& b) {
    const auto& left = a.get<slimr::Json::object_t>();
    const auto& right = b.get<slimr::Json::object_t>();
    if (left.size() != right.size()) return false;
    for (const auto& [key, value] : left) {
        if (!b.contains(key) || slimr::write_json(value) != slimr::write_json(b[key])) return false;
    }
    return true;
}

// Run text and text style, without indices that shift after earlier deletes.
inline std::string reviewWritesRunStyles(const slimr::Json& paragraph) {
    std::string out;
    for (const auto& element : paragraph["elements"].get<slimr::Json::array_t>()) {
        const auto& run = element["textRun"];
        out += slimr::write_json(run["content"]) + slimr::write_json(run.contains("textStyle") ? run["textStyle"] : slimr::Json()) + ";";
    }
    return out;
}

inline void reviewWritesSeedComment(RemoteImportFixture& fixture) {
    DriveComment comment;
    comment.id = "synthetic_comment";
    comment.content = "Synthetic note";
    comment.author = DriveCommentAuthor{.me = true};
    comment.replies.push_back(DriveCommentReply{.id = "synthetic_reply", .content = "Synthetic reply"});
    fixture.comments.push_back(std::move(comment));
}

struct ReviewWritesExpect {
    int deleteStart = 0;
    int deleteEnd = 0;
    std::vector<ReviewRun> runs;
};

// Rejects the sole suggestion and asserts one delete, no restyle, and unchanged styles and comments.
inline void reviewWritesRejectCheck(RemoteImportFixture& fixture, const ReviewWritesExpect& expect, size_t paragraphs) {
    std::vector<slimr::Json> styles, newlines;
    for (size_t i = 1; i <= paragraphs; ++i) {
        styles.push_back(reviewWritesParagraphStyle(reviewWritesParagraph(fixture, i)));
        newlines.push_back(reviewWritesNewlineStyle(reviewWritesParagraph(fixture, i)));
    }
    const auto italicBefore = paragraphs > 1 ? reviewWritesRunStyles(reviewWritesParagraph(fixture, 2)) : "";
    reviewWritesSeedComment(fixture);
    auto file = exportDocumentFile(model::docModelParse(fixture.remote.getDocument("synthetic_doc"),
        model::DocModelParseOptions{.docId = "synthetic_doc"}), fixture.comments, ExportOptions{.sourceVersion = std::nullopt});
    REQUIRE(file.comments.size() == 1);
    REQUIRE(file.suggestions.size() == 1);
    file.suggestions[0].isReject = true;

    if constexpr (reviewWritesControl == ReviewWritesControl::ExtraParagraphStyle ||
                  reviewWritesControl == ReviewWritesControl::ExtraTextStyle) {
        fixture.remote.batchUpdate = [&fixture, inner = fixture.remote.batchUpdate](const std::string& id,
            const std::vector<slimr::Json>& requests, const std::optional<std::string>& revision) {
            auto extended = requests;
            if (reviewWritesControl == ReviewWritesControl::ExtraParagraphStyle) {
                extended.push_back(slimr::Json::object_t{{"updateParagraphStyle", slimr::Json::object_t{
                    {"range", slimr::Json::object_t{{"startIndex", 1}, {"endIndex", 2}, {"tabId", "t.0"}}},
                    {"paragraphStyle", slimr::Json::object_t{{"direction", "LEFT_TO_RIGHT"}}}, {"fields", "direction"}}}});
            } else {
                const auto end = fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["body"]
                    ["content"][1]["endIndex"].get<int64_t>();
                extended.insert(extended.begin(), slimr::Json::object_t{{"updateTextStyle", slimr::Json::object_t{
                    {"range", slimr::Json::object_t{{"startIndex", end - 1}, {"endIndex", end}, {"tabId", "t.0"}}},
                    {"textStyle", slimr::Json::object_t{{"italic", true}}}, {"fields", "italic"}}}});
            }
            return inner(id, extended, revision);
        };
    }

    const auto result = fileImport(fixture.save(file), fixture.remote);
    INFO(result.summary);
    std::string errors;
    if (result.errors) for (const auto& error : *result.errors) errors += error.code + ": " + error.message + "; ";
    INFO(errors);
    CHECK(result.status == "applied");
    CHECK(result.verification == "match");
    CHECK_FALSE(result.errors);

    if constexpr (reviewWritesControl == ReviewWritesControl::StoredDirection) {
        fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["body"]["content"][1]["paragraph"]
            ["paragraphStyle"]["direction"] = "LEFT_TO_RIGHT";
    }
    if constexpr (reviewWritesControl == ReviewWritesControl::StoredBold) {
        auto& elements = fixture.backend.get_doc("synthetic_doc").json["tabs"][0]["documentTab"]["body"]["content"][1]
            ["paragraph"]["elements"];
        elements[elements.size() - 1]["textRun"]["textStyle"] = slimr::Json::object_t{{"bold", false}};
    }
    if constexpr (reviewWritesControl == ReviewWritesControl::DropComment) fixture.comments.front().replies.clear();

    // Requests: exactly one batch holding one delete over the inserted run.
    CHECK(fixture.batches == 1);
    CHECK(fixture.sentRequests.size() == 1);
    if (!fixture.sentRequests.empty()) {
        const auto& sent = fixture.sentRequests.front();
        for (const auto& request : sent) {
            INFO(slimr::write_json(request));
            CHECK_FALSE(request.contains("updateParagraphStyle"));
            CHECK_FALSE(request.contains("updateTextStyle"));
        }
        CHECK(sent.size() == 1);
        if (!sent.empty() && sent[0].contains("deleteContentRange")) {
            const auto& range = sent[0]["deleteContentRange"]["range"];
            CHECK(range["startIndex"].get<int64_t>() == expect.deleteStart);
            CHECK(range["endIndex"].get<int64_t>() == expect.deleteEnd);
        } else {
            FAIL_CHECK("first request is not deleteContentRange");
        }
    }

    // Storage: paragraph style, newline style and untouched runs stay as they were.
    for (size_t i = 1; i <= paragraphs; ++i) {
        INFO("paragraph " << i);
        const auto style = reviewWritesParagraphStyle(reviewWritesParagraph(fixture, i));
        const auto newline = reviewWritesNewlineStyle(reviewWritesParagraph(fixture, i));
        INFO(slimr::write_json(style) << " vs " << slimr::write_json(styles[i - 1]));
        CHECK(reviewWritesSameObject(style, styles[i - 1]));
        INFO(slimr::write_json(newline) << " vs " << slimr::write_json(newlines[i - 1]));
        CHECK(reviewWritesSameObject(newline, newlines[i - 1]));
    }
    if (paragraphs > 1) CHECK(reviewWritesRunStyles(reviewWritesParagraph(fixture, 2)) == italicBefore);
    CHECK(reviewRuns(fixture) == expect.runs);
    CHECK(fixture.file().suggestions.empty());

    // Review payload: the comment and reply survive and nothing is written to Drive.
    CHECK(fixture.commentWrites == 0);
    REQUIRE(fixture.comments.size() == 1);
    CHECK(fixture.comments[0].id == "synthetic_comment");
    CHECK(fixture.comments[0].content == std::optional<std::string>{"Synthetic note"});
    REQUIRE(fixture.comments[0].replies.size() == 1);
    CHECK(fixture.comments[0].replies[0].content == std::optional<std::string>{"Synthetic reply"});
}

inline void reviewWritesSeed(RemoteImportFixture& fixture, std::vector<model::BlockSpec> blocks, bool boldNewline) {
    auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic",
        .tabs = {{.tabId = "t.0", .title = "Main", .blocks = std::move(blocks)}}});
    if (boldNewline) {
        auto& elements = json["tabs"][0]["documentTab"]["body"]["content"][1]["paragraph"]["elements"];
        elements[elements.size() - 1]["textRun"]["textStyle"] = slimr::Json::object_t{{"bold", true}};
    }
    fixture.backend.seed_document("synthetic_doc", json, 1);
}

// Synthetic equivalent of the V008 suggestion-v1 shape.
TEST_CASE("import parity - review writes - rejecting a V008 insertion sends one delete and no restyle") {
    RemoteImportFixture fixture;
    reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {
        model::ParagraphTextSpec{.text = "Hello world", .sugIns = std::vector<std::string>{"s1"}}}})}, false);
    reviewWritesRejectCheck(fixture, {.deleteStart = 1, .deleteEnd = 12, .runs = {{.text = "\n"}}}, 1);
}

TEST_CASE("import parity - review writes - rejecting an insertion keeps nondefault surviving styles") {
    RemoteImportFixture fixture;
    const auto style = slimr::Json::object_t{
        {"namedStyleType", "NORMAL_TEXT"}, {"direction", "RIGHT_TO_LEFT"}, {"alignment", "CENTER"}};
    auto inserted = [](std::string text) {
        return model::ParagraphTextSpec{.text = std::move(text), .sugIns = std::vector<std::string>{"s1"}};
    };
    std::vector<model::ParagraphContentSpec> content;
    ReviewWritesExpect expect;
    SUBCASE("mid-paragraph") {
        content = {"Keep ", inserted("inserted "), "keep"};
        expect = {.deleteStart = 6, .deleteEnd = 15, .runs = {{.text = "Keep keep\n"}}};
    }
    SUBCASE("paragraph start") {
        content = {inserted("Inserted "), "keep"};
        expect = {.deleteStart = 1, .deleteEnd = 10, .runs = {{.text = "keep\n"}}};
    }
    reviewWritesSeed(fixture, {
        model::BlockSpec::make_paragraph({.content = std::move(content), .style = style}),
        model::BlockSpec::make_paragraph({.content = {"Other ",
            model::ParagraphTextSpec{.text = "italic", .style = slimr::Json::object_t{{"italic", true}}}}})}, true);
    reviewWritesRejectCheck(fixture, expect, 2);
}

// Test-only localization: mirrors TabHandle::writeSegment stage order and reports the first stage whose
// surviving paragraph differs from "original minus rejected symbols" in style or newline style.
TEST_CASE("import parity - review writes - localization records first diverging layer") {
    RemoteImportFixture fixture;
    bool v008 = true;
    SUBCASE("V008 whole-paragraph insertion") {}
    SUBCASE("styled mid-paragraph insertion") { v008 = false; }
    if (v008) {
        reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {
            model::ParagraphTextSpec{.text = "Hello world", .sugIns = std::vector<std::string>{"s1"}}}})}, false);
    } else {
        reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {"Keep ",
            model::ParagraphTextSpec{.text = "inserted ", .sugIns = std::vector<std::string>{"s1"}}, "keep"},
            .style = slimr::Json::object_t{{"namedStyleType", "NORMAL_TEXT"}, {"direction", "RIGHT_TO_LEFT"},
                {"alignment", "CENTER"}}})}, true);
    }
    auto file = fixture.file();
    REQUIRE(file.suggestions.size() == 1);
    auto json = fixture.backend.get_doc("synthetic_doc").json;
    json["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc", .keys = keys});
    REQUIRE(original.tabs.size() == 1);
    const auto& base = original.tabs[0].blocks.front().as_paragraph();

    std::string firstDiverging;
    auto record = [&](const std::string& stage, const model::TabModel& tab) {
        REQUIRE_FALSE(tab.blocks.empty());
        const auto& paragraph = tab.blocks.front().as_paragraph();
        const bool same = paragraph.key == base.key && paragraph.style == base.style &&
            paragraph.newline.style == base.newline.style;
        MESSAGE(stage << ": key=" << paragraph.key << " style=" << slimr::write_json(paragraph.style)
            << " newline=" << slimr::write_json(paragraph.newline.style)
            << " stamp=" << std::string(paragraph.stamp ? (paragraph.stamp->force ? "force" : "plain") : "none")
            << std::string(same ? "" : " DIVERGES"));
        if (!same && firstDiverging.empty()) firstDiverging = stage;
    };

    model::EditContext context{.keys = keys, .stamp = {.force = false, .tabIndex = 0}, .tombstones = {}};
    auto candidate = original.tabs[0];
    lens::AtomPutReport recreations;
    const auto hadInlines = detail::paragraphKeysWithInlines(candidate.blocks);
    const auto rejected = suggestionRejectsApply(model::EditTarget{context, candidate}, {"s1"}, original, recreations);
    auto emptied = detail::rejectionEmptiedKeys(hadInlines, candidate.blocks);
    CHECK(rejected == std::vector<std::string>{"s1"});
    record("1 suggestionRejectsApply", candidate);

    auto parsed = lens::markdownParse(file.tabs[0].markdown);
    std::erase_if(parsed.reviewMarkers, [&](const auto& marker) {
        return marker.name == "suggestion" && marker.id == "s1";
    });
    record("2 markdownParse (no model change)", candidate);

    lens::markdownPut(model::EditTarget{context, candidate}, parsed, {},
        {.carryReview = suggestionRuleApply, .rejectionEmptied = std::move(emptied)});
    record("3 markdownPut carryReview", candidate);

    auto final = original;
    final.tabs[0] = candidate;
    const auto plan = reconcile::docReconcile({.final = final, .original = original, .originalJson = json, .carryRanges = {}});
    bool restyles = false;
    for (const auto& request : plan.contentRequests) {
        MESSAGE("4 reconcile request: " << slimr::write_json(request));
        if (request.contains("updateParagraphStyle") || request.contains("updateTextStyle")) restyles = true;
    }
    if (restyles && firstDiverging.empty()) firstDiverging = "4 reconcile";
    MESSAGE("first diverging layer: " << (firstDiverging.empty() ? "none" : firstDiverging));
    CHECK(firstDiverging.empty());
}

enum class ReviewPutMode { Strict, CarryEmptySet, StrictWithSet };

// Runs reject -> markdownPut -> docReconcile on the seeded doc and serializes keys, styles, tombstones and requests.
inline std::string reviewWritesPutTrace(RemoteImportFixture& fixture, const std::set<std::string>& reject,
                                        std::optional<std::string> markdown, ReviewPutMode mode) {
    auto json = fixture.backend.get_doc("synthetic_doc").json;
    json["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
    if (!markdown) markdown = fixture.file().tabs[0].markdown;
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc", .keys = keys});
    model::EditContext context{.keys = keys, .stamp = {.force = false, .tabIndex = 0}, .tombstones = {}};
    auto candidate = original.tabs[0];
    lens::AtomPutReport recreations;
    if (!reject.empty()) suggestionRejectsApply(model::EditTarget{context, candidate}, reject, original, recreations);
    auto parsed = lens::markdownParse(*markdown);
    std::erase_if(parsed.reviewMarkers, [&](const auto& marker) {
        return marker.name == "suggestion" && reject.contains(marker.id);
    });
    std::string out;
    try {
        if (mode == ReviewPutMode::CarryEmptySet) {
            lens::markdownPut(model::EditTarget{context, candidate}, parsed, {}, {.carryReview = suggestionRuleApply});
        } else if (mode == ReviewPutMode::StrictWithSet) {
            std::set<std::string> all;
            for (const auto& block : candidate.blocks) all.insert(block.key);
            lens::markdownPut(model::EditTarget{context, candidate}, parsed, {}, {.rejectionEmptied = all});
        } else {
            lens::markdownPut(model::EditTarget{context, candidate}, parsed, {}, {});
        }
    } catch (const model::CoreError& error) {
        return "refused:" + error.code;
    }
    for (const auto& block : candidate.blocks) {
        out += "key=" + block.key;
        if (block.is_paragraph()) {
            out += " style=" + slimr::write_json(slimr::Json(block.as_paragraph().style)) +
                " newline=" + slimr::write_json(slimr::Json(block.as_paragraph().newline.style));
        }
        out += "\n";
    }
    for (const auto& tombstone : context.tombstones) out += "tombstone=" + tombstone.key + "\n";
    auto final = original;
    final.tabs[0] = candidate;
    const auto plan = reconcile::docReconcile({.final = final, .original = original, .originalJson = json, .carryRanges = {}});
    for (const auto& request : plan.contentRequests) out += slimr::write_json(request) + "\n";
    return out;
}

// C-P1-f: strict output is pinned regardless of rejectionEmptied; carry with an empty set matches too.
TEST_CASE("import parity - review writes - strict put baseline is unchanged") {
    RemoteImportFixture fixture;
    std::set<std::string> reject{"s1"};
    std::optional<std::string> markdown;
    std::string expected, expectedCarry;
    const std::string restyled = "key=n1 style={} newline={}\ntombstone=o1-t-0\n"
        "{\"deleteContentRange\":{\"range\":{\"startIndex\":1,\"endIndex\":12,\"tabId\":\"t.0\"}}}\n"
        "{\"updateParagraphStyle\":{\"range\":{\"startIndex\":1,\"endIndex\":2,\"tabId\":\"t.0\"},\"paragraphStyle\":"
        "{\"namedStyleType\":\"NORMAL_TEXT\"},\"fields\":\"alignment,avoidWidowAndOrphan,borderBetween,borderBottom,"
        "borderLeft,borderRight,borderTop,direction,indentEnd,indentFirstLine,indentStart,keepLinesTogether,keepWithNext,"
        "lineSpacing,namedStyleType,pageBreakBefore,shading,spaceAbove,spaceBelow,spacingMode\"}}\n"
        "{\"updateTextStyle\":{\"range\":{\"startIndex\":1,\"endIndex\":2,\"tabId\":\"t.0\"},\"textStyle\":{},\"fields\":"
        "\"backgroundColor,baselineOffset,bold,fontSize,foregroundColor,italic,link,smallCaps,strikethrough,underline,"
        "weightedFontFamily\"}}\n";
    SUBCASE("V008") {
        reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {
            model::ParagraphTextSpec{.text = "Hello world", .sugIns = std::vector<std::string>{"s1"}}}})}, false);
        expected = "key=o1-t-0 style={} newline={}\n"
            "{\"deleteContentRange\":{\"range\":{\"startIndex\":1,\"endIndex\":12,\"tabId\":\"t.0\"}}}\n";
        expectedCarry = expected;
    }
    SUBCASE("styled mid-paragraph insertion") {
        reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {"Keep ",
            model::ParagraphTextSpec{.text = "inserted ", .sugIns = std::vector<std::string>{"s1"}}, "keep"},
            .style = slimr::Json::object_t{{"namedStyleType", "NORMAL_TEXT"}, {"direction", "RIGHT_TO_LEFT"},
                {"alignment", "CENTER"}}})}, true);
        expected = "key=o1-t-0 style={\"alignment\":\"CENTER\",\"direction\":\"RIGHT_TO_LEFT\",\"namedStyleType\":\"NORMAL_TEXT\"}"
            " newline={\"bold\":true}\n{\"deleteContentRange\":{\"range\":{\"startIndex\":6,\"endIndex\":15,\"tabId\":\"t.0\"}}}\n";
        expectedCarry = expected;
    }
    SUBCASE("deleting all tab content") {
        reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {"Hello world"}})}, false);
        reject.clear();
        markdown = "";
        expected = restyled;
        expectedCarry = restyled;
    }
    const auto strict = reviewWritesPutTrace(fixture, reject, markdown, ReviewPutMode::Strict);
    const auto carry = reviewWritesPutTrace(fixture, reject, markdown, ReviewPutMode::CarryEmptySet);
    CHECK(strict == expected);
    CHECK(reviewWritesPutTrace(fixture, reject, markdown, ReviewPutMode::StrictWithSet) == expected);
    CHECK(carry == expectedCarry);
}

// C-P1-d: the rejection-emptied sole paragraph keeps its key and reconcile sends only the delete.
TEST_CASE("import parity - review writes - rejection emptied sole paragraph keeps its key") {
    RemoteImportFixture fixture;
    reviewWritesSeed(fixture, {model::BlockSpec::make_paragraph({.content = {
        model::ParagraphTextSpec{.text = "Hello world", .sugIns = std::vector<std::string>{"s1"}}}})}, false);
    const auto markdown = fixture.file().tabs[0].markdown;
    auto json = fixture.backend.get_doc("synthetic_doc").json;
    json["revisionId"] = fixture.backend.revision_id(fixture.backend.get_doc("synthetic_doc"));
    model::KeyAllocator keys;
    const auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc", .keys = keys});
    const auto baseKey = original.tabs[0].blocks.front().key;
    model::EditContext context{.keys = keys, .stamp = {.force = false, .tabIndex = 0}, .tombstones = {}};
    auto candidate = original.tabs[0];
    lens::AtomPutReport recreations;
    const auto hadInlines = detail::paragraphKeysWithInlines(candidate.blocks);
    suggestionRejectsApply(model::EditTarget{context, candidate}, {"s1"}, original, recreations);
    auto emptied = detail::rejectionEmptiedKeys(hadInlines, candidate.blocks);
    CHECK(emptied == std::set<std::string>{baseKey});
    auto parsed = lens::markdownParse(markdown);
    std::erase_if(parsed.reviewMarkers, [](const auto& marker) { return marker.name == "suggestion" && marker.id == "s1"; });
    lens::markdownPut(model::EditTarget{context, candidate}, parsed, {},
        {.carryReview = suggestionRuleApply, .rejectionEmptied = std::move(emptied)});
    std::vector<std::string> blockKeys;
    for (const auto& block : candidate.blocks) blockKeys.push_back(block.key);
    CHECK(blockKeys == std::vector<std::string>{baseKey});
    for (const auto& tombstone : context.tombstones) CHECK(tombstone.key != baseKey);
    auto final = original;
    final.tabs[0] = candidate;
    const auto plan = reconcile::docReconcile({.final = final, .original = original, .originalJson = json, .carryRanges = {}});
    REQUIRE(plan.contentRequests.size() == 1);
    INFO(slimr::write_json(plan.contentRequests[0]));
    REQUIRE(plan.contentRequests[0].contains("deleteContentRange"));
    CHECK(plan.contentRequests[0]["deleteContentRange"]["range"]["startIndex"].get<int64_t>() == 1);
    CHECK(plan.contentRequests[0]["deleteContentRange"]["range"]["endIndex"].get<int64_t>() == 12);
}

// C-P1-g: only paragraphs that rejection took from some inlines to none enter the set.
TEST_CASE("import parity - review writes - rejection emptied keys come from the session gate") {
    auto inserted = [](std::string text) {
        return model::ParagraphTextSpec{.text = std::move(text), .sugIns = std::vector<std::string>{"s1"}};
    };
    const auto json = model::docJsonBuild({.documentId = "synthetic_doc", .title = "Synthetic", .tabs = {{.tabId = "t.0",
        .title = "Main", .blocks = {
            model::BlockSpec::make_paragraph({.content = {inserted("Gone")}}),
            model::BlockSpec::make_paragraph({.content = {}, .style = slimr::Json::object_t{{"alignment", "CENTER"}}}),
            model::BlockSpec::make_paragraph({.content = {"Keep ", inserted("this")}}),
            model::BlockSpec::make_paragraph({.content = {inserted("Whole")}, .newlineSugIns = std::vector<std::string>{"s1"}}),
            model::BlockSpec::make_paragraph({.content = {"Untouched"}})}}}});
    model::KeyAllocator keys;
    const auto document = model::docModelParse(json, model::DocModelParseOptions{.docId = "synthetic_doc", .keys = keys});
    std::vector<std::string> originalKeys;
    for (const auto& block : document.tabs[0].blocks) originalKeys.push_back(block.key);
    REQUIRE(originalKeys.size() == 5);
    model::EditContext context{.keys = keys, .stamp = {.force = false, .tabIndex = 0}, .tombstones = {}};
    auto candidate = document.tabs[0];
    lens::AtomPutReport recreations;
    const auto hadInlines = detail::paragraphKeysWithInlines(candidate.blocks);
    CHECK(suggestionRejectsApply(model::EditTarget{context, candidate}, {"s1"}, document, recreations) ==
          std::vector<std::string>{"s1"});
    CHECK(candidate.blocks.size() == 4);
    CHECK(detail::rejectionEmptiedKeys(hadInlines, candidate.blocks) == std::set<std::string>{originalKeys[0]});
}

} // namespace gdocsmith::engine::test
