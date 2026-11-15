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
struct TransactionClient {
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
    CHECK(ids[0] == "new:fn:n2");
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
    TransactionClient client{backend};

    auto program = [](CoreSession& session) -> int {
        auto doc = session.docOpen("d1");
        auto tab = doc->tab("");
        tab.writeMarkdown("hello world");
        return 42;
    };

    TransactionOptions<TransactionClient> opts;
    opts.client = &client;
    auto result = transactionRun<int, TransactionClient>(program, opts);

    CHECK(!result.refused);
    CHECK(result.attempts == 1);
    CHECK(result.output == 42);
    CHECK(result.findings.empty());
    CHECK(!result.phases.empty());
    CHECK(result.verification.size() == 1u);
    CHECK(result.verification[0].status == "match");
    CHECK(result.documents.contains("d1"));
}

TEST_CASE("transactionRun - no-op reads back and verifies without a write") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", docJsonBuild(DocSpec{
        .tabs = {TabSpec{.tabId = "t.0", .blocks = {
            BlockSpec::make_paragraph(ParagraphSpec{.content = {"hello"}})
        }}}
    }), 1);
    struct ReadOnlyClient : TransactionClient {
        int reads = 0;
        int writes = 0;
        slimr::Json getDocument(const std::string& id) {
            ++reads;
            return TransactionClient::getDocument(id);
        }
        slimr::Json batchUpdate(const std::string&, const std::vector<slimr::Json>&,
                               const std::optional<std::string>&) {
            ++writes;
            throw std::runtime_error("a no-op must not write");
        }
    } client{{backend}};
    TransactionOptions<ReadOnlyClient> options;
    options.client = &client;
    const auto result = transactionRun<int>([](CoreSession& session) {
        session.docOpen("d1");
        return 0;
    }, options);
    CHECK(client.reads == 2);
    CHECK(client.writes == 0);
    REQUIRE(result.verification.size() == 1u);
    CHECK(result.verification[0].status == "match");
    CHECK(result.documents.at("d1").contains("revisionId"));
}

TEST_CASE("transaction Markdown uses typed putback and rejects unsupported atom changes atomically") {
    CoreSession session;
    auto doc = session.docCreate("test", "test");
    auto tab = doc->tab();
    tab.writeMarkdown("keep");
    for (const auto& markdown : {"changed :equation{id=does-not-exist}"}) {
        const auto before = tab.model();
        CHECK_THROWS_AS(tab.writeMarkdown(markdown), model::CoreError);
        CHECK(tab.model() == before);
        CHECK(tab.model().blocks[0].as_paragraph().inlines[0].text == "keep");
    }
    model::Inline atom;
    atom.kind = "atom";
    atom.type = "image";
    atom.key = "protected_image";
    tab.model().blocks[0].as_paragraph().inlines.push_back(atom);
    const auto before = tab.model();
    CHECK_THROWS_AS(tab.writeMarkdown("replacement :person{email=not-an-email}"), model::CoreError);
    CHECK(tab.model() == before);
    CHECK(tab.model().blocks[0].as_paragraph().inlines.size() == 2u);
}

TEST_CASE("contentSend retains confirmed chunk counts and revision chaining on failure") {
    struct ChunkClient {
        int calls = 0;
        slimr::Json batchUpdate(const std::string&, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            ++calls;
            if (calls == 1) {
                CHECK(requests.size() == MAX_REQUESTS_PER_BATCH);
                CHECK(revision == std::optional<std::string>("r1"));
                return slimr::Json::object_t{{"writeControl", slimr::Json::object_t{{"requiredRevisionId", "r2"}}}};
            }
            CHECK(revision == std::optional<std::string>("r2"));
            throw std::runtime_error("requests[0]: synthetic failure");
        }
    } client;
    const std::vector<slimr::Json> requests(MAX_REQUESTS_PER_BATCH + 1, slimr::Json::object_t{});
    bool failed = false;
    try {
        contentSend(&client, "d1", requests, {}, "r1");
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.landedRequestCount == static_cast<int>(MAX_REQUESTS_PER_BATCH));
        CHECK(error.partial.landed == std::vector<std::string>{"d1"});
    }
    CHECK(failed);
}

