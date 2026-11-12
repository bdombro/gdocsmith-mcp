#pragma once

#include "engine/transaction.hpp"
#include "emulator/google_docs_emulator.hpp"
#include "model/test_docs.hpp"
#include "test.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gdocsmith::engine::test {

using namespace model;

// Adapts the emulator's FakeGoogle (snake_case) to the camelCase client
// interface the transaction runner expects.
struct FakeClient {
    emulator::FakeGoogle& backend;

    slimr::Json getDocument(const std::string& id) {
        const auto& doc = backend.get_doc(id);
        slimr::Json json = doc.json;
        json["revisionId"] = backend.revision_id(doc);
        return json;
    }

    slimr::Json batchUpdate(const std::string& id,
                            const std::vector<slimr::Json>& requests,
                            const std::optional<std::string>& revision) {
        std::string raw = backend.batch_update(id, requests, revision);
        auto parsed = slimr::parse_json<slimr::Json>(raw);
        return parsed ? *parsed : slimr::Json{};
    }

    std::string createDocument(const std::string& title) {
        return backend.create_document(title);
    }

    // Drive-side surface the CoreSession ctor requires when opts.drive is set.
    std::vector<DriveComment> commentsList(const std::string&) { return {}; }
    std::string textExport(const std::string&) { return ""; }
};

TEST_CASE("tabIdMap maps new_tab_ ordinals to real tab ids") {
    Ledger ledger;
    ledger.tabs["d1"] = {"t.9", "t.10"};
    auto m = tabIdMap(ledger, "d1");
    CHECK(m.size() == 2u);
    CHECK(m["new_tab_1"] == "t.9");
    CHECK(m["new_tab_2"] == "t.10");
    CHECK(tabIdMap(ledger, "other").empty());
}

TEST_CASE("footnoteOrder returns new footnote ids in document order") {
    TabModel tab;
    tab.tabId = "t.0";
    ParagraphBlock p;
    p.key = "n1";
    Inline ref;
    ref.kind = "atom";
    ref.type = "footnoteRef";
    ref.key = "n2";
    ref.create = AtomCreate{.type = "footnoteRef"};
    p.inlines.push_back(std::move(ref));
    tab.blocks.push_back(Block(std::move(p)));

    auto ids = footnoteOrder(tab);
    REQUIRE(ids.size() == 1u);
    CHECK(ids[0] == "new_fn_n2");
}

TEST_CASE("buildNewIds maps ledger docs and tabs into aliases") {
    Ledger ledger;
    ledger.docs["d1"] = "doc_1";
    ledger.tabs["d2"] = {"t.9"};
    std::map<std::string, std::string> headings{{"n1", "h.1"}};

    auto ids = buildNewIds(ledger, headings);
    CHECK(ids.docs["new_d1"] == "doc_1");
    CHECK(ids.tabs["d2/new_tab_1"] == "t.9");
    CHECK(ids.headings["n1"] == "h.1");
}

TEST_CASE("transactionRun - editing an existing document lands and verifies") {
    auto json = docJsonBuild(DocSpec{
        .tabs = {
            TabSpec{
                .tabId = "t.0",
                .blocks = {
                    BlockSpec::make_paragraph(ParagraphSpec{.content = {"hello"}}),
                },
            },
        },
    });

    emulator::FakeGoogle backend;
    backend.seed_document("d1", json, 1);
    FakeClient client{backend};

    auto program = [](CoreSession& session) -> int {
        auto doc = session.docOpen("d1");
        auto tab = doc->tab("");
        tab.writeMarkdown("hello world");
        return 42;
    };

    TransactionOptions<FakeClient> opts;
    opts.client = &client;
    auto result = transactionRun<int, FakeClient>(program, opts);

    CHECK(!result.refused);
    CHECK(result.attempts == 1);
    CHECK(result.output == 42);
    CHECK(result.findings.empty());
    CHECK(!result.phases.empty());
    CHECK(result.verification.size() == 1u);
    CHECK(result.verification[0].status == "match");
}

} // namespace gdocsmith::engine::test
