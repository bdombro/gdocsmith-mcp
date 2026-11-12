#pragma once

#include "test.hpp"
#include "model/types.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/lists.hpp"
#include "model/edit.hpp"
#include "model/edit_tables.hpp"
#include "model/test_docs.hpp"
#include "reconcile/reconcile.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace gdocsmith::reconcile::test {

using namespace gdocsmith::model;

inline Block* findBlockDirect(TabModel& tab, std::string_view key) {
    for (auto& b : tab.blocks) {
        if (b.key == key) return &b;
        if (b.is_table()) {
            for (auto& row : b.as_table().rows) {
                for (auto& cell : row.cells) {
                    for (auto& cb : cell.blocks) {
                        if (cb.key == key) return &cb;
                    }
                }
            }
        }
    }
    return nullptr;
}

inline void fuzzParagraphSplice(
    EditTarget& target,
    std::string_view key,
    int at,
    int deleteCount,
    const std::vector<SymSpec>& insert
) {
    auto* b = findBlockDirect(target.tab, key);
    if (!b || !b->is_paragraph()) return;
    auto& paragraph = b->as_paragraph();
    auto syms = paragraphSymbols(paragraph);
    if (at < 0 || deleteCount < 0 || at + deleteCount > static_cast<int>(syms.size())) return;

    JsonObject inherited = paragraph.newline.style;
    if (at > 0 && at - 1 < static_cast<int>(syms.size())) {
        inherited = syms[at - 1].style;
    } else if (at + deleteCount < static_cast<int>(syms.size())) {
        inherited = syms[at + deleteCount].style;
    }

    auto inserted = symbolsFromSpecs(target.ctx, insert, inherited);
    for (int i = 0; i < deleteCount; ++i) {
        const auto& s = syms[at + i];
        if (s.is_atom() && s.atom.has_value()) {
            target.ctx.tombstones.push_back(Tombstone{
                .key = s.atom->key,
                .kind = "atom",
                .stamp = target.ctx.stamp,
                .tabId = target.tab.tabId,
            });
        }
    }
    syms.erase(syms.begin() + at, syms.begin() + at + deleteCount);
    syms.insert(syms.begin() + at, inserted.begin(), inserted.end());
    paragraphSymbolsSet(paragraph, syms);
    paragraph.stamp = target.ctx.stamp;
}

inline void fuzzTextStyleUpdate(
    EditTarget& target,
    std::string_view key,
    int from,
    int to,
    const JsonObject& patch
) {
    auto* b = findBlockDirect(target.tab, key);
    if (!b || !b->is_paragraph()) return;
    auto& p = b->as_paragraph();
    auto syms = paragraphSymbols(p);
    if (from < 0 || to > static_cast<int>(syms.size()) || from > to) return;
    bool changedLast = !syms.empty() && to == static_cast<int>(syms.size()) && from < to;
    for (int i = from; i < to; ++i) {
        if (syms[i].is_char()) {
            JsonObject s = syms[i].style;
            for (const auto& [k, v] : patch.get<slimr::Json::object_t>()) {
                if (v.is_null()) s.get<slimr::Json::object_t>().erase(k);
                else s[k] = styleCanonical(v);
            }
            syms[i].style = std::move(s);
        } else if (syms[i].is_atom() && syms[i].atom.has_value()) {
            JsonObject s = syms[i].atom->style.value_or(make_json_object());
            for (const auto& [k, v] : patch.get<slimr::Json::object_t>()) {
                if (v.is_null()) s.get<slimr::Json::object_t>().erase(k);
                else s[k] = styleCanonical(v);
            }
            syms[i].atom->style = std::move(s);
        }
    }
    paragraphSymbolsSet(p, syms);
    if (changedLast) {
        JsonObject s = p.newline.style;
        for (const auto& [k, v] : patch.get<slimr::Json::object_t>()) {
            if (k != "link") {
                if (v.is_null()) s.get<slimr::Json::object_t>().erase(k);
                else s[k] = styleCanonical(v);
            }
        }
        p.newline.style = std::move(s);
    }
    p.stamp = target.ctx.stamp;
}