TEST_CASE("transaction helpers reject missing clients and malformed responses") {
    TransactionOptions<TransactionClient> options;
    CHECK_THROWS_AS(transactionRun<int>([](CoreSession&) { return 0; }, options), model::CoreError);
    struct InvalidClient {
        slimr::Json createDocument(const std::string&) { return slimr::Json::object_t{}; }
        std::string getDocument(const std::string&) { return "not JSON"; }
        slimr::Json batchUpdate(const std::string&, const std::vector<slimr::Json>&,
                               const std::optional<std::string>&) { return "not JSON"; }
    } client;
    CHECK_THROWS_AS(clientCreateDocument(&client, "test"), model::CoreError);
    CHECK_THROWS_AS(clientGetDocument(&client, "d1"), model::CoreError);
    CHECK_THROWS_AS(contentSend(&client, "d1", {slimr::Json::object_t{}}, {}, "r1"), TransactionError);
    struct InvalidJsonClient {
        slimr::Json getDocument(const std::string&) { return "not a document"; }
    } invalidJson;
    CHECK_THROWS_AS(clientGetDocument(&invalidJson, "d1"), model::CoreError);
}

TEST_CASE("transaction create replans once against the real document and retains readback") {
    emulator::FakeGoogle backend;
    struct Client : TransactionClient {
        int creates = 0;
        int writes = 0;
        std::string createDocument(const std::string& title) {
            ++creates;
            return TransactionClient::createDocument(title);
        }
        slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            ++writes;
            REQUIRE(revision.has_value());
            return TransactionClient::batchUpdate(id, requests, revision);
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    int runs = 0;
    const auto result = transactionRun<int>([&](CoreSession& session) {
        ++runs;
        session.docCreate("draft", "Draft")->tab().writeMarkdown("created content");
        return runs;
    }, options);
    CHECK(runs == 2);
    CHECK(client.creates == 1);
    CHECK(client.writes > 0);
    CHECK(result.attempts == 1);
    const auto id = result.newIds.docs.at("new_draft");
    CHECK(result.documents.contains(id));
    REQUIRE(result.verification.size() == 1);
    CHECK(result.verification[0].status == "match");
    REQUIRE(result.phases.front().docs.size() == 1);
    CHECK(result.phases.front().docs[0].outcome == "confirmed");
}

TEST_CASE("transaction create failure keeps earlier creations and reports unknown identity") {
    emulator::FakeGoogle backend;
    struct Client : TransactionClient {
        int creates = 0;
        std::string createDocument(const std::string& title) {
            if (++creates == 2) throw std::runtime_error("connection closed before response");
            return TransactionClient::createDocument(title);
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    bool failed = false;
    try {
        transactionRun<int>([](CoreSession& session) {
            session.docCreate("a", "First");
            session.docCreate("b", "Second");
            return 0;
        }, options);
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.phase == "create");
        CHECK(error.partial.created.size() == 1);
        CHECK(error.partial.landed.size() == 1);
        CHECK(error.partial.unknown == std::vector<std::string>{"new_b"});
        CHECK(error.unknownRequestCount == 1);
        REQUIRE(error.phases.size() == 1);
        REQUIRE(error.phases[0].docs.size() == 2);
        CHECK(error.phases[0].docs[0].outcome == "confirmed");
        CHECK(error.phases[0].docs[1].outcome == "unknown");
    }
    CHECK(failed);
    CHECK(client.creates == 2);
}

TEST_CASE("transaction unknown content outcome is never replayed") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", docJsonBuild(DocSpec{
        .tabs = {TabSpec{.tabId = "t.0", .blocks = {
            BlockSpec::make_paragraph(ParagraphSpec{.content = {"before"}})
        }}}
    }), 1);
    struct Client : TransactionClient {
        int writes = 0;
        slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            ++writes;
            TransactionClient::batchUpdate(id, requests, revision);
            throw std::runtime_error("connection lost after send");
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    int runs = 0;
    bool failed = false;
    try {
        transactionRun<int>([&](CoreSession& session) {
            ++runs;
            session.docOpen("d1")->tab().writeMarkdown("after");
            return 0;
        }, options);
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.partial.landed.empty());
        CHECK(error.partial.unknown == std::vector<std::string>{"d1"});
        CHECK(error.unknownRequestCount > 0);
        REQUIRE(!error.phases.empty());
        CHECK(error.phases.back().docs.back().outcome == "unknown");
        CHECK(error.phases.back().docs.back().requestCount == 0);
    }
    CHECK(failed);
    CHECK(runs == 1);
    CHECK(client.writes == 1);
}

