#pragma once

#include "types.hpp"
#include "document_state.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gdocsmith::emulator {

using Unmodeled = std::function<void(const std::string&)>;

struct CellPos {
    int start = 0;
    int end = 0;
};

struct RowPos {
    int start = 0;
    int end = 0;
    std::vector<CellPos> cells;
};

struct TablePos {
    std::vector<RowPos> rows;
};

struct Region {
    int row = 0;
    int column = 0;
    int row_span = 1;
    int column_span = 1;
};

inline TablePos table_structure_at(const std::vector<TapeCell>& tape, int table_start) {
    int i = table_start + 1;
    std::vector<RowPos> rows;
    while (i < static_cast<int>(tape.size()) && tape[i].type == TapeCellType::RowStart) {
        int row_start = i;
        i += 1;
        std::vector<CellPos> cells;
        while (i < static_cast<int>(tape.size()) && tape[i].type == TapeCellType::CellStart) {
            int cell_start = i;
            i += 1;
            int depth = 0;
            while (i < static_cast<int>(tape.size()) &&
                   (depth > 0 || (tape[i].type != TapeCellType::CellStart &&
                                  tape[i].type != TapeCellType::RowStart &&
                                  tape[i].type != TapeCellType::TableEnd))) {
                if (tape[i].type == TapeCellType::TableStart) depth++;
                else if (tape[i].type == TapeCellType::TableEnd) depth--;
                i += 1;
            }
            cells.push_back(CellPos{.start = cell_start, .end = i});
        }
        rows.push_back(RowPos{.start = row_start, .end = i, .cells = std::move(cells)});
    }
    return TablePos{.rows = std::move(rows)};
}

inline std::vector<Region> table_regions(const std::vector<TapeCell>& tape, const TablePos& table) {
    std::vector<Region> out;
    for (size_t r = 0; r < table.rows.size(); ++r) {
        const auto& row = table.rows[r];
        for (size_t c = 0; c < row.cells.size(); ++c) {
            const auto& pos = row.cells[c];
            const auto& cell = tape[pos.start];
            int r_span = 1;
            int c_span = 1;
            if (cell.style.is_object()) {
                if (cell.style.contains("rowSpan")) r_span = cell.style["rowSpan"].as<int>();
                if (cell.style.contains("columnSpan")) c_span = cell.style["columnSpan"].as<int>();
            }
            if (r_span > 1 || c_span > 1) {
                out.push_back(Region{
                    .row = static_cast<int>(r),
                    .column = static_cast<int>(c),
                    .row_span = r_span,
                    .column_span = c_span
                });
            }
        }
    }
    return out;
}

inline const Region* region_at(const std::vector<Region>& regions, int row, int col) {
    for (const auto& r : regions) {
        if (r.row == row && r.column == col) return &r;
    }
    return nullptr;
}

inline bool region_covers(const Region& m, int row, int col) {
    return m.row <= row && row < m.row + m.row_span &&
           m.column <= col && col < m.column + m.column_span;
}

