#pragma once

#include "engine.hpp"
#include "test.hpp"

using namespace gdocsmith;

// Helper to construct synthetic Google Docs JSON
inline slimr::Json make_test_doc_json(const std::string& title = "Test Document",
                                      const std::string& doc_id = "test-doc-123",
                                      const std::string& revision_id = "rev-1") {
    slimr::Json j = slimr::Json::object_t{};
    j["documentId"] = doc_id;
    j["title"] = title;
    j["revisionId"] = revision_id;

    slimr::Json::array_t content;

    // Index 0..1: Leading section break
    slimr::Json sb = slimr::Json::object_t{};
    sb["startIndex"] = static_cast<int64_t>(0);
    sb["endIndex"] = static_cast<int64_t>(1);
    sb["sectionBreak"] = slimr::Json::object_t{
        {"sectionStyle", slimr::Json::object_t{{"sectionType", "CONTINUOUS"}}}
    };
    content.push_back(std::move(sb));

    // Index 1..13: Heading 1 "Overview\n"
    slimr::Json h1 = slimr::Json::object_t{};
    h1["startIndex"] = static_cast<int64_t>(1);
    h1["endIndex"] = static_cast<int64_t>(10);
    slimr::Json::array_t h1_elems;
    slimr::Json h1_tr = slimr::Json::object_t{};
    h1_tr["textRun"] = slimr::Json::object_t{
        {"content", "Overview\n"},
        {"textStyle", slimr::Json::object_t{}}
    };
    h1_elems.push_back(std::move(h1_tr));
    h1["paragraph"] = slimr::Json::object_t{
        {"elements", std::move(h1_elems)},
        {"paragraphStyle", slimr::Json::object_t{{"namedStyleType", "HEADING_1"}}}
    };
    content.push_back(std::move(h1));

    // Index 10..22: "the cat sat\n"
    slimr::Json p1 = slimr::Json::object_t{};
    p1["startIndex"] = static_cast<int64_t>(10);
    p1["endIndex"] = static_cast<int64_t>(22);
    slimr::Json::array_t p1_elems;
    slimr::Json p1_tr = slimr::Json::object_t{};
    p1_tr["textRun"] = slimr::Json::object_t{
        {"content", "the cat sat\n"},
        {"textStyle", slimr::Json::object_t{}}
    };
    p1_elems.push_back(std::move(p1_tr));
    p1["paragraph"] = slimr::Json::object_t{
        {"elements", std::move(p1_elems)},
        {"paragraphStyle", slimr::Json::object_t{{"namedStyleType", "NORMAL_TEXT"}}}
    };
    content.push_back(std::move(p1));

    slimr::Json body = slimr::Json::object_t{};
    body["content"] = std::move(content);
    j["body"] = std::move(body);

    return j;
}

// ============================================================================
// 1. Myers Diff Unit Tests
// ============================================================================

TEST_SUITE("Myers Diff") {
    TEST_CASE("Identical strings produce no hunks") {
        auto hunks = diff_myers_string("hello world", "hello world");
        CHECK(hunks.empty());
    }

    TEST_CASE("Single word change") {
        auto hunks = diff_myers_string("the cat sat", "the dog sat");
        REQUIRE(hunks.size() == 1);
        CHECK(hunks[0].a_start == 4);
        CHECK(hunks[0].a_end == 7);
        CHECK(hunks[0].b_start == 4);
        CHECK(hunks[0].b_end == 7);
    }

    TEST_CASE("Pure insertion at end") {
        auto hunks = diff_myers_string("start", "start and end");
        REQUIRE(hunks.size() == 1);
        CHECK(hunks[0].a_start == 5);
        CHECK(hunks[0].a_end == 5);
        CHECK(hunks[0].b_start == 5);
        CHECK(hunks[0].b_end == 13);
    }

    TEST_CASE("Pure deletion") {
        auto hunks = diff_myers_string("full text here", "full here");
        REQUIRE(hunks.size() == 1);
        CHECK(hunks[0].a_start == 5);
        CHECK(hunks[0].a_end == 10);
        CHECK(hunks[0].b_start == 5);
        CHECK(hunks[0].b_end == 5);
    }
}

// ============================================================================
// 2. Google Docs JSON Parser Unit Tests
// ============================================================================