TEST_CASE("revision locks are mandatory and missing response revisions stop chunk chains") {
    struct Client {
        int calls = 0;
        slimr::Json batchUpdate(const std::string&, const std::vector<slimr::Json>&,
                               const std::optional<std::string>&) {
            ++calls;
            return slimr::Json::object_t{};
        }
    } client;
    CHECK_THROWS_AS(contentSend(&client, "d1", {slimr::Json::object_t{}}, {}), model::CoreError);
    CHECK_THROWS_AS(batchSend(&client, "d1", {slimr::Json::object_t{}}, ""), model::CoreError);
    CHECK(client.calls == 0);
    bool failed = false;
    try {
        contentSend(&client, "d1", std::vector<slimr::Json>(MAX_REQUESTS_PER_BATCH + 1), {}, "r1");
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.landedRequestCount == 0);
        CHECK(error.unknownRequestCount == static_cast<int>(MAX_REQUESTS_PER_BATCH));
        CHECK(error.partial.unknown == std::vector<std::string>{"d1"});
    }
    CHECK(failed);
    CHECK(client.calls == 1);
}

TEST_CASE("subphase chunk failure retains earlier successes and unknown requests") {
    PlannedDoc doc;
    doc.docId = "d1";
    struct Client {
        int calls = 0;
        slimr::Json batchUpdate(const std::string&, const std::vector<slimr::Json>&,
                               const std::optional<std::string>& revision) {
            if (++calls == 1) {
                CHECK(revision == std::optional<std::string>("r1"));
                return slimr::Json::object_t{{"writeControl", slimr::Json::object_t{{"requiredRevisionId", "r2"}}}};
            }
            CHECK(revision == std::optional<std::string>("r2"));
            throw std::runtime_error("connection lost");
        }
    } client;
    std::vector<PhaseReport> phases;
    std::vector<std::string> landed;
    bool failed = false;
    try {
        phaseRun(phases, "footnotes", {doc}, {"created"}, landed, [&](const PlannedDoc& d, auto&&) {
            auto revision = batchSend(&client, d.docId,
                std::vector<slimr::Json>(MAX_REQUESTS_PER_BATCH + 1), "r1");
            return ContentSendResult{.requestCount = 1, .revisionAfter = revision};
        });
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.phase == "footnotes");
        CHECK(error.partial.created == std::vector<std::string>{"created"});
        CHECK(error.partial.landed == std::vector<std::string>{"d1"});
        CHECK(error.partial.unknown == std::vector<std::string>{"d1"});
        CHECK(error.landedRequestCount == static_cast<int>(MAX_REQUESTS_PER_BATCH));
        CHECK(error.unknownRequestCount == 1);
        CHECK(error.revisionAfter == std::optional<std::string>("r2"));
    }
    CHECK(failed);
    REQUIRE(phases.size() == 1);
    REQUIRE(phases[0].docs.size() == 1);
    CHECK(phases[0].docs[0].requestCount == static_cast<int>(MAX_REQUESTS_PER_BATCH));
    CHECK(phases[0].docs[0].unknownRequestCount == 1);
}

TEST_CASE("verification never calls a missing or changed revision a match") {
    auto json = docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0"}}});
    model::KeyAllocator keys;
    auto expected = model::docModelParse(json, {.docId = "d1", .keys = keys});
    json.get<slimr::Json::object_t>().erase("revisionId");
    CHECK(verifyDoc("d1", expected, json, "r1").status == "skipped");
    json["revisionId"] = "r2";
    CHECK(verifyDoc("d1", expected, json, "r1").status == "skipped");
}

