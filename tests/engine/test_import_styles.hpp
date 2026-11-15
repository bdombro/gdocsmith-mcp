#pragma once

// Requires engine/test_import.hpp (RemoteImportFixture) to be included first.
#include "engine/import.hpp"
#include "test.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace gdocsmith::engine::test {

namespace import_styles {

struct CharStyle {
    int index = 0;
    char ch = '\0';
    slimr::Json explicitStyle = slimr::Json::object_t{};
    std::optional<double> explicitSize;
    std::optional<std::string> explicitFamily;
    std::optional<double> effSize;
    std::optional<std::string> effFamily;
    std::string sizeTier;
    std::string familyTier;
};

struct Segment {
    int start = 0;
    int end = 0;
    slimr::Json textStyle = slimr::Json::object_t{};
};

inline std::optional<double> sizeOf(const slimr::Json& style) {
    if (!style.is_object() || !style.contains("fontSize")) return std::nullopt;
    const auto& size = style["fontSize"];
    if (!size.is_object() || !size.contains("magnitude")) return std::nullopt;
    return size["magnitude"].as_number();
}

// Family and weight combined, e.g. "Arial/400".
inline std::optional<std::string> familyOf(const slimr::Json& style) {
    if (!style.is_object() || !style.contains("weightedFontFamily")) return std::nullopt;
    const auto& family = style["weightedFontFamily"];
    if (!family.is_object() || !family.contains("fontFamily")) return std::nullopt;
    const auto weight = family.contains("weight") ? static_cast<int>(family["weight"].as_number()) : 400;
    return family["fontFamily"].get<std::string>() + "/" + std::to_string(weight);
}

inline const slimr::Json* namedStyle(const slimr::Json& doc, const std::string& type) {
    const auto& tab = doc["tabs"][0]["documentTab"];
    if (!tab.contains("namedStyles") || !tab["namedStyles"].contains("styles")) return nullptr;
    for (const auto& style : tab["namedStyles"]["styles"].get<slimr::Json::array_t>()) {
        if (style.contains("namedStyleType") && style["namedStyleType"].get<std::string>() == type) return &style;
    }
    return nullptr;
}

inline std::string paragraphText(const slimr::Json& paragraph) {
    std::string text;
    for (const auto& element : paragraph["elements"].get<slimr::Json::array_t>()) {
        if (element.contains("textRun")) text += element["textRun"]["content"].get<std::string>();
    }
    return text;
}

// Position of the edited paragraph in tabs[0] body content.
inline std::size_t paragraphPosition(const slimr::Json& doc) {
    const auto& content = doc["tabs"][0]["documentTab"]["body"]["content"].get<slimr::Json::array_t>();
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (content[i].contains("paragraph") && paragraphText(content[i]["paragraph"]).find("Hello") != std::string::npos) return i;
    }
    FAIL("no stored paragraph contains Hello");
    throw std::runtime_error("no stored paragraph contains Hello");
}

inline const slimr::Json& storedParagraph(const slimr::Json& doc) {
    return doc["tabs"][0]["documentTab"]["body"]["content"][paragraphPosition(doc)]["paragraph"];
}

inline slimr::Json& storedParagraph(slimr::Json& doc) {
    const auto position = paragraphPosition(doc);
    return doc["tabs"][0]["documentTab"]["body"]["content"][position]["paragraph"];
}

