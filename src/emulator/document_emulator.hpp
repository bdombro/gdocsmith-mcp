#pragma once

#include "types.hpp"
#include "document_state.hpp"
#include "document_mutator.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::emulator {

struct EmulateResult {
    slimr::Json json;
    std::vector<slimr::Json> replies;
};

// Forward declarations
inline slimr::Json request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx);
inline slimr::Json request_dispatch(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx);
inline slimr::Json segment_request_apply(EmulatorState& state, const slimr::Json& req,
                                         const std::string& segment_id, EmulateContext& ctx);

inline IdCounters id_counters_seed(const slimr::Json& json) {
    std::string text = slimr::write_json(json);
    IdCounters counters;

    auto update_counter = [&](int& counter, std::string_view prefix) {
        std::string pattern = std::string(prefix);
        // escape dots
        size_t p = 0;
        while ((p = pattern.find('.', p)) != std::string::npos) {
            pattern.replace(p, 1, "\\.");
            p += 2;
        }
        pattern += "(\\d+)";
        std::regex re(pattern);
        auto words_begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            int val = std::stoi(match[1].str());
            counter = std::max(counter, val);
        }
    };

    update_counter(counters.chip, MintPrefixes::chip);
    update_counter(counters.heading, MintPrefixes::heading);
    update_counter(counters.image, MintPrefixes::image);
    update_counter(counters.list, MintPrefixes::list);
    update_counter(counters.segment, MintPrefixes::segment);
    update_counter(counters.tab, MintPrefixes::tab);

    return counters;
}

inline std::optional<slimr::Json> field_find(const slimr::Json& value, std::string_view field) {
    if (!value.is_object()) return std::nullopt;
    if (value.contains(field)) return value[field];
    for (const auto& [k, entry] : value.get<slimr::Json::object_t>()) {
        auto found = field_find(entry, field);
        if (found) return found;
    }
    return std::nullopt;
}

inline void named_ranges_shift(TabState& tab, int at, int delta) {
    if (!tab.named_ranges.is_object()) return;
    auto move_pos = [&](int p, bool end) -> int {
        if (delta > 0) return (p > at || (p == at && !end)) ? p + delta : p;
        return (p <= at) ? p : (p >= at - delta) ? p + delta : at;
    };

    slimr::Json new_named_ranges = slimr::Json::object_t{};
    for (const auto& [name, entry] : tab.named_ranges.get<slimr::Json::object_t>()) {
        if (!entry.is_object() || !entry.contains("namedRanges") || !entry["namedRanges"].is_array()) continue;
        slimr::Json::array_t kept;
        for (const auto& range : entry["namedRanges"].get<slimr::Json::array_t>()) {
            if (!range.is_object() || !range.contains("ranges") || !range["ranges"].is_array()) continue;
            slimr::Json::array_t kept_ranges;
            for (const auto& r : range["ranges"].get<slimr::Json::array_t>()) {
                if (r.contains("segmentId")) {
                    kept_ranges.push_back(r);
                } else {
                    int s = r.contains("startIndex") ? r["startIndex"].as<int>() : 0;
                    int e = r.contains("endIndex") ? r["endIndex"].as<int>() : 0;
                    int new_s = move_pos(s, false);
                    int new_e = move_pos(e, true);
                    if (new_e > new_s) {
                        slimr::Json r_copy = r;
                        r_copy["startIndex"] = static_cast<int64_t>(new_s);
                        r_copy["endIndex"] = static_cast<int64_t>(new_e);
                        kept_ranges.push_back(std::move(r_copy));
                    }
                }
            }
            if (!kept_ranges.empty()) {
                slimr::Json range_copy = range;
                range_copy["ranges"] = std::move(kept_ranges);
                kept.push_back(std::move(range_copy));
            }
        }
        if (!kept.empty()) {
            slimr::Json entry_copy = entry;
            entry_copy["namedRanges"] = std::move(kept);
            new_named_ranges[name] = std::move(entry_copy);
        }
    }
    tab.named_ranges = std::move(new_named_ranges);
}

inline TabState& tab_resolve(EmulatorState& state, std::optional<std::string_view> tab_id, int request_index) {
    if (tab_id && !tab_id->empty()) {
        for (auto& tab : state.tabs) {
            if (tab.tab_id == *tab_id) return tab;
        }
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, request_index,
                            "unknown tabId \"" + std::string(*tab_id) + "\"");
    }
    if (state.tabs.size() == 1) {
        return state.tabs[0];
    }
    throw EmulatorError(EmulatorErrorCode::TAB_REQUIRED, request_index,
                        "document has more than one tab and no tabId was given");
}

struct RangeResolved {
    int start = 0;
    int end = 0;
    TabState& tab;
};

inline RangeResolved range_resolve(EmulatorState& state, const slimr::Json& range, EmulateContext& ctx) {
    if (!range.is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing range");
    }
    int start = range.contains("startIndex") ? range["startIndex"].as<int>() : 0;
    int end = range.contains("endIndex") ? range["endIndex"].as<int>() : 0;
    std::optional<std::string> tab_id;
    if (range.contains("tabId") && range["tabId"].is_string()) {
        tab_id = range["tabId"].get<std::string>();
    }
    TabState& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    return RangeResolved{.start = start, .end = end, .tab = tab};
}

inline std::string mint_heading_id(EmulateContext& ctx) {
    ctx.id_counters.heading += 1;
    return std::string(MintPrefixes::heading) + std::to_string(ctx.id_counters.heading);
}

inline std::string mint_list_id(EmulateContext& ctx) {
    ctx.id_counters.list += 1;
    return std::string(MintPrefixes::list) + std::to_string(ctx.id_counters.list);
}

inline std::string mint_inline_object_id(EmulateContext& ctx) {
    ctx.id_counters.image += 1;
    return std::string(MintPrefixes::image) + std::to_string(ctx.id_counters.image);
}

inline std::string mint_chip_id(EmulateContext& ctx) {
    ctx.id_counters.chip += 1;
    return std::string(MintPrefixes::chip) + std::to_string(ctx.id_counters.chip);
}

inline std::string mint_tab_id(EmulateContext& ctx) {
    ctx.id_counters.tab += 1;
    return std::string(MintPrefixes::tab) + std::to_string(ctx.id_counters.tab);
}

inline int paragraph_start_before(const std::vector<TapeCell>& tape, int pos) {
    int i = pos;
    while (i > 0) {
        auto t = tape[i - 1].type;
        if (t == TapeCellType::Nl || t == TapeCellType::SectionBreak ||
            t == TapeCellType::TableStart || t == TapeCellType::RowStart ||
            t == TapeCellType::CellStart || t == TapeCellType::TableEnd ||
            t == TapeCellType::TocStart || t == TapeCellType::TocEnd) {
            break;
        }
        i--;
    }
    return i;
}

inline void insert_position_validate(const TabState& tab, int idx, EmulateContext& ctx) {
    if (idx == static_cast<int>(tab.tape.size())) {
        throw EmulatorError(EmulatorErrorCode::RANGE_AT_SEGMENT_END, ctx.request_index,
                            "cannot insert past the segment's final newline");
    }
    if (idx < 0 || idx > static_cast<int>(tab.tape.size())) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "insert index " + std::to_string(idx) + " is out of bounds");
    }
    const auto& cell = tab.tape[idx];
    if (cell.type != TapeCellType::Char && cell.type != TapeCellType::Atom && cell.type != TapeCellType::Nl) {
        throw EmulatorError(EmulatorErrorCode::INSERT_OUTSIDE_PARAGRAPH, ctx.request_index,
                            "insert index " + std::to_string(idx) + " is not inside a paragraph");
    }
}

inline slimr::Json style_for_insert_at(const std::vector<TapeCell>& tape, int idx) {
    if (idx > 0) {
        const auto& prev = tape[idx - 1];
        if (prev.type == TapeCellType::Char) return prev.style;
        if (prev.type == TapeCellType::Atom) return prev.style.is_object() ? prev.style : slimr::Json::object_t{};
        if (prev.type == TapeCellType::AtomCont) {
            int j = idx - 1;
            while (j >= 0 && tape[j].type == TapeCellType::AtomCont) j--;
            if (j >= 0 && tape[j].type == TapeCellType::Atom) {
                return tape[j].style.is_object() ? tape[j].style : slimr::Json::object_t{};
            }
        }
    }
    if (idx < static_cast<int>(tape.size())) {
        const auto& next = tape[idx];
        if (next.type == TapeCellType::Char) return next.style;
        if (next.type == TapeCellType::Atom) return next.style.is_object() ? next.style : slimr::Json::object_t{};
        if (next.type == TapeCellType::Nl) return next.style;
    }
    return slimr::Json::object_t{};
}

inline std::optional<Sugg> sugg_for_insert_at(const std::vector<TapeCell>& tape, int idx) {
    int start = paragraph_start_before(tape, idx);
    auto marks = [](const TapeCell& c) -> std::optional<Sugg> {
        if (c.type == TapeCellType::Char || c.type == TapeCellType::Atom || c.type == TapeCellType::Nl) {
            return c.sugg;
        }
        return std::nullopt;
    };

    int before = idx - 1;
    while (before >= start && tape[before].type == TapeCellType::AtomCont) before--;
    std::optional<Sugg> previous = (before >= start) ? marks(tape[before]) : std::nullopt;
    if (sugg_present(previous) && sugg_equal(previous, marks(tape[idx]))) {
        return previous;
    }

    int end = idx;
    while (end < static_cast<int>(tape.size()) && tape[end].type != TapeCellType::Nl) end++;
    if (end >= static_cast<int>(tape.size())) return std::nullopt;

    std::vector<std::optional<Sugg>> cells_sugg;
    for (int k = start; k <= end; ++k) {
        if (tape[k].type != TapeCellType::AtomCont) {
            cells_sugg.push_back(marks(tape[k]));
        }
    }
    if (cells_sugg.empty()) return std::nullopt;
    const auto& first = cells_sugg[0];
    if (sugg_present(first)) {
        bool all_same = true;
        for (const auto& s : cells_sugg) {
            if (!sugg_equal(s, first)) {
                all_same = false;
                break;
            }
        }
        if (all_same) return first;
    }
    return std::nullopt;
}

inline void paragraph_split_at(TabState& tab, int pos, const slimr::Json& style,
                               EmulateContext& ctx, bool original_keeps_later_side) {
    int j = pos;
    while (j < static_cast<int>(tab.tape.size()) && tab.tape[j].type != TapeCellType::Nl) j++;
    if (j >= static_cast<int>(tab.tape.size())) {
        throw std::runtime_error("paragraph does not terminate with newline");
    }
    NlPara orig_para = tab.tape[j].para;

    if (original_keeps_later_side) {
        NlPara new_para = orig_para;
        new_para.suggestions_raw.reset();
        if (new_para.heading_id) {
            new_para.heading_id = mint_heading_id(ctx);
        }
        tab.tape.insert(tab.tape.begin() + pos, TapeCell{
            .type = TapeCellType::Nl,
            .ch = "\n",
            .u16 = u'\n',
            .style = style,
            .para = std::move(new_para)
        });
    } else {
        NlPara earlier_para = orig_para;
        earlier_para.suggestions_raw.reset();
        tab.tape.insert(tab.tape.begin() + pos, TapeCell{
            .type = TapeCellType::Nl,
            .ch = "\n",
            .u16 = u'\n',
            .style = style,
            .para = std::move(earlier_para)
        });
        if (orig_para.heading_id) {
            tab.tape[j + 1].para.heading_id = mint_heading_id(ctx);
        }
    }
}

inline void insert_text_core(TabState& tab, int idx, std::string_view text, EmulateContext& ctx) {
    insert_position_validate(tab, idx, ctx);
    slimr::Json style = style_for_insert_at(tab.tape, idx);
    std::optional<Sugg> sugg = (text.find('\n') != std::string_view::npos) ? std::nullopt : sugg_for_insert_at(tab.tape, idx);
    bool original_keeps_later_side = (idx == paragraph_start_before(tab.tape, idx));

    std::u16string u16 = utf8_to_utf16(text);
    int pos = idx;
    for (size_t k = 0; k < u16.size(); ++k) {
        char16_t ch16 = u16[k];
        if (ch16 == u'\n') {
            paragraph_split_at(tab, pos, style, ctx, original_keeps_later_side);
        } else {
            std::u16string single(1, ch16);
            tab.tape.insert(tab.tape.begin() + pos, TapeCell{
                .type = TapeCellType::Char,
                .ch = utf16_to_utf8(single),
                .u16 = ch16,
                .style = style,
                .sugg = sugg
            });
        }
        pos += 1;
    }
}

