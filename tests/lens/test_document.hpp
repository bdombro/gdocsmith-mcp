#pragma once

#include "lens/document.hpp"
#include "lens/put.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("Node suggestion export hints match complete ids rather than s1 s10 prefixes") {
    DocumentFile file;
    file.frontmatter.title="Synthetic";
    DocumentFileTab tab;
    tab.id="t.0";
    tab.title="Main";
    tab.markdown="::section{id=s1}\n\n:suggestion{id=s10 start}visible:suggestion{id=s10 end}\n";
    file.tabs.push_back(tab);
    SuggestionBlock hidden;
    hidden.id="s1"; hidden.kind="text"; hidden.before=":change[]"; hidden.after=":change[hidden]";
    file.suggestions.push_back(hidden);
    SuggestionBlock shown;
    shown.id="s10"; shown.kind="text"; shown.before="visible"; shown.after="visible";
    file.suggestions.push_back(shown);
    const auto raw=documentFileExport(file);
    const auto tabAt=raw.find("::tab{");
    const auto tabLine=std::count(raw.begin(),raw.begin()+static_cast<std::ptrdiff_t>(tabAt),'\n')+1;
    CHECK(raw.find("::suggestion{id=s1 kind=text tab=Main exportLine="+std::to_string(tabLine)+"}")!=std::string::npos);
    CHECK(raw.find("::suggestion{id=s10 kind=text tab=Main}\n")!=std::string::npos);
}

TEST_CASE("Node OB043 raw 3.0.20 envelope retains version declarations nulls and typed edits without normalization") {
    const auto source = documentFileParse(
        "---\ngdocsmith: 3.0.20\ndoc: null\nrevision: null\ntitle: Synthetic\n"
        "textStyles:\n  accent:\n    bold: true\n    foregroundColor: null\n"
        "paragraphStyles:\n  layout:\n    alignment: CENTER\n    keepWithNext: null\n"
        "cellStyles:\n  clear:\n    paddingTop: null\n    backgroundColor: null\n"
        "---\n\n::tab{id=t.0 title=Main pageful size=LEGAL landscape margins=10,20,30,0}\n\n"
        "::section{id=s1}\n\n:style[body]{.accent} :p{.layout}\n");
    CHECK(source.frontmatter.sourceVersion == std::optional<std::string>{"3.0.20"});
    CHECK(source.frontmatter.textStyles.at("accent").resets.contains("foregroundColor"));
    CHECK(source.frontmatter.paragraphStyles.at("layout").resets.contains("keepWithNext"));
    CHECK(source.frontmatter.cellStyles.at("clear").resets.contains("paddingTop"));
    const auto raw = documentFileExport(source);
    const auto reparsed = documentFileParse(raw);
    CHECK(reparsed.frontmatter == source.frontmatter);
    CHECK(reparsed.tabs[0].markdown == source.tabs[0].markdown);
    CHECK(reparsed.tabs[0].openingSettings == source.tabs[0].openingSettings);
    CHECK(reparsed.tabs[0].settings == source.tabs[0].settings);
    model::TabModel tab;
    model::EditContext context;
    CHECK(markdownPut(model::EditTarget{context, tab},
        documentTabMarkdown(source, source.tabs[0], model::DocModel{}, "t.0")).changed);
    REQUIRE(tab.blocks.size() == 1);
    CHECK(tab.blocks[0].as_paragraph().inlines[0].text == "body");
    CHECK(tab.blocks[0].as_paragraph().inlines[0].style["bold"] == slimr::Json(true));
    CHECK(tab.blocks[0].as_paragraph().style["alignment"] == slimr::Json("CENTER"));
    CHECK_THROWS_AS(documentFileParse("---\ngdocsmith: 3.0.19\ntitle: Synthetic\n---\nbody"), CoreError);
}

TEST_CASE("Node document opening split preserves body separators without erasing blank paragraphs") {
    const auto parsed = documentFileParse(
        "---\ngdocsmith: 3.0.20\ndoc: synthetic\nrevision: r1\ntitle: Synthetic\n---\n"
        "::tab{id=t.0 title=Main}\n\n::section{id=s1 columns=2}\n\nfirst\n\n :p{.p1}\n\n"
        "::tab{id=t.1 title=Other}\n\n::section{id=s1}\n\nsecond\n");
    REQUIRE(parsed.tabs.size() == 2);
    CHECK(parsed.tabs[0].markdown == "\n\nfirst\n\n :p{.p1}\n\n");
    CHECK(parsed.tabs[1].markdown == "\n\nsecond\n");
    REQUIRE(parsed.tabs[0].openingSettings);
    CHECK(parsed.tabs[0].openingSettings->columns == 2);
    CHECK(markdownParse(documentTabMarkdown(parsed, parsed.tabs[0], {}, "t.0")).blocks.size() == 3);
    const auto roundtrip = documentFileParse(documentFileExport(parsed));
    CHECK(roundtrip.tabs[0].markdown == parsed.tabs[0].markdown);
    CHECK(roundtrip.tabs[1].markdown == parsed.tabs[1].markdown);
}

