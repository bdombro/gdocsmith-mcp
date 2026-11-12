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
