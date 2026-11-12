#pragma once

#include "model/api_facts.hpp"
#include "model/errors.hpp"
#include "model/layout.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/types.hpp"
#include "reconcile/context.hpp"
#include "reconcile/requests.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::reconcile {

using namespace model;

inline const std::vector<std::string> SPAN_FIELDS = {"columnSpan", "rowSpan"};
inline const std::vector<std::string> HEADER_FIELDS = {"tableHeader"};

using CellFill = std::function<void(const CellModel& cell, int contentStart)>;

struct TableState {
    std::vector<JsonObject> columns;
    struct RowState {
        std::vector<JsonObject> cells;
        JsonObject style;
    };
    std::vector<RowState> rows;
};

struct TableMerge {
    int column = 0;
    int columnSpan = 1;
    int row = 0;
    int rowSpan = 1;

    bool operator==(const TableMerge&) const = default;
};

inline std::vector<TableMerge> tableMerges(const TableBlock& t) {
    std::vector<TableMerge> out;
    for (size_t r = 0; r < t.rows.size(); ++r) {
        const auto& row = t.rows[r];
        for (size_t c = 0; c < row.cells.size(); ++c) {
            const auto& cell = row.cells[c];
            int rowSpan = 1;
            int colSpan = 1;
            if (cell.style.contains("rowSpan")) {
                if (cell.style["rowSpan"].holds<int64_t>()) rowSpan = static_cast<int>(cell.style["rowSpan"].get<int64_t>());
                else if (cell.style["rowSpan"].holds<double>()) rowSpan = static_cast<int>(cell.style["rowSpan"].get<double>());
            }
            if (cell.style.contains("columnSpan")) {
                if (cell.style["columnSpan"].holds<int64_t>()) colSpan = static_cast<int>(cell.style["columnSpan"].get<int64_t>());
                else if (cell.style["columnSpan"].holds<double>()) colSpan = static_cast<int>(cell.style["columnSpan"].get<double>());
            }
            if (rowSpan > 1 || colSpan > 1) {
                out.push_back(TableMerge{
                    .column = static_cast<int>(c),
                    .columnSpan = colSpan,
                    .row = static_cast<int>(r),
                    .rowSpan = rowSpan
                });
            }
        }
    }
    return out;
}

inline TableRangeOpts mergeRange(const TableMerge& m, int tableStart, const ReconcileContext& ctx) {
    return TableRangeOpts{
        .columnIndex = m.column,
        .columnSpan = m.columnSpan,
        .rowIndex = m.row,
        .rowSpan = m.rowSpan,
        .tabId = ctx.tabId,
        .tableStart = tableStart
    };
}

inline bool mergeEqual(const TableMerge& a, const TableMerge& b) {
    return a.row == b.row && a.column == b.column && a.rowSpan == b.rowSpan && a.columnSpan == b.columnSpan;
}

struct StructureOp {
    std::string axis; // "column" | "row"
    int index = 0;
    bool insert = false;
    bool after = false;
};

inline std::vector<StructureOp> structureOps(
    const TableBlock& o,
    const TableBlock& f,
    const std::set<std::string>& keptRows,
    const std::set<std::string>& keptColumns
) {
    std::vector<StructureOp> ops;
    auto lines = [&](
        const auto& original,
        const auto& finalLines,
        const std::set<std::string>& kept,
        std::string_view axis
    ) {
        auto deletes = [&]() {
            for (int i = static_cast<int>(original.size()) - 1; i >= 0; --i) {
                if (!kept.contains(original[i].key)) {
                    ops.push_back(StructureOp{.axis = std::string(axis), .index = i, .insert = false});
                }
            }
        };
        if (!kept.empty()) {
            deletes();
            for (size_t i = 0; i < finalLines.size(); ++i) {
                if (!kept.contains(finalLines[i].key)) {
                    ops.push_back(StructureOp{
                        .axis = std::string(axis),
                        .index = static_cast<int>(i),
                        .insert = true,
                        .after = i > 0
                    });
                }
            }
        } else {
            for (size_t i = 0; i < finalLines.size(); ++i) {
                ops.push_back(StructureOp{
                    .axis = std::string(axis),
                    .index = static_cast<int>(original.size() + i),
                    .insert = true,
                    .after = true
                });
            }
            deletes();
        }
    };

    lines(o.rows, f.rows, keptRows, "row");
    lines(o.columns, f.columns, keptColumns, "column");
    return ops;
}

