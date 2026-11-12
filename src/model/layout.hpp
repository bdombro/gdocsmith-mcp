#pragma once

#include "model/symbols.hpp"
#include "model/types.hpp"

#include <numeric>
#include <unordered_map>
#include <vector>

namespace gdocsmith::model {

struct Layout {
    std::unordered_map<std::string, Range> blockRanges;
    std::unordered_map<std::string, int> cellContentStart;
    std::unordered_map<std::string, Range> cellRanges;
    std::unordered_map<std::string, Range> rowRanges;
};

inline int paragraphLength(const ParagraphBlock& p) {
    int len = 1;
    for (const auto& inlineItem : p.inlines) {
        len += inlineItem.is_text() ? utf16CodeUnitLength(inlineItem.text) : inlineItem.length;
    }
    return len;
}

int blockLength(const Block& block);

inline int containerLength(const std::vector<Block>& blocks) {
    int sum = 0;
    for (const auto& block : blocks) {
        sum += blockLength(block);
    }
    return sum;
}

inline int tableLength(const TableBlock& table) {
    int len = 2; // table start and end markers
    for (const auto& row : table.rows) {
        len += 1; // row marker
        for (const auto& cell : row.cells) {
            len += 1 + containerLength(cell.blocks); // cell marker + content
        }
    }
    return len;
}

inline int blockLength(const Block& block) {
    if (block.is_paragraph()) return paragraphLength(block.as_paragraph());
    if (block.is_section_break()) return 1;
    if (block.is_table()) return tableLength(block.as_table());
    if (block.is_toc()) return block.as_toc().length;
    return 0;
}

inline int layoutBlock(const Block& block, int start, Layout& layout);

inline int layoutCell(const CellModel& cell, int start, Layout& layout) {
    int index = start + 1;
    layout.cellContentStart[cell.key] = index;
    for (const auto& block : cell.blocks) {
        index = layoutBlock(block, index, layout);
    }
    layout.cellRanges[cell.key] = Range{.end = index, .start = start};
    return index;
}

inline int layoutRow(const RowModel& row, int start, Layout& layout) {
    int index = start + 1;
    for (const auto& cell : row.cells) {
        index = layoutCell(cell, index, layout);
    }
    layout.rowRanges[row.key] = Range{.end = index, .start = start};
    return index;
}

inline int layoutBlock(const Block& block, int start, Layout& layout) {
    if (!block.is_table()) {
        int end = start + blockLength(block);
        layout.blockRanges[block.key] = Range{.end = end, .start = start};
        return end;
    }
    const auto& table = block.as_table();
    int index = start + 1;
    for (const auto& row : table.rows) {
        index = layoutRow(row, index, layout);
    }
    index += 1; // table end marker
    layout.blockRanges[block.key] = Range{.end = index, .start = start};
    return index;
}

/**
 * Computes the real JSON index range of every block, table row, and table cell in a tab,
 * starting at index 1 (the leading section break occupies [0, 1)).
 */
inline Layout layoutCompute(const TabModel& tab) {
    Layout layout;
    int index = 1;
    for (const auto& block : tab.blocks) {
        index = layoutBlock(block, index, layout);
    }
    return layout;
}

} // namespace gdocsmith::model
