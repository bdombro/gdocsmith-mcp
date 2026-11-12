#pragma once

#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gdocsmith::model {

inline auto rngCreate(uint32_t seed) {
    uint32_t state = seed;
    return [state]() mutable -> double {
        state = (state + 0x6d2b79f5u);
        uint32_t t = state;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return static_cast<double>((t ^ (t >> 14))) / 4294967296.0;
    };
}

struct ParagraphTextSpec {
    std::string text;
    std::optional<JsonObject> style;
    std::optional<std::vector<std::string>> sugDel;
    std::optional<std::vector<std::string>> sugIns;
    std::optional<JsonObject> sugStyle;
};

struct ParagraphAtomSpec {
    AtomType type;
    std::optional<int> atomLen;
};

using ParagraphContentSpec = std::variant<std::string, ParagraphTextSpec, ParagraphAtomSpec>;

struct ParagraphSpec {
    std::vector<ParagraphContentSpec> content;
    struct Bullet {
        std::string listId;
        int nestingLevel = 0;
    };
    std::optional<Bullet> bullet;
    std::optional<std::string> headingId;
    std::optional<std::vector<std::string>> newlineSugIns;
    std::optional<JsonObject> style;
};

struct BlockSpec;

struct TableBlockSpec {
    std::vector<std::vector<std::vector<BlockSpec>>> cells; // [row][col][blocks]
};

struct SectionBreakBlockSpec {
    std::optional<JsonObject> sectionStyle;
};

struct TocBlockSpec {
    int length = 0;
};

struct BlockSpec {
    std::string kind = "paragraph";
    ParagraphSpec paragraph;
    TableBlockSpec table;
    SectionBreakBlockSpec sectionBreak;
    TocBlockSpec toc;

    static BlockSpec make_paragraph(ParagraphSpec p) {
        BlockSpec b;
        b.kind = "paragraph";
        b.paragraph = std::move(p);
        return b;
    }

    static BlockSpec make_section_break(std::optional<JsonObject> style = std::nullopt) {
        BlockSpec b;
        b.kind = "sectionBreak";
        b.sectionBreak.sectionStyle = style;
        return b;
    }

    static BlockSpec make_toc(int len) {
        BlockSpec b;
        b.kind = "toc";
        b.toc.length = len;
        return b;
    }

    static BlockSpec make_table(TableBlockSpec t) {
        BlockSpec b;
        b.kind = "table";
        b.table = std::move(t);
        return b;
    }
};

struct TabSpec {
    std::string tabId = "t.0";
    std::string title;
    std::optional<std::string> parentTabId;
    std::vector<BlockSpec> blocks;
    std::optional<JsonObject> documentStyle;
    std::optional<JsonObject> lists;
};

struct DocSpec {
    std::string documentId = "synthetic-doc";
    std::string title;
    std::string revisionId = "rev-1";
    std::vector<TabSpec> tabs;
};

inline JsonObject atomJsonBuild(std::string_view type) {
    static const std::unordered_map<std::string_view, std::string_view> VARIANTS = {
        {"autoText", "autoText"},
        {"columnBreak", "columnBreak"},
        {"date", "dateElement"},
        {"dropdown", "dropdown"},
        {"equation", "equation"},
        {"footnoteRef", "footnoteReference"},
        {"horizontalRule", "horizontalRule"},
        {"image", "inlineObjectElement"},
        {"pageBreak", "pageBreak"},
        {"person", "person"},
        {"richLink", "richLink"}
    };
    auto it = VARIANTS.find(type);
    std::string_view k = (it != VARIANTS.end()) ? it->second : "inlineObjectElement";
    slimr::Json::object_t obj;
    obj[std::string(k)] = slimr::Json::object_t{};
    JsonObject out;
    out.data = std::move(obj);
    return out;
}

struct BuiltBlock {
    JsonObject element;
    int end = 0;
};

BuiltBlock blockJsonBuild(const BlockSpec& block, int start);