// Effective style: explicit run value, then the paragraph's namedStyleType style (skipped when
// paragraphStyle has no namedStyleType), then the stored NORMAL_TEXT style. Never defaults to 11.
inline std::vector<CharStyle> perIndex(const slimr::Json& doc) {
    const auto& paragraph = storedParagraph(doc);
    const slimr::Json* paragraphNamed = nullptr;
    std::string paragraphType;
    if (paragraph.contains("paragraphStyle") && paragraph["paragraphStyle"].contains("namedStyleType")) {
        paragraphType = paragraph["paragraphStyle"]["namedStyleType"].get<std::string>();
        paragraphNamed = namedStyle(doc, paragraphType);
    }
    const auto* normal = namedStyle(doc, "NORMAL_TEXT");
    const auto namedText = [](const slimr::Json* style) {
        return style && style->contains("textStyle") ? (*style)["textStyle"] : slimr::Json(slimr::Json::object_t{});
    };

    std::vector<CharStyle> out;
    for (const auto& element : paragraph["elements"].get<slimr::Json::array_t>()) {
        if (!element.contains("textRun")) continue;
        const auto& run = element["textRun"];
        const auto style = run.contains("textStyle") ? run["textStyle"] : slimr::Json(slimr::Json::object_t{});
        const auto content = run["content"].get<std::string>();
        const auto start = static_cast<int>(element["startIndex"].as_number());
        for (std::size_t offset = 0; offset < content.size(); ++offset) {
            CharStyle c{.index = start + static_cast<int>(offset), .ch = content[offset], .explicitStyle = style};
            c.explicitSize = sizeOf(style);
            c.explicitFamily = familyOf(style);
            c.sizeTier = "unresolved";
            c.familyTier = "unresolved";
            if (c.explicitSize) { c.effSize = c.explicitSize; c.sizeTier = "run"; }
            else if (paragraphNamed && sizeOf(namedText(paragraphNamed))) { c.effSize = sizeOf(namedText(paragraphNamed)); c.sizeTier = "named:" + paragraphType; }
            else if (normal && sizeOf(namedText(normal))) { c.effSize = sizeOf(namedText(normal)); c.sizeTier = "NORMAL_TEXT"; }
            if (c.explicitFamily) { c.effFamily = c.explicitFamily; c.familyTier = "run"; }
            else if (paragraphNamed && familyOf(namedText(paragraphNamed))) { c.effFamily = familyOf(namedText(paragraphNamed)); c.familyTier = "named:" + paragraphType; }
            else if (normal && familyOf(namedText(normal))) { c.effFamily = familyOf(namedText(normal)); c.familyTier = "NORMAL_TEXT"; }
            out.push_back(std::move(c));
        }
    }
    return out;
}

inline std::vector<Segment> segments(const slimr::Json& doc) {
    std::vector<Segment> out;
    for (const auto& element : storedParagraph(doc)["elements"].get<slimr::Json::array_t>()) {
        if (!element.contains("textRun")) continue;
        const auto& run = element["textRun"];
        out.push_back(Segment{
            .start = static_cast<int>(element["startIndex"].as_number()),
            .end = static_cast<int>(element["endIndex"].as_number()),
            .textStyle = run.contains("textStyle") ? run["textStyle"] : slimr::Json(slimr::Json::object_t{}),
        });
    }
    return out;
}

inline std::string optText(const std::optional<double>& value) { return value ? std::to_string(static_cast<int>(*value)) : "-"; }
inline std::string optText(const std::optional<std::string>& value) { return value.value_or("-"); }

inline std::string dump(const std::vector<CharStyle>& chars) {
    std::ostringstream out;
    for (const auto& c : chars) {
        out << c.index << " '" << (c.ch == '\n' ? std::string("\\n") : std::string(1, c.ch)) << "' explicit="
            << optText(c.explicitSize) << "," << optText(c.explicitFamily) << " effective=" << optText(c.effSize) << "("
            << c.sizeTier << ")," << optText(c.effFamily) << "(" << c.familyTier << ") style=" << slimr::write_json(c.explicitStyle) << "\n";
    }
    return out.str();
}

inline std::string dump(const std::vector<Segment>& segs) {
    std::ostringstream out;
    for (const auto& s : segs) out << "[" << s.start << "," << s.end << ") " << slimr::write_json(s.textStyle) << "\n";
    return out.str();
}

inline bool sameStyle(const CharStyle& a, const CharStyle& b) {
    return slimr::write_json(a.explicitStyle) == slimr::write_json(b.explicitStyle) && a.effSize == b.effSize && a.effFamily == b.effFamily;
}

// A1: every character of the edited text resolves to the requested size and Arial/400.
inline bool a1(const slimr::Json& post, int pt) {
    const auto chars = perIndex(post);
    std::string text;
    for (const auto& c : chars) text += c.ch;
    const auto at = text.find("Hello revised world");
    if (at == std::string::npos) return false;
    for (std::size_t i = at; i < at + std::string("Hello revised world").size(); ++i) {
        if (chars[i].effSize != std::optional<double>{static_cast<double>(pt)}) return false;
        if (chars[i].effFamily != std::optional<std::string>{"Arial/400"}) return false;
    }
    return true;
}

// A2: the paragraph newline keeps its explicit and effective style.
inline bool a2(const slimr::Json& pre, const slimr::Json& post) {
    const auto before = perIndex(pre);
    const auto after = perIndex(post);
    if (before.empty() || after.empty() || before.back().ch != '\n' || after.back().ch != '\n') return false;
    return sameStyle(before.back(), after.back());
}