struct StructureRef {
    bool after = false;
    int ref = 0;
};

struct StructurePlan {
    std::vector<TableMerge> kept;
    std::vector<StructureRef> refs;
    std::set<int> unmerged;
};

inline StructurePlan structurePlan(
    const std::vector<StructureOp>& ops,
    const std::vector<TableMerge>& mergesO,
    const std::vector<TableMerge>& mergesF,
    int rows,
    int columns
) {
    std::set<int> unmerged;
    for (;;) {
        size_t before = unmerged.size();
        std::map<int, TableMerge> live;
        for (size_t i = 0; i < mergesO.size(); ++i) {
            if (!unmerged.contains(static_cast<int>(i))) {
                live[static_cast<int>(i)] = mergesO[i];
            }
        }
        auto cut = [&](int i) {
            live.erase(i);
            unmerged.insert(i);
        };
        std::map<std::string, int> count{{"column", columns}, {"row", rows}};
        std::vector<StructureRef> refs;

        for (const auto& op : ops) {
            std::string axis = op.axis;
            std::string cross = (axis == "row") ? "column" : "row";

            auto getCoord = [&](const TableMerge& m, std::string_view ax) -> int {
                return (ax == "row") ? m.row : m.column;
            };
            auto getSpan = [&](const TableMerge& m, std::string_view ax) -> int {
                return (ax == "row") ? m.rowSpan : m.columnSpan;
            };
            auto setCoord = [&](TableMerge& m, std::string_view ax, int val) {
                if (ax == "row") m.row = val;
                else m.column = val;
            };

            if (!op.insert) {
                std::vector<int> toCut;
                for (const auto& [i, m] : live) {
                    int c = getCoord(m, axis);
                    int sp = getSpan(m, axis);
                    if (c <= op.index && op.index < c + sp) {
                        toCut.push_back(i);
                    }
                }
                for (int i : toCut) cut(i);
                for (auto& [_, m] : live) {
                    int c = getCoord(m, axis);
                    if (c > op.index) setCoord(m, axis, c - 1);
                }
                refs.push_back(StructureRef{.after = false, .ref = 0});
                count[axis] -= 1;
                continue;
            }

            std::vector<int> toCut;
            for (const auto& [i, m] : live) {
                int c = getCoord(m, axis);
                int sp = getSpan(m, axis);
                if (c < op.index && op.index < c + sp) {
                    toCut.push_back(i);
                }
            }
            for (int i : toCut) cut(i);

            auto spanning = [&](int line) {
                std::vector<int> res;
                for (const auto& [i, m] : live) {
                    if (getSpan(m, cross) > 1 && getCoord(m, axis) <= line && line < getCoord(m, axis) + getSpan(m, axis)) {
                        res.push_back(i);
                    }
                }
                return res;
            };

            std::vector<bool> candidates;
            if (op.after) {
                candidates.push_back(true);
                if (op.index < count[axis]) candidates.push_back(false);
            } else {
                candidates.push_back(false);
            }

            bool after = candidates[0];
            for (bool c : candidates) {
                if (spanning(c ? op.index - 1 : op.index).empty()) {
                    after = c;
                    break;
                }
            }
            int line = after ? op.index - 1 : op.index;
            for (int i : spanning(line)) cut(i);

            auto covering = [&](int x) -> std::optional<int> {
                for (const auto& [i, m] : live) {
                    if (getCoord(m, axis) <= line && line < getCoord(m, axis) + getSpan(m, axis) &&
                        getCoord(m, cross) <= x && x < getCoord(m, cross) + getSpan(m, cross)) {
                        return i;
                    }
                }
                return std::nullopt;
            };

            int ref = 0;
            while (ref < count[cross] && covering(ref).has_value()) ref++;
            if (ref == count[cross]) {
                ref = 0;
                auto cov0 = covering(0);
                if (cov0) cut(*cov0);
            }
            refs.push_back(StructureRef{.after = after, .ref = ref});
            for (auto& [_, m] : live) {
                int c = getCoord(m, axis);
                if (c >= op.index) setCoord(m, axis, c + 1);
            }
            count[axis] += 1;
        }

        std::vector<int> finalCut;
        for (const auto& [i, m] : live) {
            bool found = false;
            for (const auto& x : mergesF) {
                if (mergeEqual(x, m)) { found = true; break; }
            }
            if (!found) finalCut.push_back(i);
        }
        for (int i : finalCut) cut(i);

        if (unmerged.size() == before) {
            std::vector<TableMerge> kept;
            for (const auto& [_, m] : live) kept.push_back(m);
            return StructurePlan{.kept = std::move(kept), .refs = std::move(refs), .unmerged = std::move(unmerged)};
        }
    }
}