TEST_SUITE("Google Docs JSON Parser") {
    TEST_CASE("Parse single-tab legacy document") {
        slimr::Json doc_json = make_test_doc_json();
        DocModel doc = parse_doc_json(doc_json);

        CHECK(doc.doc_id == "test-doc-123");
        CHECK(doc.title == "Test Document");
        CHECK(doc.revision_id == "rev-1");
        REQUIRE(doc.tabs.size() == 1);
        CHECK(doc.tabs[0].tab_id == "t.0");
        REQUIRE(doc.tabs[0].blocks.size() == 2);

        // Block 0: Heading 1
        const auto& b0 = doc.tabs[0].blocks[0];
        CHECK(b0.kind == BlockKind::Paragraph);
        CHECK(b0.paragraph.heading_level == 1);
        CHECK(b0.paragraph.named_style_type == "HEADING_1");
        CHECK(b0.paragraph.plain_text() == "Overview");

        // Block 1: Paragraph
        const auto& b1 = doc.tabs[0].blocks[1];
        CHECK(b1.kind == BlockKind::Paragraph);
        CHECK(b1.paragraph.heading_level == 0);
        CHECK(b1.paragraph.plain_text() == "the cat sat");
    }

    TEST_CASE("Parse multi-tab document with tabProperties") {
        slimr::Json j = slimr::Json::object_t{};
        j["documentId"] = "multi-123";
        j["title"] = "Multi Tab Doc";
        j["revisionId"] = "rev-42";

        slimr::Json::array_t tabs_arr;

        // Tab 1
        slimr::Json t1 = slimr::Json::object_t{};
        t1["tabProperties"] = slimr::Json::object_t{
            {"tabId", "t.first"},
            {"title", "First Tab"},
            {"index", static_cast<int64_t>(0)}
        };
        slimr::Json dt1 = slimr::Json::object_t{};
        dt1["body"] = slimr::Json::object_t{
            {"content", slimr::Json::array_t{
                slimr::Json::object_t{
                    {"startIndex", static_cast<int64_t>(1)},
                    {"endIndex", static_cast<int64_t>(7)},
                    {"paragraph", slimr::Json::object_t{
                        {"elements", slimr::Json::array_t{
                            slimr::Json::object_t{
                                {"textRun", slimr::Json::object_t{{"content", "Hello\n"}}}
                            }
                        }}
                    }}
                }
            }}
        };
        t1["documentTab"] = std::move(dt1);
        tabs_arr.push_back(std::move(t1));

        // Tab 2
        slimr::Json t2 = slimr::Json::object_t{};
        t2["tabProperties"] = slimr::Json::object_t{
            {"tabId", "t.second"},
            {"title", "Second Tab"},
            {"index", static_cast<int64_t>(1)}
        };
        slimr::Json dt2 = slimr::Json::object_t{};
        dt2["body"] = slimr::Json::object_t{
            {"content", slimr::Json::array_t{
                slimr::Json::object_t{
                    {"startIndex", static_cast<int64_t>(1)},
                    {"endIndex", static_cast<int64_t>(7)},
                    {"paragraph", slimr::Json::object_t{
                        {"elements", slimr::Json::array_t{
                            slimr::Json::object_t{
                                {"textRun", slimr::Json::object_t{{"content", "World\n"}}}
                            }
                        }}
                    }}
                }
            }}
        };
        t2["documentTab"] = std::move(dt2);
        tabs_arr.push_back(std::move(t2));

        j["tabs"] = std::move(tabs_arr);

        DocModel doc = parse_doc_json(j);
        CHECK(doc.doc_id == "multi-123");
        REQUIRE(doc.tabs.size() == 2);
        CHECK(doc.tabs[0].tab_id == "t.first");
        CHECK(doc.tabs[0].title == "First Tab");
        CHECK(doc.tabs[0].blocks[0].paragraph.plain_text() == "Hello");
        CHECK(doc.tabs[1].tab_id == "t.second");
        CHECK(doc.tabs[1].title == "Second Tab");
        CHECK(doc.tabs[1].blocks[0].paragraph.plain_text() == "World");
    }

    TEST_CASE("Parse formatted text runs: bold, italic, code, link") {
        slimr::Json j = make_test_doc_json();
        auto& content = j["body"]["content"].get<slimr::Json::array_t>();

        slimr::Json p = slimr::Json::object_t{};
        p["startIndex"] = static_cast<int64_t>(22);
        p["endIndex"] = static_cast<int64_t>(50);

        slimr::Json::array_t elems;
        // bold run
        elems.push_back(slimr::Json::object_t{
            {"textRun", slimr::Json::object_t{
                {"content", "Bold "},
                {"textStyle", slimr::Json::object_t{{"bold", true}}}
            }}
        });
        // italic run
        elems.push_back(slimr::Json::object_t{
            {"textRun", slimr::Json::object_t{
                {"content", "Italic "},
                {"textStyle", slimr::Json::object_t{{"italic", true}}}
            }}
        });
        // code run (Consolas)
        elems.push_back(slimr::Json::object_t{
            {"textRun", slimr::Json::object_t{
                {"content", "Code "},
                {"textStyle", slimr::Json::object_t{
                    {"weightedFontFamily", slimr::Json::object_t{{"fontFamily", "Consolas"}}}
                }}
            }}
        });
        // link run
        elems.push_back(slimr::Json::object_t{
            {"textRun", slimr::Json::object_t{
                {"content", "Link\n"},
                {"textStyle", slimr::Json::object_t{
                    {"link", slimr::Json::object_t{{"url", "https://example.com"}}}
                }}
            }}
        });

        p["paragraph"] = slimr::Json::object_t{{"elements", std::move(elems)}};
        content.push_back(std::move(p));

        DocModel doc = parse_doc_json(j);
        REQUIRE(doc.tabs[0].blocks.size() == 3);
        const auto& inlines = doc.tabs[0].blocks[2].paragraph.inlines;
        REQUIRE(inlines.size() == 4);

        CHECK(inlines[0].text_run.text == "Bold ");
        CHECK(inlines[0].text_run.style.bold == true);
        CHECK(inlines[1].text_run.text == "Italic ");
        CHECK(inlines[1].text_run.style.italic == true);
        CHECK(inlines[2].text_run.text == "Code ");
        CHECK(inlines[2].text_run.style.code == true);
        CHECK(inlines[3].text_run.text == "Link");
        CHECK(inlines[3].text_run.style.link_url == "https://example.com");
    }

    TEST_CASE("Parse chips and atoms: person, date, image, pageBreak") {
        slimr::Json j = make_test_doc_json();
        auto& content = j["body"]["content"].get<slimr::Json::array_t>();

        slimr::Json p = slimr::Json::object_t{};
        p["startIndex"] = static_cast<int64_t>(22);
        p["endIndex"] = static_cast<int64_t>(40);

        slimr::Json::array_t elems;
        elems.push_back(slimr::Json::object_t{
            {"person", slimr::Json::object_t{
                {"personProperties", slimr::Json::object_t{{"email", "person@example.com"}}}
            }}
        });
        elems.push_back(slimr::Json::object_t{
            {"dateElement", slimr::Json::object_t{
                {"dateProperties", slimr::Json::object_t{{"timestamp", "2026-11-10T00:00:00Z"}}}
            }}
        });
        elems.push_back(slimr::Json::object_t{
            {"inlineObjectElement", slimr::Json::object_t{
                {"inlineObjectId", "img_obj_123"}
            }}
        });
        elems.push_back(slimr::Json::object_t{
            {"pageBreak", slimr::Json::object_t{}}
        });
        elems.push_back(slimr::Json::object_t{
            {"textRun", slimr::Json::object_t{{"content", "\n"}}}
        });

        p["paragraph"] = slimr::Json::object_t{{"elements", std::move(elems)}};
        content.push_back(std::move(p));

        DocModel doc = parse_doc_json(j);
        REQUIRE(doc.tabs[0].blocks.size() == 3);
        const auto& inlines = doc.tabs[0].blocks[2].paragraph.inlines;
        REQUIRE(inlines.size() == 4);

        CHECK(inlines[0].is_atom);
        CHECK(inlines[0].atom.type == AtomType::Person);
        CHECK(inlines[0].atom.email == "person@example.com");

        CHECK(inlines[1].is_atom);
        CHECK(inlines[1].atom.type == AtomType::Date);
        CHECK(inlines[1].atom.timestamp == "2026-11-10T00:00:00Z");

        CHECK(inlines[2].is_atom);
        CHECK(inlines[2].atom.type == AtomType::Image);
        CHECK(inlines[2].atom.uri == "img_obj_123");

        CHECK(inlines[3].is_atom);
        CHECK(inlines[3].atom.type == AtomType::PageBreak);
    }

    TEST_CASE("Parse tables") {
        slimr::Json j = make_test_doc_json();
        auto& content = j["body"]["content"].get<slimr::Json::array_t>();

        slimr::Json tbl = slimr::Json::object_t{};
        tbl["startIndex"] = static_cast<int64_t>(22);
        tbl["endIndex"] = static_cast<int64_t>(60);

        slimr::Json::array_t rows;
        slimr::Json r1 = slimr::Json::object_t{};
        r1["tableCells"] = slimr::Json::array_t{
            slimr::Json::object_t{
                {"content", slimr::Json::array_t{
                    slimr::Json::object_t{
                        {"paragraph", slimr::Json::object_t{
                            {"elements", slimr::Json::array_t{
                                slimr::Json::object_t{{"textRun", slimr::Json::object_t{{"content", "A1\n"}}}}
                            }}
                        }}
                    }
                }}
            },
            slimr::Json::object_t{
                {"content", slimr::Json::array_t{
                    slimr::Json::object_t{
                        {"paragraph", slimr::Json::object_t{
                            {"elements", slimr::Json::array_t{
                                slimr::Json::object_t{{"textRun", slimr::Json::object_t{{"content", "B1\n"}}}}
                            }}
                        }}
                    }
                }}
            }
        };
        rows.push_back(std::move(r1));

        tbl["table"] = slimr::Json::object_t{
            {"rows", static_cast<int64_t>(1)},
            {"columns", static_cast<int64_t>(2)},
            {"tableRows", std::move(rows)}
        };
        content.push_back(std::move(tbl));

        DocModel doc = parse_doc_json(j);
        REQUIRE(doc.tabs[0].blocks.size() == 3);
        CHECK(doc.tabs[0].blocks[2].kind == BlockKind::Table);
        CHECK(doc.tabs[0].blocks[2].table.rows == 1);
        CHECK(doc.tabs[0].blocks[2].table.columns == 2);
        CHECK(doc.tabs[0].blocks[2].table.cells[0][0].paragraphs[0].plain_text() == "A1");
        CHECK(doc.tabs[0].blocks[2].table.cells[0][1].paragraphs[0].plain_text() == "B1");
    }

    TEST_CASE("Parse comments and anchor quote matching") {
        slimr::Json j = make_test_doc_json();
        slimr::Json::array_t comments;
        slimr::Json c1 = slimr::Json::object_t{};
        c1["id"] = "c_101";
        c1["author"] = slimr::Json::object_t{{"displayName", "Bob"}};
        c1["content"] = "Please review this sentence";
        c1["quotedFileContent"] = slimr::Json::object_t{{"value", "cat sat"}};
        c1["resolved"] = false;
        c1["replies"] = slimr::Json::array_t{
            slimr::Json::object_t{
                {"id", "r_201"},
                {"author", slimr::Json::object_t{{"displayName", "Alice"}}},
                {"content", "Done"}
            }
        };
        comments.push_back(std::move(c1));
        j["comments"] = std::move(comments);

        DocModel doc = parse_doc_json(j);
        REQUIRE(doc.comments.size() == 1);
        CHECK(doc.comments[0].id == "c_101");
        CHECK(doc.comments[0].author == "Bob");
        CHECK(doc.comments[0].quoted_file_content == "cat sat");
        CHECK(doc.comments[0].anchor_start >= 0);
        CHECK(doc.comments[0].tab_id == "t.0");
        REQUIRE(doc.comments[0].replies.size() == 1);
        CHECK(doc.comments[0].replies[0].author == "Alice");
        CHECK(doc.comments[0].replies[0].content == "Done");
    }
}