inline BuiltBlock paragraphJsonBuild(const ParagraphSpec& spec, int start) {
    slimr::Json::array_t elements;
    int index = start;
    for (const auto& item : spec.content) {
        if (std::holds_alternative<std::string>(item)) {
            const auto& str = std::get<std::string>(item);
            if (str.empty()) continue;
            int end = index + static_cast<int>(str.size());
            elements.push_back(slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(index)},
                {"endIndex", static_cast<int64_t>(end)},
                {"textRun", slimr::Json::object_t{
                    {"content", str},
                    {"textStyle", slimr::Json::object_t{}}
                }}
            });
            index = end;
        } else if (std::holds_alternative<ParagraphTextSpec>(item)) {
            const auto& t = std::get<ParagraphTextSpec>(item);
            if (t.text.empty()) continue;
            int end = index + static_cast<int>(t.text.size());
            slimr::Json::object_t tr{
                {"content", t.text},
                {"textStyle", t.style.value_or(make_json_object())}
            };
            if (t.sugDel.has_value()) {
                slimr::Json::array_t arr;
                for (const auto& id : *t.sugDel) arr.push_back(id);
                tr["suggestedDeletionIds"] = arr;
            }
            if (t.sugIns.has_value()) {
                slimr::Json::array_t arr;
                for (const auto& id : *t.sugIns) arr.push_back(id);
                tr["suggestedInsertionIds"] = arr;
            }
            if (t.sugStyle.has_value()) {
                tr["suggestedTextStyleChanges"] = *t.sugStyle;
            }
            elements.push_back(slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(index)},
                {"endIndex", static_cast<int64_t>(end)},
                {"textRun", std::move(tr)}
            });
            index = end;
        } else {
            const auto& a = std::get<ParagraphAtomSpec>(item);
            int len = a.atomLen.value_or(a.type == "equation" ? 3 : 1);
            auto atomObj = atomJsonBuild(a.type);
            atomObj["startIndex"] = static_cast<int64_t>(index);
            atomObj["endIndex"] = static_cast<int64_t>(index + len);
            elements.push_back(atomObj);
            index += len;
        }
    }

    slimr::Json::object_t nlTr{
        {"content", "\n"},
        {"textStyle", slimr::Json::object_t{}}
    };
    if (spec.newlineSugIns.has_value()) {
        slimr::Json::array_t arr;
        for (const auto& id : *spec.newlineSugIns) arr.push_back(id);
        nlTr["suggestedInsertionIds"] = arr;
    }
    elements.push_back(slimr::Json::object_t{
        {"startIndex", static_cast<int64_t>(index)},
        {"endIndex", static_cast<int64_t>(index + 1)},
        {"textRun", std::move(nlTr)}
    });
    index += 1;

    JsonObject pStyle = spec.style.value_or(make_json_object());
    if (spec.headingId.has_value()) {
        pStyle["headingId"] = *spec.headingId;
    }

    slimr::Json::object_t pObj{
        {"elements", std::move(elements)},
        {"paragraphStyle", pStyle}
    };
    if (spec.bullet.has_value()) {
        pObj["bullet"] = slimr::Json::object_t{
            {"listId", spec.bullet->listId},
            {"nestingLevel", static_cast<int64_t>(spec.bullet->nestingLevel)}
        };
    }

    return BuiltBlock{
        .element = slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index)},
            {"paragraph", std::move(pObj)}
        },
        .end = index
    };
}

inline BuiltBlock tableJsonBuild(const TableBlockSpec& spec, int start) {
    int index = start + 1;
    slimr::Json::array_t tableRows;
    for (const auto& row : spec.cells) {
        int rowStart = index;
        index += 1;
        slimr::Json::array_t tableCells;
        for (const auto& cellBlocks : row) {
            int cellStart = index;
            index += 1;
            slimr::Json::array_t content;
            for (const auto& b : cellBlocks) {
                auto built = blockJsonBuild(b, index);
                content.push_back(built.element);
                index = built.end;
            }
            tableCells.push_back(slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(cellStart)},
                {"endIndex", static_cast<int64_t>(index)},
                {"tableCellStyle", slimr::Json::object_t{}},
                {"content", std::move(content)}
            });
        }
        tableRows.push_back(slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(rowStart)},
            {"endIndex", static_cast<int64_t>(index)},
            {"tableRowStyle", slimr::Json::object_t{}},
            {"tableCells", std::move(tableCells)}
        });
    }
    index += 1; // table end marker
    int cols = spec.cells.empty() ? 0 : static_cast<int>(spec.cells[0].size());
    int rows = static_cast<int>(spec.cells.size());
    return BuiltBlock{
        .element = slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index)},
            {"table", slimr::Json::object_t{
                {"columns", static_cast<int64_t>(cols)},
                {"rows", static_cast<int64_t>(rows)},
                {"tableStyle", slimr::Json::object_t{}},
                {"tableRows", std::move(tableRows)}
            }}
        },
        .end = index
    };
}

inline BuiltBlock blockJsonBuild(const BlockSpec& block, int start) {
    if (block.kind == "paragraph") return paragraphJsonBuild(block.paragraph, start);
    if (block.kind == "sectionBreak") {
        return BuiltBlock{
            .element = slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(start)},
                {"endIndex", static_cast<int64_t>(start + 1)},
                {"sectionBreak", slimr::Json::object_t{
                    {"sectionStyle", block.sectionBreak.sectionStyle.value_or(make_json_object())}
                }}
            },
            .end = start + 1
        };
    }
    if (block.kind == "toc") {
        return BuiltBlock{
            .element = slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(start)},
                {"endIndex", static_cast<int64_t>(start + block.toc.length)},
                {"tableOfContents", slimr::Json::object_t{{"content", slimr::Json::array_t{}}}}
            },
            .end = start + block.toc.length
        };
    }
    return tableJsonBuild(block.table, start);
}