inline void mergesEmit(
    const std::vector<TableMerge>& merges,
    TableState& state,
    int tableStart,
    ReconcileContext& ctx,
    const RequestOrigin& origin
) {
    for (const auto& m : merges) {
        requestPush(ctx, RequestBuilder::tableCellsMerge(mergeRange(m, tableStart, ctx)), origin);
        for (int r = m.row; r < m.row + m.rowSpan; ++r) {
            state.rows[r].style = mergedRowStyle(state.rows[r].style);
        }
    }
}

inline std::vector<std::string> changedFields(
    const JsonObject& before,
    const JsonObject& after,
    const std::vector<std::string>& skip
) {
    std::set<std::string> keys;
    if (before.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : before.get<slimr::Json::object_t>()) keys.insert(k);
    }
    if (after.holds<slimr::Json::object_t>()) {
        for (const auto& [k, _] : after.get<slimr::Json::object_t>()) keys.insert(k);
    }
    std::vector<std::string> fields;
    for (const auto& k : keys) {
        if (std::find(skip.begin(), skip.end(), k) == skip.end()) fields.push_back(k);
    }
    auto changed = styleFieldsChanged(before, after, fields);
    std::vector<std::string> out;
    for (const auto& f : changed) {
        JsonObject bVal = before.contains(f) ? before[f] : make_json_object();
        JsonObject aVal = after.contains(f) ? after[f] : make_json_object();
        if (!styleEqual(bVal, aVal)) {
            out.push_back(f);
        }
    }
    return out;
}