// A3: surviving "Hello " and "world" keep their style; mapped by text offset since the insert shifts indices.
inline bool a3(const slimr::Json& pre, const slimr::Json& post) {
    const auto before = perIndex(pre);
    const auto after = perIndex(post);
    std::string beforeText;
    std::string afterText;
    for (const auto& c : before) beforeText += c.ch;
    for (const auto& c : after) afterText += c.ch;
    for (const std::string piece : {"Hello ", "world"}) {
        const auto from = beforeText.find(piece);
        const auto to = afterText.find(piece);
        if (from == std::string::npos || to == std::string::npos) return false;
        for (std::size_t i = 0; i < piece.size(); ++i) {
            if (!sameStyle(before[from + i], after[to + i])) return false;
        }
    }
    return true;
}

inline void omitFonts(slimr::Json& json) {
    for (auto& tab : json["tabs"].get<slimr::Json::array_t>()) {
        for (auto& element : tab["documentTab"]["body"]["content"].get<slimr::Json::array_t>()) {
            if (!element.contains("paragraph")) continue;
            for (auto& inl : element["paragraph"]["elements"].get<slimr::Json::array_t>()) {
                if (!inl.contains("textRun") || !inl["textRun"].contains("textStyle")) continue;
                auto& style = inl["textRun"]["textStyle"].get<slimr::Json::object_t>();
                style.erase("fontSize");
                style.erase("weightedFontFamily");
            }
        }
    }
}

// Replaces the constructor's blank doc with the font-readback-v1 variant (file seed, not inlined).
inline void seedFont(RemoteImportFixture& f, int pt) {
    std::optional<std::string> text;
    for (const std::string prefix : {"", "../", "../../"}) {
        std::ifstream input(prefix + "tests/parity/seeds/font-readback-v1.json");
        if (!input.is_open()) continue;
        std::ostringstream buffer;
        buffer << input.rdbuf();
        text = buffer.str();
        break;
    }
    if (!text) {
        FAIL("cannot find tests/parity/seeds/font-readback-v1.json");
        return;
    }
    auto parsed = slimr::parse_json<slimr::Json>(*text);
    REQUIRE(parsed);
    auto json = (*parsed)["variants"][std::to_string(pt)];
    json["documentId"] = "synthetic_doc";
    f.backend.seed_document("synthetic_doc", json, 1);
}

struct Run {
    std::optional<slimr::Json> post;
    int captures = 0;
    slimr::Json pre;
    FileWriteResult result;
    // Saved services live here so the wrappers stay valid for A4 after observe() returns.
    decltype(ImportServices::batchUpdate) send;
    decltype(ImportServices::getDocument) read;
};

inline std::vector<int> normalFollowIndices(const slimr::Json& doc) {
    auto probe = doc;
    for (auto& style : probe["tabs"][0]["documentTab"]["namedStyles"]["styles"].get<slimr::Json::array_t>()) {
        if (style.contains("namedStyleType") && style["namedStyleType"].get<std::string>() == "NORMAL_TEXT")
            style["textStyle"]["fontSize"] = slimr::Json::object_t{{"magnitude", 13}, {"unit", "PT"}};
    }
    std::vector<int> out;
    for (const auto& c : perIndex(probe)) {
        if (c.effSize == std::optional<double>{13.0}) out.push_back(c.index);
    }
    return out;
}

inline std::vector<slimr::Json> updateTextStyleRequests(const RemoteImportFixture& fixture) {
    std::vector<slimr::Json> out;
    for (const auto& batch : fixture.sentRequests) {
        for (const auto& request : batch) {
            if (request.contains("updateTextStyle")) out.push_back(request["updateTextStyle"]);
        }
    }
    return out;
}