inline void fuzzParagraphStyleUpdate(
    EditTarget& target,
    std::string_view key,
    const JsonObject& patch
) {
    auto* b = findBlockDirect(target.tab, key);
    if (!b || !b->is_paragraph()) return;
    auto& p = b->as_paragraph();
    std::string oldType = p.style.contains("namedStyleType") && p.style["namedStyleType"].is_string()
                              ? p.style["namedStyleType"].get<std::string>() : "NORMAL_TEXT";
    bool wasHeading = headingStyleIs(oldType);

    JsonObject s = p.style;
    for (const auto& [k, v] : patch.get<slimr::Json::object_t>()) {
        if (v.is_null()) s.get<slimr::Json::object_t>().erase(k);
        else s[k] = styleCanonical(v);
    }
    p.style = std::move(s);

    std::string newType = p.style.contains("namedStyleType") && p.style["namedStyleType"].is_string()
                              ? p.style["namedStyleType"].get<std::string>() : "NORMAL_TEXT";
    bool isHeading = headingStyleIs(newType);
    if (isHeading) {
        p.bullet.reset();
        if (!wasHeading) p.headingId.reset();
    } else {
        p.headingId.reset();
    }
    p.stamp = target.ctx.stamp;
}

inline void fuzzNestingSet(EditTarget& target, std::string_view key, int nesting) {
    auto* b = findBlockDirect(target.tab, key);
    if (!b || !b->is_paragraph()) return;
    auto& p = b->as_paragraph();
    if (!p.bullet.has_value()) return;
    p.bullet->nestingLevel = nesting;
    p.style["indentFirstLine"] = slimr::Json::object_t{{"magnitude", static_cast<double>(36 * (nesting + 1) - 18)}, {"unit", "PT"}};
    p.style["indentStart"] = slimr::Json::object_t{{"magnitude", static_cast<double>(36 * (nesting + 1))}, {"unit", "PT"}};
    p.stamp = target.ctx.stamp;
}

inline void fuzzRowsInsert(EditTarget& target, std::string_view tableKey, int at, int count) {
    auto* b = findBlockDirect(target.tab, tableKey);
    if (!b || !b->is_table()) return;
    auto& table = b->as_table();
    if (at < 0 || at > static_cast<int>(table.rows.size()) || table.rows.empty()) return;
    int refIndex = (at > 0) ? at - 1 : 0;
    const auto& refRow = table.rows[refIndex];

    std::vector<RowModel> newRows;
    for (int i = 0; i < count; ++i) {
        RowModel newRow;
        newRow.key = target.ctx.keys.next("n");
        newRow.style = refRow.style;
        for (const auto& cell : refRow.cells) {
            CellModel newCell;
            newCell.key = target.ctx.keys.next("n");
            newCell.style = cell.style;
            auto p = paragraphEmptyCreate(target.ctx.keys, make_table_cell_paragraph_style_default(), target.ctx.stamp);
            newCell.blocks.push_back(Block(std::move(p)));
            newRow.cells.push_back(std::move(newCell));
        }
        newRows.push_back(std::move(newRow));
    }
    table.rows.insert(table.rows.begin() + at, newRows.begin(), newRows.end());
    table.stamp = target.ctx.stamp;
}

inline void fuzzRowsDelete(EditTarget& target, std::string_view tableKey, const std::vector<int>& rows) {
    auto* b = findBlockDirect(target.tab, tableKey);
    if (!b || !b->is_table()) return;
    auto& table = b->as_table();
    std::set<int> doomed(rows.begin(), rows.end());
    if (doomed.size() >= table.rows.size()) return;

    std::vector<RowModel> kept;
    for (size_t r = 0; r < table.rows.size(); ++r) {
        if (!doomed.contains(static_cast<int>(r))) {
            kept.push_back(std::move(table.rows[r]));
        }
    }
    table.rows = std::move(kept);
    table.stamp = target.ctx.stamp;
}