inline slimr::Json insert_text_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("text") || !req["text"].is_string()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "insertText.text must be a string");
    }
    std::string text = req["text"].get<std::string>();
    std::optional<std::string> tab_id;
    int idx = 0;

    if (req.contains("endOfSegmentLocation") && req["endOfSegmentLocation"].is_object()) {
        const auto& eos = req["endOfSegmentLocation"];
        if (eos.contains("tabId") && eos["tabId"].is_string()) tab_id = eos["tabId"].get<std::string>();
        auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
        idx = static_cast<int>(tab.tape.size()) - 1;
        int target = hidden_cell_redirect(tab, idx).value_or(idx);
        insert_text_core(tab, target, text, ctx);
    } else if (req.contains("location") && req["location"].is_object()) {
        const auto& loc = req["location"];
        if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
        idx = loc.contains("index") ? loc["index"].as<int>() : 0;
        auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
        int target = hidden_cell_redirect(tab, idx).value_or(idx);
        insert_text_core(tab, target, text, ctx);
    } else {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    return slimr::Json::object_t{};
}

inline std::vector<CellPos> structural_spans(const std::vector<TapeCell>& tape) {
    std::vector<CellPos> spans;
    std::vector<int> stack;
    for (size_t i = 0; i < tape.size(); ++i) {
        if (tape[i].type == TapeCellType::TableStart || tape[i].type == TapeCellType::TocStart) {
            stack.push_back(static_cast<int>(i));
        } else if (tape[i].type == TapeCellType::TableEnd || tape[i].type == TapeCellType::TocEnd) {
            if (!stack.empty()) {
                int open_idx = stack.back();
                stack.pop_back();
                spans.push_back(CellPos{.start = open_idx, .end = static_cast<int>(i) + 1});
            }
        }
    }
    return spans;
}

inline bool is_high_surrogate(char16_t c) {
    return c >= 0xD800 && c <= 0xDBFF;
}

inline bool is_low_surrogate(char16_t c) {
    return c >= 0xDC00 && c <= 0xDFFF;
}

inline void delete_range_validate(const TabState& tab, int start, int end, EmulateContext& ctx) {
    if (end <= start) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "range must have endIndex > startIndex");
    }
    if (end > static_cast<int>(tab.tape.size())) {
        throw EmulatorError(EmulatorErrorCode::RANGE_AT_SEGMENT_END, ctx.request_index,
                            "range extends past the end of the segment");
    }
    if (end == static_cast<int>(tab.tape.size())) {
        throw EmulatorError(EmulatorErrorCode::DELETE_LAST_NEWLINE, ctx.request_index,
                            "cannot delete the final newline of the segment");
    }

    auto spans = structural_spans(tab.tape);
    for (int i = start; i < end; ++i) {
        const auto& cell = tab.tape[i];
        if (cell.type == TapeCellType::Atom || cell.type == TapeCellType::AtomCont) {
            int atom_start = i;
            while (atom_start > 0 && tab.tape[atom_start].type == TapeCellType::AtomCont) atom_start--;
            int atom_end = atom_start + tab.tape[atom_start].span;
            if (start > atom_start || end < atom_end) {
                throw EmulatorError(EmulatorErrorCode::DELETE_PARTIAL_STRUCTURE, ctx.request_index,
                                    "range partially overlaps an atomic (unsplittable) element");
            }
        }
        if (cell.type == TapeCellType::TableStart || cell.type == TapeCellType::TocStart ||
            cell.type == TapeCellType::TableEnd || cell.type == TapeCellType::TocEnd ||
            cell.type == TapeCellType::RowStart || cell.type == TapeCellType::CellStart) {
            const CellPos* inner_span = nullptr;
            for (const auto& s : spans) {
                if (i >= s.start && i < s.end) {
                    if (!inner_span || s.start > inner_span->start) {
                        inner_span = &s;
                    }
                }
            }
            if (inner_span && (start > inner_span->start || end < inner_span->end)) {
                throw EmulatorError(EmulatorErrorCode::DELETE_PARTIAL_STRUCTURE, ctx.request_index,
                                    "range partially overlaps a table or table of contents");
            }
        }
        if (cell.type == TapeCellType::Nl && i == end - 1) {
            if (i + 1 < static_cast<int>(tab.tape.size())) {
                const auto& next = tab.tape[i + 1];
                if (next.type == TapeCellType::TableStart || next.type == TapeCellType::TocStart ||
                    next.type == TapeCellType::SectionBreak) {
                    throw EmulatorError(EmulatorErrorCode::DELETE_NEWLINE_BEFORE_STRUCTURE, ctx.request_index,
                                        "cannot delete the newline immediately before a table, TOC, or section break without also deleting it");
                }
            }
        }
    }

    if (start < static_cast<int>(tab.tape.size())) {
        const auto& start_cell = tab.tape[start];
        if (start_cell.type == TapeCellType::Char && is_low_surrogate(start_cell.u16) && start > 0) {
            const auto& before = tab.tape[start - 1];
            if (before.type == TapeCellType::Char && is_high_surrogate(before.u16)) {
                throw EmulatorError(EmulatorErrorCode::SURROGATE_SPLIT, ctx.request_index,
                                    "range starts inside a surrogate pair");
            }
        }
    }
    if (end - 1 >= 0 && end - 1 < static_cast<int>(tab.tape.size())) {
        const auto& last_included = tab.tape[end - 1];
        if (last_included.type == TapeCellType::Char && is_high_surrogate(last_included.u16)) {
            if (end < static_cast<int>(tab.tape.size())) {
                const auto& after = tab.tape[end];
                if (after.type == TapeCellType::Char && is_low_surrogate(after.u16)) {
                    throw EmulatorError(EmulatorErrorCode::SURROGATE_SPLIT, ctx.request_index,
                                        "range ends inside a surrogate pair");
                }
            }
        }
    }
}

inline slimr::Json delete_content_range_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    delete_range_validate(tab, start, end, ctx);

    int a_nl = start;
    while (a_nl < end && a_nl < static_cast<int>(tab.tape.size()) && tab.tape[a_nl].type != TapeCellType::Nl) a_nl++;
    std::optional<NlPara> a_para;
    if (a_nl < end && start > paragraph_start_before(tab.tape, start)) {
        a_para = tab.tape[a_nl].para;
    }
    bool re_break = (a_para.has_value() && (end - start == 1));

    tab.tape.erase(tab.tape.begin() + start, tab.tape.begin() + end);

    if (a_para) {
        int survivor = start;
        while (survivor < static_cast<int>(tab.tape.size()) && tab.tape[survivor].type != TapeCellType::Nl) survivor++;
        if (survivor < static_cast<int>(tab.tape.size()) && tab.tape[survivor].type == TapeCellType::Nl) {
            auto& nl = tab.tape[survivor];
            if (re_break || !a_para->suggestions_raw) {
                NlPara copy = *a_para;
                copy.suggestions_raw.reset();
                nl.para = std::move(copy);
            } else {
                nl.para = *a_para;
            }
        }
    }
    return slimr::Json::object_t{};
}

inline slimr::Json link_normalize(const slimr::Json& patch, std::string_view tab_id) {
    if (!patch.is_object() || !patch.contains("link") || !patch["link"].is_object()) return patch;
    const auto& link = patch["link"];
    if (!link.contains("headingId") || !link["headingId"].is_string()) return patch;

    slimr::Json copy = patch;
    std::string h_tab_id = link.contains("tabId") && link["tabId"].is_string()
                               ? link["tabId"].get<std::string>()
                               : std::string(tab_id);
    copy["link"] = slimr::Json::object_t{
        {"heading", slimr::Json::object_t{
            {"id", link["headingId"].get<std::string>()},
            {"tabId", std::move(h_tab_id)}
        }}
    };
    return copy;
}

inline std::optional<slimr::Json> inherited_text_style_value(const TabState& tab, int index, const std::string& field) {
    int nl_index = index;
    while (nl_index < static_cast<int>(tab.tape.size()) && tab.tape[nl_index].type != TapeCellType::Nl) nl_index++;
    std::string type = "NORMAL_TEXT";
    if (nl_index < static_cast<int>(tab.tape.size()) && tab.tape[nl_index].type == TapeCellType::Nl) {
        if (tab.tape[nl_index].para.style.is_object() && tab.tape[nl_index].para.style.contains("namedStyleType")) {
            type = tab.tape[nl_index].para.style["namedStyleType"].get<std::string>();
        }
    }

    if (!tab.named_styles.is_object() || !tab.named_styles.contains("styles") || !tab.named_styles["styles"].is_array()) {
        return std::nullopt;
    }
    const auto& styles = tab.named_styles["styles"].get<slimr::Json::array_t>();
    const slimr::Json* own_style = nullptr;
    const slimr::Json* normal_style = nullptr;
    for (const auto& st : styles) {
        if (!st.is_object() || !st.contains("namedStyleType")) continue;
        std::string st_type = st["namedStyleType"].get<std::string>();
        if (st_type == type) own_style = st.contains("textStyle") ? &st["textStyle"] : nullptr;
        if (st_type == "NORMAL_TEXT") normal_style = st.contains("textStyle") ? &st["textStyle"] : nullptr;
    }
    if (own_style && own_style->is_object() && own_style->contains(field)) return (*own_style)[field];
    if (normal_style && normal_style->is_object() && normal_style->contains(field)) return (*normal_style)[field];
    return std::nullopt;
}

inline slimr::Json text_style_apply(const TabState& tab, int index, const slimr::Json& style,
                                    const slimr::Json& patch, const std::vector<std::string>& fields) {
    slimr::Json next = apply_style_fields(style, patch, fields);
    bool has_link = (std::find(fields.begin(), fields.end(), "link") != fields.end());
    if (has_link) {
        if (patch.is_object() && patch.contains("link") && !patch["link"].is_null()) {
            if (std::find(fields.begin(), fields.end(), "underline") == fields.end()) next["underline"] = true;
            if (std::find(fields.begin(), fields.end(), "foregroundColor") == fields.end()) {
                next["foregroundColor"] = style_canonical(make_link_chrome_color());
            }
        } else {
            if (std::find(fields.begin(), fields.end(), "underline") == fields.end()) {
                if (next.is_object() && next.contains("underline") && json_bool(next["underline"])) {
                    next.get<slimr::Json::object_t>().erase("underline");
                }
            }
            if (std::find(fields.begin(), fields.end(), "foregroundColor") == fields.end()) {
                if (next.is_object() && next.contains("foregroundColor")) {
                    if (style_equal(next["foregroundColor"], style_canonical(make_link_chrome_color()))) {
                        next.get<slimr::Json::object_t>().erase("foregroundColor");
                    }
                }
            }
        }
    }
    for (const auto& field : TEXT_STYLE_INHERIT_DROP_FIELDS) {
        if (std::find(fields.begin(), fields.end(), field) == fields.end()) continue;
        if (!next.is_object() || !next.contains(field)) continue;
        auto inherited = inherited_text_style_value(tab, index, field);
        if (inherited && style_equal(next[field], *inherited)) {
            next.get<slimr::Json::object_t>().erase(field);
        }
    }
    return next;
}

inline slimr::Json update_text_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    slimr::Json patch = link_normalize(req.contains("textStyle") ? req["textStyle"] : slimr::Json::object_t{}, tab.tab_id);

    for (int i = start; i < end; ++i) {
        auto& cell = tab.tape[i];
        if (cell.type == TapeCellType::AtomCont) continue;
        if (cell.type != TapeCellType::Char && cell.type != TapeCellType::Nl && cell.type != TapeCellType::Atom) {
            throw EmulatorError(EmulatorErrorCode::INSERT_OUTSIDE_PARAGRAPH, ctx.request_index,
                                "text style range at index " + std::to_string(i) + " covers a non-text element");
        }
        cell.style = text_style_apply(tab, i, cell.style, patch, fields);
    }

    if (end < static_cast<int>(tab.tape.size())) {
        auto& nl = tab.tape[end];
        std::vector<std::string> nl_fields;
        for (const auto& f : fields) if (f != "link") nl_fields.push_back(f);
        bool ends_text = (end > start && tab.tape[end - 1].type != TapeCellType::Nl);
        if (nl.type == TapeCellType::Nl && ends_text && !nl_fields.empty()) {
            nl.style = text_style_apply(tab, end, nl.style, patch, nl_fields);
        }
    }
    return slimr::Json::object_t{};
}

inline bool tape_in_table(const std::vector<TapeCell>& tape, int i) {
    int depth = 0;
    for (int k = 0; k < i && k < static_cast<int>(tape.size()); ++k) {
        if (tape[k].type == TapeCellType::TableStart) depth++;
        else if (tape[k].type == TapeCellType::TableEnd) depth--;
    }
    return depth > 0;
}