TEST_CASE("heading verification resolves provisional links without ignoring target differences") {
    auto json = docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0", .blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {
            ParagraphTextSpec{.text = "linked", .style = slimr::Json::object_t{
                {"link", slimr::Json::object_t{{"heading", slimr::Json::object_t{
                    {"id", "h.real"}, {"tabId", "t.0"}}}}}
            }}
        }})
    }}}});
    json["revisionId"] = "r1";
    model::KeyAllocator keys;
    PlannedDoc doc;
    doc.docId = "d1";
    doc.input.final = model::docModelParse(json, {.docId = "d1", .keys = keys});
    doc.input.final.tabs[0].blocks[0].as_paragraph().inlines[0].style["link"] =
        slimr::Json::object_t{{"heading", slimr::Json::object_t{{"key", "nTarget"}, {"tabId", "t.0"}}}};
    VerifyResolvedOptions resolved{.headingIds = {{"nTarget", "h.real"}}};
    CHECK(verify(doc, json, "r1", resolved).status == "match");
    resolved.headingIds["nTarget"] = "h.wrong";
    CHECK(verify(doc, json, "r1", resolved).status == "mismatch");
    CHECK_THROWS_AS(verify(doc, json, "r1", {}), model::CoreError);
    CHECK(doc.input.final.tabs[0].blocks[0].as_paragraph().inlines[0].style["link"]["heading"].contains("key"));
}

TEST_CASE("named-range readback requires retained identity and exact predicted UTF16 edges") {
    auto json = docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0", .blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {"xhello"}})
    }}}});
    json["tabs"][0]["documentTab"]["namedRanges"] = slimr::Json::object_t{
        {"kept", slimr::Json::object_t{{"namedRanges", slimr::Json::array_t{
            slimr::Json::object_t{{"name", "kept"}, {"namedRangeId", "r.kept"},
                {"ranges", slimr::Json::array_t{slimr::Json::object_t{
                    {"startIndex", 2}, {"endIndex", 7}}}}}
        }}}}
    };
    model::KeyAllocator keys;
    auto expected = model::docModelParse(json, {.docId = "d1", .keys = keys});
    expected.tabs[0].namedRanges[0].ranges[0].start = 1;
    expected.tabs[0].namedRanges[0].ranges[0].end = 6;
    std::vector<slimr::Json> requests{slimr::Json::object_t{{"insertText", slimr::Json::object_t{
        {"text", "x"}, {"location", slimr::Json::object_t{{"index", 1}, {"tabId", "t.0"}}}
    }}}};
    CHECK(verifyNamedRanges("d1", expected, json, requests, {}, {}).status == "match");
    CHECK(verifyNamedRanges("d1", expected, json, {}, {}, {}).status == "mismatch");
    requests = {slimr::Json::object_t{{"insertTable", slimr::Json::object_t{}}}};
    CHECK(verifyNamedRanges("d1", expected, json, requests, {}, {}).status == "inconclusive");
}

TEST_CASE("transaction creates parent tabs before children and chains create revisions") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", emulator::blank_doc_template("Source", "d1"), 1);
    struct Client : TransactionClient {
        std::vector<std::string> events;
        std::optional<std::string> lastRevision;
        slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            REQUIRE(revision.has_value());
            if (lastRevision) CHECK(revision == lastRevision);
            for (const auto& request : requests) {
                if (request.contains("addDocumentTab")) {
                    const auto& properties = request["addDocumentTab"]["tabProperties"];
                    events.push_back(properties["title"].get<std::string>());
                    if (events.size() == 2) {
                        CHECK(properties.contains("parentTabId"));
                        CHECK(!properties["parentTabId"].get<std::string>().starts_with("new_"));
                    }
                }
            }
            auto response = TransactionClient::batchUpdate(id, requests, revision);
            lastRevision = responseRevision(response);
            return response;
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    int runs = 0;
    const auto result = transactionRun<int>([&](CoreSession& session) {
        ++runs;
        auto doc = session.docOpen("d1");
        auto parent = doc->tabCreate("Parent");
        doc->tabCreate("Child", parent.tabId);
        return 0;
    }, options);
    CHECK(runs == 2);
    CHECK(result.attempts == 1);
    CHECK(client.events == std::vector<std::string>{"Parent", "Child"});
    REQUIRE(result.verification.size() == 1);
    CHECK(result.verification[0].status == "match");
}