inline void fuzzColumnsInsert(EditTarget& target, std::string_view tableKey, int at, int count) {
    auto* b = findBlockDirect(target.tab, tableKey);
    if (!b || !b->is_table()) return;
    auto& table = b->as_table();
    if (at < 0 || at > static_cast<int>(table.columns.size()) || table.columns.empty()) return;
    int refIndex = (at > 0) ? at - 1 : 0;
    auto refProp = table.columns[refIndex].props;

    std::vector<ColumnModel> newCols;
    for (int i = 0; i < count; ++i) {
        newCols.push_back(ColumnModel{.key = target.ctx.keys.next("n"), .props = refProp});
    }
    table.columns.insert(table.columns.begin() + at, newCols.begin(), newCols.end());

    for (auto& row : table.rows) {
        auto refStyle = row.cells[refIndex].style;
        std::vector<CellModel> newCells;
        for (int i = 0; i < count; ++i) {
            CellModel cell;
            cell.key = target.ctx.keys.next("n");
            cell.style = refStyle;
            auto p = paragraphEmptyCreate(target.ctx.keys, make_table_cell_paragraph_style_default(), target.ctx.stamp);
            cell.blocks.push_back(Block(std::move(p)));
            newCells.push_back(std::move(cell));
        }
        row.cells.insert(row.cells.begin() + at, newCells.begin(), newCells.end());
    }
    table.stamp = target.ctx.stamp;
}

inline void fuzzCellsMerge(EditTarget& target, std::string_view tableKey, int row, int col, int rowSpan, int colSpan) {
    auto* b = findBlockDirect(target.tab, tableKey);
    if (!b || !b->is_table()) return;
    auto& table = b->as_table();
    if (row < 0 || row + rowSpan > static_cast<int>(table.rows.size())) return;
    if (col < 0 || col + colSpan > static_cast<int>(table.columns.size())) return;
    for (int r = row; r < row + rowSpan; ++r) {
        for (int c = col; c < col + colSpan; ++c) {
            if (r == row && c == col) continue;
            auto& cell = table.rows[r].cells[c];
            if (cell.blocks.size() != 1 || !cell.blocks[0].is_paragraph()) return;
            const auto& p = cell.blocks[0].as_paragraph();
            if (!p.inlines.empty() || p.bullet.has_value()) return;
        }
    }
    auto& head = table.rows[row].cells[col];
    head.style["columnSpan"] = static_cast<int64_t>(colSpan);
    head.style["rowSpan"] = static_cast<int64_t>(rowSpan);
    table.stamp = target.ctx.stamp;
}

inline void fuzzPageBreakParagraphInsert(EditTarget& target, int index) {
    if (index < 0 || index > static_cast<int>(target.tab.blocks.size())) return;
    auto p = paragraphEmptyCreate(target.ctx.keys, make_json_object(), target.ctx.stamp);
    Atom a;
    a.kind = "atom";
    a.key = target.ctx.keys.next("n");
    a.type = "pageBreak";
    a.length = 1;
    a.create = AtomCreate{.type = "pageBreak"};
    p.inlines.push_back(Inline(std::move(a)));
    target.tab.blocks.insert(target.tab.blocks.begin() + index, Block(std::move(p)));
    containerRenormalize(target, ContainerRef{.kind = "body"});
}

inline void fuzzSectionBreakInsert(EditTarget& target, int index, std::string_view sectionType) {
    if (index <= 0 || index >= static_cast<int>(target.tab.blocks.size())) return;
    SectionBreakBlock sb;
    sb.kind = "sectionBreak";
    sb.key = target.ctx.keys.next("n");
    slimr::Json style = make_section_style_default();
    style["sectionType"] = std::string(sectionType);
    sb.sectionStyle = std::move(style);
    target.tab.blocks.insert(target.tab.blocks.begin() + index, Block(std::move(sb)));
    containerRenormalize(target, ContainerRef{.kind = "body"});
}

inline std::vector<std::string> paragraphKeys(const std::vector<Block>& blocks) {
    std::vector<std::string> out;
    for (const auto& b : blocks) {
        if (b.is_paragraph()) {
            out.push_back(b.key);
        } else if (b.is_table()) {
            for (const auto& row : b.as_table().rows) {
                for (const auto& cell : row.cells) {
                    for (const auto& cb : cell.blocks) {
                        if (cb.is_paragraph()) out.push_back(cb.key);
                    }
                }
            }
        }
    }
    return out;
}