inline JsonObject tabJsonBuild(const TabSpec& spec, std::string_view tabId) {
    slimr::Json::array_t content;
    content.push_back(slimr::Json::object_t{
        {"startIndex", static_cast<int64_t>(0)},
        {"endIndex", static_cast<int64_t>(1)},
        {"sectionBreak", slimr::Json::object_t{{"sectionStyle", slimr::Json::object_t{}}}}
    });
    int index = 1;
    for (const auto& b : spec.blocks) {
        auto built = blockJsonBuild(b, index);
        content.push_back(built.element);
        index = built.end;
    }

    slimr::Json::object_t docTab{
        {"body", slimr::Json::object_t{{"content", std::move(content)}}},
        {"lists", spec.lists.value_or(make_json_object())},
        {"namedStyles", slimr::Json::object_t{{"styles", slimr::Json::array_t{}}}}
    };
    if (spec.documentStyle.has_value()) {
        docTab["documentStyle"] = *spec.documentStyle;
    }

    slimr::Json::object_t tabProps{
        {"tabId", std::string(tabId)},
        {"title", spec.title}
    };
    if (spec.parentTabId.has_value()) {
        tabProps["parentTabId"] = *spec.parentTabId;
    }

    return slimr::Json::object_t{
        {"documentTab", std::move(docTab)},
        {"tabProperties", std::move(tabProps)}
    };
}

inline JsonObject docJsonBuild(const DocSpec& spec) {
    slimr::Json::array_t tabs;
    for (size_t i = 0; i < spec.tabs.size(); ++i) {
        std::string tabId = spec.tabs[i].tabId.empty() ? std::format("t.{}", i) : spec.tabs[i].tabId;
        tabs.push_back(tabJsonBuild(spec.tabs[i], tabId));
    }
    return slimr::Json::object_t{
        {"documentId", spec.documentId},
        {"revisionId", spec.revisionId},
        {"title", spec.title},
        {"tabs", std::move(tabs)}
    };
}

inline std::vector<ParagraphContentSpec> randomParagraphContentSpecs(auto& rng) {
    static const std::vector<std::string> RANDOM_WORDS = {
        "alpha", "bravo", "charlie", "delta", "echo", "foxtrot", "golf", "hotel"
    };
    static const std::vector<AtomType> RANDOM_ATOM_TYPES = {
        "person", "richLink", "image", "footnoteRef", "horizontalRule", "pageBreak", "equation"
    };
    int n = 1 + static_cast<int>(rng() * 3);
    std::vector<ParagraphContentSpec> content;
    for (int i = 0; i < n; ++i) {
        if (rng() < 0.3) {
            content.push_back(ParagraphAtomSpec{.type = RANDOM_ATOM_TYPES[static_cast<int>(rng() * RANDOM_ATOM_TYPES.size())]});
        } else {
            content.push_back(RANDOM_WORDS[static_cast<int>(rng() * RANDOM_WORDS.size())]);
        }
    }
    return content;
}

inline std::vector<BlockSpec> randomBlockSpecs(auto& rng, int count) {
    std::vector<BlockSpec> blocks;
    for (int i = 0; i < count; ++i) {
        double roll = rng();
        if (roll < 0.15) {
            blocks.push_back(BlockSpec::make_section_break());
        } else if (roll < 0.3) {
            blocks.push_back(BlockSpec::make_toc(2 + static_cast<int>(rng() * 8)));
        } else if (roll < 0.55) {
            int rows = 1 + static_cast<int>(rng() * 2);
            int cols = 1 + static_cast<int>(rng() * 2);
            std::vector<std::vector<std::vector<BlockSpec>>> tableCells;
            for (int r = 0; r < rows; ++r) {
                std::vector<std::vector<BlockSpec>> rowCells;
                for (int c = 0; c < cols; ++c) {
                    rowCells.push_back({BlockSpec::make_paragraph(ParagraphSpec{.content = randomParagraphContentSpecs(rng)})});
                }
                tableCells.push_back(std::move(rowCells));
            }
            blocks.push_back(BlockSpec::make_table(TableBlockSpec{.cells = std::move(tableCells)}));
        } else {
            blocks.push_back(BlockSpec::make_paragraph(ParagraphSpec{.content = randomParagraphContentSpecs(rng)}));
        }
    }
    return blocks;
}

} // namespace gdocsmith::model