TEST_CASE("transaction readback failure preserves landed receipts instead of returning success") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0", .blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {"before"}})
    }}}}), 1);
    struct Client : TransactionClient {
        int reads = 0;
        slimr::Json getDocument(const std::string& id) {
            if (++reads == 2) throw std::runtime_error("authoritative readback unavailable");
            return TransactionClient::getDocument(id);
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    bool failed = false;
    try {
        transactionRun<int>([](CoreSession& session) {
            session.docOpen("d1")->tab().writeMarkdown("landed");
            return 0;
        }, options);
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.phase == "verification");
        CHECK(error.partial.landed == std::vector<std::string>{"d1"});
        CHECK(error.partial.unknown.empty());
        REQUIRE(!error.phases.empty());
        CHECK(error.phases.back().docs.back().outcome == "confirmed");
    }
    CHECK(failed);
}

TEST_CASE("transaction creation subphase rejection retains prior confirmed tabs") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", emulator::blank_doc_template("Source", "d1"), 1);
    struct Client : TransactionClient {
        int writes = 0;
        slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            if (++writes == 2) throw std::runtime_error("requests[0]: synthetic rejected tab");
            return TransactionClient::batchUpdate(id, requests, revision);
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    int runs = 0;
    bool failed = false;
    try {
        transactionRun<int>([&](CoreSession& session) {
            ++runs;
            auto doc = session.docOpen("d1");
            doc->tabCreate("First");
            doc->tabCreate("Second");
            return 0;
        }, options);
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.partial.landed == std::vector<std::string>{"d1"});
        CHECK(error.partial.unknown.empty());
        REQUIRE(error.phases.size() == 1);
        REQUIRE(error.phases[0].docs.size() == 2);
        CHECK(error.phases[0].docs[0].requestCount == 1);
        CHECK(error.phases[0].docs[0].outcome == "confirmed");
        CHECK(error.phases[0].docs[1].outcome == "notSent");
    }
    CHECK(failed);
    CHECK(runs == 1);
    CHECK(client.writes == 2);
}

TEST_CASE("transaction create replan refuses a revision changed since creation") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", emulator::blank_doc_template("Source", "d1"), 1);
    struct Client : TransactionClient {
        int reads = 0;
        int writes = 0;
        slimr::Json getDocument(const std::string& id) {
            auto json = TransactionClient::getDocument(id);
            if (++reads == 2) json["revisionId"] = "concurrent-revision";
            return json;
        }
        slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            ++writes;
            return TransactionClient::batchUpdate(id, requests, revision);
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    bool failed = false;
    try {
        transactionRun<int>([](CoreSession& session) {
            session.docOpen("d1")->tabCreate("Created once");
            return 0;
        }, options);
    } catch (const TransactionError& error) {
        failed = true;
        CHECK(error.phase == "planning");
        CHECK(error.partial.landed == std::vector<std::string>{"d1"});
        CHECK(error.partial.unknown.empty());
    }
    CHECK(failed);
    CHECK(client.writes == 1);
}