inline std::vector<BlockSpec> docSpec(auto& rng) {
    static const std::vector<std::string> WORDS = {"alpha", "beta", "gamma", "delta", "émoji😀", "x"};
    auto pick = [&](const auto& xs) {
        return xs[static_cast<size_t>(rng() * xs.size())];
    };
    std::vector<BlockSpec> blocks;
    int count = 2 + static_cast<int>(rng() * 6);
    for (int i = 0; i < count; ++i) {
        double roll = rng();
        int word_count = 1 + static_cast<int>(rng() * 3);
        std::string text;
        for (int w = 0; w < word_count; ++w) {
            if (w > 0) text += " ";
            text += pick(WORDS);
        }

        if (roll < 0.15) {
            ParagraphSpec ps;
            ps.content.push_back(text);
            ps.headingId = std::format("h.{}", i);
            ps.style = slimr::Json::object_t{{"namedStyleType", pick(std::vector<std::string>{"HEADING_1", "HEADING_2"})}};
            blocks.push_back(BlockSpec::make_paragraph(std::move(ps)));
        } else if (roll < 0.35) {
            int level = static_cast<int>(rng() * 2);
            ParagraphSpec ps;
            ps.content.push_back(text);
            ps.bullet = ParagraphSpec::Bullet{.listId = "kix.p", .nestingLevel = level};
            ps.style = slimr::Json::object_t{
                {"indentFirstLine", slimr::Json::object_t{{"magnitude", static_cast<double>(36 * (level + 1) - 18)}, {"unit", "PT"}}},
                {"indentStart", slimr::Json::object_t{{"magnitude", static_cast<double>(36 * (level + 1))}, {"unit", "PT"}}}
            };
            blocks.push_back(BlockSpec::make_paragraph(std::move(ps)));
        } else if (roll < 0.45 && !blocks.empty() && blocks.back().kind == "paragraph") {
            TableBlockSpec ts;
            std::vector<std::vector<std::vector<BlockSpec>>> cells(2, std::vector<std::vector<BlockSpec>>(2));
            ParagraphSpec p00; p00.content.push_back(pick(WORDS));
            cells[0][0].push_back(BlockSpec::make_paragraph(p00));

            ParagraphSpec p01;
            cells[0][1].push_back(BlockSpec::make_paragraph(p01));

            ParagraphSpec p10; p10.content.push_back(pick(WORDS));
            cells[1][0].push_back(BlockSpec::make_paragraph(p10));

            ParagraphSpec p11; p11.content.push_back(pick(WORDS));
            cells[1][1].push_back(BlockSpec::make_paragraph(p11));

            ts.cells = std::move(cells);
            blocks.push_back(BlockSpec::make_table(std::move(ts)));
        } else if (roll < 0.6) {
            ParagraphSpec ps;
            ps.content.push_back(ParagraphTextSpec{
                .text = text,
                .style = slimr::Json::object_t{{"bold", true}}
            });
            ps.content.push_back(" ");
            ps.content.push_back(pick(WORDS));
            blocks.push_back(BlockSpec::make_paragraph(std::move(ps)));
        } else {
            ParagraphSpec ps;
            ps.content.push_back(text);
            blocks.push_back(BlockSpec::make_paragraph(std::move(ps)));
        }
    }
    ParagraphSpec end_p;
    end_p.content.push_back("end");
    blocks.push_back(BlockSpec::make_paragraph(std::move(end_p)));
    return blocks;
}