// ============================================================================
// 3. Markdown & YAML Export Unit Tests
// ============================================================================

TEST_SUITE("Markdown & YAML Export") {
    TEST_CASE("Export doc to base.md and next.md with frontmatter") {
        slimr::Json doc_json = make_test_doc_json("My Project", "doc-xyz", "rev-7");
        DocModel doc = parse_doc_json(doc_json);

        ExportResult res = export_artifacts(doc);
        CHECK(res.base_md == res.next_md);
        CHECK(res.base_md.starts_with("---\n"));
        CHECK(res.base_md.find("gdocsmith:") != std::string::npos);
        CHECK(res.base_md.find("doc: doc-xyz") != std::string::npos);
        CHECK(res.base_md.find("revision: rev-7") != std::string::npos);
        CHECK(res.base_md.find("title: My Project") != std::string::npos);
        CHECK(res.base_md.find("# Overview") != std::string::npos);
        CHECK(res.base_md.find("the cat sat") != std::string::npos);
    }

    TEST_CASE("Export comments.yaml and suggestions.yaml") {
        slimr::Json doc_json = make_test_doc_json();
        // Add comment
        doc_json["comments"] = slimr::Json::array_t{
            slimr::Json::object_t{
                {"id", "c1"},
                {"author", "Reviewer"},
                {"content", "Needs citation"},
                {"quotedFileContent", "cat"}
            }
        };
        DocModel doc = parse_doc_json(doc_json);
        doc.suggestions.push_back(Suggestion{
            .id = "s1",
            .tab_id = "t.0",
            .kind = "insertion",
            .text = "suggested addition"
        });

        ExportResult res = export_artifacts(doc);
        REQUIRE(res.comments_yaml.has_value());
        CHECK(res.comments_yaml->find("Needs citation") != std::string::npos);
        CHECK(res.comments_yaml->find("Reviewer") != std::string::npos);

        REQUIRE(res.suggestions_yaml.has_value());
        CHECK(res.suggestions_yaml->find("s1") != std::string::npos);
        CHECK(res.suggestions_yaml->find("suggested addition") != std::string::npos);
    }

    TEST_CASE("Export multi-tab document with ::tab directives") {
        DocModel doc;
        doc.doc_id = "m1";
        doc.title = "Tabs Test";
        doc.revision_id = "r1";

        TabModel t1;
        t1.tab_id = "t.0";
        t1.title = "Tab Zero";
        ParagraphBlock p1;
        p1.inlines.push_back(Inline::text("First content"));
        t1.blocks.push_back(Block::make_paragraph(p1));

        TabModel t2;
        t2.tab_id = "t.1";
        t2.title = "Tab One";
        ParagraphBlock p2;
        p2.inlines.push_back(Inline::text("Second content"));
        t2.blocks.push_back(Block::make_paragraph(p2));

        doc.tabs.push_back(t1);
        doc.tabs.push_back(t2);

        ExportResult res = export_artifacts(doc);
        CHECK(res.base_md.find("::tab{id=t.0 title=\"Tab Zero\"}") != std::string::npos);
        CHECK(res.base_md.find("First content") != std::string::npos);
        CHECK(res.base_md.find("::tab{id=t.1 title=\"Tab One\"}") != std::string::npos);
        CHECK(res.base_md.find("Second content") != std::string::npos);
    }
}

