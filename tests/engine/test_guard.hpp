#pragma once

#include "engine/guard.hpp"
#include "model/edit.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <vector>

namespace gdocsmith::engine::test {

inline std::vector<std::string> findingKinds(const std::vector<GuardFinding>& findings) {
    std::vector<std::string> res;
    for (const auto& f : findings) {
        std::string s = f.kind + ":" + f.severity;
        if (f.waived) s += ":waived";
        res.push_back(std::move(s));
    }
    return res;
}

TEST_CASE("guard - deleting commented text or editing part of it blocks") {
    using namespace gdocsmith::engine;
    using namespace gdocsmith::model;

    KeyAllocator keys;
    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"keep"}}),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"gone text"}}),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"z"}}),
                },
            },
        },
    });

    auto original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    auto finalDoc = original;

    std::vector<Tombstone> tombstones;
    EditContext ctx{.keys = keys, .stamp = EditStamp{.force = false, .tabIndex = 2}, .tombstones = tombstones};
    EditTarget target{.ctx = ctx, .tab = finalDoc.tabs[0]};

    // Delete "gone text"
    blocksDelete(target, {finalDoc.tabs[0].blocks[1].key});

    // Mock plan with deletedRange for the second paragraph: [6, 16]
    // (section break is [0,1); "keep" is [1,6); "gone text" is [6,16);
    //  the comment quote "gone" anchors to doc indices [6,10)).
    DocPlan plan;
    plan.docId = "d";
    plan.deletedRanges.push_back(RangeWithTab{
        .range = Range{.end = 16, .start = 6},
        .tabId = "t.0",
    });
    plan.contentRequests.push_back(slimr::Json::object_t{
        {"deleteContentRange", slimr::Json::object_t{
            {"range", slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(6)},
                {"endIndex", static_cast<int64_t>(16)},
                {"tabId", "t.0"}
            }}
        }}
    });
    plan.origins.push_back(RequestOrigin{.tabIndex = 2});

    std::vector<DriveComment> comments = {
        DriveComment{.id = "c0", .quotedFileContent = DriveCommentQuoted{.value = "gone"}},
    };
    auto anchors = commentAnchorsMatch(original, comments);

    GuardDoc gdoc{
        .comments = anchors,
        .final = finalDoc,
        .original = original,
        .plan = plan,
        .tombstones = ctx.tombstones,
    };

    auto findings = guardEvaluate(GuardInput{
        .docs = {gdoc},
        .allow = {},
    });

    auto kinds = findingKinds(findings);
    REQUIRE(kinds.size() == 1);
    CHECK_EQ(kinds[0], "comment:block");
}

TEST_CASE("guard - an allow kind waives only its own findings") {
    using namespace gdocsmith::engine;
    using namespace gdocsmith::model;

    KeyAllocator keys;
    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"a"}}),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"gone"}}),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"z"}}),
                },
            },
        },
    });

    auto original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    auto finalDoc = original;

    std::vector<Tombstone> tombstones;
    EditContext ctx{.keys = keys, .stamp = EditStamp{.force = false, .tabIndex = 2}, .tombstones = tombstones};
    EditTarget target{.ctx = ctx, .tab = finalDoc.tabs[0]};

    blocksDelete(target, {finalDoc.tabs[0].blocks[1].key});

    DocPlan plan;
    plan.docId = "d";
    plan.deletedRanges.push_back(RangeWithTab{
        .range = Range{.end = 9, .start = 3},
        .tabId = "t.0",
    });
    plan.contentRequests.push_back(slimr::Json::object_t{
        {"deleteContentRange", slimr::Json::object_t{
            {"range", slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(3)},
                {"endIndex", static_cast<int64_t>(9)},
                {"tabId", "t.0"}
            }}
        }}
    });
    plan.origins.push_back(RequestOrigin{.tabIndex = 2});

    std::vector<DriveComment> comments = {
        DriveComment{.id = "c0", .quotedFileContent = DriveCommentQuoted{.value = "gone"}},
    };
    auto anchors = commentAnchorsMatch(original, comments);

    GuardDoc gdoc{
        .comments = anchors,
        .final = finalDoc,
        .original = original,
        .plan = plan,
        .tombstones = ctx.tombstones,
    };

    // Waived with commentChanges
    auto waived = guardEvaluate(GuardInput{
        .docs = {gdoc},
        .allow = {"commentChanges"},
    });
    auto kindsWaived = findingKinds(waived);
    REQUIRE(kindsWaived.size() == 1);
    CHECK_EQ(kindsWaived[0], "comment:block:waived");

    // Not waived with brokenLink
    auto notWaived = guardEvaluate(GuardInput{
        .docs = {gdoc},
        .allow = {"brokenLink"},
    });
    auto kindsNotWaived = findingKinds(notWaived);
    REQUIRE(kindsNotWaived.size() == 1);
    CHECK_EQ(kindsNotWaived[0], "comment:block");
}

TEST_CASE("guard - inbound-linked headings: deleting or un-heading blocks") {
    using namespace gdocsmith::engine;
    using namespace gdocsmith::model;

    KeyAllocator keys;
    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{
                        .content = {"Target"},
                        .headingId = "h.t",
                        .style = slimr::Json::object_t{{"namedStyleType", "HEADING_1"}},
                    }),
                    BlockSpec::make_paragraph(ParagraphSpec{
                        .content = {ParagraphTextSpec{
                            .text = "see",
                            .style = slimr::Json::object_t{{"link", slimr::Json::object_t{
                                {"heading", slimr::Json::object_t{{"id", "h.t"}, {"tabId", "t.0"}}}
                            }}},
                        }},
                    }),
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"z"}}),
                },
            },
        },
    });

    auto original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    auto finalDoc = original;

    // Delete the heading
    std::vector<Tombstone> tombstones;
    EditContext ctx{.keys = keys, .stamp = EditStamp{.force = false, .tabIndex = 0}, .tombstones = tombstones};
    EditTarget target{.ctx = ctx, .tab = finalDoc.tabs[0]};
    blocksDelete(target, {finalDoc.tabs[0].blocks[0].key});

    DocPlan plan;
    plan.docId = "d";

    GuardDoc gdoc{
        .comments = {},
        .final = finalDoc,
        .original = original,
        .plan = plan,
        .tombstones = ctx.tombstones,
    };

    auto findings = guardEvaluate(GuardInput{
        .docs = {gdoc},
        .allow = {},
    });

    auto kinds = findingKinds(findings);
    REQUIRE(kinds.size() == 1);
    CHECK_EQ(kinds[0], "headingLink:block");
}

TEST_CASE("guard - document not loaded in SUGGESTIONS_INLINE mode blocks edits") {
    using namespace gdocsmith::engine;
    using namespace gdocsmith::model;

    KeyAllocator keys;
    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"a"}}),
                },
            },
        },
    });

    auto original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
    original.suggestionsViewMode = "PREVIEW_WITHOUT_SUGGESTIONS";
    auto finalDoc = original;

    DocPlan plan;
    plan.docId = "d";
    plan.contentRequests.push_back(slimr::Json::object_t{{"insertText", slimr::Json::object_t{}}});

    GuardDoc gdoc{
        .comments = {},
        .final = finalDoc,
        .original = original,
        .plan = plan,
        .tombstones = {},
    };

    auto findings = guardEvaluate(GuardInput{
        .docs = {gdoc},
        .allow = allowKinds(),
    });

    auto kinds = findingKinds(findings);
    REQUIRE(kinds.size() == 1);
    CHECK_EQ(kinds[0], "viewMode:block");
}

} // namespace gdocsmith::engine::test