inline void editRandom(EditTarget& t, auto& rng, std::vector<std::string>& log) {
    static const std::vector<std::string> WORDS = {"alpha", "beta", "gamma", "delta", "émoji😀", "x"};
    auto pick = [&](const auto& xs) {
        return xs[static_cast<size_t>(rng() * xs.size())];
    };
    auto paragraphs = paragraphKeys(t.tab.blocks);
    if (paragraphs.empty()) return;
    std::string key = pick(paragraphs);
    auto* b = findBlockDirect(t.tab, key);
    if (!b || !b->is_paragraph()) return;
    auto& p = b->as_paragraph();

    int symbols = 0;
    for (const auto& inlineItem : p.inlines) {
        if (inlineItem.is_text()) {
            symbols += static_cast<int>(inlineItem.text.size());
        } else {
            symbols += 1;
        }
    }
    int at = static_cast<int>(rng() * (symbols + 1));

    std::vector<std::string> tables;
    for (const auto& blockItem : t.tab.blocks) {
        if (blockItem.is_table()) tables.push_back(blockItem.key);
    }

    int op = static_cast<int>(rng() * 13);
    log.push_back(std::format("op{} {} at{}", op, key, at));

    try {
        switch (op) {
            case 0: {
                int delCount = std::min(symbols - at, static_cast<int>(rng() * 4));
                std::vector<SymSpec> insert;
                insert.push_back(SymSpec{.ch = pick(WORDS)});
                fuzzParagraphSplice(t, key, at, delCount, insert);
                break;
            }
            case 1: {
                if (symbols > 0) {
                    slimr::Json::object_t patch;
                    if (rng() < 0.5) patch["bold"] = true;
                    else patch["bold"] = nullptr;
                    if (rng() < 0.3) patch["italic"] = true;
                    else patch["italic"] = nullptr;
                    fuzzTextStyleUpdate(t, key, std::min(at, symbols - 1), symbols, patch);
                }
                break;
            }
            case 2: {
                slimr::Json::object_t patch;
                if (rng() < 0.5) {
                    patch["namedStyleType"] = pick(std::vector<std::string>{"NORMAL_TEXT", "HEADING_1", "HEADING_3"});
                } else {
                    auto a = pick(std::vector<std::string>{"CENTER", "END", "null"});
                    if (a == "null") patch["alignment"] = nullptr;
                    else patch["alignment"] = a;
                }
                fuzzParagraphStyleUpdate(t, key, patch);
                break;
            }
            case 3: {
                std::vector<EditParagraphSpec> toInsert;
                EditParagraphSpec spec1;
                spec1.syms.push_back(SymSpec{.ch = pick(WORDS)});
                toInsert.push_back(std::move(spec1));
                if (rng() < 0.3) {
                    EditParagraphSpec spec2;
                    spec2.syms.push_back(SymSpec{
                        .ch = pick(WORDS),
                        .style = slimr::Json::object_t{{"italic", true}}
                    });
                    toInsert.push_back(std::move(spec2));
                }
                int insIdx = static_cast<int>(rng() * (t.tab.blocks.size() + 1));
                paragraphsInsert(t, ContainerRef{.kind = "body"}, insIdx, toInsert);
                break;
            }
            case 4: {
                if (t.tab.blocks.size() > 2) {
                    std::vector<std::string> nonFinal;
                    for (size_t i = 0; i < t.tab.blocks.size() - 1; ++i) {
                        nonFinal.push_back(t.tab.blocks[i].key);
                    }
                    blocksDelete(t, {pick(nonFinal)});
                }
                break;
            }
            case 5: {
                if (rng() < 0.3) {
                    auto* found_p = findBlockDirect(t.tab, key);
                    if (found_p && found_p->is_paragraph()) {
                        auto& par = found_p->as_paragraph();
                        par.bullet.reset();
                        par.style.get<slimr::Json::object_t>().erase("indentFirstLine");
                        par.style["indentStart"] = slimr::Json::object_t{{"magnitude", 0.0}, {"unit", "PT"}};
                        par.stamp = t.ctx.stamp;
                    }
                } else {
                    auto* found_p = findBlockDirect(t.tab, key);
                    if (found_p && found_p->is_paragraph()) {
                        auto& par = found_p->as_paragraph();
                        std::string kind = pick(std::vector<std::string>{"bullet", "number"});
                        std::string listId = (kind == "number") ? "kix.num" : "kix.p";
                        par.bullet = BulletRef{.listId = listId, .nestingLevel = 0};
                        par.style["indentFirstLine"] = slimr::Json::object_t{{"magnitude", 18.0}, {"unit", "PT"}};
                        par.style["indentStart"] = slimr::Json::object_t{{"magnitude", 36.0}, {"unit", "PT"}};
                        par.stamp = t.ctx.stamp;
                    }
                }
                break;
            }
            case 6: {
                auto* found_p = findBlockDirect(t.tab, key);
                if (found_p && found_p->is_paragraph() && found_p->as_paragraph().bullet.has_value()) {
                    fuzzNestingSet(t, key, static_cast<int>(rng() * 3));
                }
                break;
            }
            case 7: {
                EditTableSpec spec;
                spec.rows = {
                    {{SymSpec{.ch = pick(WORDS)}}, {}}
                };
                int insIdx = static_cast<int>(rng() * (t.tab.blocks.size() + 1));
                tableCreate(t, insIdx, spec);
                break;
            }
            case 8: {
                if (!tables.empty()) {
                    fuzzRowsInsert(t, pick(tables), static_cast<int>(rng() * 3), 1);
                }
                break;
            }
            case 9: {
                if (!tables.empty()) {
                    if (rng() < 0.5) {
                        fuzzRowsDelete(t, pick(tables), {0});
                    } else {
                        fuzzColumnsInsert(t, pick(tables), static_cast<int>(rng() * 3), 1);
                    }
                }
                break;
            }
            case 10: {
                fuzzPageBreakParagraphInsert(t, static_cast<int>(rng() * t.tab.blocks.size()));
                break;
            }
            case 11: {
                if (t.tab.blocks.size() > 2) {
                    int idx = 1 + static_cast<int>(rng() * (t.tab.blocks.size() - 1));
                    std::string secType = (rng() < 0.5) ? "CONTINUOUS" : "NEXT_PAGE";
                    fuzzSectionBreakInsert(t, idx, secType);
                }
                break;
            }
            case 12: {
                if (!tables.empty()) {
                    fuzzCellsMerge(t, pick(tables), static_cast<int>(rng() * 2), 0, 1, 2);
                }
                break;
            }
        }
    } catch (const CoreError& err) {
        log.push_back(std::format("  refused: {}", err.code));
    } catch (const std::exception& err) {
        log.push_back(std::format("  refused: {}", err.what()));
    }
}

} // namespace gdocsmith::reconcile::test

