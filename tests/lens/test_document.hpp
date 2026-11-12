#pragma once

#include "lens/document.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

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

} // namespace gdocsmith::lens::test