inline std::vector<int> paragraph_newlines_overlapping(const std::vector<TapeCell>& tape, int start, int end) {
    std::vector<int> out;
    int limit = std::max(end, start + 1);
    for (int i = start; i < limit && i < static_cast<int>(tape.size()); ++i) {
        int nl = i;
        while (nl < static_cast<int>(tape.size()) && tape[nl].type != TapeCellType::Nl) {
            if (tape[nl].type != TapeCellType::Char && tape[nl].type != TapeCellType::Atom &&
                tape[nl].type != TapeCellType::AtomCont) {
                break;
            }
            nl++;
        }
        if (nl < static_cast<int>(tape.size()) && tape[nl].type == TapeCellType::Nl) {
            out.push_back(nl);
            i = nl;
        }
    }
    return out;
}

inline void paragraph_text_styles_reset(std::vector<TapeCell>& tape, int start, int nl) {
    for (int k = start; k <= nl && k < static_cast<int>(tape.size()); ++k) {
        auto& cell = tape[k];
        if (cell.type != TapeCellType::Char && cell.type != TapeCellType::Atom && cell.type != TapeCellType::Nl) continue;
        std::optional<slimr::Json> link;
        if (cell.style.is_object() && cell.style.contains("link")) link = cell.style["link"];
        if (link) {
            cell.style = slimr::Json::object_t{
                {"foregroundColor", style_canonical(make_link_chrome_color())},
                {"link", *link},
                {"underline", true}
            };
        } else {
            cell.style = slimr::Json::object_t{};
        }
    }
}

inline slimr::Json update_paragraph_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    slimr::Json patch = req.contains("paragraphStyle") && req["paragraphStyle"].is_object()
                            ? req["paragraphStyle"]
                            : slimr::Json::object_t{};

    if (std::find(fields.begin(), fields.end(), "namedStyleType") != fields.end()) {
        if (!patch.contains("namedStyleType") || !patch["namedStyleType"].is_string()) {
            throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                                "Named style property is not inherited and cannot be cleared.");
        }
    }
    auto newlines = paragraph_newlines_overlapping(tab.tape, start, end);
    if (std::find(fields.begin(), fields.end(), "pageBreakBefore") != fields.end()) {
        for (int i : newlines) {
            if (tape_in_table(tab.tape, i)) {
                throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                                    "Cannot update page-break-before when the range contains paragraphs in a table.");
            }
        }
    }

    for (int i : newlines) {
        auto& cell = tab.tape[i];
        std::string before_type = "NORMAL_TEXT";
        if (cell.para.style.is_object() && cell.para.style.contains("namedStyleType")) {
            before_type = cell.para.style["namedStyleType"].get<std::string>();
        }
        if (std::find(fields.begin(), fields.end(), "namedStyleType") != fields.end()) {
            std::string patch_type = patch["namedStyleType"].get<std::string>();
            if (before_type == patch_type) {
                paragraph_text_styles_reset(tab.tape, paragraph_start_before(tab.tape, i), i);
            }
        }

        cell.para.style = apply_style_fields(cell.para.style, patch, fields);
        if (!cell.para.style.contains("direction")) {
            cell.para.style["direction"] = "LEFT_TO_RIGHT";
        }

        if (std::find(fields.begin(), fields.end(), "namedStyleType") != fields.end()) {
            std::string new_type = patch["namedStyleType"].get<std::string>();
            bool was_heading = cell.para.heading_id.has_value();
            bool becomes_heading = (new_type != "NORMAL_TEXT");
            if (!was_heading && becomes_heading) {
                cell.para.heading_id = mint_heading_id(ctx);
            } else if (was_heading && !becomes_heading) {
                cell.para.heading_id.reset();
            }
        }
    }
    return slimr::Json::object_t{};
}

inline void bullets_modeled_assert(const TabState& tab, int start, int end,
                                   const BulletPreset& preset, EmulateContext& ctx) {
    auto newlines = paragraph_newlines_overlapping(tab.tape, start, end);
    for (int i : newlines) {
        const auto& nl = tab.tape[i];
        if (!nl.para.bullet && nl.para.style.is_object() && nl.para.style.contains("indentStart")) {
            const auto& ind = nl.para.style["indentStart"];
            if (ind.is_object() && ind.contains("magnitude") && ind["magnitude"].as<double>() > 0) {
                throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index,
                                    "bulleting an indented paragraph");
            }
        }
    }

    std::vector<int> bulleted;
    for (int i : newlines) {
        if (tab.tape[i].para.bullet.has_value()) bulleted.push_back(i);
    }
    if (bulleted.empty()) return;

    if (bulleted.size() < newlines.size()) {
        throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index,
                            "bulleting over existing list items re-lists them");
    }
    for (int i : bulleted) {
        const auto& b = *tab.tape[i].para.bullet;
        std::string list_id = b.contains("listId") ? b["listId"].get<std::string>() : "";
        if (tab.lists.is_object() && tab.lists.contains(list_id)) {
            const auto& list_def = tab.lists[list_id];
            if (list_def.contains("listProperties") && list_def["listProperties"].contains("nestingLevels") &&
                list_def["listProperties"]["nestingLevels"].is_array()) {
                auto inferred = list_preset_infer(list_def["listProperties"]["nestingLevels"].get<slimr::Json::array_t>(),
                                                   list_preset_table());
                if (inferred != preset) {
                    throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index,
                                        "bulleting over existing list items re-lists them");
                }
            }
        }
    }
}

inline std::optional<std::string> same_list_before(const TabState& tab, int p_start, const BulletPreset& preset) {
    if (p_start <= 0) return std::nullopt;
    const auto& prev = tab.tape[p_start - 1];
    if (prev.type != TapeCellType::Nl || !prev.para.bullet) return std::nullopt;
    const auto& b = *prev.para.bullet;
    if (!b.contains("listId") || !b["listId"].is_string()) return std::nullopt;
    std::string list_id = b["listId"].get<std::string>();
    if (!tab.lists.is_object() || !tab.lists.contains(list_id)) return std::nullopt;
    const auto& list_def = tab.lists[list_id];
    if (!list_def.contains("listProperties") || !list_def["listProperties"].contains("nestingLevels") ||
        !list_def["listProperties"]["nestingLevels"].is_array()) return std::nullopt;
    auto inferred = list_preset_infer(list_def["listProperties"]["nestingLevels"].get<slimr::Json::array_t>(),
                                       list_preset_table());
    return (inferred == preset) ? std::optional<std::string>(list_id) : std::nullopt;
}

inline std::string list_mint(TabState& tab, const BulletPreset& preset, EmulateContext& ctx) {
    std::string list_id = mint_list_id(ctx);
    const auto& table = list_preset_table();
    auto it = table.find(preset);
    slimr::Json::array_t levels;
    if (it != table.end()) {
        for (const auto& l : it->second) levels.push_back(l);
    } else {
        levels.resize(9, slimr::Json::object_t{});
    }
    if (!tab.lists.is_object()) tab.lists = slimr::Json::object_t{};
    tab.lists[list_id] = slimr::Json::object_t{
        {"listProperties", slimr::Json::object_t{{"nestingLevels", std::move(levels)}}}
    };
    return list_id;
}

inline void create_paragraph_bullets_apply(TabState& tab, int start, int end,
                                           const BulletPreset& preset, EmulateContext& ctx) {
    bullets_modeled_assert(tab, start, end, preset, ctx);
    int pos = start;
    int bound_end = end;
    std::optional<std::string> new_list_id;
    bool prev_minted_here = false;

    while (pos < bound_end) {
        int p_start = paragraph_start_before(tab.tape, pos);
        int nl_index = pos;
        while (nl_index < static_cast<int>(tab.tape.size()) && tab.tape[nl_index].type != TapeCellType::Nl) nl_index++;
        if (nl_index >= static_cast<int>(tab.tape.size())) break;

        if (tab.tape[nl_index].para.bullet) {
            prev_minted_here = false;
            pos = nl_index + 1;
            continue;
        }

        int tab_count = 0;
        while (p_start + tab_count < static_cast<int>(tab.tape.size()) &&
               tab.tape[p_start + tab_count].type == TapeCellType::Char &&
               tab.tape[p_start + tab_count].ch == "\t") {
            tab_count++;
        }
        if (tab_count > 0) {
            tab.tape.erase(tab.tape.begin() + p_start, tab.tape.begin() + (p_start + tab_count));
            nl_index -= tab_count;
            bound_end -= tab_count;
        }

        auto& nl_cell = tab.tape[nl_index];

        std::string list_id;
        int nesting = 0;
        auto join_id = prev_minted_here ? std::nullopt : same_list_before(tab, p_start, preset);
        const TapeCell* before = (p_start > 0) ? &tab.tape[p_start - 1] : nullptr;

        if (!join_id && !prev_minted_here && tab_count > 0 && before &&
            before->type == TapeCellType::Nl && before->para.bullet) {
            throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index,
                                "a new list that starts nested right after another list");
        }

        if (join_id) {
            list_id = *join_id;
            nesting = (before && before->type == TapeCellType::Nl && before->para.bullet &&
                       before->para.bullet->contains("nestingLevel"))
                          ? before->para.bullet->operator[]("nestingLevel").as<int>()
                          : 0;
        } else {
            if (!new_list_id) new_list_id = list_mint(tab, preset, ctx);
            list_id = *new_list_id;
            nesting = std::min(tab_count, 8);
        }
        prev_minted_here = (new_list_id && list_id == *new_list_id);

        slimr::Json level = slimr::Json::object_t{};
        if (tab.lists.is_object() && tab.lists.contains(list_id)) {
            const auto& list_def = tab.lists[list_id];
            if (list_def.contains("listProperties") && list_def["listProperties"].contains("nestingLevels") &&
                list_def["listProperties"]["nestingLevels"].is_array()) {
                const auto& nls = list_def["listProperties"]["nestingLevels"].get<slimr::Json::array_t>();
                if (nesting >= 0 && nesting < static_cast<int>(nls.size())) {
                    level = nls[nesting];
                }
            }
        }

        slimr::Json style = nl_cell.para.style.is_object() ? nl_cell.para.style : slimr::Json::object_t{};
        std::string named_style_type = style.contains("namedStyleType") && style["namedStyleType"].is_string()
                                           ? style["namedStyleType"].get<std::string>()
                                           : "NORMAL_TEXT";
        std::string inherited_spacing = "COLLAPSE_LISTS";
        if (tab.named_styles.is_object() && tab.named_styles.contains("styles") &&
            tab.named_styles["styles"].is_array()) {
            const auto& styles_arr = tab.named_styles["styles"].get<slimr::Json::array_t>();
            for (const auto& entry : styles_arr) {
                if (entry.is_object() && entry.contains("namedStyleType") &&
                    entry["namedStyleType"].is_string() &&
                    entry["namedStyleType"].get<std::string>() == named_style_type) {
                    if (entry.contains("paragraphStyle") && entry["paragraphStyle"].is_object() &&
                        entry["paragraphStyle"].contains("spacingMode") &&
                        entry["paragraphStyle"]["spacingMode"].is_string()) {
                        inherited_spacing = entry["paragraphStyle"]["spacingMode"].get<std::string>();
                    }
                    break;
                }
            }
        }
        if (inherited_spacing != "COLLAPSE_LISTS" || style.contains("spacingMode")) {
            style["spacingMode"] = "COLLAPSE_LISTS";
        }
        for (const auto& field : {"indentFirstLine", "indentStart"}) {
            if (level.is_object() && level.contains(field)) {
                style[field] = style_canonical(level[field]);
            } else if (style.is_object() && style.contains(field)) {
                style.get<slimr::Json::object_t>().erase(field);
            }
        }

        slimr::Json bullet = slimr::Json::object_t{
            {"listId", list_id},
            {"nestingLevel", static_cast<int64_t>(nesting)}
        };
        if (level.is_object() && level.contains("textStyle")) {
            bullet["textStyle"] = level["textStyle"];
        }

        nl_cell.para.bullet = std::move(bullet);
        nl_cell.para.style = std::move(style);
        pos = nl_index + 1;
    }
}

inline slimr::Json create_paragraph_bullets_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    std::string preset = req.contains("bulletPreset") ? req["bulletPreset"].get<std::string>() : "BULLET_DISC_CIRCLE_SQUARE";
    create_paragraph_bullets_apply(tab, start, end, preset, ctx);
    return slimr::Json::object_t{};
}

