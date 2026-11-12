#pragma once

#include "model/types.hpp"

#include <vector>

namespace gdocsmith::model {

/** Every block of a container in document order, recursing into table cells. */
inline std::vector<Block> blocksDeep(const std::vector<Block>& blocks) {
    std::vector<Block> out;
    auto visit = [&](auto& self, const std::vector<Block>& container) -> void {
        for (const auto& block : container) {
            out.push_back(block);
            if (block.is_table()) {
                const auto& table = block.as_table();
                for (const auto& row : table.rows) {
                    for (const auto& cell : row.cells) {
                        self(self, cell.blocks);
                    }
                }
            }
        }
    };
    visit(visit, blocks);
    return out;
}

/** Every paragraph of a container in document order, table cells too. */
inline std::vector<ParagraphBlock> paragraphsDeep(const std::vector<Block>& blocks) {
    std::vector<ParagraphBlock> out;
    for (const auto& b : blocksDeep(blocks)) {
        if (b.is_paragraph()) {
            out.push_back(b.as_paragraph());
        }
    }
    return out;
}

/** Every atom of a container in document order, table cells too. */
inline std::vector<Atom> atomsDeep(const std::vector<Block>& blocks) {
    std::vector<Atom> out;
    for (const auto& p : paragraphsDeep(blocks)) {
        for (const auto& inlineItem : p.inlines) {
            if (inlineItem.is_atom()) {
                out.push_back(inlineItem.to_atom());
            }
        }
    }
    return out;
}

/** Every table of a container in document order, nested tables included. */
inline std::vector<TableBlock> tablesDeep(const std::vector<Block>& blocks) {
    std::vector<TableBlock> out;
    for (const auto& b : blocksDeep(blocks)) {
        if (b.is_table()) {
            out.push_back(b.as_table());
        }
    }
    return out;
}

/** Every cell of every table in a container, nested tables' cells included. */
inline std::vector<CellModel> cellsDeep(const std::vector<Block>& blocks) {
    std::vector<CellModel> out;
    for (const auto& table : tablesDeep(blocks)) {
        for (const auto& row : table.rows) {
            for (const auto& cell : row.cells) {
                out.push_back(cell);
            }
        }
    }
    return out;
}

/** The block list of the container itself and of every cell below it, outermost first. */
inline std::vector<std::vector<Block>> containersDeep(const std::vector<Block>& blocks) {
    std::vector<std::vector<Block>> out;
    out.push_back(blocks);
    for (const auto& cell : cellsDeep(blocks)) {
        out.push_back(cell.blocks);
    }
    return out;
}

} // namespace gdocsmith::model