TEST_CASE("footnote verification resolves segment and reference IDs and still compares body text") {
    auto json = docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0", .blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {ParagraphAtomSpec{.type = "footnoteRef"}}})
    }}}});
    json["revisionId"] = "r1";
    auto& dt = json["tabs"][0]["documentTab"];
    dt["body"]["content"][1]["paragraph"]["elements"][0]["footnoteReference"]["footnoteId"] = "fn.real";
    dt["footnotes"] = slimr::Json::object_t{
        {"fn.real", slimr::Json::object_t{{"content", slimr::Json::array_t{
            paragraphJsonBuild(ParagraphSpec{.content = {"note body"}}, 0).element
        }}}}
    };
    model::KeyAllocator keys;
    PlannedDoc doc;
    doc.docId = "d1";
    doc.input.final = model::docModelParse(json, {.docId = "d1", .keys = keys});
    REQUIRE(doc.input.final.tabs[0].segments.size() == 1);
    doc.input.final.tabs[0].segments[0].id = "new:fn:n1";
    auto& atom = doc.input.final.tabs[0].blocks[0].as_paragraph().inlines[0];
    (*atom.raw)["footnoteReference"]["footnoteId"] = "new:fn:n1";
    VerifyResolvedOptions resolved{.footnoteIds = {{"new:fn:n1", "fn.real"}}};
    CHECK(verify(doc, json, "r1", resolved).status == "match");
    CHECK(verify(doc, json, "r1", {}).status == "mismatch");
    doc.input.final.tabs[0].segments[0].tab->blocks[0].as_paragraph().inlines[0].text = "wrong body";
    CHECK(verify(doc, json, "r1", resolved).status == "mismatch");
}

TEST_CASE("transaction edits existing footnote content with detached originals and authoritative readback") {
    auto json = docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0", .blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {ParagraphAtomSpec{.type = "footnoteRef"}}})
    }}}});
    json["tabs"][0]["documentTab"]["body"]["content"][1]["paragraph"]["elements"][0]
        ["footnoteReference"]["footnoteId"] = "fn.1";
    json["tabs"][0]["documentTab"]["body"]["content"][1]["paragraph"]["elements"][0]
        ["footnoteReference"]["textStyle"] = slimr::Json::object_t{};
    json["tabs"][0]["documentTab"]["footnotes"] = slimr::Json::object_t{
        {"fn.1", slimr::Json::object_t{{"content", slimr::Json::array_t{
            paragraphJsonBuild(ParagraphSpec{.content = {"original note"}}, 0).element
        }}}}
    };
    emulator::FakeGoogle backend;
    backend.seed_document("d1", json, 1);
    TransactionClient client{backend};
    TransactionOptions<TransactionClient> options{.client = &client};
    const auto result = transactionRun<int>([](CoreSession& session) {
        auto handle = session.docOpen("d1");
        auto& tab = handle->tab().model();
        REQUIRE(tab.segments.size() == 1);
        tab.segments[0].tab->blocks[0].as_paragraph().inlines[0].text = "updated note";
        CHECK(session.docs.at("d1").original.tabs[0].segments[0].tab->blocks[0]
            .as_paragraph().inlines[0].text == "original note");
        return 0;
    }, options);
    REQUIRE(result.verification.size() == 1);
    CHECK(result.verification[0].status == "match");
    CHECK(result.documents.contains("d1"));
}