inline slimr::Json delete_paragraph_bullets_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    auto newlines = paragraph_newlines_overlapping(tab.tape, start, end);
    for (int i : newlines) {
        auto& cell = tab.tape[i];
        if (!cell.para.bullet) continue;
        int nesting = cell.para.bullet->contains("nestingLevel") ? cell.para.bullet->operator[]("nestingLevel").as<int>() : 0;
        slimr::Json style = cell.para.style.is_object() ? cell.para.style : slimr::Json::object_t{};
        style["indentStart"] = slimr::Json::object_t{{"unit", "PT"}};
        if (style.contains("indentFirstLine")) {
            style.get<slimr::Json::object_t>().erase("indentFirstLine");
        }
        if (nesting > 0) {
            style["indentStart"] = slimr::Json::object_t{{"magnitude", static_cast<double>(36 * nesting)}, {"unit", "PT"}};
            style["indentFirstLine"] = slimr::Json::object_t{{"magnitude", static_cast<double>(36 * nesting)}, {"unit", "PT"}};
        }
        cell.para.bullet.reset();
        cell.para.style = std::move(style);
    }
    return slimr::Json::object_t{};
}

inline slimr::Json insert_page_break_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    if (tape_in_table(tab.tape, idx)) {
        throw EmulatorError(EmulatorErrorCode::INSERT_IN_TABLE, ctx.request_index,
                            "Cannot insert page break in a table (X6a)");
    }
    slimr::Json style = style_for_insert_at(tab.tape, idx);
    bool original_keeps_later_side = (idx == paragraph_start_before(tab.tape, idx));

    tab.tape.insert(tab.tape.begin() + idx, TapeCell{
        .type = TapeCellType::Atom,
        .style = style,
        .raw = slimr::Json::object_t{{"pageBreak", slimr::Json::object_t{}}},
        .span = 1
    });
    paragraph_split_at(tab, idx + 1, style, ctx, original_keeps_later_side);
    return slimr::Json::object_t{};
}

inline slimr::Json insert_section_break_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    if (tape_in_table(tab.tape, idx)) {
        throw EmulatorError(EmulatorErrorCode::INSERT_IN_TABLE, ctx.request_index,
                            "Cannot insert section break in a table (X5)");
    }
    slimr::Json style = style_for_insert_at(tab.tape, idx);
    bool original_keeps_later_side = (idx == paragraph_start_before(tab.tape, idx));

    paragraph_split_at(tab, idx, style, ctx, original_keeps_later_side);
    std::string section_type = req.contains("sectionType") && req["sectionType"].is_string()
                                   ? req["sectionType"].get<std::string>()
                                   : "CONTINUOUS";
    slimr::Json sec_style = make_section_style_default();
    sec_style["sectionType"] = std::move(section_type);

    tab.tape.insert(tab.tape.begin() + (idx + 1), TapeCell{
        .type = TapeCellType::SectionBreak,
        .raw = slimr::Json::object_t{{"sectionStyle", std::move(sec_style)}}
    });
    return slimr::Json::object_t{};
}

inline const std::set<std::string> SECTION_STYLE_FIELDS = {
    "columnProperties", "columnSeparatorStyle", "contentDirection", "defaultFooterId",
    "defaultHeaderId", "evenPageFooterId", "evenPageHeaderId", "firstPageFooterId",
    "firstPageHeaderId", "flipPageOrientation", "marginBottom", "marginFooter",
    "marginHeader", "marginLeft", "marginRight", "marginTop", "pageNumberStart",
    "sectionType", "useFirstPageHeaderFooter"
};

inline slimr::Json update_section_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    slimr::Json patch = req.contains("sectionStyle") && req["sectionStyle"].is_object()
                            ? req["sectionStyle"]
                            : slimr::Json::object_t{};

    if (patch.is_object()) {
        for (const auto& [k, v] : patch.get<slimr::Json::object_t>()) {
            if (!SECTION_STYLE_FIELDS.contains(std::string(k))) {
                throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                                    "Unknown name \"" + std::string(k) + "\" in sectionStyle");
            }
        }
    }

    for (const auto& field : fields) {
        if ((field.starts_with("margin") || field == "pageNumberStart" ||
             field == "flipPageOrientation" || field == "useFirstPageHeaderFooter") &&
            (!patch.is_object() || !patch.contains(field))) {
            throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                                "A value for " + field + " must be specified in order to update it.");
        }
    }
    bool has_col_props = (std::find(fields.begin(), fields.end(), "columnProperties") != fields.end());
    if (has_col_props) {
        const auto* columns = patch.is_object() && patch.contains("columnProperties") && patch["columnProperties"].is_array()
                                  ? &patch["columnProperties"].get<slimr::Json::array_t>()
                                  : nullptr;
        if (!columns || columns->empty()) {
            throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                                "At least one set of column properties must be specified in order to update them.");
        }
        for (const auto& c : *columns) {
            if (!c.is_object() || !c.contains("paddingEnd")) {
                throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                                    "Column padding must be set in order to update column properties.");
            }
        }
    }

    std::vector<int> breaks;
    for (size_t i = 0; i < tab.tape.size(); ++i) {
        if (tab.tape[i].type == TapeCellType::SectionBreak) breaks.push_back(static_cast<int>(i));
    }

    for (size_t k = 0; k < breaks.size(); ++k) {
        int i = breaks[k];
        int next_break = (k + 1 < breaks.size()) ? breaks[k + 1] : static_cast<int>(tab.tape.size());
        if (!(start < next_break && end > i)) continue;

        auto& cell = tab.tape[i];
        slimr::Json cur_style = (cell.raw.is_object() && cell.raw.contains("sectionStyle"))
                                    ? cell.raw["sectionStyle"]
                                    : slimr::Json::object_t{};
        slimr::Json sec_style = apply_style_fields(cur_style, patch, fields);

        if (sec_style.contains("columnProperties") && sec_style["columnProperties"].is_array()) {
            auto& cols = sec_style["columnProperties"].get<slimr::Json::array_t>();
            double page_w = 612.0;
            if (tab.document_style.is_object() && tab.document_style.contains("pageSize") &&
                tab.document_style["pageSize"].contains("width")) {
                page_w = tab.document_style["pageSize"]["width"].contains("magnitude")
                             ? tab.document_style["pageSize"]["width"]["magnitude"].as<double>()
                             : 612.0;
            }
            double left = 72.0, right = 72.0;
            if (sec_style.contains("marginLeft") && sec_style["marginLeft"].contains("magnitude")) {
                left = sec_style["marginLeft"]["magnitude"].as<double>();
            } else if (tab.document_style.is_object() && tab.document_style.contains("marginLeft")) {
                left = tab.document_style["marginLeft"]["magnitude"].as<double>();
            }
            if (sec_style.contains("marginRight") && sec_style["marginRight"].contains("magnitude")) {
                right = sec_style["marginRight"]["magnitude"].as<double>();
            } else if (tab.document_style.is_object() && tab.document_style.contains("marginRight")) {
                right = tab.document_style["marginRight"]["magnitude"].as<double>();
            }
            double padding = 0.0;
            for (const auto& col : cols) {
                if (col.is_object() && col.contains("paddingEnd") && col["paddingEnd"].contains("magnitude")) {
                    padding += col["paddingEnd"]["magnitude"].as<double>();
                }
            }
            double col_w = (!cols.empty()) ? (page_w - left - right - padding) / cols.size() : page_w;
            for (auto& col : cols) {
                col["width"] = slimr::Json::object_t{{"magnitude", col_w}, {"unit", "PT"}};
            }
        }
        cell.raw["sectionStyle"] = std::move(sec_style);
    }

    if (start == 0) {
        if (!tab.document_style.is_object()) tab.document_style = slimr::Json::object_t{};
        if (!tab.document_style.contains("documentFormat") || !tab.document_style["documentFormat"].is_object()) {
            tab.document_style["documentFormat"] = slimr::Json::object_t{};
        }
        tab.document_style["documentFormat"]["documentMode"] = "PAGES";
    }
    return slimr::Json::object_t{};
}

inline void hidden_cell_insert_refuse(const TabState& tab, int idx, EmulateContext& ctx) {
    if (hidden_cell_redirect(tab, idx).has_value()) {
        throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index,
                            "an object inserted into a merge-hidden cell");
    }
}

inline slimr::Json insert_atom_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx,
                                             std::string_view type, std::string_view properties_field) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    hidden_cell_insert_refuse(tab, idx, ctx);
    slimr::Json style = style_for_insert_at(tab.tape, idx);

    std::string id_field;
    if (type == "dateElement") id_field = "dateId";
    else if (type == "person") id_field = "personId";
    else if (type == "richLink") id_field = "richLinkId";

    slimr::Json props = req.contains(properties_field) ? req[properties_field] : slimr::Json::object_t{};
    if (type == "dateElement") {
        if (props.contains("timestamp") && props["timestamp"].is_string()) {
            std::string ts = props["timestamp"].get<std::string>();
            bool has_offset = ts.ends_with("Z") || (ts.size() >= 6 && (ts[ts.size() - 6] == '+' || ts[ts.size() - 6] == '-'));
            if (!has_offset) {
                throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                                    "timestamps must end with 'Z' or have a valid timezone offset");
            }
        }
        auto defs = make_date_element_defaults();
        for (const auto& [k, v] : defs.get<slimr::Json::object_t>()) {
            if (!props.contains(k)) props[k] = v;
        }
        bool is_default_format = (props.contains("dateFormat") && props["dateFormat"].is_string() &&
                                  props["dateFormat"].get<std::string>() == "DATE_FORMAT_MONTH_DAY_YEAR_ABBREVIATED" &&
                                  props.contains("timeFormat") && props["timeFormat"].is_string() &&
                                  props["timeFormat"].get<std::string>() == "TIME_FORMAT_DISABLED");
        if (is_default_format && props.contains("timestamp") && props["timestamp"].is_string() && !props.contains("displayText")) {
            std::string ts = props["timestamp"].get<std::string>();
            int y = 0, m = 0, d = 0;
            if (sscanf(ts.c_str(), "%d-%d-%d", &y, &m, &d) == 3 && m >= 1 && m <= 12) {
                static const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                props["displayText"] = std::format("{} {}, {}", MONTHS[m - 1], d, y);
            }
        }
    }

    slimr::Json atom_payload = slimr::Json::object_t{
        {id_field, mint_chip_id(ctx)},
        {std::string(properties_field), std::move(props)}
    };
    slimr::Json raw = slimr::Json::object_t{{std::string(type), std::move(atom_payload)}};

    tab.tape.insert(tab.tape.begin() + idx, TapeCell{
        .type = TapeCellType::Atom,
        .style = style,
        .raw = std::move(raw),
        .span = 1
    });
    return slimr::Json::object_t{};
}

inline slimr::Json delete_positioned_object_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string id = req.contains("objectId") && req["objectId"].is_string() ? req["objectId"].get<std::string>() : "";
    if (!tab.positioned_objects.is_object() || !tab.positioned_objects.contains(id)) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "no positioned object " + id);
    }
    tab.positioned_objects.get<slimr::Json::object_t>().erase(id);
    for (auto& cell : tab.tape) {
        if (cell.type == TapeCellType::Nl && !cell.para.positioned_object_ids.empty()) {
            auto& pids = cell.para.positioned_object_ids;
            pids.erase(std::remove(pids.begin(), pids.end(), id), pids.end());
        }
    }
    return slimr::Json::object_t{};
}

inline slimr::Json image_replace_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string id = req.contains("imageObjectId") && req["imageObjectId"].is_string()
                         ? req["imageObjectId"].get<std::string>()
                         : "";
    if (!tab.inline_objects.is_object() || !tab.inline_objects.contains(id)) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "no image " + id);
    }
    auto& object = tab.inline_objects[id];
    if (!object.contains("inlineObjectProperties") || !object["inlineObjectProperties"].is_object()) {
        object["inlineObjectProperties"] = slimr::Json::object_t{};
    }
    auto& props = object["inlineObjectProperties"];
    if (!props.contains("embeddedObject") || !props["embeddedObject"].is_object()) {
        props["embeddedObject"] = slimr::Json::object_t{};
    }
    auto& emb = props["embeddedObject"];
    if (!emb.contains("imageProperties") || !emb["imageProperties"].is_object()) {
        emb["imageProperties"] = slimr::Json::object_t{};
    }
    std::string uri = req.contains("uri") && req["uri"].is_string() ? req["uri"].get<std::string>() : "";
    emb["imageProperties"]["contentUri"] = uri;
    emb["imageProperties"]["sourceUri"] = uri;
    return slimr::Json::object_t{};
}

