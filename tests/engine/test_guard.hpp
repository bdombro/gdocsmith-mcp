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

TEST_CASE("guard - image recreation requires loss approval but deletion does not") {
    model::DocModel original;
    original.docId = "d";
    model::TabModel tab;
    tab.tabId = "t.0";
    model::ParagraphBlock paragraph;
    paragraph.key = "p";
    model::Atom image;
    image.key = "image";
    image.type = "image";
    image.raw = slimr::Json::object_t{
        {"inlineObjectElement", slimr::Json::object_t{{"inlineObjectId", "google_image"}}}
    };
    paragraph.inlines.push_back(model::Inline::from_atom(image));
    tab.blocks.emplace_back(paragraph);
    tab.inlineObjects["google_image"] = slimr::Json::object_t{
        {"inlineObjectProperties", slimr::Json::object_t{
            {"embeddedObject", slimr::Json::object_t{
                {"description", "important alt text"},
                {"imageProperties", slimr::Json::object_t{{"contentUri", "https://example.invalid/image"}}}
            }}
        }}
    };
    original.tabs.push_back(tab);
    GuardDoc document;
    document.original = original;
    document.final = original;
    document.plan.deletedRanges.push_back({.range = {.end = 2, .start = 1}, .tabId = "t.0"});

    auto findings = guardEvaluate({.docs = {document}});
    REQUIRE(findings.size() == 1u);
    CHECK(findings[0].allowKind == std::optional<std::string>("lossyImageMove"));
    CHECK(!findings[0].waived);
    findings = guardEvaluate({.docs = {document}, .allow = {"lossyImageMove"}});
    REQUIRE(findings.size() == 1u);
    CHECK(findings[0].waived);

    auto safe = document;
    safe.original.tabs[0].inlineObjects["google_image"]["inlineObjectProperties"]["embeddedObject"] =
        slimr::Json::object_t{{"imageProperties", slimr::Json::object_t{{"brightness", 0.0}}}};
    CHECK(guardEvaluate({.docs = {safe}}).empty());

    auto moved = document;
    moved.plan.deletedRanges.clear();
    moved.final.tabs[0].tabId = "t.1";
    CHECK(guardEvaluate({.docs = {moved}}).size() == 1u);

    document.final.tabs.clear();
    CHECK(guardEvaluate({.docs = {document}}).empty());
    document.plan.contentRequests.push_back(slimr::Json::object_t{{"insertInlineImage", slimr::Json::object_t{}}});
    findings = guardEvaluate({.docs = {document}, .allow = {"lossyImageMove"}});
    REQUIRE(findings.size() == 1u);
    CHECK(!findings[0].waived);
}

TEST_CASE("guard - named ranges block overlaps, boundary insertions and explicit removal") {
    model::DocModel original;
    original.docId = "d";
    model::TabModel tab;
    tab.tabId = "t.0";
    model::NamedRangeDef range;
    range.name = "scope";
    range.namedRangeId = "nr1";
    model::NamedRangeDef::RangePart part;
    part.start = 5;
    part.end = 10;
    range.ranges.push_back(part);
    tab.namedRanges.push_back(range);
    original.tabs.push_back(tab);
    GuardDoc document;
    document.original = original;
    document.final = original;
    document.plan.deletedRanges.push_back({.range = {.end = 3, .start = 1}, .tabId = "t.0"});
    CHECK(guardEvaluate({.docs = {document}}).empty());

    document.plan.deletedRanges[0].range = {.end = 7, .start = 6};
    auto findings = guardEvaluate({.docs = {document}});
    REQUIRE(findings.size() == 1u);
    CHECK(findings[0].allowKind == std::optional<std::string>("namedRangeDamage"));
    findings = guardEvaluate({.docs = {document}, .allow = {"namedRangeDamage"}});
    REQUIRE(findings.size() == 1u);
    CHECK(findings[0].waived);

    document.plan.deletedRanges.clear();
    document.plan.contentRequests.push_back(slimr::Json::object_t{
        {"insertText", slimr::Json::object_t{
            {"location", slimr::Json::object_t{{"index", int64_t{5}}, {"tabId", "t.0"}}}, {"text", "new"}
        }}
    });
    CHECK(guardEvaluate({.docs = {document}}).size() == 1u);
    document.plan.contentRequests.clear();
    document.final.tabs[0].namedRanges.clear();
    CHECK(guardEvaluate({.docs = {document}}).size() == 1u);
    document.original.tabs[0].namedRanges[0].ranges[0].segmentId = "header";
    document.final = document.original;
    document.plan.contentRequests.push_back(slimr::Json::object_t{{"updateTextStyle", slimr::Json::object_t{}}});
    findings = guardEvaluate({.docs = {document}, .allow = {"namedRangeDamage"}});
    REQUIRE(findings.size() == 1u);
    CHECK(!findings[0].waived);
}