TEST_CASE("Node source review envelope uses double directives quote attributes and reversible literal directive text") {
    DocumentFile file;
    file.frontmatter.title = "Synthetic";
    DocumentFileTab tab;
    tab.id = "t.0";
    tab.title = "Main";
    tab.markdown = "::section{id=s1}\n\nbody\n";
    file.tabs.push_back(tab);
    CommentBlock comment;
    comment.id = "synthetic-comment";
    comment.quote = "quoted text";
    comment.text = "::literal\n> body text";
    CommentReply reply;
    reply.id = "synthetic-reply";
    reply.text = "\\::literal reply";
    comment.replies.push_back(reply);
    file.comments.push_back(comment);
    SuggestionBlock suggestion;
    suggestion.id = "synthetic-suggestion";
    suggestion.kind = "text";
    suggestion.before = "old";
    suggestion.after = "new";
    file.suggestions.push_back(suggestion);
    const auto raw = documentFileExport(file);
    CHECK(raw.find(":::") == std::string::npos);
    CHECK(raw.find("::comment{id=synthetic-comment quote=\"quoted text\"}") != std::string::npos);
    CHECK(raw.find("\\::literal\n> body text") != std::string::npos);
    const auto tabAt=raw.find("::tab{");
    const auto tabLine=std::count(raw.begin(),raw.begin()+static_cast<std::ptrdiff_t>(tabAt),'\n')+1;
    CHECK(raw.find("::suggestion{id=synthetic-suggestion kind=text tab=Main exportLine="+std::to_string(tabLine)+"}") != std::string::npos);
    const auto parsed = documentFileParse(raw);
    REQUIRE(parsed.comments.size() == 1);
    CHECK(parsed.comments[0].quote == comment.quote);
    CHECK(parsed.comments[0].text == comment.text);
    REQUIRE(parsed.comments[0].replies.size() == 1);
    CHECK(parsed.comments[0].replies[0].text == reply.text);
    REQUIRE(parsed.suggestions.size() == 1);
    CHECK(parsed.suggestions[0].before == suggestion.before);
    CHECK(parsed.suggestions[0].after == suggestion.after);
}

TEST_CASE("Node dropdown declarations validate roundtrip and new aliases reach typed atom creation") {
    const auto file=documentFileParse(
        "---\ngdocsmith: 3.0.20\ndoc: synthetic\nrevision: r1\ntitle: Synthetic\n"
        "dropdowns:\n  t.0:\n    new-list:\n      title: Status\n      options:\n        - Ready: {color: '#aabbcc', background: '#112233'}\n"
        "---\n::tab{id=t.0 title=Main}\n\n::section{id=s1}\n\n:dropdown{list=new-list option=Ready}\n");
    CHECK(file.frontmatter.dropdowns["t.0"]["new-list"]["options"][0]["Ready"]["color"] == slimr::Json("#AABBCC"));
    CHECK(documentFileParse(documentFileExport(file)).frontmatter.dropdowns == file.frontmatter.dropdowns);
    model::TabModel tab;
    tab.tabId="t.0";
    tab.dropdownDefinitions["new-provider-id"]=slimr::Json::object_t{{"dropdownDefinitionProperties",
        slimr::Json::object_t{{"options",slimr::Json::array_t{
            slimr::Json::object_t{{"displayValue","Ready"},{"optionId","new-option-id"}}}}}}};
    model::EditContext context;
    AtomPutOptions options;
    options.createdDropdownIds["new-list"]="new-provider-id";
    CHECK(markdownPut(model::EditTarget{context,tab},documentTabMarkdown(file,file.tabs[0],{},"t.0"),options).changed);
    REQUIRE(tab.blocks[0].as_paragraph().inlines[0].create);
    CHECK(tab.blocks[0].as_paragraph().inlines[0].create->definitionId == "new-provider-id");
    CHECK(tab.blocks[0].as_paragraph().inlines[0].create->optionId == "new-option-id");
    for (const auto* invalid : {
        "dropdowns: []", "dropdowns: {t.0: {x: {title: Status}}}",
        "dropdowns: {t.0: {x: {options: ['']}}}",
        "dropdowns: {t.0: {x: {options: [{Ready: {color: red}}]}}}",
        "dropdowns: {t.0: {x: {options: [], other: true}}}"})
        CHECK_THROWS_AS(documentFileParse("---\ngdocsmith: 3.0.20\ntitle: Synthetic\n"+std::string(invalid)+
            "\n---\n::tab{id=t.0 title=Main}\n\n::section{id=s1}\n\nbody\n"),model::CoreError);
}
TEST_CASE("Node document120 page settings reject ambiguous invalid and unknown raw attributes") {
    for (const auto* attrs : {"size=A4", "landscape", "margins=1,1,1,1", "pageful size=a4",
            "pageful size=0x700", "pageful size=500x700 landscape", "pageful margins=1,1,1",
            "pageful margins=1,1,-1,1", "pageful=yes", "pageful pageful", "depth=0",
            "headerMargin=nan", "footerMargin=inf", "pageNumberStart=1.5"}) {
        CHECK_THROWS_AS(documentFileParse("---\ngdocsmith: 3.0.20\ntitle: Synthetic\n---\n\n::tab{id=t.0 title=Main " +
            std::string(attrs) + "}\n\n::section{id=s1}\n\nbody\n"), CoreError);
    }
}