// Shared flow: seed, edit, import with storage capture after each batch and a readback omission
// that mirrors the parity adapter (fonts dropped from getDocument copies once a batch was sent).
inline void observe(RemoteImportFixture& fixture, int pt, Run& run) {
    seedFont(fixture, pt);
    run.pre = fixture.backend.get_doc("synthetic_doc").json;
    MESSAGE("O2/O3 pre " << pt << "pt:\n" << dump(perIndex(run.pre)));

    auto file = fixture.file();
    const auto at = file.tabs[0].markdown.find("Hello world");
    CHECK(at != std::string::npos);
    if (at != std::string::npos) file.tabs[0].markdown.replace(at, std::string("Hello world").size(), "Hello revised world");

    // Capture authoritative storage right after the write lands; beforeBatch runs before it.
    run.send = fixture.remote.batchUpdate;
    fixture.remote.batchUpdate = [&fixture, &run](const std::string& id, const std::vector<slimr::Json>& requests,
                                      const std::optional<std::string>& revision) {
        auto response = run.send(id, requests, revision);
        run.post = fixture.backend.get_doc("synthetic_doc").json;
        ++run.captures;
        return response;
    };
    run.read = fixture.remote.getDocument;
    fixture.remote.getDocument = [&fixture, &run](const std::string& id) {
        auto json = run.read(id);
        if (fixture.batches > 0) omitFonts(json);
        return json;
    };

    run.result = fileImport(fixture.save(file), fixture.remote);
    INFO(run.result.summary);
    REQUIRE(fixture.batches >= 1);
    REQUIRE(run.post.has_value());
    MESSAGE("captures=" << run.captures);

    for (const auto& batch : fixture.sentRequests) {
        for (const auto& request : batch) {
            if (request.contains("updateTextStyle")) MESSAGE("O1 updateTextStyle: " << slimr::write_json(request["updateTextStyle"]));
        }
    }
    MESSAGE("O2/O3 post " << pt << "pt:\n" << dump(perIndex(*run.post)));
    MESSAGE("O4 segments " << pt << "pt:\n" << dump(segments(*run.post)));

    // O5: indices that follow a changed NORMAL_TEXT size have no pinned run size.
    auto probe = *run.post;
    for (auto& style : probe["tabs"][0]["documentTab"]["namedStyles"]["styles"].get<slimr::Json::array_t>()) {
        if (style.contains("namedStyleType") && style["namedStyleType"].get<std::string>() == "NORMAL_TEXT")
            style["textStyle"]["fontSize"] = slimr::Json::object_t{{"magnitude", 13}, {"unit", "PT"}};
    }
    std::ostringstream follows;
    for (const auto& c : perIndex(probe)) {
        if (c.effSize == std::optional<double>{13.0}) follows << c.index << " ";
    }
    MESSAGE("O5 indices following NORMAL_TEXT 13pt (" << pt << "pt): " << follows.str());
}

} // namespace import_styles

TEST_CASE("import styles - 11pt stored font before readback omission") {
    using namespace import_styles;
    RemoteImportFixture fixture;
    Run run;
    observe(fixture, 11, run);
    const auto& result = run.result;

    CHECK_FALSE(result.errors.has_value());
    CHECK(result.verification == "match");
    CHECK(result.status == "applied");

    CHECK(a1(*run.post, 11));
    CHECK(a2(run.pre, *run.post));
    CHECK(a3(run.pre, *run.post));

    // O1: outgoing updateTextStyle on [7, 15) omits fontSize and weightedFontFamily.
    const auto updates = updateTextStyleRequests(fixture);
    REQUIRE(updates.size() == 1);
    CHECK(static_cast<int>(updates[0]["range"]["startIndex"].as_number()) == 7);
    CHECK(static_cast<int>(updates[0]["range"]["endIndex"].as_number()) == 15);
    CHECK(slimr::write_json(updates[0]["textStyle"]) == "{}");

    // O2: inserted "revised " (indices 7..14) inherits 11pt and Arial/400 from NORMAL_TEXT.
    const auto chars = perIndex(*run.post);
    REQUIRE(chars.size() == 20);
    for (int idx = 6; idx < 14; ++idx) {
        CHECK_FALSE(chars[idx].explicitSize.has_value());
        CHECK_FALSE(chars[idx].explicitFamily.has_value());
        CHECK(chars[idx].sizeTier == "NORMAL_TEXT");
        CHECK(chars[idx].familyTier == "NORMAL_TEXT");
    }

    // O4: 3 body runs [1,7), [7,15), [15,20) plus newline [20,21).
    const auto segs = segments(*run.post);
    REQUIRE(segs.size() == 4);
    CHECK(segs[0].start == 1);
    CHECK(segs[0].end == 7);
    CHECK(segs[1].start == 7);
    CHECK(segs[1].end == 15);
    CHECK(slimr::write_json(segs[1].textStyle) == "{}");
    CHECK(segs[2].start == 15);
    CHECK(segs[2].end == 20);
    CHECK(segs[3].start == 20);
    CHECK(segs[3].end == 21);

    // O5: inserted "revised " (7..14) and newline (20) follow NORMAL_TEXT 13pt.
    CHECK(normalFollowIndices(*run.post) == std::vector<int>{7, 8, 9, 10, 11, 12, 13, 14, 20});

    // N2: styling the stored newline must trip A2.
    auto styled = *run.post;
    auto& elements = storedParagraph(styled)["elements"].get<slimr::Json::array_t>();
    REQUIRE_FALSE(elements.empty());
    elements.back()["textRun"]["textStyle"]["bold"] = true;
    CHECK_FALSE(a2(run.pre, styled));
    CHECK(a2(run.pre, slimr::Json(*run.post)));
}