TEST_CASE("transaction Markdown range-removal intent reaches the guard without blessing unmanaged loss") {
    auto json = docJsonBuild(DocSpec{.tabs = {TabSpec{.tabId = "t.0", .blocks = {
        BlockSpec::make_paragraph(ParagraphSpec{.content = {"hello world"}})
    }}}});
    json["tabs"][0]["documentTab"]["namedRanges"] = slimr::Json::object_t{
        {"selection", slimr::Json::object_t{{"namedRanges", slimr::Json::array_t{
            slimr::Json::object_t{{"name", "selection"}, {"namedRangeId", "range.original"},
                {"ranges", slimr::Json::array_t{slimr::Json::object_t{
                    {"startIndex", 7}, {"endIndex", 12}}}}}
        }}}}
    };
    auto blockedRange = [](const Plan& plan) {
        return std::any_of(plan.findings.begin(), plan.findings.end(), [](const auto& finding) {
            return finding.kind == "namedRange" && finding.severity == "block" && !finding.waived;
        });
    };
    CoreSession unchanged;
    unchanged.getDocumentFn = [json](const std::string&) { return json; };
    auto noOpTab = unchanged.docOpen("d1")->tab();
    const auto markdown = lens::markdownRender(lens::projectionBuild(noOpTab.model()));
    REQUIRE(markdown.find(":namedrange{name=selection start}") != std::string::npos);
    CHECK_FALSE(noOpTab.writeMarkdown(markdown).changed);
    CHECK(unchanged.docs.at("d1").managedNamedRangeIds.empty());
    CHECK_FALSE(blockedRange(planBuild(unchanged)));

    CoreSession unmanaged;
    unmanaged.getDocumentFn = [json](const std::string&) { return json; };
    unmanaged.docOpen("d1")->tab().model().namedRanges.clear();
    CHECK(blockedRange(planBuild(unmanaged)));

    noOpTab.model().blocks[0].as_paragraph().inlines[0].text = "hello";
    CHECK(blockedRange(planBuild(unchanged)));

    CoreSession retained;
    retained.getDocumentFn = [json](const std::string&) { return json; };
    auto retainedTab = retained.docOpen("d1")->tab();
    CHECK(retainedTab.writeMarkdown("X " + markdown).changed);
    CHECK(retained.docs.at("d1").managedNamedRangeIds == std::set<std::string>{"range.original"});
    REQUIRE(retainedTab.model().namedRanges.size() == 1);
    CHECK(retainedTab.model().namedRanges[0].namedRangeId == "range.original");
    retainedTab.model().blocks[0].as_paragraph().inlines = {model::Inline::from_text("X hello xxxxx")};
    CHECK(blockedRange(planBuild(retained)));

    emulator::FakeGoogle backend;
    backend.seed_document("d1", json, 1);
    TransactionClient client{backend};
    TransactionOptions<TransactionClient> options{.client = &client};
    const auto result = transactionRun<int>([](CoreSession& session) {
        auto tab = session.docOpen("d1")->tab();
        const auto report = tab.writeMarkdown("hello world");
        CHECK(report.changed);
        CHECK(session.docs.at("d1").managedNamedRangeIds == std::set<std::string>{"range.original"});
        CHECK(tab.model().namedRanges.empty());
        return 0;
    }, options);
    CHECK_FALSE(result.refused);
    REQUIRE(result.verification.size() == 1);
    CHECK(result.verification[0].status == "match");
}

TEST_CASE("transaction creates a footnote reference before its revision-locked body and verifies resolved IDs") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", emulator::blank_doc_template("Source", "d1"), 1);
    struct Client : TransactionClient {
        std::optional<std::string> revisionAfter;
        int writes = 0;
        slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                               const std::optional<std::string>& revision) {
            ++writes;
            REQUIRE(revision.has_value());
            if (revisionAfter) CHECK(revision == revisionAfter);
            auto response = TransactionClient::batchUpdate(id, requests, revision);
            revisionAfter = responseRevision(response);
            return response;
        }
    } client{{backend}};
    TransactionOptions<Client> options{.client = &client};
    const auto result = transactionRun<int>([](CoreSession& session) {
        auto& tab = session.docOpen("d1")->tab().model();
        model::Inline reference;
        reference.kind = "atom";
        reference.type = "footnoteRef";
        reference.key = session.keys.next("n");
        reference.create = AtomCreate{.type = "footnoteRef"};
        const auto id = "new:fn:" + reference.key;
        tab.blocks[0].as_paragraph().inlines.push_back(reference);
        auto original = model::segmentTabParse(tab.tabId, tab.title, id,
            reconcile::make_footnote_blank_content(),
            session.docs.at("d1").json["tabs"][0]["documentTab"], session.keys);
        auto desired = original;
        desired.blocks[0].as_paragraph().inlines = {model::Inline::from_text("new note body")};
        tab.segments.push_back(model::SegmentModel{
            .id = id, .kind = "footnote",
            .original = std::make_shared<model::TabModel>(std::move(original)),
            .tab = std::make_shared<model::TabModel>(std::move(desired)),
        });
        return 0;
    }, options);
    CHECK_FALSE(result.refused);
    CHECK(client.writes == 2);
    REQUIRE(result.verification.size() == 1);
    CHECK(result.verification[0].status == "match");
    const auto phase = std::find_if(result.phases.begin(), result.phases.end(),
        [](const auto& report) { return report.phase == "footnotes"; });
    REQUIRE(phase != result.phases.end());
    REQUIRE(phase->docs.size() == 1);
    CHECK(phase->docs[0].outcome == "confirmed");
    model::KeyAllocator keys;
    const auto actual = model::docModelParse(result.documents.at("d1"), {.docId = "d1", .keys = keys});
    REQUIRE(actual.tabs[0].segments.size() == 1);
    CHECK(!actual.tabs[0].segments[0].id.starts_with("new:"));
    CHECK(actual.tabs[0].segments[0].tab->blocks[0].as_paragraph().inlines[0].text == "new note body");
}

