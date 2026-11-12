#pragma once

#include "types.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::emulator {

inline std::u16string utf8_to_utf16(std::string_view s) {
    std::u16string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = 0;
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= s.size()) break;
            cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= s.size()) break;
            cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= s.size()) break;
            cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 3]) & 0x3F);
            i += 4;
        } else {
            i += 1;
            continue;
        }
        if (cp < 0x10000) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

inline std::string utf16_to_utf8(std::u16string_view s) {
    std::string out;
    out.reserve(s.size() * 3);
    for (size_t i = 0; i < s.size(); ++i) {
        char16_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF) {
            if (i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
                uint32_t cp = 0x10000 + (((c - 0xD800) << 10) | (s[i + 1] - 0xDC00));
                i += 1;
                out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                continue;
            }
        }
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

inline const std::vector<std::string> ATOM_KEYS = {
    "autoText",
    "columnBreak",
    "dateElement",
    "dropdown",
    "equation",
    "footnoteReference",
    "horizontalRule",
    "inlineObjectElement",
    "pageBreak",
    "person",
    "richLink"
};

// Forward declarations
void tape_append_element(std::vector<TapeCell>& tape, const slimr::Json& el);
void tape_append_paragraph(std::vector<TapeCell>& tape, const slimr::Json& p);
void tape_append_table(std::vector<TapeCell>& tape, const slimr::Json& t);
TabState tab_state_build(std::string_view tab_id, std::string_view title,
                         std::optional<std::string_view> parent_tab_id, const slimr::Json& dt);

struct AtomRawResult {
    slimr::Json raw;
    int span = 1;
    slimr::Json style;
    std::optional<Sugg> sugg;
};

inline AtomRawResult atom_from_raw_element(const slimr::Json& el) {
    if (!el.is_object()) {
        throw std::runtime_error("paragraph element is not an object");
    }
    int64_t start = el.contains("startIndex") ? el["startIndex"].as<int64_t>() : 0;
    int64_t end = el.contains("endIndex") ? el["endIndex"].as<int64_t>() : start + 1;
    int span = static_cast<int>(std::max<int64_t>(1, end - start));

    for (const auto& key : ATOM_KEYS) {
        if (!el.contains(key) || !el[key].is_object()) continue;

        slimr::Json payload = el[key];
        std::vector<std::string> del_ids;
        std::vector<std::string> ins_ids;
        slimr::Json style = slimr::Json::object_t{};

        if (payload.contains("suggestedDeletionIds") && payload["suggestedDeletionIds"].is_array()) {
            for (const auto& id : payload["suggestedDeletionIds"].get<slimr::Json::array_t>()) {
                if (id.is_string()) del_ids.push_back(id.get<std::string>());
            }
            payload.get<slimr::Json::object_t>().erase("suggestedDeletionIds");
        }
        if (payload.contains("suggestedInsertionIds") && payload["suggestedInsertionIds"].is_array()) {
            for (const auto& id : payload["suggestedInsertionIds"].get<slimr::Json::array_t>()) {
                if (id.is_string()) ins_ids.push_back(id.get<std::string>());
            }
            payload.get<slimr::Json::object_t>().erase("suggestedInsertionIds");
        }
        if (payload.contains("textStyle") && payload["textStyle"].is_object()) {
            style = style_canonical(payload["textStyle"]);
            payload.get<slimr::Json::object_t>().erase("textStyle");
        }

        slimr::Json raw = slimr::Json::object_t{{key, std::move(payload)}};
        return AtomRawResult{
            .raw = std::move(raw),
            .span = span,
            .style = std::move(style),
            .sugg = sugg_from_ids(del_ids, ins_ids)
        };
    }
    throw std::runtime_error("paragraph element has no recognized non-text variant");
}

inline std::optional<slimr::Json> paragraph_suggestions_raw(const slimr::Json& p) {
    bool has_bullet = p.contains("suggestedBulletChanges");
    bool has_style = p.contains("suggestedParagraphStyleChanges");
    bool has_pos = p.contains("suggestedPositionedObjectIds");
    if (!has_bullet && !has_style && !has_pos) return std::nullopt;

    slimr::Json raw = slimr::Json::object_t{};
    if (has_bullet) raw["suggestedBulletChanges"] = p["suggestedBulletChanges"];
    if (has_style) raw["suggestedParagraphStyleChanges"] = p["suggestedParagraphStyleChanges"];
    if (has_pos) raw["suggestedPositionedObjectIds"] = p["suggestedPositionedObjectIds"];
    return raw;
}

inline void tape_append_paragraph(std::vector<TapeCell>& tape, const slimr::Json& p) {
    slimr::Json raw_para_style = p.contains("paragraphStyle") && p["paragraphStyle"].is_object()
                                     ? p["paragraphStyle"]
                                     : slimr::Json::object_t{};
    std::optional<std::string> heading_id;
    if (raw_para_style.contains("headingId") && raw_para_style["headingId"].is_string()) {
        heading_id = raw_para_style["headingId"].get<std::string>();
        raw_para_style.get<slimr::Json::object_t>().erase("headingId");
    }

    std::optional<slimr::Json> bullet;
    if (p.contains("bullet") && p["bullet"].is_object()) {
        const auto& b = p["bullet"];
        slimr::Json b_obj = slimr::Json::object_t{};
        if (b.contains("listId")) b_obj["listId"] = b["listId"];
        if (b.contains("nestingLevel")) b_obj["nestingLevel"] = b["nestingLevel"];
        if (b.contains("textStyle")) b_obj["textStyle"] = b["textStyle"];
        bullet = std::move(b_obj);
    }

    std::vector<std::string> pos_ids;
    if (p.contains("positionedObjectIds") && p["positionedObjectIds"].is_array()) {
        for (const auto& id : p["positionedObjectIds"].get<slimr::Json::array_t>()) {
            if (id.is_string()) pos_ids.push_back(id.get<std::string>());
        }
    }

    const auto& elements = p.contains("elements") && p["elements"].is_array()
                               ? p["elements"].get<slimr::Json::array_t>()
                               : slimr::Json::array_t{};

    for (size_t i = 0; i < elements.size(); ++i) {
        const auto& el = elements[i];
        bool is_last = (i == elements.size() - 1);

        if (el.contains("textRun") && el["textRun"].is_object()) {
            const auto& tr = el["textRun"];
            std::string content = tr.contains("content") && tr["content"].is_string()
                                      ? tr["content"].get<std::string>()
                                      : "";
            slimr::Json style = tr.contains("textStyle") && tr["textStyle"].is_object()
                                    ? style_canonical(tr["textStyle"])
                                    : slimr::Json::object_t{};

            std::vector<std::string> del_ids;
            std::vector<std::string> ins_ids;
            if (tr.contains("suggestedDeletionIds") && tr["suggestedDeletionIds"].is_array()) {
                for (const auto& id : tr["suggestedDeletionIds"].get<slimr::Json::array_t>()) {
                    if (id.is_string()) del_ids.push_back(id.get<std::string>());
                }
            }
            if (tr.contains("suggestedInsertionIds") && tr["suggestedInsertionIds"].is_array()) {
                for (const auto& id : tr["suggestedInsertionIds"].get<slimr::Json::array_t>()) {
                    if (id.is_string()) ins_ids.push_back(id.get<std::string>());
                }
            }
            slimr::Json style_changes = tr.contains("suggestedTextStyleChanges")
                                            ? tr["suggestedTextStyleChanges"]
                                            : slimr::Json{nullptr};
            auto sugg = sugg_from_ids(del_ids, ins_ids, style_changes);

            std::u16string u16 = utf8_to_utf16(content);
            for (size_t j = 0; j < u16.size(); ++j) {
                bool is_last_char = is_last && (j == u16.size() - 1);
                char16_t ch16 = u16[j];
                if (is_last_char && ch16 == u'\n') {
                    NlPara np;
                    np.bullet = bullet;
                    np.heading_id = heading_id;
                    np.positioned_object_ids = pos_ids;
                    np.style = style_canonical(raw_para_style);
                    np.suggestions_raw = paragraph_suggestions_raw(p);

                    tape.push_back(TapeCell{
                        .type = TapeCellType::Nl,
                        .ch = "\n",
                        .u16 = u'\n',
                        .style = style,
                        .sugg = sugg,
                        .para = std::move(np)
                    });
                } else {
                    std::u16string single(1, ch16);
                    tape.push_back(TapeCell{
                        .type = TapeCellType::Char,
                        .ch = utf16_to_utf8(single),
                        .u16 = ch16,
                        .style = style,
                        .sugg = sugg
                    });
                }
            }
        } else {
            auto atom = atom_from_raw_element(el);
            tape.push_back(TapeCell{
                .type = TapeCellType::Atom,
                .style = atom.style,
                .sugg = atom.sugg,
                .raw = atom.raw,
                .span = atom.span
            });
            for (int k = 1; k < atom.span; ++k) {
                tape.push_back(TapeCell{.type = TapeCellType::AtomCont});
            }
        }
    }
}

inline void tape_append_table(std::vector<TapeCell>& tape, const slimr::Json& t) {
    slimr::Json table_rest = t.is_object() ? t : slimr::Json::object_t{};
    if (table_rest.is_object() && table_rest.contains("tableRows")) {
        table_rest.get<slimr::Json::object_t>().erase("tableRows");
    }
    tape.push_back(TapeCell{
        .type = TapeCellType::TableStart,
        .raw = std::move(table_rest)
    });

    if (t.contains("tableRows") && t["tableRows"].is_array()) {
        for (const auto& row : t["tableRows"].get<slimr::Json::array_t>()) {
            std::vector<std::string> del_ids;
            std::vector<std::string> ins_ids;
            if (row.contains("suggestedDeletionIds") && row["suggestedDeletionIds"].is_array()) {
                for (const auto& id : row["suggestedDeletionIds"].get<slimr::Json::array_t>()) {
                    if (id.is_string()) del_ids.push_back(id.get<std::string>());
                }
            }
            if (row.contains("suggestedInsertionIds") && row["suggestedInsertionIds"].is_array()) {
                for (const auto& id : row["suggestedInsertionIds"].get<slimr::Json::array_t>()) {
                    if (id.is_string()) ins_ids.push_back(id.get<std::string>());
                }
            }
            slimr::Json sc = row.contains("suggestedTableRowStyleChanges")
                                 ? row["suggestedTableRowStyleChanges"]
                                 : slimr::Json{nullptr};
            slimr::Json row_style = row.contains("tableRowStyle") && row["tableRowStyle"].is_object()
                                        ? style_canonical(row["tableRowStyle"])
                                        : slimr::Json::object_t{};

            tape.push_back(TapeCell{
                .type = TapeCellType::RowStart,
                .style = std::move(row_style),
                .sugg = sugg_from_ids(del_ids, ins_ids, sc)
            });

            if (row.contains("tableCells") && row["tableCells"].is_array()) {
                for (const auto& cell : row["tableCells"].get<slimr::Json::array_t>()) {
                    std::vector<std::string> c_del_ids;
                    std::vector<std::string> c_ins_ids;
                    if (cell.contains("suggestedDeletionIds") && cell["suggestedDeletionIds"].is_array()) {
                        for (const auto& id : cell["suggestedDeletionIds"].get<slimr::Json::array_t>()) {
                            if (id.is_string()) c_del_ids.push_back(id.get<std::string>());
                        }
                    }
                    if (cell.contains("suggestedInsertionIds") && cell["suggestedInsertionIds"].is_array()) {
                        for (const auto& id : cell["suggestedInsertionIds"].get<slimr::Json::array_t>()) {
                            if (id.is_string()) c_ins_ids.push_back(id.get<std::string>());
                        }
                    }
                    slimr::Json c_sc = cell.contains("suggestedTableCellStyleChanges")
                                           ? cell["suggestedTableCellStyleChanges"]
                                           : slimr::Json{nullptr};
                    slimr::Json cell_style = cell.contains("tableCellStyle") && cell["tableCellStyle"].is_object()
                                                 ? style_canonical(cell["tableCellStyle"])
                                                 : slimr::Json::object_t{};

                    tape.push_back(TapeCell{
                        .type = TapeCellType::CellStart,
                        .style = std::move(cell_style),
                        .sugg = sugg_from_ids(c_del_ids, c_ins_ids, c_sc)
                    });

                    if (cell.contains("content") && cell["content"].is_array()) {
                        for (const auto& ce : cell["content"].get<slimr::Json::array_t>()) {
                            tape_append_element(tape, ce);
                        }
                    }
                }
            }
        }
    }
    tape.push_back(TapeCell{.type = TapeCellType::TableEnd});
}

inline void tape_append_element(std::vector<TapeCell>& tape, const slimr::Json& el) {
    if (el.contains("paragraph") && el["paragraph"].is_object()) {
        tape_append_paragraph(tape, el["paragraph"]);
    } else if (el.contains("table") && el["table"].is_object()) {
        tape_append_table(tape, el["table"]);
    } else if (el.contains("tableOfContents") && el["tableOfContents"].is_object()) {
        int64_t start = el.contains("startIndex") ? el["startIndex"].as<int64_t>() : 0;
        int64_t end = el.contains("endIndex") ? el["endIndex"].as<int64_t>() : start + 1;
        int span = static_cast<int>(std::max<int64_t>(2, end - start));
        tape.push_back(TapeCell{
            .type = TapeCellType::TocStart,
            .raw = el["tableOfContents"]
        });
        for (int i = 1; i < span - 1; ++i) {
            tape.push_back(TapeCell{.type = TapeCellType::AtomCont});
        }
        tape.push_back(TapeCell{.type = TapeCellType::TocEnd});
    } else if (el.contains("sectionBreak") && el["sectionBreak"].is_object()) {
        tape.push_back(TapeCell{
            .type = TapeCellType::SectionBreak,
            .raw = el["sectionBreak"]
        });
    } else {
        throw std::runtime_error("structural element has none of paragraph, table, tableOfContents, sectionBreak");
    }
}

inline TabState tab_state_build(std::string_view tab_id, std::string_view title,
                                std::optional<std::string_view> parent_tab_id, const slimr::Json& dt) {
    TabState tab;
    tab.tab_id = std::string(tab_id);
    tab.title = std::string(title);
    if (parent_tab_id) tab.parent_tab_id = std::string(*parent_tab_id);

    if (dt.contains("body") && dt["body"].is_object() && dt["body"].contains("content") &&
        dt["body"]["content"].is_array()) {
        for (const auto& el : dt["body"]["content"].get<slimr::Json::array_t>()) {
            tape_append_element(tab.tape, el);
        }
    }

    tab.document_style = dt.contains("documentStyle") ? dt["documentStyle"] : slimr::Json::object_t{};
    tab.dropdown_definitions = dt.contains("dropdownDefinitions") ? dt["dropdownDefinitions"] : slimr::Json::object_t{};
    tab.footers = dt.contains("footers") ? dt["footers"] : slimr::Json::object_t{};
    tab.footnotes = dt.contains("footnotes") ? dt["footnotes"] : slimr::Json::object_t{};
    tab.headers = dt.contains("headers") ? dt["headers"] : slimr::Json::object_t{};
    tab.inline_objects = dt.contains("inlineObjects") ? dt["inlineObjects"] : slimr::Json::object_t{};
    tab.lists = dt.contains("lists") ? dt["lists"] : slimr::Json::object_t{};
    tab.named_ranges = dt.contains("namedRanges") ? dt["namedRanges"] : slimr::Json::object_t{};
    tab.named_styles = dt.contains("namedStyles") ? dt["namedStyles"] : slimr::Json::object_t{};
    tab.positioned_objects = dt.contains("positionedObjects") ? dt["positionedObjects"] : slimr::Json::object_t{};

    slimr::Json sugg = slimr::Json::object_t{};
    if (dt.contains("suggestedDocumentStyleChanges")) {
        sugg["suggestedDocumentStyleChanges"] = dt["suggestedDocumentStyleChanges"];
    }
    if (dt.contains("suggestedNamedStylesChanges")) {
        sugg["suggestedNamedStylesChanges"] = dt["suggestedNamedStylesChanges"];
    }
    tab.suggested = std::move(sugg);

    return tab;
}

inline std::vector<TabState> tabs_flatten_dfs(const slimr::Json& raw) {
    if (!raw.contains("tabProperties") || !raw["tabProperties"].is_object() ||
        !raw["tabProperties"].contains("tabId")) {
        throw std::runtime_error("tab is missing tabProperties.tabId");
    }
    const auto& props = raw["tabProperties"];
    std::string tab_id = props["tabId"].get<std::string>();
    std::string title = props.contains("title") && props["title"].is_string() ? props["title"].get<std::string>() : "";
    std::optional<std::string> parent_tab_id;
    if (props.contains("parentTabId") && props["parentTabId"].is_string()) {
        parent_tab_id = props["parentTabId"].get<std::string>();
    }

    const auto& dt = raw.contains("documentTab") && raw["documentTab"].is_object()
                         ? raw["documentTab"]
                         : slimr::Json::object_t{};

    TabState tab = tab_state_build(tab_id, title, parent_tab_id, dt);
    if (props.contains("iconEmoji") && props["iconEmoji"].is_string()) {
        tab.icon_emoji = props["iconEmoji"].get<std::string>();
    }

    std::vector<TabState> out;
    out.push_back(std::move(tab));

    if (raw.contains("childTabs") && raw["childTabs"].is_array()) {
        for (const auto& child : raw["childTabs"].get<slimr::Json::array_t>()) {
            auto child_tabs = tabs_flatten_dfs(child);
            for (auto& ct : child_tabs) out.push_back(std::move(ct));
        }
    }
    return out;
}

inline EmulatorState doc_state_build(const slimr::Json& json) {
    EmulatorState state;
    if (json.contains("documentId") && json["documentId"].is_string()) {
        state.document_id = json["documentId"].get<std::string>();
    }
    if (json.contains("revisionId") && json["revisionId"].is_string()) {
        state.revision_id = json["revisionId"].get<std::string>();
    }
    if (json.contains("title") && json["title"].is_string()) {
        state.title = json["title"].get<std::string>();
    }

    if (json.contains("tabs") && json["tabs"].is_array() && !json["tabs"].get<slimr::Json::array_t>().empty()) {
        for (const auto& t : json["tabs"].get<slimr::Json::array_t>()) {
            auto flattened = tabs_flatten_dfs(t);
            for (auto& tab : flattened) state.tabs.push_back(std::move(tab));
        }
    } else {
        // Single-body document
        TabState single = tab_state_build("t.0", state.title, std::nullopt, json);
        state.tabs.push_back(std::move(single));
    }
    return state;
}

// TapeReader that turns flat tape back into nested Google Docs JSON
class TapeReader {
public:
    explicit TapeReader(const std::vector<TapeCell>& tape) : tape_(tape) {}

    std::vector<slimr::Json> read_content(const std::set<TapeCellType>& stop_types) {
        std::vector<slimr::Json> out;
        while (i_ < tape_.size() && stop_types.find(tape_[i_].type) == stop_types.end()) {
            out.push_back(read_one_element());
        }
        return out;
    }

private:
    const std::vector<TapeCell>& tape_;
    size_t i_ = 0;
    int index_ = 0;

    void advance(int n) {
        i_ += n;
        index_ += n;
    }

    slimr::Json read_one_element() {
        const auto& cell = tape_[i_];
        if (cell.type == TapeCellType::SectionBreak) return read_section_break();
        if (cell.type == TapeCellType::TocStart) return read_toc();
        if (cell.type == TapeCellType::TableStart) return read_table();
        return read_paragraph();
    }

    slimr::Json read_section_break() {
        int start = index_;
        const auto& cell = tape_[i_];
        advance(1);
        return slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index_)},
            {"sectionBreak", cell.raw}
        };
    }

    slimr::Json read_toc() {
        int start = index_;
        const auto& cell = tape_[i_];
        advance(1);
        while (i_ < tape_.size() && tape_[i_].type == TapeCellType::AtomCont) advance(1);
        advance(1); // tocEnd
        return slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index_)},
            {"tableOfContents", cell.raw}
        };
    }

    slimr::Json read_table() {
        int start = index_;
        const auto& cell = tape_[i_];
        advance(1); // tableStart
        slimr::Json::array_t table_rows;
        while (i_ < tape_.size() && tape_[i_].type == TapeCellType::RowStart) {
            table_rows.push_back(read_row());
        }
        advance(1); // tableEnd

        slimr::Json tbl = cell.raw.is_object() ? cell.raw : slimr::Json::object_t{};
        tbl["tableRows"] = std::move(table_rows);

        return slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index_)},
            {"table", std::move(tbl)}
        };
    }

    slimr::Json read_row() {
        int start = index_;
        const auto& cell = tape_[i_];
        advance(1); // rowStart
        slimr::Json::array_t table_cells;
        while (i_ < tape_.size() && tape_[i_].type == TapeCellType::CellStart) {
            table_cells.push_back(read_cell());
        }

        slimr::Json row = slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index_)},
            {"tableRowStyle", cell.style},
            {"tableCells", std::move(table_cells)}
        };
        sugg_fields_for(cell.sugg, row, "suggestedTableRowStyleChanges");
        return row;
    }

    slimr::Json read_cell() {
        int start = index_;
        const auto& cell = tape_[i_];
        advance(1); // cellStart
        std::set<TapeCellType> stops = {
            TapeCellType::CellStart,
            TapeCellType::RowStart,
            TapeCellType::TableEnd
        };
        auto content_arr = read_content(stops);
        slimr::Json::array_t content;
        for (auto& el : content_arr) content.push_back(std::move(el));

        slimr::Json c = slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index_)},
            {"tableCellStyle", cell.style},
            {"content", std::move(content)}
        };
        sugg_fields_for(cell.sugg, c, "suggestedTableCellStyleChanges");
        return c;
    }

    struct RunBuffer {
        std::optional<Sugg> sugg;
        slimr::Json style;
        std::u16string text;
    };

    slimr::Json read_paragraph() {
        int start = index_;
        slimr::Json::array_t elements;
        int run_start = index_;
        std::optional<RunBuffer> buf;

        auto flush = [&]() {
            if (!buf) return;
            slimr::Json tr = slimr::Json::object_t{
                {"content", utf16_to_utf8(buf->text)},
                {"textStyle", buf->style}
            };
            sugg_fields_for(buf->sugg, tr, "suggestedTextStyleChanges");
            int run_len = static_cast<int>(buf->text.size());
            elements.push_back(slimr::Json::object_t{
                {"startIndex", static_cast<int64_t>(run_start)},
                {"endIndex", static_cast<int64_t>(run_start + run_len)},
                {"textRun", std::move(tr)}
            });
            buf.reset();
        };

        slimr::Json para_out = slimr::Json::object_t{};
        while (true) {
            if (i_ >= tape_.size()) {
                throw std::runtime_error("paragraph runs off the end of the tape without a newline");
            }
            const auto& cell = tape_[i_];
            if (cell.type == TapeCellType::Char) {
                if (buf && style_equal(buf->style, cell.style) && sugg_equal(buf->sugg, cell.sugg)) {
                    buf->text.push_back(cell.u16 != 0 ? cell.u16 : utf8_to_utf16(cell.ch)[0]);
                } else {
                    flush();
                    run_start = index_;
                    buf = RunBuffer{
                        .sugg = cell.sugg,
                        .style = cell.style,
                        .text = std::u16string(1, cell.u16 != 0 ? cell.u16 : utf8_to_utf16(cell.ch)[0])
                    };
                }
                advance(1);
                continue;
            }
            if (cell.type == TapeCellType::Atom) {
                flush();
                int atom_start = index_;
                std::string variant_key;
                slimr::Json variant_payload = slimr::Json::object_t{};
                if (cell.raw.is_object() && !cell.raw.get<slimr::Json::object_t>().empty()) {
                    variant_key = cell.raw.get<slimr::Json::object_t>().begin()->first;
                    variant_payload = cell.raw[variant_key];
                }
                if (cell.style.is_object()) {
                    variant_payload["textStyle"] = cell.style;
                }
                sugg_fields_for(cell.sugg, variant_payload, "suggestedTextStyleChanges");

                elements.push_back(slimr::Json::object_t{
                    {"startIndex", static_cast<int64_t>(atom_start)},
                    {"endIndex", static_cast<int64_t>(atom_start + cell.span)},
                    {variant_key, std::move(variant_payload)}
                });
                advance(cell.span);
                run_start = index_;
                continue;
            }
            if (cell.type == TapeCellType::Nl) {
                if (buf && style_equal(buf->style, cell.style) && sugg_equal(buf->sugg, cell.sugg)) {
                    buf->text.push_back(u'\n');
                    flush();
                } else {
                    flush();
                    run_start = index_;
                    buf = RunBuffer{
                        .sugg = cell.sugg,
                        .style = cell.style,
                        .text = u"\n"
                    };
                    flush();
                }
                slimr::Json para_style = cell.para.style.is_object() ? cell.para.style : slimr::Json::object_t{};
                if (cell.para.heading_id) {
                    para_style["headingId"] = *cell.para.heading_id;
                }

                if (cell.para.bullet) {
                    para_out["bullet"] = *cell.para.bullet;
                }
                para_out["elements"] = std::move(elements);
                para_out["paragraphStyle"] = std::move(para_style);

                if (!cell.para.positioned_object_ids.empty()) {
                    slimr::Json::array_t p_arr;
                    for (const auto& id : cell.para.positioned_object_ids) p_arr.push_back(id);
                    para_out["positionedObjectIds"] = std::move(p_arr);
                }

                if (cell.para.suggestions_raw && cell.para.suggestions_raw->is_object()) {
                    const auto& raw_s = *cell.para.suggestions_raw;
                    if (raw_s.contains("suggestedBulletChanges")) {
                        para_out["suggestedBulletChanges"] = raw_s["suggestedBulletChanges"];
                    }
                    if (raw_s.contains("suggestedParagraphStyleChanges")) {
                        para_out["suggestedParagraphStyleChanges"] = raw_s["suggestedParagraphStyleChanges"];
                    }
                    if (raw_s.contains("suggestedPositionedObjectIds")) {
                        para_out["suggestedPositionedObjectIds"] = raw_s["suggestedPositionedObjectIds"];
                    }
                }
                advance(1);
                break;
            }
            throw std::runtime_error("unexpected tape cell type inside a paragraph");
        }
        return slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(start)},
            {"endIndex", static_cast<int64_t>(index_)},
            {"paragraph", std::move(para_out)}
        };
    }
};