inline slimr::Json named_range_create_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [start, end, tab] = range_resolve(state, req.contains("range") ? req["range"] : slimr::Json{nullptr}, ctx);
    std::string name = req.contains("name") && req["name"].is_string() ? req["name"].get<std::string>() : "";
    if (name.empty()) {
        throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index, "A named range needs a name.");
    }
    std::string named_range_id = mint_chip_id(ctx);
    if (!tab.named_ranges.is_object()) tab.named_ranges = slimr::Json::object_t{};
    if (!tab.named_ranges.contains(name) || !tab.named_ranges[name].is_object()) {
        tab.named_ranges[name] = slimr::Json::object_t{
            {"name", name},
            {"namedRanges", slimr::Json::array_t{}}
        };
    }
    auto& entry = tab.named_ranges[name];
    if (!entry.contains("namedRanges") || !entry["namedRanges"].is_array()) {
        entry["namedRanges"] = slimr::Json::array_t{};
    }
    slimr::Json range_item = slimr::Json::object_t{
        {"name", name},
        {"namedRangeId", named_range_id},
        {"ranges", slimr::Json::array_t{
            slimr::Json::object_t{{"startIndex", static_cast<int64_t>(start)}, {"endIndex", static_cast<int64_t>(end)}}
        }}
    };
    entry["namedRanges"].get<slimr::Json::array_t>().push_back(std::move(range_item));
    return slimr::Json::object_t{
        {"createNamedRange", slimr::Json::object_t{{"namedRangeId", named_range_id}}}
    };
}

inline slimr::Json named_range_delete_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabsCriteria") && req["tabsCriteria"].contains("tabIds") &&
        req["tabsCriteria"]["tabIds"].is_array() && !req["tabsCriteria"]["tabIds"].get<slimr::Json::array_t>().empty()) {
        tab_id = req["tabsCriteria"]["tabIds"].get<slimr::Json::array_t>()[0].get<std::string>();
    }
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string id = req.contains("namedRangeId") && req["namedRangeId"].is_string()
                         ? req["namedRangeId"].get<std::string>()
                         : "";

    if (tab.named_ranges.is_object()) {
        std::vector<std::string> to_delete;
        for (const auto& [name, entry] : tab.named_ranges.get<slimr::Json::object_t>()) {
            if (!entry.is_object() || !entry.contains("namedRanges") || !entry["namedRanges"].is_array()) continue;
            slimr::Json::array_t kept;
            for (const auto& r : entry["namedRanges"].get<slimr::Json::array_t>()) {
                if (r.is_object() && r.contains("namedRangeId") && r["namedRangeId"].get<std::string>() != id) {
                    kept.push_back(r);
                }
            }
            if (kept.empty()) {
                to_delete.push_back(std::string(name));
            } else {
                tab.named_ranges[name]["namedRanges"] = std::move(kept);
            }
        }
        for (const auto& d : to_delete) {
            tab.named_ranges.get<slimr::Json::object_t>().erase(d);
        }
    }
    return slimr::Json::object_t{};
}

inline std::vector<slimr::Json*> dropdown_chips(TabState& tab) {
    std::vector<slimr::Json*> out;
    for (auto& cell : tab.tape) {
        if (cell.type == TapeCellType::Atom && cell.raw.is_object() && cell.raw.contains("dropdown")) {
            out.push_back(&cell.raw["dropdown"]);
        }
    }
    return out;
}

inline slimr::Json dropdown_definition_create_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    slimr::Json def = req.contains("dropdownDefinition") && req["dropdownDefinition"].is_object()
                          ? req["dropdownDefinition"]
                          : slimr::Json::object_t{};
    std::string id = def.contains("dropdownDefinitionId") && def["dropdownDefinitionId"].is_string()
                         ? def["dropdownDefinitionId"].get<std::string>()
                         : mint_chip_id(ctx);
    def["dropdownDefinitionId"] = id;
    if (!tab.dropdown_definitions.is_object()) tab.dropdown_definitions = slimr::Json::object_t{};
    tab.dropdown_definitions[id] = def;
    return slimr::Json::object_t{
        {"createDropdownDefinition", slimr::Json::object_t{{"dropdownDefinition", std::move(def)}}}
    };
}

inline slimr::Json dropdown_definition_update_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string id = req.contains("dropdownDefinitionId") && req["dropdownDefinitionId"].is_string()
                         ? req["dropdownDefinitionId"].get<std::string>()
                         : "";
    if (!tab.dropdown_definitions.is_object() || !tab.dropdown_definitions.contains(id)) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "no dropdown definition " + id);
    }
    auto& def = tab.dropdown_definitions[id];
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    const auto& patch = req.contains("dropdownDefinitionProperties") && req["dropdownDefinitionProperties"].is_object()
                            ? req["dropdownDefinitionProperties"]
                            : slimr::Json::object_t{};

    if (!def.contains("dropdownDefinitionProperties") || !def["dropdownDefinitionProperties"].is_object()) {
        def["dropdownDefinitionProperties"] = slimr::Json::object_t{};
    }
    auto& props = def["dropdownDefinitionProperties"];
    if (std::find(fields.begin(), fields.end(), "title") != fields.end() && patch.contains("title")) {
        props["title"] = patch["title"];
    }
    if (std::find(fields.begin(), fields.end(), "options") != fields.end() && patch.contains("options")) {
        props["options"] = patch["options"];
    }
    return slimr::Json::object_t{};
}

inline slimr::Json dropdown_definition_delete_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string id = req.contains("dropdownDefinitionId") && req["dropdownDefinitionId"].is_string()
                         ? req["dropdownDefinitionId"].get<std::string>()
                         : "";
    if (tab.dropdown_definitions.is_object() && tab.dropdown_definitions.contains(id)) {
        tab.dropdown_definitions.get<slimr::Json::object_t>().erase(id);
    }
    return slimr::Json::object_t{};
}

inline slimr::Json insert_dropdown_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    hidden_cell_insert_refuse(tab, idx, ctx);
    std::string def_id = req.contains("dropdownDefinitionId") && req["dropdownDefinitionId"].is_string()
                             ? req["dropdownDefinitionId"].get<std::string>()
                             : "";
    std::string opt_id = req.contains("selectedOptionId") && req["selectedOptionId"].is_string()
                             ? req["selectedOptionId"].get<std::string>()
                             : "";
    std::string display_val;

    if (tab.dropdown_definitions.is_object() && tab.dropdown_definitions.contains(def_id)) {
        const auto& def = tab.dropdown_definitions[def_id];
        if (def.contains("dropdownDefinitionProperties") && def["dropdownDefinitionProperties"].contains("options") &&
            def["dropdownDefinitionProperties"]["options"].is_array()) {
            const auto& opts = def["dropdownDefinitionProperties"]["options"].get<slimr::Json::array_t>();
            for (const auto& o : opts) {
                if (opt_id.empty() || (o.contains("optionId") && o["optionId"].get<std::string>() == opt_id)) {
                    if (opt_id.empty() && o.contains("optionId")) opt_id = o["optionId"].get<std::string>();
                    if (o.contains("displayValue")) display_val = o["displayValue"].get<std::string>();
                    break;
                }
            }
        }
    }

    std::string chip_id = mint_chip_id(ctx);
    slimr::Json dropdown = slimr::Json::object_t{
        {"dropdownId", chip_id},
        {"dropdownProperties", slimr::Json::object_t{
            {"displayValue", display_val},
            {"dropdownDefinitionId", def_id},
            {"selectedOptionId", opt_id}
        }}
    };

    slimr::Json style = style_for_insert_at(tab.tape, idx);
    tab.tape.insert(tab.tape.begin() + idx, TapeCell{
        .type = TapeCellType::Atom,
        .style = style,
        .raw = slimr::Json::object_t{{"dropdown", dropdown}},
        .span = 1
    });
    return slimr::Json::object_t{
        {"insertDropdown", slimr::Json::object_t{{"dropdown", std::move(dropdown)}}}
    };
}

inline slimr::Json dropdown_select_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string chip_id = req.contains("dropdownId") && req["dropdownId"].is_string() ? req["dropdownId"].get<std::string>() : "";
    std::string opt_id = (req.contains("dropdownProperties") && req["dropdownProperties"].contains("selectedOptionId"))
                             ? req["dropdownProperties"]["selectedOptionId"].get<std::string>()
                             : "";

    for (auto* chip : dropdown_chips(tab)) {
        if (chip->contains("dropdownId") && (*chip)["dropdownId"].get<std::string>() == chip_id) {
            if (chip->contains("dropdownProperties")) {
                auto& props = (*chip)["dropdownProperties"];
                props["selectedOptionId"] = opt_id;
                std::string def_id = props.contains("dropdownDefinitionId") ? props["dropdownDefinitionId"].get<std::string>() : "";
                if (tab.dropdown_definitions.is_object() && tab.dropdown_definitions.contains(def_id)) {
                    const auto& def = tab.dropdown_definitions[def_id];
                    if (def.contains("dropdownDefinitionProperties") && def["dropdownDefinitionProperties"].contains("options") &&
                        def["dropdownDefinitionProperties"]["options"].is_array()) {
                        for (const auto& o : def["dropdownDefinitionProperties"]["options"].get<slimr::Json::array_t>()) {
                            if (o.contains("optionId") && o["optionId"].get<std::string>() == opt_id && o.contains("displayValue")) {
                                props["displayValue"] = o["displayValue"];
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
    }
    return slimr::Json::object_t{};
}

inline slimr::Json create_footnote_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    hidden_cell_insert_refuse(tab, idx, ctx);

    ctx.id_counters.segment += 1;
    std::string footnote_id = std::string(MintPrefixes::segment) + std::to_string(ctx.id_counters.segment);

    int number = 1;
    for (int i = 0; i < idx && i < static_cast<int>(tab.tape.size()); ++i) {
        if (tab.tape[i].type == TapeCellType::Atom && tab.tape[i].raw.is_object() &&
            tab.tape[i].raw.contains("footnoteReference")) {
            number++;
        }
    }

    slimr::Json raw = slimr::Json::object_t{
        {"footnoteReference", slimr::Json::object_t{
            {"footnoteId", footnote_id},
            {"footnoteNumber", std::to_string(number)}
        }}
    };
    tab.tape.insert(tab.tape.begin() + idx, TapeCell{
        .type = TapeCellType::Atom,
        .style = slimr::Json::object_t{},
        .raw = std::move(raw),
        .span = 1
    });

    auto fn_content = make_footnote_blank_content();
    slimr::Json::array_t fn_arr;
    for (auto& el : fn_content) fn_arr.push_back(std::move(el));
    if (!tab.footnotes.is_object()) tab.footnotes = slimr::Json::object_t{};
    tab.footnotes[footnote_id] = slimr::Json::object_t{
        {"content", std::move(fn_arr)},
        {"footnoteId", footnote_id}
    };

    return slimr::Json::object_t{
        {"createFootnote", slimr::Json::object_t{{"footnoteId", footnote_id}}}
    };
}

inline slimr::Json insert_inline_image_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    hidden_cell_insert_refuse(tab, idx, ctx);

    slimr::Json style = style_for_insert_at(tab.tape, idx);
    std::string object_id = mint_inline_object_id(ctx);
    std::string uri = req.contains("uri") && req["uri"].is_string() ? req["uri"].get<std::string>() : "";

    if (!tab.inline_objects.is_object()) tab.inline_objects = slimr::Json::object_t{};
    tab.inline_objects[object_id] = slimr::Json::object_t{
        {"inlineObjectProperties", slimr::Json::object_t{
            {"embeddedObject", slimr::Json::object_t{
                {"imageProperties", slimr::Json::object_t{{"sourceUri", uri}}}
            }}
        }},
        {"objectId", object_id}
    };

    slimr::Json raw = slimr::Json::object_t{
        {"inlineObjectElement", slimr::Json::object_t{{"inlineObjectId", object_id}}}
    };
    tab.tape.insert(tab.tape.begin() + idx, TapeCell{
        .type = TapeCellType::Atom,
        .style = style,
        .raw = std::move(raw),
        .span = 1
    });
    return slimr::Json::object_t{
        {"insertInlineImage", slimr::Json::object_t{{"objectId", object_id}}}
    };
}

// Table requests dispatch
inline slimr::Json insert_table_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("location") || !req["location"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "missing location");
    }
    const auto& loc = req["location"];
    std::optional<std::string> tab_id;
    if (loc.contains("tabId") && loc["tabId"].is_string()) tab_id = loc["tabId"].get<std::string>();
    int idx = loc.contains("index") ? loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    insert_position_validate(tab, idx, ctx);
    hidden_cell_insert_refuse(tab, idx, ctx);

    slimr::Json style = style_for_insert_at(tab.tape, idx);
    bool original_keeps_later_side = (idx == paragraph_start_before(tab.tape, idx));

    int nl_index = idx;
    while (nl_index < static_cast<int>(tab.tape.size()) && tab.tape[nl_index].type != TapeCellType::Nl) nl_index++;
    std::string named_type = "NORMAL_TEXT";
    if (nl_index < static_cast<int>(tab.tape.size())) {
        const auto& np = tab.tape[nl_index].para;
        if (np.style.is_object() && np.style.contains("namedStyleType")) {
            named_type = np.style["namedStyleType"].get<std::string>();
        }
    }
    slimr::Json cell_text_style = style;
    if (original_keeps_later_side && named_type != "NORMAL_TEXT") {
        if (tab.named_styles.is_object() && tab.named_styles.contains("styles") &&
            tab.named_styles["styles"].is_array()) {
            for (const auto& st : tab.named_styles["styles"].get<slimr::Json::array_t>()) {
                if (st.is_object() && st.contains("namedStyleType") &&
                    st["namedStyleType"].get<std::string>() == named_type) {
                    if (st.contains("textStyle") && st["textStyle"].is_object()) {
                        slimr::Json combined = st["textStyle"];
                        if (style.is_object()) {
                            for (const auto& [k, v] : style.get<slimr::Json::object_t>()) combined[k] = v;
                        }
                        cell_text_style = style_canonical(combined);
                    }
                    break;
                }
            }
        }
    }

    int rows = req.contains("rows") ? req["rows"].as<int>() : 1;
    int cols = req.contains("columns") ? req["columns"].as<int>() : 1;

    insert_table(
        tab, idx, rows, cols, cell_text_style,
        [&](int pos) { paragraph_split_at(tab, pos, style, ctx, original_keeps_later_side); },
        tape_in_table(tab.tape, idx)
    );
    return slimr::Json::object_t{};
}

struct TableLocationResolved {
    int table_start = 0;
    int row_index = 0;
    int column_index = 0;
    TabState& tab;
};

inline TableLocationResolved table_cell_location_resolve(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& cell_loc = req.contains("tableCellLocation") ? req["tableCellLocation"] : slimr::Json::object_t{};
    const auto& start_loc = cell_loc.contains("tableStartLocation") ? cell_loc["tableStartLocation"] : slimr::Json::object_t{};
    std::optional<std::string> tab_id;
    if (start_loc.contains("tabId") && start_loc["tabId"].is_string()) tab_id = start_loc["tabId"].get<std::string>();
    int t_start = start_loc.contains("index") ? start_loc["index"].as<int>() : 0;
    int r_idx = cell_loc.contains("rowIndex") ? cell_loc["rowIndex"].as<int>() : 0;
    int c_idx = cell_loc.contains("columnIndex") ? cell_loc["columnIndex"].as<int>() : 0;

    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    return TableLocationResolved{.table_start = t_start, .row_index = r_idx, .column_index = c_idx, .tab = tab};
}

inline slimr::Json insert_table_row_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, req, ctx);
    bool insert_below = req.contains("insertBelow") ? json_bool(req["insertBelow"]) : false;
    insert_table_row(tab, table_start, r_idx, c_idx, insert_below, [&](const std::string& msg) {
        throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index, msg);
    });
    return slimr::Json::object_t{};
}