inline std::optional<int> hidden_cell_redirect(const TabState& tab, int index) {
    std::vector<int> stack;
    for (int k = 0; k < index && k < static_cast<int>(tab.tape.size()); ++k) {
        if (tab.tape[k].type == TapeCellType::TableStart) stack.push_back(k);
        else if (tab.tape[k].type == TapeCellType::TableEnd && !stack.empty()) stack.pop_back();
    }
    if (stack.empty()) return std::nullopt;
    int table_start = stack.back();

    TablePos table = table_structure_at(tab.tape, table_start);
    auto regions = table_regions(tab.tape, table);

    for (size_t r = 0; r < table.rows.size(); ++r) {
        const auto& row = table.rows[r];
        for (size_t c = 0; c < row.cells.size(); ++c) {
            const auto& cell = row.cells[c];
            if (index <= cell.start || index >= cell.end) continue;
            for (const auto& m : regions) {
                if (region_covers(m, static_cast<int>(r), static_cast<int>(c)) &&
                    (m.row != static_cast<int>(r) || m.column != static_cast<int>(c))) {
                    return table.rows[m.row].cells[m.column].end - 1;
                }
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

inline void insert_table(TabState& tab, int index, int rows, int columns,
                         const slimr::Json& cell_text_style,
                         std::function<void(int)> split_nl,
                         bool nested = false) {
    split_nl(index);
    auto table_column_properties = nested
                                       ? nested_table_column_properties_default(columns)
                                       : table_column_properties_default(columns);
    slimr::Json::array_t tcp_arr;
    for (auto& prop : table_column_properties) tcp_arr.push_back(std::move(prop));
    slimr::Json table_style = slimr::Json::object_t{
        {"tableColumnProperties", std::move(tcp_arr)}
    };

    slimr::Json raw = slimr::Json::object_t{
        {"columns", static_cast<int64_t>(columns)},
        {"rows", static_cast<int64_t>(rows)},
        {"tableStyle", std::move(table_style)}
    };

    std::vector<TapeCell> cells;
    cells.push_back(TapeCell{.type = TapeCellType::TableStart, .raw = std::move(raw)});
    for (int r = 0; r < rows; ++r) {
        cells.push_back(TapeCell{.type = TapeCellType::RowStart, .style = make_table_row_style_default()});
        for (int c = 0; c < columns; ++c) {
            cells.push_back(TapeCell{.type = TapeCellType::CellStart, .style = make_table_cell_style_default()});
            NlPara np;
            np.style = make_table_cell_paragraph_style_default();
            cells.push_back(TapeCell{
                .type = TapeCellType::Nl,
                .ch = "\n",
                .u16 = u'\n',
                .style = cell_text_style,
                .para = std::move(np)
            });
        }
    }
    cells.push_back(TapeCell{.type = TapeCellType::TableEnd});

    tab.tape.insert(tab.tape.begin() + (index + 1), cells.begin(), cells.end());
}

inline TapeCell cell_paragraph_like(const std::vector<TapeCell>& tape, const CellPos& ref_cell) {
    const auto& last = tape[ref_cell.end - 1];
    slimr::Json style = make_table_cell_paragraph_style_default();
    if (last.type == TapeCellType::Nl && last.para.style.is_object()) {
        for (const auto& [k, v] : last.para.style.get<slimr::Json::object_t>()) {
            style[k] = v;
        }
    }
    NlPara np;
    np.style = std::move(style);
    return TapeCell{
        .type = TapeCellType::Nl,
        .ch = "\n",
        .u16 = u'\n',
        .style = slimr::Json::object_t{},
        .para = std::move(np)
    };
}

inline void insert_table_row(TabState& tab, int table_start, int row_index, int column_index,
                             bool insert_below, Unmodeled unmodeled) {
    TablePos table = table_structure_at(tab.tape, table_start);
    int ref_index = (row_index >= 0 && row_index < static_cast<int>(table.rows.size()))
                        ? row_index
                        : static_cast<int>(table.rows.size()) - 1;
    if (ref_index < 0 || ref_index >= static_cast<int>(table.rows.size())) {
        throw std::runtime_error("table has no rows to insert next to");
    }
    const auto& ref_row = table.rows[ref_index];
    auto regions = table_regions(tab.tape, table);
    const Region* head = region_at(regions, ref_index, column_index);
    if (!head) {
        for (const auto& m : regions) {
            if (region_covers(m, ref_index, column_index)) {
                unmodeled("a row inserted next to a merged-away cell");
            }
        }
    }
    int at = insert_below ? ref_index + (head ? head->row_span : 1) : ref_index;
    for (const auto& m : regions) {
        if (m.row < at && at < m.row + m.row_span) {
            unmodeled("a row inserted inside a merged region");
        }
    }

    const auto& ref_row_cell = tab.tape[ref_row.start];
    std::vector<TapeCell> cells;
    cells.push_back(TapeCell{.type = TapeCellType::RowStart, .style = ref_row_cell.style});

    for (size_t c = 0; c < ref_row.cells.size(); ++c) {
        const auto& cell_pos = ref_row.cells[c];
        const auto& ref_cell_cell = tab.tape[cell_pos.start];
        const Region* crossing = nullptr;
        for (const auto& m : regions) {
            if (m.column == static_cast<int>(c) && region_covers(m, ref_index, static_cast<int>(c))) {
                crossing = &m;
                break;
            }
        }
        slimr::Json c_style = ref_cell_cell.style.is_object() ? ref_cell_cell.style : slimr::Json::object_t{};
        c_style["columnSpan"] = static_cast<int64_t>(crossing ? crossing->column_span : 1);
        c_style["rowSpan"] = static_cast<int64_t>(1);

        cells.push_back(TapeCell{.type = TapeCellType::CellStart, .style = std::move(c_style)});
        cells.push_back(cell_paragraph_like(tab.tape, cell_pos));
    }

    int insert_pos = (at < static_cast<int>(table.rows.size())) ? table.rows[at].start : ref_row.end;
    tab.tape.insert(tab.tape.begin() + insert_pos, cells.begin(), cells.end());

    // Bump table row count
    auto& start_cell = tab.tape[table_start];
    if (start_cell.raw.is_object() && start_cell.raw.contains("rows")) {
        start_cell.raw["rows"] = static_cast<int64_t>(start_cell.raw["rows"].as<int>() + 1);
    }
}

inline void table_column_count_bump(TabState& tab, int table_start, int delta) {
    auto& start_cell = tab.tape[table_start];
    if (start_cell.raw.is_object() && start_cell.raw.contains("columns")) {
        int next_cols = std::max(0, start_cell.raw["columns"].as<int>() + delta);
        start_cell.raw["columns"] = static_cast<int64_t>(next_cols);
    }
}

inline void insert_table_column(TabState& tab, int table_start, int row_index, int column_index,
                                bool insert_right, Unmodeled unmodeled) {
    TablePos table = table_structure_at(tab.tape, table_start);
    auto regions = table_regions(tab.tape, table);
    const Region* head = region_at(regions, row_index, column_index);
    if (!head) {
        for (const auto& m : regions) {
            if (region_covers(m, row_index, column_index)) {
                unmodeled("a column inserted next to a merged-away cell");
            }
        }
    }
    int at = insert_right ? column_index + (head ? head->column_span : 1) : column_index;
    for (const auto& m : regions) {
        if (m.column < at && at < m.column + m.column_span) {
            unmodeled("a column inserted inside a merged region");
        }
    }

    for (int r = static_cast<int>(table.rows.size()) - 1; r >= 0; --r) {
        const auto& row = table.rows[r];
        if (row.cells.empty()) continue;
        int ref_c = std::min(column_index, static_cast<int>(row.cells.size()) - 1);
        const auto& ref_cell_pos = row.cells[ref_c];
        const auto& ref_cell_cell = tab.tape[ref_cell_pos.start];

        const Region* crossing = nullptr;
        for (const auto& m : regions) {
            if (m.row == r && region_covers(m, r, column_index)) {
                crossing = &m;
                break;
            }
        }
        slimr::Json c_style = ref_cell_cell.style.is_object() ? ref_cell_cell.style : slimr::Json::object_t{};
        c_style["columnSpan"] = static_cast<int64_t>(1);
        c_style["rowSpan"] = static_cast<int64_t>(crossing ? crossing->row_span : 1);

        int insert_pos = (at < static_cast<int>(row.cells.size())) ? row.cells[at].start : row.end;
        std::vector<TapeCell> to_insert;
        to_insert.push_back(TapeCell{.type = TapeCellType::CellStart, .style = std::move(c_style)});
        to_insert.push_back(cell_paragraph_like(tab.tape, ref_cell_pos));
        tab.tape.insert(tab.tape.begin() + insert_pos, to_insert.begin(), to_insert.end());
    }

    table_column_count_bump(tab, table_start, 1);

    auto& raw = tab.tape[table_start].raw;
    if (raw.is_object() && raw.contains("tableStyle") && raw["tableStyle"].is_object() &&
        raw["tableStyle"].contains("tableColumnProperties") &&
        raw["tableStyle"]["tableColumnProperties"].is_array()) {
        auto& props = raw["tableStyle"]["tableColumnProperties"].get<slimr::Json::array_t>();
        if (!props.empty()) {
            int ref_idx = std::min(column_index, static_cast<int>(props.size()) - 1);
            auto copy = props[ref_idx];
            int insert_idx = std::min(at, static_cast<int>(props.size()));
            props.insert(props.begin() + insert_idx, copy);
        }
    }
}

inline void delete_table_row(TabState& tab, int table_start, int row_index) {
    TablePos table = table_structure_at(tab.tape, table_start);
    if (row_index < 0 || row_index >= static_cast<int>(table.rows.size())) {
        throw std::runtime_error("row " + std::to_string(row_index) + " does not exist");
    }
    const auto& row = table.rows[row_index];
    tab.tape.erase(tab.tape.begin() + row.start, tab.tape.begin() + row.end);

    auto& start_cell = tab.tape[table_start];
    if (start_cell.raw.is_object() && start_cell.raw.contains("rows")) {
        start_cell.raw["rows"] = static_cast<int64_t>(std::max(0, start_cell.raw["rows"].as<int>() - 1));
    }
}

inline void delete_table_column(TabState& tab, int table_start, int column_index) {
    TablePos table = table_structure_at(tab.tape, table_start);
    for (int r = static_cast<int>(table.rows.size()) - 1; r >= 0; --r) {
        const auto& row = table.rows[r];
        if (column_index < 0 || column_index >= static_cast<int>(row.cells.size())) continue;
        const auto& cell = row.cells[column_index];
        tab.tape.erase(tab.tape.begin() + cell.start, tab.tape.begin() + cell.end);
    }
    table_column_count_bump(tab, table_start, -1);
}

inline TapeCell moved_paragraph_cell(TapeCell cell) {
    if (cell.type != TapeCellType::Nl) return cell;
    slimr::Json filtered = slimr::Json::object_t{};
    if (cell.para.style.is_object()) {
        for (const auto& field : MERGE_MOVED_PARAGRAPH_FIELDS) {
            if (cell.para.style.contains(field)) {
                filtered[field] = cell.para.style[field];
            }
        }
    }
    cell.para.style = std::move(filtered);
    return cell;
}

inline void merge_table_cells(TabState& tab, int table_start, int row_index, int column_index,
                              int row_span, int column_span) {
    TablePos table = table_structure_at(tab.tape, table_start);
    if (row_index < 0 || row_index >= static_cast<int>(table.rows.size()) ||
        column_index < 0 || column_index >= static_cast<int>(table.rows[row_index].cells.size())) {
        throw std::runtime_error("merge head cell does not exist");
    }
    const auto& head_pos = table.rows[row_index].cells[column_index];
    std::vector<CellPos> covered;
    for (int r = row_index; r < row_index + row_span; ++r) {
        for (int c = column_index; c < column_index + column_span; ++c) {
            if (r == row_index && c == column_index) continue;
            if (r < static_cast<int>(table.rows.size()) && c < static_cast<int>(table.rows[r].cells.size())) {
                covered.push_back(table.rows[r].cells[c]);
            }
        }
    }

    // Row markers first
    for (int r = row_index; r < row_index + row_span; ++r) {
        if (r < static_cast<int>(table.rows.size())) {
            auto& row_cell = tab.tape[table.rows[r].start];
            row_cell.style = merged_row_style(row_cell.style);
        }
    }

    std::sort(covered.begin(), covered.end(), [](const CellPos& a, const CellPos& b) {
        return b.start < a.start; // descending
    });

    std::vector<TapeCell> absorbed;
    for (const auto& pos : covered) {
        int count = pos.end - (pos.start + 1);
        std::vector<TapeCell> content(tab.tape.begin() + (pos.start + 1), tab.tape.begin() + pos.end);
        bool is_empty = (content.size() == 1);
        TapeCell last = content.back();

        if (!is_empty) {
            std::vector<TapeCell> moved;
            moved.reserve(content.size());
            for (auto& item : content) {
                moved.push_back(moved_paragraph_cell(std::move(item)));
            }
            absorbed.insert(absorbed.begin(), moved.begin(), moved.end());
        }

        // Replace content with 1 empty newline
        NlPara np;
        np.style = last.para.style;
        TapeCell empty_nl{
            .type = TapeCellType::Nl,
            .ch = "\n",
            .u16 = u'\n',
            .style = last.style,
            .para = std::move(np)
        };
        tab.tape.erase(tab.tape.begin() + (pos.start + 1), tab.tape.begin() + pos.end);
        tab.tape.insert(tab.tape.begin() + (pos.start + 1), empty_nl);
    }

    if (!absorbed.empty()) {
        tab.tape.insert(tab.tape.begin() + head_pos.end, absorbed.begin(), absorbed.end());
    }

    auto& head_cell = tab.tape[head_pos.start];
    head_cell.style["columnSpan"] = static_cast<int64_t>(column_span);
    head_cell.style["rowSpan"] = static_cast<int64_t>(row_span);
}

inline void unmerge_table_cells(TabState& tab, int table_start, int row_index, int column_index) {
    TablePos table = table_structure_at(tab.tape, table_start);
    if (row_index < 0 || row_index >= static_cast<int>(table.rows.size()) ||
        column_index < 0 || column_index >= static_cast<int>(table.rows[row_index].cells.size())) {
        throw std::runtime_error("unmerge head cell does not exist");
    }
    const auto& head_pos = table.rows[row_index].cells[column_index];
    auto& head_cell = tab.tape[head_pos.start];
    head_cell.style["columnSpan"] = static_cast<int64_t>(1);
    head_cell.style["rowSpan"] = static_cast<int64_t>(1);
}

struct SharedBorder {
    std::string side;
    std::string shared;
    int dr;
    int dc;
};

inline const std::vector<SharedBorder> SHARED_BORDERS = {
    {"borderBottom", "borderTop", 1, 0},
    {"borderTop", "borderBottom", -1, 0},
    {"borderRight", "borderLeft", 0, 1},
    {"borderLeft", "borderRight", 0, -1}
};

inline void update_table_cell_style(TabState& tab, int table_start,
                                    std::optional<int> row_index,
                                    std::optional<int> column_index,
                                    std::optional<int> row_span,
                                    std::optional<int> column_span,
                                    const slimr::Json& patch,
                                    const std::vector<std::string>& fields) {
    TablePos table = table_structure_at(tab.tape, table_start);
    int r_start = row_index.value_or(0);
    int r_end = row_index.has_value() ? *row_index + row_span.value_or(1) : static_cast<int>(table.rows.size());

    for (int r = r_start; r < r_end && r < static_cast<int>(table.rows.size()); ++r) {
        const auto& row = table.rows[r];
        int c_start = column_index.value_or(0);
        int c_end = column_index.has_value() ? *column_index + column_span.value_or(1) : static_cast<int>(row.cells.size());
        for (int c = c_start; c < c_end && c < static_cast<int>(row.cells.size()); ++c) {
            const auto& cell_pos = row.cells[c];
            auto& cell = tab.tape[cell_pos.start];
            cell.style = apply_style_fields(cell.style, patch, fields);

            for (const auto& sb : SHARED_BORDERS) {
                if (std::find(fields.begin(), fields.end(), sb.side) == fields.end()) continue;
                int nr = r + sb.dr;
                int nc = c + sb.dc;
                if (nr >= r_start && nr < r_end && nc >= c_start && nc < c_end) continue;
                if (nr >= 0 && nr < static_cast<int>(table.rows.size()) &&
                    nc >= 0 && nc < static_cast<int>(table.rows[nr].cells.size())) {
                    const auto& neighbor_pos = table.rows[nr].cells[nc];
                    auto& neighbor = tab.tape[neighbor_pos.start];
                    slimr::Json n_patch = slimr::Json::object_t{};
                    if (patch.is_object() && patch.contains(sb.side)) {
                        n_patch[sb.shared] = patch[sb.side];
                    }
                    neighbor.style = apply_style_fields(neighbor.style, n_patch, {sb.shared});
                }
            }
        }
    }
}

inline void update_table_column_properties(TabState& tab, int table_start,
                                           const std::vector<int>& column_indices,
                                           const slimr::Json& patch,
                                           const std::vector<std::string>& fields) {
    auto& raw = tab.tape[table_start].raw;
    if (!raw.is_object()) raw = slimr::Json::object_t{};
    if (!raw.contains("tableStyle") || !raw["tableStyle"].is_object()) {
        raw["tableStyle"] = slimr::Json::object_t{};
    }
    auto& table_style = raw["tableStyle"];
    if (!table_style.contains("tableColumnProperties") || !table_style["tableColumnProperties"].is_array()) {
        table_style["tableColumnProperties"] = slimr::Json::array_t{};
    }
    auto& props = table_style["tableColumnProperties"].get<slimr::Json::array_t>();
    for (int idx : column_indices) {
        if (idx >= 0) {
            if (idx >= static_cast<int>(props.size())) {
                props.resize(idx + 1, slimr::Json::object_t{});
            }
            props[idx] = apply_style_fields(props[idx], patch, fields);
        }
    }
}

inline void update_table_row_style(TabState& tab, int table_start,
                                   const std::vector<int>& row_indices,
                                   const slimr::Json& patch,
                                   const std::vector<std::string>& fields) {
    TablePos table = table_structure_at(tab.tape, table_start);
    for (int idx : row_indices) {
        if (idx >= 0 && idx < static_cast<int>(table.rows.size())) {
            auto& row_cell = tab.tape[table.rows[idx].start];
            row_cell.style = apply_style_fields(row_cell.style, patch, fields);
        }
    }
}

inline void pin_table_header_rows(TabState& tab, int table_start, int count) {
    TablePos table = table_structure_at(tab.tape, table_start);
    for (size_t r = 0; r < table.rows.size(); ++r) {
        auto& row_cell = tab.tape[table.rows[r].start];
        if (static_cast<int>(r) < count) {
            row_cell.style["tableHeader"] = true;
        } else {
            if (row_cell.style.is_object() && row_cell.style.contains("tableHeader")) {
                row_cell.style.get<slimr::Json::object_t>().erase("tableHeader");
            }
        }
    }
}

} // namespace gdocsmith::emulator