TEST_SUITE("reconcile::fuzz") {
    TEST_CASE("differential fuzzing: random edits match emulator self-check") {
        using namespace gdocsmith::reconcile::test;
        using namespace gdocsmith::model;

        const int SEEDS = 50;
        int passed = 0;
        int unrealizable = 0;
        std::vector<std::string> failures;

        for (int seed = 1; seed <= SEEDS; ++seed) {
            auto rng = rngCreate(seed);

            TabSpec tab;
            tab.tabId = "t.0";
            tab.title = "Tab 1";
            tab.blocks = docSpec(rng);

            const auto& listTable = listPresetTable();
            auto it = listTable.find("BULLET_DISC_CIRCLE_SQUARE");
            slimr::Json::array_t levels;
            if (it != listTable.end()) {
                for (const auto& l : it->second) levels.push_back(l);
            }
            tab.lists = slimr::Json::object_t{
                {"kix.p", slimr::Json::object_t{
                    {"listProperties", slimr::Json::object_t{{"nestingLevels", std::move(levels)}}},
                    {"preset", "BULLET_DISC_CIRCLE_SQUARE"}
                }}
            };

            DocSpec spec;
            spec.documentId = "d";
            spec.title = "Fuzz Doc";
            spec.tabs.push_back(std::move(tab));

            auto json = docJsonBuild(spec);
            KeyAllocator keys;
            DocModel original = docModelParse(json, DocModelParseOptions{.docId = "d", .keys = keys});
            DocModel finalDoc = original;

            EditContext editCtx{
                .keys = keys,
                .stamp = EditStamp{.force = false, .tabIndex = 0},
                .tombstones = {}
            };
            EditTarget target{
                .ctx = editCtx,
                .tab = finalDoc.tabs[0]
            };

            std::vector<std::string> log;
            try {
                int edits = 1 + static_cast<int>(rng() * 8);
                for (int e = 0; e < edits; ++e) {
                    editRandom(target, rng, log);
                }

                gdocsmith::reconcile::DocReconcileInput input{
                    .final = std::move(finalDoc),
                    .original = std::move(original),
                    .originalJson = json,
                    .carryRanges = {}
                };
                auto plan = gdocsmith::reconcile::docReconcile(input);
                auto check = gdocsmith::reconcile::docPlanSelfCheck(plan, input);
                if (check.equal) {
                    passed++;
                } else {
                    std::string err = std::format("seed {}: {}\n", seed, log.empty() ? "(no ops)" : log[0]);
                    for (size_t d = 0; d < std::min<size_t>(check.diffs.size(), 4); ++d) {
                        err += "  " + check.diffs[d] + "\n";
                    }
                    failures.push_back(err);
                }
            } catch (const CoreError& err) {
                if (err.code == "unrealizableList") unrealizable++;
                else failures.push_back(std::format("seed {}: threw CoreError({})", seed, err.code));
            } catch (const std::exception& err) {
                failures.push_back(std::format("seed {}: threw std::exception({})", seed, err.what()));
            }
        }

        CHECK(passed > 0);
        if (!failures.empty()) {
            for (const auto& f : failures) {
                MESSAGE(f);
            }
        }
        CHECK(failures.empty());
    }
}