inline slimr::Json tab_json_build(const TabState& tab) {
    TapeReader reader(tab.tape);
    auto content_arr = reader.read_content({});
    slimr::Json::array_t content;
    for (auto& el : content_arr) content.push_back(std::move(el));

    // Footnotes pruning: deleting a footnote reference atom deletes the footnote
    slimr::Json filtered_footnotes = slimr::Json::object_t{};
    if (tab.footnotes.is_object()) {
        for (const auto& [id, body] : tab.footnotes.get<slimr::Json::object_t>()) {
            bool found = false;
            for (const auto& cell : tab.tape) {
                if (cell.type == TapeCellType::Atom && cell.raw.is_object() &&
                    cell.raw.contains("footnoteReference") && cell.raw["footnoteReference"].is_object() &&
                    cell.raw["footnoteReference"].contains("footnoteId")) {
                    if (cell.raw["footnoteReference"]["footnoteId"].get<std::string>() == id) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                filtered_footnotes[id] = body;
            }
        }
    }

    slimr::Json document_tab = slimr::Json::object_t{
        {"body", slimr::Json::object_t{{"content", std::move(content)}}},
        {"documentStyle", tab.document_style},
        {"footers", tab.footers},
        {"footnotes", std::move(filtered_footnotes)},
        {"headers", tab.headers},
        {"inlineObjects", tab.inline_objects},
        {"lists", tab.lists},
        {"namedRanges", tab.named_ranges},
        {"namedStyles", tab.named_styles},
        {"positionedObjects", tab.positioned_objects}
    };
    if (tab.dropdown_definitions.is_object() && !tab.dropdown_definitions.get<slimr::Json::object_t>().empty()) {
        document_tab["dropdownDefinitions"] = tab.dropdown_definitions;
    }
    if (tab.suggested.is_object()) {
        for (const auto& [k, v] : tab.suggested.get<slimr::Json::object_t>()) {
            document_tab[k] = v;
        }
    }

    slimr::Json tab_props = slimr::Json::object_t{
        {"tabId", tab.tab_id},
        {"title", tab.title}
    };
    if (tab.parent_tab_id) tab_props["parentTabId"] = *tab.parent_tab_id;
    if (tab.icon_emoji) tab_props["iconEmoji"] = *tab.icon_emoji;

    return slimr::Json::object_t{
        {"documentTab", std::move(document_tab)},
        {"tabProperties", std::move(tab_props)}
    };
}

inline slimr::Json doc_state_json(const EmulatorState& state) {
    std::vector<slimr::Json> built;
    built.reserve(state.tabs.size());
    for (const auto& tab : state.tabs) {
        built.push_back(tab_json_build(tab));
    }

    std::map<std::string, size_t> by_id;
    for (size_t i = 0; i < state.tabs.size(); ++i) {
        by_id[state.tabs[i].tab_id] = i;
    }

    std::map<std::string, std::vector<size_t>> children_of;
    std::vector<size_t> root_indices;
    for (size_t i = 0; i < state.tabs.size(); ++i) {
        const auto& tab = state.tabs[i];
        if (tab.parent_tab_id && by_id.contains(*tab.parent_tab_id)) {
            children_of[*tab.parent_tab_id].push_back(i);
        } else {
            root_indices.push_back(i);
        }
    }

    auto assemble = [&](auto& self, size_t idx) -> slimr::Json {
        slimr::Json node = std::move(built[idx]);
        const auto& tab_id = state.tabs[idx].tab_id;
        auto it = children_of.find(tab_id);
        if (it != children_of.end()) {
            slimr::Json::array_t child_tabs;
            for (size_t child_idx : it->second) {
                child_tabs.push_back(self(self, child_idx));
            }
            node["childTabs"] = std::move(child_tabs);
        }
        return node;
    };

    slimr::Json::array_t roots;
    for (size_t root_idx : root_indices) {
        roots.push_back(assemble(assemble, root_idx));
    }

    slimr::Json out = slimr::Json::object_t{
        {"tabs", std::move(roots)}
    };
    if (!state.document_id.empty()) out["documentId"] = state.document_id;
    if (!state.revision_id.empty()) out["revisionId"] = state.revision_id;
    if (!state.title.empty()) out["title"] = state.title;

    return out;
}

} // namespace gdocsmith::emulator