inline slimr::Json insert_table_column_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, req, ctx);
    bool insert_right = req.contains("insertRight") ? json_bool(req["insertRight"]) : false;
    insert_table_column(tab, table_start, r_idx, c_idx, insert_right, [&](const std::string& msg) {
        throw EmulatorError(EmulatorErrorCode::UNMODELED, ctx.request_index, msg);
    });
    return slimr::Json::object_t{};
}

inline slimr::Json delete_table_row_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, req, ctx);
    delete_table_row(tab, table_start, r_idx);
    return slimr::Json::object_t{};
}

inline slimr::Json delete_table_column_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, req, ctx);
    delete_table_column(tab, table_start, c_idx);
    return slimr::Json::object_t{};
}

inline slimr::Json merge_table_cells_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& range = req.contains("tableRange") ? req["tableRange"] : slimr::Json::object_t{};
    auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, range, ctx);
    int r_span = range.contains("rowSpan") ? range["rowSpan"].as<int>() : 1;
    int c_span = range.contains("columnSpan") ? range["columnSpan"].as<int>() : 1;
    merge_table_cells(tab, table_start, r_idx, c_idx, r_span, c_span);
    return slimr::Json::object_t{};
}

inline slimr::Json unmerge_table_cells_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& range = req.contains("tableRange") ? req["tableRange"] : slimr::Json::object_t{};
    auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, range, ctx);
    unmerge_table_cells(tab, table_start, r_idx, c_idx);
    return slimr::Json::object_t{};
}

inline slimr::Json update_table_cell_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    slimr::Json patch = req.contains("tableCellStyle") && req["tableCellStyle"].is_object()
                            ? req["tableCellStyle"]
                            : slimr::Json::object_t{};

    if (req.contains("tableRange") && req["tableRange"].is_object()) {
        const auto& range = req["tableRange"];
        auto [table_start, r_idx, c_idx, tab] = table_cell_location_resolve(state, range, ctx);
        std::optional<int> r_span = range.contains("rowSpan") ? std::optional<int>(range["rowSpan"].as<int>()) : std::nullopt;
        std::optional<int> c_span = range.contains("columnSpan") ? std::optional<int>(range["columnSpan"].as<int>()) : std::nullopt;
        update_table_cell_style(tab, table_start, r_idx, c_idx, r_span, c_span, patch, fields);
    } else if (req.contains("tableStartLocation") && req["tableStartLocation"].is_object()) {
        const auto& start_loc = req["tableStartLocation"];
        std::optional<std::string> tab_id;
        if (start_loc.contains("tabId") && start_loc["tabId"].is_string()) tab_id = start_loc["tabId"].get<std::string>();
        int t_start = start_loc.contains("index") ? start_loc["index"].as<int>() : 0;
        auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
        update_table_cell_style(tab, t_start, std::nullopt, std::nullopt, std::nullopt, std::nullopt, patch, fields);
    }
    return slimr::Json::object_t{};
}

inline slimr::Json update_table_column_properties_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& start_loc = req.contains("tableStartLocation") ? req["tableStartLocation"] : slimr::Json::object_t{};
    std::optional<std::string> tab_id;
    if (start_loc.contains("tabId") && start_loc["tabId"].is_string()) tab_id = start_loc["tabId"].get<std::string>();
    int t_start = start_loc.contains("index") ? start_loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    if (std::find(fields.begin(), fields.end(), "width") != fields.end() &&
        std::find(fields.begin(), fields.end(), "widthType") == fields.end()) {
        throw EmulatorError(EmulatorErrorCode::INVALID_FIELD, ctx.request_index,
                            "Width type must be provided when updating column width.");
    }

    std::vector<int> col_indices;
    if (req.contains("columnIndices") && req["columnIndices"].is_array()) {
        for (const auto& ci : req["columnIndices"].get<slimr::Json::array_t>()) {
            col_indices.push_back(ci.as<int>());
        }
    }
    slimr::Json patch = req.contains("tableColumnProperties") && req["tableColumnProperties"].is_object()
                            ? req["tableColumnProperties"]
                            : slimr::Json::object_t{};
    update_table_column_properties(tab, t_start, col_indices, patch, fields);
    return slimr::Json::object_t{};
}

inline slimr::Json update_table_row_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& start_loc = req.contains("tableStartLocation") ? req["tableStartLocation"] : slimr::Json::object_t{};
    std::optional<std::string> tab_id;
    if (start_loc.contains("tabId") && start_loc["tabId"].is_string()) tab_id = start_loc["tabId"].get<std::string>();
    int t_start = start_loc.contains("index") ? start_loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");
    std::vector<int> row_indices;
    if (req.contains("rowIndices") && req["rowIndices"].is_array()) {
        for (const auto& ri : req["rowIndices"].get<slimr::Json::array_t>()) {
            row_indices.push_back(ri.as<int>());
        }
    }
    slimr::Json patch = req.contains("tableRowStyle") && req["tableRowStyle"].is_object()
                            ? req["tableRowStyle"]
                            : slimr::Json::object_t{};
    update_table_row_style(tab, t_start, row_indices, patch, fields);
    return slimr::Json::object_t{};
}

inline slimr::Json pin_table_header_rows_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& start_loc = req.contains("tableStartLocation") ? req["tableStartLocation"] : slimr::Json::object_t{};
    std::optional<std::string> tab_id;
    if (start_loc.contains("tabId") && start_loc["tabId"].is_string()) tab_id = start_loc["tabId"].get<std::string>();
    int t_start = start_loc.contains("index") ? start_loc["index"].as<int>() : 0;
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    int count = req.contains("pinnedHeaderRowsCount") ? req["pinnedHeaderRowsCount"].as<int>() : 0;
    pin_table_header_rows(tab, t_start, count);
    return slimr::Json::object_t{};
}

// Tab operations
inline void tab_title_free(EmulatorState& state, std::string_view title, const TabState* self, EmulateContext& ctx) {
    if (!title.empty()) {
        for (const auto& t : state.tabs) {
            if (&t != self && t.title == title) {
                throw EmulatorError(EmulatorErrorCode::TAB_TITLE_TAKEN, ctx.request_index, "Tab title must be unique");
            }
        }
    }
}

inline std::vector<size_t> tab_siblings_indices(const EmulatorState& state, std::optional<std::string_view> parent_tab_id) {
    std::vector<size_t> out;
    for (size_t i = 0; i < state.tabs.size(); ++i) {
        if (state.tabs[i].parent_tab_id == parent_tab_id) {
            out.push_back(i);
        }
    }
    return out;
}

inline int tab_depth(const EmulatorState& state, const TabState& tab) {
    int depth = 0;
    auto parent = tab.parent_tab_id;
    while (parent) {
        depth++;
        const TabState* p_tab = nullptr;
        for (const auto& t : state.tabs) {
            if (t.tab_id == *parent) {
                p_tab = &t;
                break;
            }
        }
        parent = p_tab ? p_tab->parent_tab_id : std::nullopt;
    }
    return depth;
}

inline std::pair<int, int> tab_subtree_range(const EmulatorState& state, std::string_view tab_id) {
    int from = -1;
    for (size_t i = 0; i < state.tabs.size(); ++i) {
        if (state.tabs[i].tab_id == tab_id) {
            from = static_cast<int>(i);
            break;
        }
    }
    if (from < 0) return {0, 0};
    int depth = tab_depth(state, state.tabs[from]);
    int to = from + 1;
    while (to < static_cast<int>(state.tabs.size()) && tab_depth(state, state.tabs[to]) > depth) to++;
    return {from, to};
}

inline void tabs_insert(EmulatorState& state, std::vector<TabState> to_insert,
                        std::optional<std::string_view> parent_tab_id, int index) {
    auto siblings = tab_siblings_indices(state, parent_tab_id);
    int at = static_cast<int>(state.tabs.size());
    if (index < static_cast<int>(siblings.size())) {
        at = static_cast<int>(siblings[index]);
    } else if (!siblings.empty()) {
        at = tab_subtree_range(state, state.tabs[siblings.back()].tab_id).second;
    } else if (parent_tab_id) {
        at = tab_subtree_range(state, *parent_tab_id).second;
    }
    state.tabs.insert(state.tabs.begin() + at,
                      std::make_move_iterator(to_insert.begin()),
                      std::make_move_iterator(to_insert.end()));
}

inline slimr::Json add_document_tab_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::string tab_id = mint_tab_id(ctx);
    const auto& props = req.contains("tabProperties") && req["tabProperties"].is_object()
                            ? req["tabProperties"]
                            : slimr::Json::object_t{};
    std::string title = props.contains("title") && props["title"].is_string() ? props["title"].get<std::string>() : "";
    std::optional<std::string> parent_tab_id;
    if (props.contains("parentTabId") && props["parentTabId"].is_string()) {
        parent_tab_id = props["parentTabId"].get<std::string>();
    }

    tab_title_free(state, title, nullptr, ctx);

    slimr::Json raw = blank_tab_template();
    raw["documentStyle"] = tab_document_style_new(!state.tabs.empty() ? std::optional<slimr::Json>(state.tabs[0].document_style) : std::nullopt);
    if (!state.tabs.empty()) {
        raw["namedStyles"] = state.tabs[0].named_styles;
    }

    TabState tab = tab_state_build(tab_id, title, parent_tab_id ? std::optional<std::string_view>(*parent_tab_id) : std::nullopt, raw);
    auto siblings = tab_siblings_indices(state, parent_tab_id ? std::optional<std::string_view>(*parent_tab_id) : std::nullopt);
    int index = props.contains("index") ? std::min(props["index"].as<int>(), static_cast<int>(siblings.size())) : static_cast<int>(siblings.size());

    int nesting_level = tab_depth(state, tab);
    tabs_insert(state, {std::move(tab)}, parent_tab_id ? std::optional<std::string_view>(*parent_tab_id) : std::nullopt, index);

    slimr::Json reply = slimr::Json::object_t{
        {"index", static_cast<int64_t>(index)},
        {"tabId", tab_id},
        {"title", title}
    };
    if (parent_tab_id) reply["parentTabId"] = *parent_tab_id;
    if (nesting_level > 0) reply["nestingLevel"] = static_cast<int64_t>(nesting_level);

    return slimr::Json::object_t{
        {"addDocumentTab", slimr::Json::object_t{{"tabProperties", std::move(reply)}}}
    };
}