// ============================================================================
// 4. Markdown Parsing and Frontmatter Validation
// ============================================================================

TEST_SUITE("Markdown Parsing and Validation") {
    TEST_CASE("accepts frontmatter from the previous major version") {
        std::string md = R"(---
gdocsmith: "3.0.20"
doc: doc-101
revision: rev-9
title: Existing Document
---
Body text
)";
        CHECK(parse_markdown(md).has_value());
    }

    TEST_CASE("Valid frontmatter parse") {
        std::string md = R"(---
gdocsmith: "4.0.0"
doc: doc-101
revision: rev-9
title: Hello World
---

# Heading 1

This is a paragraph with **bold** and *italic* and `code`.
)";
        auto res = parse_markdown(md);
        REQUIRE(res.has_value());
        CHECK(res->doc_id == "doc-101");
        CHECK(res->revision_id == "rev-9");
        CHECK(res->title == "Hello World");
        REQUIRE(res->tabs.size() == 1);
        REQUIRE(res->tabs[0].blocks.size() == 2);
        CHECK(res->tabs[0].blocks[0].paragraph.heading_level == 1);
        CHECK(res->tabs[0].blocks[0].paragraph.plain_text() == "Heading 1");
    }

    TEST_CASE("Validate against base document: stale revision rejected") {
        DocModel base;
        base.doc_id = "doc-101";
        base.revision_id = "rev-9";

        std::string stale_md = R"(---
gdocsmith: "4.0.0"
doc: doc-101
revision: rev-old
title: Hello World
---
Body text
)";
        auto res = parse_markdown(stale_md, base);
        CHECK(!res.has_value());
        CHECK(res.error().find("staleBase") != std::string::npos);
    }

    TEST_CASE("Validate against base document: doc mismatch rejected") {
        DocModel base;
        base.doc_id = "doc-101";
        base.revision_id = "rev-9";

        std::string bad_doc_md = R"(---
gdocsmith: "4.0.0"
doc: doc-DIFFERENT
revision: rev-9
title: Hello World
---
Body text
)";
        auto res = parse_markdown(bad_doc_md, base);
        CHECK(!res.has_value());
        CHECK(res.error().find("mismatch") != std::string::npos);
    }

    TEST_CASE("Duplicate tab IDs rejected") {
        std::string dup_md = R"(---
gdocsmith: "4.0.0"
doc: doc-101
revision: rev-9
title: Dup Tabs
---

::tab{id=t.0 title="Tab One"}
Content 1

::tab{id=t.0 title="Tab Two"}
Content 2
)";
        auto res = parse_markdown(dup_md);
        CHECK(!res.has_value());
        CHECK(res.error().find("appears more than once") != std::string::npos);
    }

    TEST_CASE("Parse lists and tables from markdown") {
        std::string md = R"(---
gdocsmith: "4.0.0"
doc: doc-101
revision: rev-9
title: Lists & Tables
---

- Item 1
  - Nested item
- Item 2

1. First
2. Second

| Col A | Col B |
| --- | --- |
| Val 1 | Val 2 |
)";
        auto res = parse_markdown(md);
        REQUIRE(res.has_value());
        const auto& blocks = res->tabs[0].blocks;
        REQUIRE(blocks.size() >= 4);

        // Bullet items
        CHECK(blocks[0].paragraph.bullet.has_value());
        CHECK(blocks[0].paragraph.bullet->is_numbered == false);
        CHECK(blocks[0].paragraph.bullet->nesting_level == 0);

        CHECK(blocks[1].paragraph.bullet.has_value());
        CHECK(blocks[1].paragraph.bullet->nesting_level == 1);

        // Numbered items
        CHECK(blocks[3].paragraph.bullet.has_value());
        CHECK(blocks[3].paragraph.bullet->is_numbered == true);

        // Table
        CHECK(blocks.back().kind == BlockKind::Table);
        CHECK(blocks.back().table.columns == 2);
    }
}