TEST_CASE("guard - exact marker edges permit partial interior edits but never full destruction") {
    GuardDoc document;
    document.original.docId = "d";
    model::TabModel tab;
    tab.tabId = "t";
    model::ParagraphBlock p;
    p.key = "p";
    p.inlines.push_back(model::Inline::from_text("abcdefghijklmno"));
    tab.blocks.emplace_back(p);
    model::NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "nr";
    nr.ranges.emplace_back();
    nr.ranges[0].start = 5;
    nr.ranges[0].end = 10;
    nr.expect = model::NamedRangeDef::EdgePair{.end={"p", 8}, .start={"p", 4}};
    tab.namedRanges.push_back(nr);
    document.original.tabs.push_back(tab);
    document.original.tabs[0].namedRanges[0].expect.reset();
    document.final = document.original;
    document.final.tabs[0].namedRanges[0].expect = nr.expect;
    document.plan.deletedRanges.push_back({.range={.end=7, .start=6}, .tabId="t"});
    document.plan.contentRequests.push_back(slimr::Json::object_t{
        {"deleteContentRange", slimr::Json::object_t{{"range", slimr::Json::object_t{
            {"startIndex", int64_t{6}}, {"endIndex", int64_t{7}}, {"tabId", "t"}}}}}});
    CHECK(guardEvaluate({.docs={document}}).empty());
    document.final.tabs[0].namedRanges[0].expect->end.offset = 9;
    CHECK(guardEvaluate({.docs={document}}).size() == 1u); // wrong prediction isn't a bypass
    document.final.tabs[0].namedRanges[0].expect->end.offset = 8;
    document.plan.deletedRanges[0].range = {.end=10, .start=5};
    document.plan.contentRequests[0]["deleteContentRange"]["range"]["startIndex"] = int64_t{5};
    document.plan.contentRequests[0]["deleteContentRange"]["range"]["endIndex"] = int64_t{10};
    CHECK(guardEvaluate({.docs={document}}).size() == 1u);
}

TEST_CASE("guard - tracked start and end boundary insertions keep Google outside-edge semantics") {
    GuardDoc document;
    model::TabModel tab;
    tab.tabId = "t";
    model::ParagraphBlock p;
    p.key = "p";
    p.inlines.push_back(model::Inline::from_text("abcdefghijklmno"));
    tab.blocks.emplace_back(p);
    model::NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "nr";
    nr.ranges.emplace_back();
    nr.ranges[0].start = 5;
    nr.ranges[0].end = 10;
    tab.namedRanges.push_back(nr);
    document.original.tabs.push_back(tab);
    document.final = document.original;
    document.final.tabs[0].namedRanges[0].expect =
        model::NamedRangeDef::EdgePair{.end={"p", 11}, .start={"p", 6}};
    document.plan.contentRequests.push_back(slimr::Json::object_t{
        {"insertText", slimr::Json::object_t{{"location", slimr::Json::object_t{
            {"index", int64_t{5}}, {"tabId", "t"}}}, {"text", "xy"}}}});
    CHECK(guardEvaluate({.docs={document}}).empty());
    document.final.tabs[0].namedRanges[0].expect->start.offset = 4;
    CHECK(guardEvaluate({.docs={document}}).size() == 1u); // typed inside means recreate
    document.plan.contentRequests[0]["insertText"]["location"]["index"] = int64_t{10};
    document.final.tabs[0].namedRanges[0].expect = model::NamedRangeDef::EdgePair{
        .end={"p", 9}, .start={"p", 4}};
    CHECK(guardEvaluate({.docs={document}}).empty());
}