TEST_CASE("transaction resolves a new heading link after content and restores newline chrome") {
    emulator::FakeGoogle backend;
    backend.seed_document("d1", emulator::blank_doc_template("Source", "d1"), 1);
    TransactionClient client{backend};
    TransactionOptions<TransactionClient> options{.client = &client};
    const auto result = transactionRun<int>([](CoreSession& session) {
        auto tab = session.docOpen("d1")->tab();
        tab.writeMarkdown("# New heading\n\nsee");
        auto& blocks = tab.model().blocks;
        REQUIRE(blocks.size() == 2);
        blocks[1].as_paragraph().inlines[0].style["link"] =
            slimr::Json::object_t{{"heading", slimr::Json::object_t{
                {"key", blocks[0].key}, {"tabId", tab.tabId}}}};
        return 0;
    }, options);
    CHECK_FALSE(result.refused);
    REQUIRE(result.verification.size() == 1);
    CHECK(result.verification[0].status == "match");
    CHECK(!result.newIds.headings.empty());
    const auto phase = std::find_if(result.phases.begin(), result.phases.end(),
        [](const auto& report) { return report.phase == "links"; });
    REQUIRE(phase != result.phases.end());
    REQUIRE(phase->docs.size() == 1);
    CHECK(phase->docs[0].requestCount == 2);
    CHECK(phase->docs[0].outcome == "confirmed");
}

TEST_CASE("transaction revision conflicts never replay and retain earlier document successes") {
    for (const std::string failId : {"a", "b"}) {
        emulator::FakeGoogle backend;
        backend.seed_document("a", emulator::blank_doc_template("A", "a"), 1);
        backend.seed_document("b", emulator::blank_doc_template("B", "b"), 1);
        struct Client : TransactionClient {
            std::string failId;
            int writes = 0;
            slimr::Json batchUpdate(const std::string& id, const std::vector<slimr::Json>& requests,
                                   const std::optional<std::string>& revision) {
                ++writes;
                CHECK(revision.has_value());
                if (id == failId) throw std::runtime_error("required revision does not match the latest revision");
                return TransactionClient::batchUpdate(id, requests, revision);
            }
        } client{{backend}, failId};
        TransactionOptions<Client> options{.client = &client};
        int runs = 0;
        bool failed = false;
        try {
            transactionRun<int>([&](CoreSession& session) {
                ++runs;
                session.docOpen("a")->tab().writeMarkdown("updated A");
                session.docOpen("b")->tab().writeMarkdown("updated B");
                return 0;
            }, options);
        } catch (const TransactionError& error) {
            failed = true;
            CHECK(failId == "b");
            CHECK(error.partial.landed == std::vector<std::string>{"a"});
            CHECK(error.partial.unknown.empty());
            REQUIRE(!error.phases.empty());
            CHECK(error.phases.back().docs.back().outcome == "notSent");
        } catch (const model::CoreError& error) {
            failed = true;
            CHECK(failId == "a");
            CHECK(error.code == "staleBase");
        }
        CHECK(failed);
        CHECK(runs == 1);
        CHECK(client.writes == (failId == "a" ? 1 : 2));
    }
}

} // namespace gdocsmith::engine::test