TEST_CASE("import styles - 22pt stored font before readback omission") {
    using namespace import_styles;
    RemoteImportFixture fixture;
    Run run;
    observe(fixture, 22, run);
    const auto& result = run.result;

    CHECK(result.verification == "mismatch");
    CHECK(result.status == "failed");
    CHECK(result.summary.starts_with("Every write landed, but the readback did not match the file"));

    CHECK(a1(*run.post, 22));
    CHECK(a2(run.pre, *run.post));
    CHECK(a3(run.pre, *run.post));

    // O1: outgoing updateTextStyle on [7, 15) sets fontSize=22pt without weightedFontFamily.
    const auto updates = updateTextStyleRequests(fixture);
    REQUIRE(updates.size() == 1);
    CHECK(static_cast<int>(updates[0]["range"]["startIndex"].as_number()) == 7);
    CHECK(static_cast<int>(updates[0]["range"]["endIndex"].as_number()) == 15);
    CHECK(sizeOf(updates[0]["textStyle"]) == std::optional<double>{22.0});
    CHECK_FALSE(familyOf(updates[0]["textStyle"]).has_value());

    // O2: inserted "revised " (indices 7..14) has explicit 22pt and inherits Arial/400 from NORMAL_TEXT.
    const auto chars = perIndex(*run.post);
    REQUIRE(chars.size() == 20);
    for (int idx = 6; idx < 14; ++idx) {
        CHECK(chars[idx].explicitSize == std::optional<double>{22.0});
        CHECK(chars[idx].sizeTier == "run");
        CHECK_FALSE(chars[idx].explicitFamily.has_value());
        CHECK(chars[idx].familyTier == "NORMAL_TEXT");
    }

    // O4: 3 body runs [1,7), [7,15), [15,20) plus newline [20,21).
    const auto segs = segments(*run.post);
    REQUIRE(segs.size() == 4);
    CHECK(segs[0].start == 1);
    CHECK(segs[0].end == 7);
    CHECK(segs[1].start == 7);
    CHECK(segs[1].end == 15);
    CHECK(sizeOf(segs[1].textStyle) == std::optional<double>{22.0});
    CHECK_FALSE(familyOf(segs[1].textStyle).has_value());
    CHECK(segs[2].start == 15);
    CHECK(segs[2].end == 20);
    CHECK(segs[3].start == 20);
    CHECK(segs[3].end == 21);

    // O5: only newline (20) follows NORMAL_TEXT 13pt because body runs have 22pt.
    CHECK(normalFollowIndices(*run.post) == std::vector<int>{20});

    // A4 + N3: the omitting readback drops fonts (fault reached) without touching storage.
    const auto readback = fixture.remote.getDocument("synthetic_doc");
    CHECK_FALSE(a1(readback, 22));
    const auto stored = fixture.backend.get_doc("synthetic_doc").json;
    CHECK(dump(perIndex(stored)) == dump(perIndex(*run.post)));
    CHECK(a1(stored, 22) == a1(*run.post, 22));
    CHECK(a2(run.pre, stored) == a2(run.pre, *run.post));
    CHECK(a3(run.pre, stored) == a3(run.pre, *run.post));

    // N1: one "Hello " character without a run size must trip A1/A3.
    auto cleared = *run.post;
    auto& elements = storedParagraph(cleared)["elements"].get<slimr::Json::array_t>();
    auto it = elements.begin();
    while (it != elements.end() && !(it->contains("textRun") && (*it)["textRun"]["content"].get<std::string>().starts_with("H"))) ++it;
    REQUIRE(it != elements.end());
    const auto content = (*it)["textRun"]["content"].get<std::string>();
    const auto start = static_cast<int>((*it)["startIndex"].as_number());
    auto head = *it;
    head["textRun"]["textStyle"].get<slimr::Json::object_t>().erase("fontSize");
    if (content.size() > 1) {
        head["textRun"]["content"] = content.substr(0, 1);
        head["endIndex"] = start + 1;
        auto tail = *it;
        tail["textRun"]["content"] = content.substr(1);
        tail["startIndex"] = start + 1;
        *it = head;
        elements.insert(it + 1, tail);
    } else {
        *it = head;
    }
    CHECK_FALSE((a1(cleared, 22) && a3(run.pre, cleared)));
    CHECK((a1(*run.post, 22) && a3(run.pre, *run.post)));
}

} // namespace gdocsmith::engine::test