TEST_CASE("guard - only explicitly managed absent ranges may be removed without damage approval") {
    GuardDoc document;
    model::TabModel tab;
    tab.tabId = "t";
    model::NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "nr";
    nr.ranges.emplace_back();
    nr.ranges[0].start = 5;
    nr.ranges[0].end = 10;
    tab.namedRanges.push_back(nr);
    document.original.tabs.push_back(tab);
    document.final = document.original;
    document.final.tabs[0].namedRanges.clear();
    document.plan.deletedRanges.push_back({.range={.end=10, .start=5}, .tabId="t"});
    REQUIRE(guardEvaluate({.docs={document}}).size() == 1u);
    document.managedNamedRangeIds.insert("different");
    CHECK(guardEvaluate({.docs={document}}).size() == 1u);
    document.managedNamedRangeIds.insert("nr");
    CHECK(guardEvaluate({.docs={document}}).empty());
    // Sidecar is not a blanket damage waiver for retained identities.
    document.final = document.original;
    CHECK(guardEvaluate({.docs={document}}).size() == 1u);
}

TEST_CASE("guard - structural cell deletion cannot silently destroy a retained named range") {
    GuardDoc document;
    model::TabModel tab;
    tab.tabId = "t";
    model::ParagraphBlock p;
    p.key = "cell-paragraph";
    p.inlines.push_back(model::Inline::from_text("retained text"));
    model::CellModel cell;
    cell.key = "cell";
    cell.blocks.emplace_back(p);
    model::RowModel row;
    row.key = "row";
    row.cells.push_back(cell);
    model::TableBlock table;
    table.key = "table";
    table.rows.push_back(row);
    tab.blocks.emplace_back(table);
    const auto layout = model::layoutCompute(tab);
    const auto body = layout.blockRanges.at("cell-paragraph");
    model::NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "retained";
    nr.ranges.emplace_back();
    nr.ranges[0].start = body.start;
    nr.ranges[0].end = body.end - 1;
    tab.namedRanges.push_back(nr); // no expect: guard must not rely on marker tracking
    document.original.tabs.push_back(tab);
    document.final = document.original;
    document.final.tabs[0].blocks[0].as_table().rows.clear();
    document.plan.contentRequests.push_back(reconcile::RequestBuilder::deleteTableRow({
        .tabId = "t", .tableStart = layout.blockRanges.at("table").start}));
    REQUIRE(document.plan.deletedRanges.empty());
    auto findings = guardEvaluate({.docs={document}, .allow={"namedRangeDamage"}});
    REQUIRE(findings.size() == 1u);
    CHECK(findings[0].kind == "namedRange");
    CHECK_FALSE(findings[0].allowKind.has_value());
    CHECK_FALSE(findings[0].waived);

    document.plan.contentRequests[0] = reconcile::RequestBuilder::deleteTableColumn({
        .tabId = "t", .tableStart = layout.blockRanges.at("table").start});
    findings = guardEvaluate({.docs={document}, .allow={"namedRangeDamage"}});
    REQUIRE(findings.size() == 1u);
    CHECK_FALSE(findings[0].waived);
    // Managed-ID provenance only authorizes absence, not retained structural damage.
    document.managedNamedRangeIds.insert("retained");
    CHECK(guardEvaluate({.docs={document}}).size() == 1u);

    // An operation on another tab cannot remove this range.
    document.final = document.original;
    document.plan.contentRequests[0] = reconcile::RequestBuilder::deleteTableRow({
        .tabId = "other", .tableStart = layout.blockRanges.at("table").start});
    CHECK(guardEvaluate({.docs={document}}).empty());
    // Formatting on this tab preserves indices and remains safe.
    document.plan.contentRequests[0] = slimr::Json::object_t{
        {"updateTextStyle", slimr::Json::object_t{}}};
    CHECK(guardEvaluate({.docs={document}}).empty());
    // Unclassified effects do not become safe just because overlap is unknown.
    document.plan.contentRequests[0] = slimr::Json::object_t{
        {"unclassifiedStructuralOperation", slimr::Json::object_t{{"tabId", "other"}}}};
    findings = guardEvaluate({.docs={document}, .allow={"namedRangeDamage"}});
    REQUIRE(findings.size() == 1u);
    CHECK_FALSE(findings[0].waived);
}