inline void stylesEmit(
    const TableState& state,
    const TableBlock& f,
    int headerRowsBefore,
    int tableStart,
    ReconcileContext& ctx,
    const RequestOrigin& origin
) {
    for (size_t r = 0; r < f.rows.size(); ++r) {
        const auto& row = f.rows[r];
        auto fields = changedFields(state.rows[r].style, row.style, HEADER_FIELDS);
        if (!fields.empty()) {
            requestPush(
                ctx,
                RequestBuilder::updateTableRowStyle(RequestBuilder::UpdateTableRowStyleOpts{
                    .fields = [&]() {
                        std::string s;
                        for (size_t i = 0; i < fields.size(); ++i) {
                            if (i > 0) s += ",";
                            s += fields[i];
                        }
                        return s;
                    }(),
                    .rowIndices = {static_cast<int>(r)},
                    .tabId = ctx.tabId,
                    .tableRowStyle = pick(row.style, fields),
                    .tableStart = tableStart
                }),
                origin
            );
        }

        for (size_t c = 0; c < row.cells.size(); ++c) {
            const auto& cell = row.cells[c];
            auto cellFields = changedFields(state.rows[r].cells[c], cell.style, SPAN_FIELDS);
            if (cellFields.empty()) continue;
            requestPush(
                ctx,
                RequestBuilder::tableCellStyleUpdate(RequestBuilder::TableCellStyleUpdateOpts{
                    TableRangeOpts{
                        .columnIndex = static_cast<int>(c),
                        .columnSpan = 1,
                        .rowIndex = static_cast<int>(r),
                        .rowSpan = 1,
                        .tabId = ctx.tabId,
                        .tableStart = tableStart
                    },
                    cellFields,
                    pick(cell.style, cellFields)
                }),
                origin
            );
        }
    }

    for (size_t c = 0; c < f.columns.size(); ++c) {
        const auto& col = f.columns[c];
        JsonObject colBefore = (c < state.columns.size()) ? state.columns[c] : make_json_object();
        auto fields = changedFields(colBefore, col.props, {});
        if (fields.empty()) continue;
        if (std::find(fields.begin(), fields.end(), "width") != fields.end() &&
            std::find(fields.begin(), fields.end(), "widthType") == fields.end()) {
            fields.push_back("widthType");
        }
        requestPush(
            ctx,
            RequestBuilder::tableColumnPropertiesUpdate(RequestBuilder::TableColumnPropertiesUpdateOpts{
                .columnIndices = {static_cast<int>(c)},
                .fields = fields,
                .tabId = ctx.tabId,
                .tableColumnProperties = pick(col.props, fields),
                .tableStart = tableStart
            }),
            origin
        );
    }

    int header = 0;
    while (header < static_cast<int>(f.rows.size())) {
        const auto& st = f.rows[header].style;
        if (st.contains("tableHeader") && st["tableHeader"].holds<bool>() && st["tableHeader"].get<bool>()) {
            header++;
        } else {
            break;
        }
    }
    for (size_t r = header; r < f.rows.size(); ++r) {
        const auto& st = f.rows[r].style;
        if (st.contains("tableHeader") && st["tableHeader"].holds<bool>() && st["tableHeader"].get<bool>()) {
            throw CoreError("internal", "header rows must be the table's first rows");
        }
    }
    if (header != headerRowsBefore) {
        requestPush(
            ctx,
            RequestBuilder::pinTableHeaderRows(RequestBuilder::PinHeaderRowsOpts{
                .pinnedHeaderRowsCount = header,
                .tabId = ctx.tabId,
                .tableStart = tableStart
            }),
            origin
        );
    }
}

inline ParagraphBlock cellEmptyParagraph(
    std::string_view key,
    int start
) {
    ParagraphBlock p;
    p.key = std::string(key);
    p.kind = "paragraph";
    p.origin = Range{.end = start + 1, .start = start};
    p.is_protected = false;
    p.style = make_table_cell_paragraph_style_default();
    p.newline = NewlineMark{.style = make_json_object()};
    return p;
}

inline void tableNewEmit(
    const TableBlock& t,
    int pos,
    ReconcileContext& ctx,
    const CellFill& fill
) {
    RequestOrigin origin{.key = t.key, .tabIndex = t.stamp ? std::optional<int>(t.stamp->tabIndex) : std::nullopt};
    int columns = static_cast<int>(t.columns.size());
    requestPush(ctx, RequestBuilder::tableInsert(pos, static_cast<int>(t.rows.size()), columns, ctx.tabId), origin);
    int tableStart = pos + 1;

    for (int r = static_cast<int>(t.rows.size()) - 1; r >= 0; --r) {
        for (int c = columns - 1; c >= 0; --c) {
            fill(t.rows[r].cells[c], tableStart + 1 + r * (1 + 2 * columns) + 1 + 2 * c + 1);
        }
    }

    TableState state;
    for (int c = 0; c < columns; ++c) state.columns.push_back(make_json_object());
    for (size_t r = 0; r < t.rows.size(); ++r) {
        TableState::RowState rowState;
        for (int c = 0; c < columns; ++c) rowState.cells.push_back(make_table_cell_style_default());
        rowState.style = make_table_row_style_default();
        state.rows.push_back(std::move(rowState));
    }

    mergesEmit(tableMerges(t), state, tableStart, ctx, origin);
    stylesEmit(state, t, 0, tableStart, ctx, origin);
}