inline slimr::Json delete_tab_request_apply(EmulatorState& state, const slimr::Json& req) {
    std::string tab_id = req.contains("tabId") && req["tabId"].is_string() ? req["tabId"].get<std::string>() : "";
    auto [from, to] = tab_subtree_range(state, tab_id);
    if (to > from) {
        state.tabs.erase(state.tabs.begin() + from, state.tabs.begin() + to);
    }
    return slimr::Json::object_t{};
}

inline slimr::Json update_document_tab_properties_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    const auto& patch = req.contains("tabProperties") && req["tabProperties"].is_object()
                            ? req["tabProperties"]
                            : slimr::Json::object_t{};
    std::string tab_id = patch.contains("tabId") && patch["tabId"].is_string() ? patch["tabId"].get<std::string>() : "";
    auto& tab = tab_resolve(state, tab_id, ctx.request_index);
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");

    if (std::find(fields.begin(), fields.end(), "title") != fields.end() && patch.contains("title")) {
        std::string new_title = patch["title"].get<std::string>();
        tab_title_free(state, new_title, &tab, ctx);
        tab.title = new_title;
    }
    if (std::find(fields.begin(), fields.end(), "iconEmoji") != fields.end()) {
        tab.icon_emoji = (patch.contains("iconEmoji") && patch["iconEmoji"].is_string() && !patch["iconEmoji"].get<std::string>().empty())
                             ? std::optional<std::string>(patch["iconEmoji"].get<std::string>())
                             : std::nullopt;
    }

    bool reparent = (std::find(fields.begin(), fields.end(), "parentTabId") != fields.end());
    bool reindex = (std::find(fields.begin(), fields.end(), "index") != fields.end() && patch.contains("index"));

    if (reparent || reindex) {
        auto [from, to] = tab_subtree_range(state, tab.tab_id);
        std::vector<TabState> moved(std::make_move_iterator(state.tabs.begin() + from),
                                    std::make_move_iterator(state.tabs.begin() + to));
        state.tabs.erase(state.tabs.begin() + from, state.tabs.begin() + to);

        if (reparent) {
            moved[0].parent_tab_id = (patch.contains("parentTabId") && patch["parentTabId"].is_string() && !patch["parentTabId"].get<std::string>().empty())
                                         ? std::optional<std::string>(patch["parentTabId"].get<std::string>())
                                         : std::nullopt;
        }
        std::optional<std::string> target_parent = moved[0].parent_tab_id;
        std::optional<std::string_view> parent_view = target_parent ? std::optional<std::string_view>(*target_parent) : std::nullopt;
        auto siblings = tab_siblings_indices(state, parent_view);
        int target_idx = reindex ? std::min(patch["index"].as<int>(), static_cast<int>(siblings.size())) : static_cast<int>(siblings.size());
        tabs_insert(state, std::move(moved), parent_view, target_idx);
    }
    return slimr::Json::object_t{};
}

// Dotted field utilities
inline std::optional<slimr::Json> dotted_field_get(const slimr::Json& obj, const std::vector<std::string>& parts) {
    const slimr::Json* cur = &obj;
    for (const auto& part : parts) {
        if (!cur->is_object() || !cur->contains(part)) return std::nullopt;
        cur = &(*cur)[part];
    }
    return *cur;
}

inline void dotted_field_set(slimr::Json& target, const std::vector<std::string>& parts, size_t part_idx,
                             const std::optional<slimr::Json>& value) {
    if (part_idx >= parts.size()) return;
    const auto& key = parts[part_idx];
    if (part_idx == parts.size() - 1) {
        if (!value || value->is_null()) {
            if (target.is_object() && target.contains(key)) target.get<slimr::Json::object_t>().erase(key);
        } else {
            target[key] = *value;
        }
        return;
    }
    if (!target.is_object()) target = slimr::Json::object_t{};
    if (!target.contains(key) || !target[key].is_object()) target[key] = slimr::Json::object_t{};
    dotted_field_set(target[key], parts, part_idx + 1, value);
}

inline std::vector<std::string> split_dot(std::string_view s) {
    std::vector<std::string> res;
    size_t start = 0;
    while (start < s.size()) {
        size_t dot = s.find('.', start);
        if (dot == std::string_view::npos) dot = s.size();
        res.emplace_back(s.substr(start, dot - start));
        start = dot + 1;
    }
    return res;
}

inline slimr::Json update_document_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    auto fields = parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "");

    for (const auto& f : fields) {
        if (f.ends_with("HeaderId") || f.ends_with("FooterId")) {
            throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index, "Unallowed field: " + f);
        }
    }
    const auto& patch = req.contains("documentStyle") && req["documentStyle"].is_object()
                            ? req["documentStyle"]
                            : slimr::Json::object_t{};
    for (const auto& f : fields) {
        auto parts = split_dot(f);
        auto val = dotted_field_get(patch, parts);
        dotted_field_set(tab.document_style, parts, 0, val);
    }
    return slimr::Json::object_t{};
}

inline slimr::Json update_named_style_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    const auto& patch = req.contains("namedStyle") && req["namedStyle"].is_object()
                            ? req["namedStyle"]
                            : slimr::Json::object_t{};
    std::string type = patch.contains("namedStyleType") ? patch["namedStyleType"].get<std::string>() : "";

    if (!tab.named_styles.is_object() || !tab.named_styles.contains("styles") || !tab.named_styles["styles"].is_array()) {
        tab.named_styles = slimr::Json::object_t{{"styles", slimr::Json::array_t{}}};
    }
    auto& styles = tab.named_styles["styles"].get<slimr::Json::array_t>();
    int idx = -1;
    for (size_t i = 0; i < styles.size(); ++i) {
        if (styles[i].is_object() && styles[i].contains("namedStyleType") &&
            styles[i]["namedStyleType"].get<std::string>() == type) {
            idx = static_cast<int>(i);
            break;
        }
    }
    if (idx < 0) {
        styles.push_back(slimr::Json::object_t{
            {"namedStyleType", type},
            {"paragraphStyle", slimr::Json::object_t{{"namedStyleType", type}}},
            {"textStyle", slimr::Json::object_t{}}
        });
        idx = static_cast<int>(styles.size()) - 1;
    }

    auto& entry = styles[idx];
    for (const auto& field : parse_fields(req.contains("fields") ? req["fields"].get<std::string>() : "")) {
        if (field == "namedStyleType") continue;
        auto parts = split_dot(field);
        auto val = dotted_field_get(patch, parts);
        dotted_field_set(entry, parts, 0, val);
    }
    return slimr::Json::object_t{};
}

// Headers and Footers
inline const std::vector<std::string> FOOTER_FIELDS = {"defaultFooterId", "firstPageFooterId", "evenPageFooterId"};
inline const std::vector<std::string> HEADER_FIELDS = {"defaultHeaderId", "firstPageHeaderId", "evenPageHeaderId"};

inline slimr::Json segment_create_apply(EmulatorState& state, const slimr::Json& req,
                                        std::string_view kind, EmulateContext& ctx) {
    if (!req.contains("type") || req["type"].get<std::string>() != "DEFAULT") {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "only a DEFAULT " + std::string(kind) + " can be created");
    }
    std::optional<std::string> tab_id;
    int at = 0;
    if (req.contains("sectionBreakLocation") && req["sectionBreakLocation"].is_object()) {
        const auto& sbl = req["sectionBreakLocation"];
        if (sbl.contains("tabId") && sbl["tabId"].is_string()) tab_id = sbl["tabId"].get<std::string>();
        if (sbl.contains("index")) at = sbl["index"].as<int>();
    }
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    std::string field = (kind == "header") ? HEADER_FIELDS[0] : FOOTER_FIELDS[0];
    if (at > 0 && (at >= static_cast<int>(tab.tape.size()) || tab.tape[at].type != TapeCellType::SectionBreak)) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "no section break at index " + std::to_string(at));
    }

    auto& owner = (at > 0 && tab.tape[at].type == TapeCellType::SectionBreak)
                      ? tab.tape[at].raw["sectionStyle"]
                      : tab.document_style;
    if (owner.is_object() && owner.contains(field)) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "the section already has a default " + std::string(kind));
    }

    ctx.id_counters.segment += 1;
    std::string id = std::string(MintPrefixes::segment) + std::to_string(ctx.id_counters.segment);

    slimr::Json::array_t content = {
        slimr::Json::object_t{
            {"startIndex", static_cast<int64_t>(0)},
            {"endIndex", static_cast<int64_t>(1)},
            {"paragraph", slimr::Json::object_t{
                {"elements", slimr::Json::array_t{
                    slimr::Json::object_t{
                        {"startIndex", static_cast<int64_t>(0)},
                        {"endIndex", static_cast<int64_t>(1)},
                        {"textRun", slimr::Json::object_t{{"content", "\n"}, {"textStyle", slimr::Json::object_t{}}}}
                    }
                }},
                {"paragraphStyle", slimr::Json::object_t{{"namedStyleType", "NORMAL_TEXT"}}}
            }}
        }
    };

    auto& store = (kind == "header") ? tab.headers : tab.footers;
    if (!store.is_object()) store = slimr::Json::object_t{};
    store[id] = slimr::Json::object_t{
        {"content", std::move(content)},
        {std::string(kind) + "Id", id}
    };
    owner[field] = id;

    std::string reply_key = (kind == "header") ? "createHeader" : "createFooter";
    return slimr::Json::object_t{
        {reply_key, slimr::Json::object_t{{std::string(kind) + "Id", id}}}
    };
}

inline slimr::Json segment_delete_apply(EmulatorState& state, const slimr::Json& req,
                                        std::string_view kind, EmulateContext& ctx) {
    std::optional<std::string> tab_id;
    if (req.contains("tabId") && req["tabId"].is_string()) tab_id = req["tabId"].get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);
    std::string id_key = std::string(kind) + "Id";
    std::string id = req.contains(id_key) && req[id_key].is_string() ? req[id_key].get<std::string>() : "";

    auto& store = (kind == "header") ? tab.headers : tab.footers;
    if (!store.is_object() || !store.contains(id)) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "no " + std::string(kind) + " " + id);
    }
    store.get<slimr::Json::object_t>().erase(id);

    const auto& fields = (kind == "header") ? HEADER_FIELDS : FOOTER_FIELDS;
    for (const auto& f : fields) {
        if (tab.document_style.is_object() && tab.document_style.contains(f) &&
            tab.document_style[f].get<std::string>() == id) {
            tab.document_style.get<slimr::Json::object_t>().erase(f);
        }
    }
    for (auto& cell : tab.tape) {
        if (cell.type == TapeCellType::SectionBreak && cell.raw.is_object() &&
            cell.raw.contains("sectionStyle") && cell.raw["sectionStyle"].is_object()) {
            auto& ss = cell.raw["sectionStyle"];
            for (const auto& f : fields) {
                if (ss.contains(f) && ss[f].get<std::string>() == id) {
                    ss.get<slimr::Json::object_t>().erase(f);
                }
            }
        }
    }
    return slimr::Json::object_t{};
}

inline slimr::Json segment_local(const slimr::Json& value, std::string_view tab_id) {
    if (value.is_array()) {
        slimr::Json::array_t arr;
        for (const auto& item : value.get<slimr::Json::array_t>()) {
            arr.push_back(segment_local(item, tab_id));
        }
        return slimr::Json{std::move(arr)};
    }
    if (!value.is_object()) return value;
    slimr::Json out = slimr::Json::object_t{};
    for (const auto& [k, v] : value.get<slimr::Json::object_t>()) {
        if (k == "segmentId") continue;
        if (k == "tabId") out["tabId"] = std::string(tab_id);
        else out[k] = segment_local(v, tab_id);
    }
    return out;
}

