#pragma once

#include "model/api_facts.hpp"
#include "model/edit.hpp"
#include "model/errors.hpp"
#include "model/types.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model {

struct CellRange {
    int column = 0;
    int columnSpan = 1;
    int row = 0;
    int rowSpan = 1;
};

struct EditTableSpec {
    std::vector<std::string> alignments;
    std::vector<std::vector<std::vector<SymSpec>>> rows; // [row][col][syms]
};

inline CellModel tableCellCreate(EditContext& context, const CellModel* inherit = nullptr) {
    CellModel cell;
    cell.key = context.keys.next("n");
    cell.style = inherit ? inherit->style : make_table_cell_style_default();
    cell.style["rowSpan"] = 1;
    cell.style["columnSpan"] = 1;
    auto paragraphStyle = make_table_cell_paragraph_style_default();
    if (inherit && !inherit->blocks.empty() && inherit->blocks.back().is_paragraph()) {
        paragraphStyle = inherit->blocks.back().as_paragraph().style;
    }
    cell.blocks.emplace_back(paragraphEmptyCreate(context.keys, paragraphStyle, context.stamp));
    return cell;
}

inline void tableGeometrySet(EditTarget target, TableBlock& table,
                             const std::vector<std::optional<int>>& rows,
                             const std::vector<std::optional<int>>& columns) {
    if (rows.empty() || columns.empty()) throw CoreError("unsupportedSyntax", "A table needs at least one cell.");
    const auto original = table;
    table.columns.clear();
    for (size_t c = 0; c < columns.size(); ++c) {
        const auto& column = columns[c];
        if (column) table.columns.push_back(original.columns.at(*column));
        else {
            auto props = model::make_json_object();
            if (!table.columns.empty()) props = table.columns.back().props;
            else {
                const auto ref = std::find_if(columns.begin() + static_cast<std::ptrdiff_t>(c + 1), columns.end(),
                                              [](const auto& value) { return value.has_value(); });
                if (ref != columns.end()) props = original.columns.at(**ref).props;
                else props = original.columns.front().props;
            }
            table.columns.push_back({.key = target.ctx.keys.next("n"), .props = std::move(props)});
        }
    }
    table.rows.clear();
    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        RowModel next;
        if (row) next = original.rows.at(*row);
        else {
            next.key = target.ctx.keys.next("n");
            if (!table.rows.empty()) next.style = table.rows.back().style;
            else {
                const auto ref = std::find_if(rows.begin() + static_cast<std::ptrdiff_t>(r + 1), rows.end(),
                                              [](const auto& value) { return value.has_value(); });
                next.style = ref == rows.end() ? make_table_row_style_default() : original.rows.at(**ref).style;
            }
        }
        next.cells.clear();
        for (size_t c = 0; c < columns.size(); ++c) {
            const auto& column = columns[c];
            if (row && column) next.cells.push_back(original.rows.at(*row).cells.at(*column));
            else {
                const CellModel* inherit = nullptr;
                if (!row && !table.rows.empty()) inherit = &table.rows.back().cells[c];
                else if (row && !next.cells.empty()) inherit = &next.cells.back();
                else {
                    const auto refRow = row ? row : [&]() -> std::optional<int> {
                        const auto found = std::find_if(rows.begin() + static_cast<std::ptrdiff_t>(r + 1), rows.end(),
                                                        [](const auto& value) { return value.has_value(); });
                        return found == rows.end() ? std::nullopt : *found;
                    }();
                    const auto refColumn = column ? column : [&]() -> std::optional<int> {
                        const auto found = std::find_if(columns.begin() + static_cast<std::ptrdiff_t>(c + 1), columns.end(),
                                                        [](const auto& value) { return value.has_value(); });
                        return found == columns.end() ? std::nullopt : *found;
                    }();
                    if (refRow && refColumn) inherit = &original.rows.at(*refRow).cells.at(*refColumn);
                }
                next.cells.push_back(tableCellCreate(target.ctx, inherit));
            }
        }
        table.rows.push_back(std::move(next));
    }
    for (size_t r = 0; r < original.rows.size(); ++r) for (size_t c = 0; c < original.rows[r].cells.size(); ++c) {
        if (std::find(rows.begin(), rows.end(), std::optional<int>{static_cast<int>(r)}) == rows.end() ||
            std::find(columns.begin(), columns.end(), std::optional<int>{static_cast<int>(c)}) == columns.end()) {
            for (const auto& block : original.rows[r].cells[c].blocks) tombstonesAdd(target, block);
        }
    }
    table.stamp = target.ctx.stamp;
}

inline std::string tableCreate(
    EditTarget& target,
    int index,
    const EditTableSpec& spec,
    const ContainerRef& ref = ContainerRef{.kind = "body"}
) {
    int colCount = 0;
    for (const auto& r : spec.rows) colCount = std::max(colCount, static_cast<int>(r.size()));
    if (spec.rows.empty() || colCount < 1) {
        throw CoreError(std::string(error_codes::INTERNAL), "a table needs at least one cell");
    }

    auto& keys = target.ctx.keys;
    auto colProps = ref.is_body() ? tableColumnPropertiesDefault(colCount) : nestedTableColumnPropertiesDefault(colCount);
    std::vector<ColumnModel> columns;
    for (const auto& cp : colProps) {
        columns.push_back(ColumnModel{.key = keys.next("n"), .props = cp});
    }

    std::vector<RowModel> tableRows;
    for (const auto& r : spec.rows) {
        std::vector<CellModel> cells;
        for (const auto& cellSyms : r) {
            auto p = paragraphEmptyCreate(keys, make_table_cell_paragraph_style_default(), target.ctx.stamp);
            paragraphSymbolsSet(p, symbolsFromSpecs(target.ctx, cellSyms, make_json_object()));
            CellModel cell;
            cell.key = keys.next("n");
            cell.style = make_table_cell_style_default();
            cell.blocks.push_back(Block(std::move(p)));
            cells.push_back(std::move(cell));
        }
        RowModel row;
        row.key = keys.next("n");
        row.style = make_table_row_style_default();
        row.cells = std::move(cells);
        tableRows.push_back(std::move(row));
    }

    TableBlock table;
    table.kind = "table";
    table.key = keys.next("n");
    table.columns = std::move(columns);
    table.rows = std::move(tableRows);
    table.stamp = target.ctx.stamp;
    std::string key = table.key;

    auto& blocks = containerOf(target.tab, ref);
    blocks.insert(blocks.begin() + index, Block(std::move(table)));
    containerRenormalize(target, ref);
    return key;
}

} // namespace gdocsmith::model