inline std::set<std::string> keptOrder(
    const auto& original,
    const auto& finalItems,
    std::string_view what
) {
    std::map<std::string, int> index;
    for (size_t i = 0; i < original.size(); ++i) {
        index[original[i].key] = static_cast<int>(i);
    }
    int last = -1;
    std::set<std::string> kept;
    for (const auto& item : finalItems) {
        auto it = index.find(item.key);
        if (it == index.end()) continue;
        if (it->second <= last) {
            throw CoreError("internal", std::format("a table {} moved; a move must be a delete plus an insert", what));
        }
        last = it->second;
        kept.insert(item.key);
    }
    return kept;
}

inline void tableReconcile(
    const TableBlock& o,
    const TableBlock& f,
    ReconcileContext& ctx,
    const std::function<void(const CellModel& o, const CellModel& f)>& cellReconcile,
    const CellFill& fill
) {
    if (!o.origin) {
        throw CoreError("internal", std::format("table {} has no original range", o.key));
    }
    RequestOrigin origin{.key = f.key, .tabIndex = f.stamp ? std::optional<int>(f.stamp->tabIndex) : std::nullopt};
    int tableStart = o.origin->start;
    auto keptRows = keptOrder(o.rows, f.rows, "row");
    auto keptColumns = keptOrder(o.columns, f.columns, "column");

    std::map<std::string, int> oRow;
    for (size_t i = 0; i < o.rows.size(); ++i) oRow[o.rows[i].key] = static_cast<int>(i);
    std::map<std::string, int> oColumn;
    for (size_t i = 0; i < o.columns.size(); ++i) oColumn[o.columns[i].key] = static_cast<int>(i);

    auto mergesO = tableMerges(o);
    auto mergesF = tableMerges(f);
    auto ops = structureOps(o, f, keptRows, keptColumns);
    auto plan = structurePlan(ops, mergesO, mergesF, static_cast<int>(o.rows.size()), static_cast<int>(o.columns.size()));

    // (0) unmerges
    for (size_t i = 0; i < mergesO.size(); ++i) {
        if (plan.unmerged.contains(static_cast<int>(i))) {
            requestPush(ctx, RequestBuilder::tableCellsUnmerge(mergeRange(mergesO[i], tableStart, ctx)), origin);
        }
    }

    // (1) kept cells, last first
    for (int r = static_cast<int>(f.rows.size()) - 1; r >= 0; --r) {
        auto itR = oRow.find(f.rows[r].key);
        if (itR == oRow.end()) continue;
        int orIdx = itR->second;
        for (int c = static_cast<int>(f.columns.size()) - 1; c >= 0; --c) {
            auto itC = oColumn.find(f.columns[c].key);
            if (itC != oColumn.end()) {
                cellReconcile(o.rows[orIdx].cells[itC->second], f.rows[r].cells[c]);
            }
        }
    }

    // (2)+(3) rows, then columns, simulating what the API holds.
    TableState state;
    for (const auto& col : o.columns) state.columns.push_back(col.props);
    for (const auto& row : o.rows) {
        TableState::RowState rs;
        for (const auto& cell : row.cells) rs.cells.push_back(cell.style);
        rs.style = row.style;
        state.rows.push_back(std::move(rs));
    }

    for (size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        const auto& [after, ref] = plan.refs[i];
        if (op.axis == "row" && !op.insert) {
            requestPush(ctx, RequestBuilder::deleteTableRow(RequestBuilder::TableRowLocationOpts{
                .columnIndex = ref,
                .rowIndex = op.index,
                .tabId = ctx.tabId,
                .tableStart = tableStart
            }), origin);
            state.rows.erase(state.rows.begin() + op.index);
        } else if (op.axis == "row" && op.insert) {
            int refIndex = after ? op.index - 1 : op.index;
            requestPush(ctx, RequestBuilder::insertTableRow(RequestBuilder::TableRowLocationOpts{
                .columnIndex = ref,
                .insertBelow = after,
                .rowIndex = refIndex,
                .tabId = ctx.tabId,
                .tableStart = tableStart
            }), origin);
            const auto& refRow = state.rows[refIndex];
            TableState::RowState newRow;
            for (const auto& st : refRow.cells) {
                JsonObject unmerged = st;
                unmerged["columnSpan"] = static_cast<int64_t>(1);
                unmerged["rowSpan"] = static_cast<int64_t>(1);
                newRow.cells.push_back(std::move(unmerged));
            }
            newRow.style = refRow.style;
            state.rows.insert(state.rows.begin() + op.index, std::move(newRow));
        } else if (!op.insert) {
            requestPush(ctx, RequestBuilder::deleteTableColumn(RequestBuilder::TableColumnLocationOpts{
                .columnIndex = op.index,
                .rowIndex = ref,
                .tabId = ctx.tabId,
                .tableStart = tableStart
            }), origin);
            state.columns.erase(state.columns.begin() + op.index);
            for (auto& row : state.rows) {
                row.cells.erase(row.cells.begin() + op.index);
            }
        } else {
            int refIndex = after ? op.index - 1 : op.index;
            requestPush(ctx, RequestBuilder::insertTableColumn(RequestBuilder::TableColumnLocationOpts{
                .columnIndex = refIndex,
                .insertRight = after,
                .rowIndex = ref,
                .tabId = ctx.tabId,
                .tableStart = tableStart
            }), origin);
            JsonObject refCol = (refIndex < static_cast<int>(state.columns.size())) ? state.columns[refIndex] : make_json_object();
            state.columns.insert(state.columns.begin() + op.index, refCol);
            for (auto& row : state.rows) {
                JsonObject baseCell = (refIndex < static_cast<int>(row.cells.size())) ? row.cells[refIndex] : make_table_cell_style_default();
                baseCell["columnSpan"] = static_cast<int64_t>(1);
                baseCell["rowSpan"] = static_cast<int64_t>(1);
                row.cells.insert(row.cells.begin() + op.index, std::move(baseCell));
            }
        }
    }

    // (4) new cells, last first, at their positions in current table
    struct NewCell {
        CellModel cell;
        int start = 0;
    };
    std::vector<NewCell> newCells;
    int pos = tableStart + 1;
    for (const auto& row : f.rows) {
        pos += 1;
        bool rowIsNew = !keptRows.contains(row.key);
        for (size_t c = 0; c < row.cells.size(); ++c) {
            pos += 1;
            const auto& cell = row.cells[c];
            bool isNew = rowIsNew || !keptColumns.contains(f.columns[c].key);
            if (isNew) newCells.push_back(NewCell{.cell = cell, .start = pos});
            pos += isNew ? 1 : containerLength(cell.blocks);
        }
    }
    for (auto it = newCells.rbegin(); it != newCells.rend(); ++it) {
        fill(it->cell, it->start);
    }

    // (5) merges, (6) styles
    std::vector<TableMerge> filteredMergesF;
    for (const auto& m : mergesF) {
        bool inKept = false;
        for (const auto& k : plan.kept) {
            if (mergeEqual(k, m)) { inKept = true; break; }
        }
        if (!inKept) filteredMergesF.push_back(m);
    }
    mergesEmit(filteredMergesF, state, tableStart, ctx, origin);

    int headerBefore = 0;
    for (const auto& r : o.rows) {
        if (r.style.contains("tableHeader") && r.style["tableHeader"].holds<bool>() && r.style["tableHeader"].get<bool>()) {
            headerBefore++;
        }
    }
    stylesEmit(state, f, headerBefore, tableStart, ctx, origin);
}

} // namespace gdocsmith::reconcile