inline slimr::Json segment_request_apply(EmulatorState& state, const slimr::Json& req,
                                         const std::string& segment_id, EmulateContext& ctx) {
    auto tab_id_found = field_find(req, "tabId");
    std::optional<std::string> tab_id;
    if (tab_id_found && tab_id_found->is_string()) tab_id = tab_id_found->get<std::string>();
    auto& tab = tab_resolve(state, tab_id ? std::optional<std::string_view>(*tab_id) : std::nullopt, ctx.request_index);

    slimr::Json* store = nullptr;
    if (tab.headers.is_object() && tab.headers.contains(segment_id)) store = &tab.headers[segment_id];
    else if (tab.footers.is_object() && tab.footers.contains(segment_id)) store = &tab.footers[segment_id];
    else if (tab.footnotes.is_object() && tab.footnotes.contains(segment_id)) store = &tab.footnotes[segment_id];

    if (!store) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "no header, footer or footnote " + segment_id);
    }

    if (req.contains("updateParagraphStyle") && req["updateParagraphStyle"].is_object()) {
        const auto& up = req["updateParagraphStyle"];
        if (up.contains("fields")) {
            auto fields = parse_fields(up["fields"].get<std::string>());
            if (std::find(fields.begin(), fields.end(), "pageBreakBefore") != fields.end()) {
                throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                                    "Cannot update page-break-before in a header.");
            }
        }
    }

    std::string local = "emu.segment";
    slimr::Json dt = slimr::Json::object_t{
        {"body", slimr::Json::object_t{{"content", (*store)["content"]}}},
        {"inlineObjects", tab.inline_objects},
        {"lists", tab.lists},
        {"namedStyles", tab.named_styles}
    };
    TabState segment = tab_state_build(local, "", std::nullopt, dt);
    EmulatorState seg_state{.tabs = {std::move(segment)}};

    slimr::Json local_req = segment_local(req, local);
    slimr::Json reply = request_apply(seg_state, local_req, ctx);

    slimr::Json built = doc_state_json(seg_state);
    if (built.contains("tabs") && built["tabs"].is_array() && !built["tabs"].get<slimr::Json::array_t>().empty()) {
        const auto& seg_tab = built["tabs"].get<slimr::Json::array_t>()[0];
        if (seg_tab.contains("documentTab") && seg_tab["documentTab"].contains("body")) {
            (*store)["content"] = seg_tab["documentTab"]["body"]["content"];
        }
    }
    tab.inline_objects = seg_state.tabs[0].inline_objects;
    tab.lists = seg_state.tabs[0].lists;

    return reply;
}

// replaceAllText
inline slimr::Json replace_all_text_request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (!req.contains("containsText") || !req["containsText"].is_object()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "replaceAllText.containsText must be an object");
    }
    const auto& ct = req["containsText"];
    if (!ct.contains("text") || !ct["text"].is_string()) {
        throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                            "replaceAllText.containsText.text must be a string");
    }
    std::string find_text = ct["text"].get<std::string>();
    if (find_text.empty()) {
        return slimr::Json::object_t{
            {"replaceAllText", slimr::Json::object_t{{"occurrencesChanged", static_cast<int64_t>(0)}}}
        };
    }
    bool match_case = ct.contains("matchCase") ? json_bool(ct["matchCase"]) : false;
    std::string replace_text = req.contains("replaceText") && req["replaceText"].is_string()
                                   ? req["replaceText"].get<std::string>()
                                   : "";

    std::set<std::string> tab_filter;
    if (req.contains("tabsCriteria") && req["tabsCriteria"].contains("tabIds") &&
        req["tabsCriteria"]["tabIds"].is_array()) {
        for (const auto& tid : req["tabsCriteria"]["tabIds"].get<slimr::Json::array_t>()) {
            if (tid.is_string()) tab_filter.insert(tid.get<std::string>());
        }
    }

    std::u16string u16_find = utf8_to_utf16(find_text);
    std::u16string u16_replace = utf8_to_utf16(replace_text);
    int total_occurrences = 0;

    for (auto& tab : state.tabs) {
        if (!tab_filter.empty() && !tab_filter.contains(tab.tab_id)) continue;

        int k = 0;
        while (k + static_cast<int>(u16_find.size()) <= static_cast<int>(tab.tape.size())) {
            bool matches = true;
            for (size_t m = 0; m < u16_find.size(); ++m) {
                const auto& cell = tab.tape[k + m];
                if (cell.type != TapeCellType::Char) {
                    matches = false;
                    break;
                }
                char16_t c1 = cell.u16 != 0 ? cell.u16 : utf8_to_utf16(cell.ch)[0];
                char16_t c2 = u16_find[m];
                if (match_case ? (c1 != c2) : (std::towlower(c1) != std::towlower(c2))) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                total_occurrences++;
                slimr::Json style = tab.tape[k].style;
                std::optional<Sugg> sugg = tab.tape[k].sugg;
                tab.tape.erase(tab.tape.begin() + k, tab.tape.begin() + (k + u16_find.size()));

                std::vector<TapeCell> inserted;
                for (char16_t rch : u16_replace) {
                    std::u16string s(1, rch);
                    inserted.push_back(TapeCell{
                        .type = TapeCellType::Char,
                        .ch = utf16_to_utf8(s),
                        .u16 = rch,
                        .style = style,
                        .sugg = sugg
                    });
                }
                tab.tape.insert(tab.tape.begin() + k, inserted.begin(), inserted.end());
                k += static_cast<int>(u16_replace.size());
            } else {
                k++;
            }
        }
    }

    return slimr::Json::object_t{
        {"replaceAllText", slimr::Json::object_t{{"occurrencesChanged", static_cast<int64_t>(total_occurrences)}}}
    };
}

// Request dispatch
inline slimr::Json request_dispatch(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    if (req.contains("createHeader")) return segment_create_apply(state, req["createHeader"], "header", ctx);
    if (req.contains("createFooter")) return segment_create_apply(state, req["createFooter"], "footer", ctx);
    if (req.contains("deleteHeader")) return segment_delete_apply(state, req["deleteHeader"], "header", ctx);
    if (req.contains("deleteFooter")) return segment_delete_apply(state, req["deleteFooter"], "footer", ctx);
    if (req.contains("insertText")) return insert_text_request_apply(state, req["insertText"], ctx);
    if (req.contains("deleteContentRange")) return delete_content_range_request_apply(state, req["deleteContentRange"], ctx);
    if (req.contains("updateTextStyle")) return update_text_style_request_apply(state, req["updateTextStyle"], ctx);
    if (req.contains("updateParagraphStyle")) return update_paragraph_style_request_apply(state, req["updateParagraphStyle"], ctx);
    if (req.contains("createParagraphBullets")) return create_paragraph_bullets_request_apply(state, req["createParagraphBullets"], ctx);
    if (req.contains("deleteParagraphBullets")) return delete_paragraph_bullets_request_apply(state, req["deleteParagraphBullets"], ctx);
    if (req.contains("insertPageBreak")) return insert_page_break_request_apply(state, req["insertPageBreak"], ctx);
    if (req.contains("insertSectionBreak")) return insert_section_break_request_apply(state, req["insertSectionBreak"], ctx);
    if (req.contains("updateSectionStyle")) return update_section_style_request_apply(state, req["updateSectionStyle"], ctx);
    if (req.contains("insertPerson")) return insert_atom_request_apply(state, req["insertPerson"], ctx, "person", "personProperties");
    if (req.contains("createDropdownDefinition")) return dropdown_definition_create_apply(state, req["createDropdownDefinition"], ctx);
    if (req.contains("updateDropdownDefinitionProperties")) return dropdown_definition_update_apply(state, req["updateDropdownDefinitionProperties"], ctx);
    if (req.contains("deleteDropdownDefinition")) return dropdown_definition_delete_apply(state, req["deleteDropdownDefinition"], ctx);
    if (req.contains("insertDropdown")) return insert_dropdown_apply(state, req["insertDropdown"], ctx);
    if (req.contains("updateDropdownProperties")) return dropdown_select_apply(state, req["updateDropdownProperties"], ctx);
    if (req.contains("createFootnote")) return create_footnote_request_apply(state, req["createFootnote"], ctx);
    if (req.contains("insertDate")) return insert_atom_request_apply(state, req["insertDate"], ctx, "dateElement", "dateElementProperties");
    if (req.contains("insertRichLink")) return insert_atom_request_apply(state, req["insertRichLink"], ctx, "richLink", "richLinkProperties");
    if (req.contains("insertInlineImage")) return insert_inline_image_request_apply(state, req["insertInlineImage"], ctx);
    if (req.contains("insertTable")) return insert_table_request_apply(state, req["insertTable"], ctx);
    if (req.contains("insertTableRow")) return insert_table_row_request_apply(state, req["insertTableRow"], ctx);
    if (req.contains("insertTableColumn")) return insert_table_column_request_apply(state, req["insertTableColumn"], ctx);
    if (req.contains("deleteTableRow")) return delete_table_row_request_apply(state, req["deleteTableRow"], ctx);
    if (req.contains("deleteTableColumn")) return delete_table_column_request_apply(state, req["deleteTableColumn"], ctx);
    if (req.contains("mergeTableCells")) return merge_table_cells_request_apply(state, req["mergeTableCells"], ctx);
    if (req.contains("unmergeTableCells")) return unmerge_table_cells_request_apply(state, req["unmergeTableCells"], ctx);
    if (req.contains("updateTableCellStyle")) return update_table_cell_style_request_apply(state, req["updateTableCellStyle"], ctx);
    if (req.contains("updateTableColumnProperties")) return update_table_column_properties_request_apply(state, req["updateTableColumnProperties"], ctx);
    if (req.contains("updateTableRowStyle")) return update_table_row_style_request_apply(state, req["updateTableRowStyle"], ctx);
    if (req.contains("pinTableHeaderRows")) return pin_table_header_rows_request_apply(state, req["pinTableHeaderRows"], ctx);
    if (req.contains("deletePositionedObject")) return delete_positioned_object_apply(state, req["deletePositionedObject"], ctx);
    if (req.contains("replaceImage")) return image_replace_apply(state, req["replaceImage"], ctx);
    if (req.contains("createNamedRange")) return named_range_create_apply(state, req["createNamedRange"], ctx);
    if (req.contains("deleteNamedRange")) return named_range_delete_apply(state, req["deleteNamedRange"], ctx);
    if (req.contains("addDocumentTab")) return add_document_tab_request_apply(state, req["addDocumentTab"], ctx);
    if (req.contains("deleteTab")) return delete_tab_request_apply(state, req["deleteTab"]);
    if (req.contains("updateDocumentTabProperties")) return update_document_tab_properties_request_apply(state, req["updateDocumentTabProperties"], ctx);
    if (req.contains("updateDocumentStyle")) return update_document_style_request_apply(state, req["updateDocumentStyle"], ctx);
    if (req.contains("updateNamedStyle")) return update_named_style_request_apply(state, req["updateNamedStyle"], ctx);
    if (req.contains("replaceAllText")) return replace_all_text_request_apply(state, req["replaceAllText"], ctx);

    std::string kinds;
    if (req.is_object()) {
        for (const auto& [k, v] : req.get<slimr::Json::object_t>()) {
            if (!kinds.empty()) kinds += ", ";
            kinds += k;
        }
    }
    throw EmulatorError(EmulatorErrorCode::UNKNOWN_REQUEST, ctx.request_index,
                        "unrecognized (or not yet implemented) request kind: " + kinds);
}

inline slimr::Json request_apply(EmulatorState& state, const slimr::Json& req, EmulateContext& ctx) {
    auto segment_id = field_find(req, "segmentId");
    if (segment_id && segment_id->is_string() && !segment_id->get<std::string>().empty()) {
        return segment_request_apply(state, req, segment_id->get<std::string>(), ctx);
    }

    auto tab_id = field_find(req, "tabId");
    TabState* tab = nullptr;
    if (tab_id && tab_id->is_string() && !tab_id->get<std::string>().empty()) {
        for (auto& t : state.tabs) {
            if (t.tab_id == tab_id->get<std::string>()) {
                tab = &t;
                break;
            }
        }
    } else if (!state.tabs.empty()) {
        tab = &state.tabs[0];
    }

    size_t before = tab ? tab->tape.size() : 0;
    slimr::Json reply = request_dispatch(state, req, ctx);

    auto at_val = field_find(req, "index");
    if (!at_val) at_val = field_find(req, "startIndex");
    if (tab && at_val && (at_val->is_int64() || at_val->is_number()) && tab->tape.size() != before) {
        int at = at_val->as<int>();
        named_ranges_shift(*tab, at, static_cast<int>(tab->tape.size()) - static_cast<int>(before));
    }
    return reply;
}

inline EmulateResult requests_emulate(const slimr::Json& json, const std::vector<slimr::Json>& requests) {
    EmulatorState state = doc_state_build(json);
    EmulateContext ctx{.id_counters = id_counters_seed(json), .request_index = 0};

    std::vector<slimr::Json> replies;
    replies.reserve(requests.size());

    for (size_t i = 0; i < requests.size(); ++i) {
        ctx.request_index = static_cast<int>(i);
        replies.push_back(request_apply(state, requests[i], ctx));
    }
    return EmulateResult{
        .json = doc_state_json(state),
        .replies = std::move(replies)
    };
}

} // namespace gdocsmith::emulator