TEST_CASE("guard - same-tab later tables are safe in sequential range coordinates") {
    GuardDoc document;
    model::TabModel tab;
    tab.tabId = "t";
    model::ParagraphBlock p;
    p.key = "p";
    p.inlines.push_back(model::Inline::from_text("range"));
    tab.blocks.emplace_back(p);
    model::ParagraphBlock nested;
    nested.key = "cell-paragraph";
    nested.inlines.push_back(model::Inline::from_text("later table"));
    model::CellModel cell;
    cell.blocks.emplace_back(nested);
    model::RowModel row;
    row.cells.push_back(cell);
    model::TableBlock table;
    table.key = "table";
    table.rows.push_back(row);
    tab.blocks.emplace_back(table);
    model::NamedRangeDef nr;
    nr.name = "scope";
    nr.namedRangeId = "retained";
    nr.ranges.emplace_back();
    nr.ranges[0].start = 1;
    nr.ranges[0].end = 6;
    tab.namedRanges.push_back(nr);
    const int tableStart = model::layoutCompute(tab).blockRanges.at("table").start;
    REQUIRE(tableStart == 7);
    document.original.tabs.push_back(tab);
    document.final = document.original;
    document.final.tabs[0].blocks[1].as_table().rows.clear();
    document.plan.contentRequests.push_back(reconcile::RequestBuilder::deleteTableRow({
        .tabId = "t", .tableStart = tableStart}));
    CHECK(guardEvaluate({.docs={document}}).empty());
    document.plan.contentRequests[0] = reconcile::RequestBuilder::deleteTableColumn({
        .tabId = "t", .tableStart = tableStart});
    CHECK(guardEvaluate({.docs={document}}).empty());

    // Prefix insertions move BOTH the retained range and the later table. The
    // spatial proof must use those current UTF-16 coordinates, not origin indices.
    document.final.tabs[0].blocks[0].as_paragraph().inlines[0].text = "\xF0\x9F\x98\x80range";
    document.final.tabs[0].namedRanges[0].expect =
        model::NamedRangeDef::EdgePair{.end={"p", 6}, .start={"p", 1}};
    document.plan.contentRequests.insert(document.plan.contentRequests.begin(),
        reconcile::RequestBuilder::insertTextAt(1, "\xF0\x9F\x98\x80", "t"));
    document.plan.contentRequests[1] = reconcile::RequestBuilder::deleteTableRow({
        .tabId = "t", .tableStart = tableStart + 2});
    CHECK(guardEvaluate({.docs={document}}).empty());
    document.plan.contentRequests[1] = reconcile::RequestBuilder::deleteTableColumn({
        .tabId = "t", .tableStart = tableStart + 2});
    CHECK(guardEvaluate({.docs={document}}).empty());

    // An origin index that was after the old range is now inside the current
    // range. It must not pass the safe-later-table proof.
    document.plan.contentRequests[1] = reconcile::RequestBuilder::deleteTableRow({
        .tabId = "t", .tableStart = tableStart});
    const auto findings = guardEvaluate({.docs={document}, .allow={"namedRangeDamage"}});
    CHECK(std::any_of(findings.begin(), findings.end(), [](const auto& finding) {
        return finding.kind == "namedRange" && !finding.allowKind && !finding.waived;
    }));
    // The safe proof is strict: touching/unresolved table boundaries still refuse.
    document.plan.contentRequests[1] = reconcile::RequestBuilder::deleteTableRow({
        .tabId = "t", .tableStart = 8});
    const auto boundaryFindings = guardEvaluate({.docs={document}});
    CHECK(std::any_of(boundaryFindings.begin(), boundaryFindings.end(),
        [](const auto& finding) { return !finding.allowKind; }));
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

TEST_CASE("guard - inbound-linked headings are preserved or explicitly protected") {
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
    CHECK(detail::blockDeep(original.tabs[0], original.tabs[0].blocks[0].key) ==
          &original.tabs[0].blocks[0]);
    CHECK(guardEvaluate({.docs = {{.final = original, .original = original}}}).empty());
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