TEST_CASE("document file omitted tab id means a new tab and notice-only reviews survive parsing") {
    const auto parsed = documentFileParse(
        "---\ngdocsmith: 1\ntitle: Synthetic\n---\n\n"
        "::tab{title=New}\n\nbody\n\n"
        ":::suggestion{id=s kind=text}\nRead-only notice.\n:::\n");
    REQUIRE(parsed.tabs.size() == 1);
    CHECK(parsed.tabs[0].id.empty());
    REQUIRE(parsed.suggestions.size() == 1);
    CHECK(parsed.suggestions[0].notice == std::optional<std::string>{"Read-only notice."});
}

TEST_CASE("documentFileParse and Export round trip") {
    std::string sample =
        "---\n"
        "doc: doc123\n"
        "gdocsmith: 1\n"
        "revision: r1\n"
        "title: My Document\n"
        "---\n\n"
        "::tab{id=t.0 title=\"Main\"}\n\n"
        "# Introduction\n\n"
        "This is a paragraph.\n\n"
        "::tab{id=t.1 title=\"Notes\" pageless}\n\n"
        "Notes content.\n\n"
        ":::comment{id=c1 author=\"Alice\" resolved}\n"
        "> quoted text\n\n"
        "Looks good!\n\n"
        "::reply{author=\"Bob\"}\n"
        "Thanks!\n"
        ":::\n\n"
        ":::suggestion{id=s1 kind=text}\n"
        "::before\n"
        "old text\n"
        "::after\n"
        "new text\n"
        ":::\n";

    auto doc = documentFileParse(sample);
    CHECK(doc.frontmatter.title == "My Document");
    CHECK(doc.frontmatter.doc == std::optional<std::string>("doc123"));
    CHECK(doc.frontmatter.revision == std::optional<std::string>("r1"));
    CHECK(doc.tabs.size() == 2u);
    CHECK(doc.tabs[0].id == "t.0");
    CHECK(doc.tabs[0].title == "Main");
    CHECK(doc.tabs[1].id == "t.1");
    CHECK(doc.tabs[1].title == "Notes");
    CHECK(doc.tabs[1].settings.pageless);

    CHECK(doc.comments.size() == 1u);
    CHECK(doc.comments[0].id == "c1");
    CHECK(doc.comments[0].author == std::optional<std::string>("Alice"));
    CHECK(doc.comments[0].resolved);
    CHECK(doc.comments[0].quote == std::optional<std::string>("quoted text"));
    CHECK(doc.comments[0].text == "Looks good!");
    CHECK(doc.comments[0].replies.size() == 1u);
    CHECK(doc.comments[0].replies[0].author == std::optional<std::string>("Bob"));
    CHECK(doc.comments[0].replies[0].text == "Thanks!");

    CHECK(doc.suggestions.size() == 1u);
    CHECK(doc.suggestions[0].id == "s1");
    CHECK(doc.suggestions[0].kind == "text");
    CHECK(doc.suggestions[0].before == std::optional<std::string>("old text"));
    CHECK(doc.suggestions[0].after == std::optional<std::string>("new text"));

    std::string exported = documentFileExport(doc);
    auto doc2 = documentFileParse(exported);
    CHECK(doc == doc2);
}

TEST_CASE("W1 - doc: null parses without opening ::section{id=s1} in both 3.0.20 and legacy 1 envelopes") {
    const auto v3 = documentFileParse(
        "---\ngdocsmith: 3.0.20\ndoc: null\nrevision: null\ntitle: New Doc\n---\n\nHello world\n");
    CHECK(!v3.frontmatter.doc.has_value());
    REQUIRE(v3.tabs.size() == 1);
    CHECK(v3.tabs[0].markdown.find("Hello world") != std::string::npos);

    const auto v1 = documentFileParse(
        "---\ngdocsmith: 1\ndoc: null\nrevision: null\ntitle: New Doc\n---\n\nHello legacy\n");
    CHECK(!v1.frontmatter.doc.has_value());
    REQUIRE(v1.tabs.size() == 1);
    CHECK(v1.tabs[0].markdown.find("Hello legacy") != std::string::npos);
}

} // namespace gdocsmith::lens::test