// ============================================================================
// 5. Reconcile Engine & BatchUpdate Request Generation
// ============================================================================

TEST_SUITE("Reconcile Engine") {
    TEST_CASE("Unchanged document produces zero requests") {
        slimr::Json doc_json = make_test_doc_json();
        DocModel base = parse_doc_json(doc_json);
        ExportResult exp = export_artifacts(base);

        auto plan = reconcile(base, exp.base_md);
        REQUIRE(plan.has_value());
        CHECK(plan->content_requests.empty());
        CHECK(plan->tab_ops.empty());
    }

    TEST_CASE("Word change in paragraph produces deleteContentRange and insertText") {
        slimr::Json doc_json = make_test_doc_json();
        DocModel base = parse_doc_json(doc_json);
        ExportResult exp = export_artifacts(base);

        // Replace "the cat sat" with "the dog sat"
        std::string edited = exp.base_md;
        size_t pos = edited.find("the cat sat");
        REQUIRE(pos != std::string::npos);
        edited.replace(pos, 11, "the dog sat");

        auto plan = reconcile(base, edited);
        REQUIRE(plan.has_value());
        REQUIRE(plan->content_requests.size() == 2);

        // Check requests
        const auto& req0 = plan->content_requests[0];
        const auto& req1 = plan->content_requests[1];

        CHECK(req0.contains("deleteContentRange"));
        CHECK(req0["deleteContentRange"]["range"]["startIndex"].get<int64_t>() == 14); // 10 + 4
        CHECK(req0["deleteContentRange"]["range"]["endIndex"].get<int64_t>() == 17);   // 10 + 7

        CHECK(req1.contains("insertText"));
        CHECK(req1["insertText"]["location"]["index"].get<int64_t>() == 14);
        CHECK(req1["insertText"]["text"].get<std::string>() == "dog");
    }

    TEST_CASE("Heading level change produces updateParagraphStyle") {
        slimr::Json doc_json = make_test_doc_json();
        DocModel base = parse_doc_json(doc_json);
        ExportResult exp = export_artifacts(base);

        // Change # Overview to ## Overview
        std::string edited = exp.base_md;
        size_t pos = edited.find("# Overview");
        REQUIRE(pos != std::string::npos);
        edited.replace(pos, 10, "## Overview");

        auto plan = reconcile(base, edited);
        REQUIRE(plan.has_value());
        REQUIRE(plan->content_requests.size() == 1);
        const auto& req = plan->content_requests[0];
        CHECK(req.contains("updateParagraphStyle"));
        CHECK(req["updateParagraphStyle"]["paragraphStyle"]["namedStyleType"].get<std::string>() == "HEADING_2");
    }

    TEST_CASE("Text formatting toggle produces updateTextStyle") {
        slimr::Json doc_json = make_test_doc_json();
        DocModel base = parse_doc_json(doc_json);
        ExportResult exp = export_artifacts(base);

        // Change "the cat sat" to "**the cat sat**"
        std::string edited = exp.base_md;
        size_t pos = edited.find("the cat sat");
        REQUIRE(pos != std::string::npos);
        edited.replace(pos, 11, "**the cat sat**");

        auto plan = reconcile(base, edited);
        REQUIRE(plan.has_value());
        REQUIRE(plan->content_requests.size() == 1);
        const auto& req = plan->content_requests[0];
        CHECK(req.contains("updateTextStyle"));
        CHECK(req["updateTextStyle"]["fields"].get<std::string>().find("bold") != std::string::npos);
        CHECK(req["updateTextStyle"]["textStyle"]["bold"].get<bool>() == true);
    }

    TEST_CASE("Adding bullet list produces createParagraphBullets") {
        slimr::Json doc_json = make_test_doc_json();
        DocModel base = parse_doc_json(doc_json);
        ExportResult exp = export_artifacts(base);

        // Change "the cat sat" to "- the cat sat"
        std::string edited = exp.base_md;
        size_t pos = edited.find("the cat sat");
        REQUIRE(pos != std::string::npos);
        edited.replace(pos, 11, "- the cat sat");

        auto plan = reconcile(base, edited);
        REQUIRE(plan.has_value());
        bool has_bullet_req = false;
        for (const auto& r : plan->content_requests) {
            if (r.contains("createParagraphBullets")) has_bullet_req = true;
        }
        CHECK(has_bullet_req);
    }

    TEST_CASE("Tab lifecycle: rename and delete") {
        DocModel base;
        base.doc_id = "doc-tabs";
        base.revision_id = "rev-1";
        base.title = "Tab Lifecycle";

        TabModel t0;
        t0.tab_id = "t.0";
        t0.title = "Initial Title";
        base.tabs.push_back(t0);

        TabModel t1;
        t1.tab_id = "t.1";
        t1.title = "Delete Me";
        base.tabs.push_back(t1);

        DocModel final_doc = base;
        final_doc.tabs[0].title = "Renamed Title";
        final_doc.tabs.pop_back(); // Delete t.1

        auto plan = reconcile(base, final_doc);
        REQUIRE(plan.has_value());
        REQUIRE(plan->tab_ops.size() == 2);

        // One deleteTab and one updateDocumentTabProperties
        bool has_delete = false;
        bool has_rename = false;
        for (const auto& op : plan->tab_ops) {
            if (op.contains("deleteTab")) {
                CHECK(op["deleteTab"]["tabId"].get<std::string>() == "t.1");
                has_delete = true;
            }
            if (op.contains("updateDocumentTabProperties")) {
                CHECK(op["updateDocumentTabProperties"]["tabProperties"]["title"].get<std::string>() == "Renamed Title");
                has_rename = true;
            }
        }
        CHECK(has_delete);
        CHECK(has_rename);
    }
}
