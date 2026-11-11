#pragma once

#include "json.hpp"
#include "yaml.hpp"

#include <algorithm>
#include <cctype>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace gdocsmith {

inline constexpr std::string_view GDOCSMITH_VERSION = "4.0.0";

// ============================================================================
// 1. Data Models (AST)
// ============================================================================

struct TextStyle {
    bool bold = false;
    bool italic = false;
    bool strikethrough = false;
    bool underline = false;
    bool code = false; // Monospace font
    std::string link_url;
    std::string foreground_color;
    std::string background_color;
    std::string font_family;
    double font_size_pt = 0.0;

    bool is_empty() const {
        return !bold && !italic && !strikethrough && !underline && !code &&
               link_url.empty() && foreground_color.empty() &&
               background_color.empty() && font_family.empty() && font_size_pt == 0.0;
    }

    bool operator==(const TextStyle& other) const = default;
};

struct TextRun {
    std::string text;
    TextStyle style;
    std::vector<std::string> suggested_insertion_ids;
    std::vector<std::string> suggested_deletion_ids;
    slimr::Json suggested_text_style_changes;
};

enum class AtomType {
    Image,
    Person,
    Date,
    RichLink,
    Dropdown,
    HorizontalRule,
    PageBreak,
    ColumnBreak,
    FootnoteRef,
    Equation,
    AutoText,
    Custom
};

struct Atom {
    AtomType type = AtomType::Custom;
    std::string type_name; // "image", "person", "date", "richLink", etc.
    std::string key;
    int length = 1;
    std::string uri;
    std::string email;
    std::string timestamp;
    std::string date_format;
    std::string time_format;
    std::string time_zone_id;
    std::string alt;
    std::optional<double> width_pt;
    std::optional<double> height_pt;
    std::string label;
    std::string footnote_id;
    std::string footnote_number;
    std::string dropdown_id;
    std::string option_id;
    TextStyle style;
    slimr::Json raw;
};

struct Inline {
    bool is_atom = false;
    TextRun text_run;
    Atom atom;

    static Inline text(std::string str, TextStyle style = {}) {
        Inline in;
        in.is_atom = false;
        in.text_run.text = std::move(str);
        in.text_run.style = std::move(style);
        return in;
    }

    static Inline make_atom(Atom a) {
        Inline in;
        in.is_atom = true;
        in.atom = std::move(a);
        return in;
    }

    bool is_text() const { return !is_atom; }
};

struct BulletRef {
    std::string list_id;
    int nesting_level = 0;
    std::string preset = "BULLET_DISC_CIRCLE_SQUARE";
    bool is_numbered = false;
    bool is_checkbox = false;

    bool operator==(const BulletRef& other) const = default;
};

struct ParagraphBlock {
    std::string key;
    std::string named_style_type = "NORMAL_TEXT"; // "NORMAL_TEXT", "HEADING_1" ... "HEADING_6", "TITLE", "SUBTITLE"
    int heading_level = 0; // 0 for normal/title/subtitle, 1..6 for HEADING_1..HEADING_6
    std::string heading_id;
    std::vector<Inline> inlines;
    std::optional<BulletRef> bullet;
    std::string alignment; // "START", "CENTER", "END", "JUSTIFIED"
    int start_index = -1;
    int end_index = -1;
    TextStyle newline_style;
    slimr::Json raw_style;

    std::string plain_text() const {
        std::string res;
        for (const auto& in : inlines) {
            if (in.is_text()) {
                res += in.text_run.text;
            } else if (in.atom.type == AtomType::Person && !in.atom.label.empty()) {
                res += in.atom.label;
            } else {
                res += " ";
            }
        }
        return res;
    }
};

struct TableCell {
    std::vector<ParagraphBlock> paragraphs;
    int row_span = 1;
    int col_span = 1;
};

struct TableBlock {
    std::string key;
    int rows = 0;
    int columns = 0;
    std::vector<std::vector<TableCell>> cells; // [row][col]
    std::vector<std::string> alignments; // "START", "CENTER", "END"
    int start_index = -1;
    int end_index = -1;
};

struct SectionBreakBlock {
    std::string key;
    std::string section_type = "CONTINUOUS"; // "CONTINUOUS", "NEXT_PAGE"
    slimr::Json section_style;
    int start_index = -1;
    int end_index = -1;
};

struct PageBreakBlock {
    std::string key;
    int start_index = -1;
    int end_index = -1;
};

struct HorizontalRuleBlock {
    std::string key;
    int start_index = -1;
    int end_index = -1;
};

enum class BlockKind {
    Paragraph,
    Table,
    SectionBreak,
    PageBreak,
    HorizontalRule,
    CodeBlock
};

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    ParagraphBlock paragraph;
    TableBlock table;
    SectionBreakBlock section_break;
    PageBreakBlock page_break;
    HorizontalRuleBlock hr;
    std::string code_fence_info;
    std::vector<std::string> code_lines;

    static Block make_paragraph(ParagraphBlock p) {
        Block b;
        b.kind = BlockKind::Paragraph;
        b.paragraph = std::move(p);
        return b;
    }

    static Block make_table(TableBlock t) {
        Block b;
        b.kind = BlockKind::Table;
        b.table = std::move(t);
        return b;
    }

    static Block make_section_break(SectionBreakBlock sb) {
        Block b;
        b.kind = BlockKind::SectionBreak;
        b.section_break = std::move(sb);
        return b;
    }

    static Block make_page_break(PageBreakBlock pb) {
        Block b;
        b.kind = BlockKind::PageBreak;
        b.page_break = std::move(pb);
        return b;
    }

    static Block make_hr(HorizontalRuleBlock h) {
        Block b;
        b.kind = BlockKind::HorizontalRule;
        b.hr = std::move(h);
        return b;
    }

    static Block make_code_block(std::string info, std::vector<std::string> lines) {
        Block b;
        b.kind = BlockKind::CodeBlock;
        b.code_fence_info = std::move(info);
        b.code_lines = std::move(lines);
        return b;
    }
};

struct CommentReply {
    std::string id;
    std::string author;
    std::string content;
    std::string timestamp;
    bool mine = false;
};

struct Comment {
    std::string id;
    std::string author;
    std::string content;
    std::string quoted_file_content;
    bool resolved = false;
    std::string tab_id;
    int anchor_start = -1;
    int anchor_end = -1;
    bool ambiguous = false;
    bool mine = false;
    std::vector<CommentReply> replies;
};

struct Suggestion {
    std::string id;
    std::string tab_id;
    std::string kind; // "insertion", "deletion", "style"
    std::string text;
    std::string author;
    slimr::Json style;
};

struct TabModel {
    std::string tab_id;
    std::string title;
    std::optional<std::string> parent_tab_id;
    int index = 0;
    std::vector<Block> blocks;
    slimr::Json document_style;
    slimr::Json named_styles;
    slimr::Json lists;
    bool is_new = false;
    bool is_deleted = false;
};

struct DocModel {
    std::string doc_id;
    std::string title;
    std::string revision_id;
    std::vector<TabModel> tabs;
    std::vector<Comment> comments;
    std::vector<Suggestion> suggestions;
    slimr::Json raw_doc;
};

// ============================================================================
// 2. Myers Diff Algorithm
// ============================================================================

struct DiffHunk {
    int a_start = 0;
    int a_end = 0;
    int b_start = 0;
    int b_end = 0;
};

template <typename T, typename Eq = std::equal_to<T>>
inline std::vector<DiffHunk> diff_myers(const std::vector<T>& a, const std::vector<T>& b, Eq eq = Eq{}) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    if (n == m) {
        bool same = true;
        for (int i = 0; i < n; ++i) {
            if (!eq(a[i], b[i])) { same = false; break; }
        }
        if (same) return {};
    }

    int pre = 0;
    while (pre < n && pre < m && eq(a[pre], b[pre])) pre++;
    int suf = 0;
    while (suf < n - pre && suf < m - pre && eq(a[n - 1 - suf], b[m - 1 - suf])) suf++;

    const int n_mid = n - pre - suf;
    const int m_mid = m - pre - suf;

    if (n_mid == 0 && m_mid == 0) return {};
    if (n_mid == 0) {
        return { DiffHunk{ pre, pre, pre, pre + m_mid } };
    }
    if (m_mid == 0) {
        return { DiffHunk{ pre, pre + n_mid, pre, pre } };
    }

    const int max_d = n_mid + m_mid;
    const int offset = max_d + 1;
    std::vector<int> v(2 * max_d + 3, 0);
    std::vector<std::vector<int>> trace;

    bool found = false;
    for (int d = 0; d <= max_d && !found; ++d) {
        trace.push_back(v);
        for (int k = -d; k <= d; k += 2) {
            int x = 0;
            if (k == -d || (k != d && v[offset + k - 1] < v[offset + k + 1])) {
                x = v[offset + k + 1];
            } else {
                x = v[offset + k - 1] + 1;
            }
            int y = x - k;
            while (x < n_mid && y < m_mid && eq(a[pre + x], b[pre + y])) {
                x++;
                y++;
            }
            v[offset + k] = x;
            if (x >= n_mid && y >= m_mid) {
                found = true;
                break;
            }
        }
    }

    std::vector<std::pair<int, int>> matches;
    int x = n_mid;
    int y = m_mid;
    for (int d = static_cast<int>(trace.size()) - 1; d >= 0 && (x > 0 || y > 0); --d) {
        const auto& vd = trace[d];
        int k = x - y;
        int prev_k = (k == -d || (k != d && vd[offset + k - 1] < vd[offset + k + 1])) ? k + 1 : k - 1;
        int prev_x = (d == 0) ? 0 : vd[offset + prev_k];
        int prev_y = prev_x - prev_k;
        while (x > prev_x && y > prev_y) {
            x--;
            y--;
            matches.push_back({ pre + x, pre + y });
        }
        x = prev_x;
        y = prev_y;
    }
    std::reverse(matches.begin(), matches.end());

    std::vector<DiffHunk> hunks;
    int cur_a = pre;
    int cur_b = pre;
    for (const auto& [ma, mb] : matches) {
        if (ma > cur_a || mb > cur_b) {
            hunks.push_back(DiffHunk{ cur_a, ma, cur_b, mb });
        }
        cur_a = ma + 1;
        cur_b = mb + 1;
    }
    if (cur_a < n - suf || cur_b < m - suf) {
        hunks.push_back(DiffHunk{ cur_a, n - suf, cur_b, m - suf });
    }
    return hunks;
}

inline std::vector<DiffHunk> diff_myers_string(std::string_view a, std::string_view b) {
    std::vector<char> va(a.begin(), a.end());
    std::vector<char> vb(b.begin(), b.end());
    return diff_myers(va, vb);
}

// ============================================================================
// 3. RequestBuilder for Google Docs batchUpdate
// ============================================================================

class RequestBuilder {
public:
    static slimr::Json make_range(int start_index, int end_index, std::string_view tab_id = "") {
        slimr::Json j = slimr::Json::object_t{};
        j["startIndex"] = static_cast<int64_t>(start_index);
        j["endIndex"] = static_cast<int64_t>(end_index);
        if (!tab_id.empty()) {
            j["tabId"] = std::string(tab_id);
        }
        return j;
    }

    static slimr::Json make_location(int index, std::string_view tab_id = "") {
        slimr::Json j = slimr::Json::object_t{};
        j["index"] = static_cast<int64_t>(index);
        if (!tab_id.empty()) {
            j["tabId"] = std::string(tab_id);
        }
        return j;
    }

    static slimr::Json insert_text(int index, std::string_view text, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["location"] = make_location(index, tab_id);
        body["text"] = std::string(text);
        req["insertText"] = std::move(body);
        return req;
    }

    static slimr::Json delete_content_range(int start_index, int end_index, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["range"] = make_range(start_index, end_index, tab_id);
        req["deleteContentRange"] = std::move(body);
        return req;
    }

    static slimr::Json update_text_style(int start_index, int end_index, const TextStyle& style,
                                        std::string_view fields, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["range"] = make_range(start_index, end_index, tab_id);
        body["fields"] = std::string(fields);
        slimr::Json ts = slimr::Json::object_t{};
        if (fields.find("bold") != std::string_view::npos) ts["bold"] = style.bold;
        if (fields.find("italic") != std::string_view::npos) ts["italic"] = style.italic;
        if (fields.find("strikethrough") != std::string_view::npos) ts["strikethrough"] = style.strikethrough;
        if (fields.find("underline") != std::string_view::npos) ts["underline"] = style.underline;
        if (fields.find("link") != std::string_view::npos) {
            slimr::Json link = slimr::Json::object_t{};
            link["url"] = style.link_url;
            ts["link"] = std::move(link);
        }
        if (fields.find("foregroundColor") != std::string_view::npos && !style.foreground_color.empty()) {
            slimr::Json fg = slimr::Json::object_t{};
            // Example color packaging
            fg["color"] = style.foreground_color;
            ts["foregroundColor"] = std::move(fg);
        }
        body["textStyle"] = std::move(ts);
        req["updateTextStyle"] = std::move(body);
        return req;
    }

    static slimr::Json update_paragraph_style(int start_index, int end_index, std::string_view named_style,
                                             std::string_view fields = "namedStyleType", std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["range"] = make_range(start_index, end_index, tab_id);
        body["fields"] = std::string(fields);
        slimr::Json ps = slimr::Json::object_t{};
        ps["namedStyleType"] = std::string(named_style);
        body["paragraphStyle"] = std::move(ps);
        req["updateParagraphStyle"] = std::move(body);
        return req;
    }

    static slimr::Json create_paragraph_bullets(int start_index, int end_index, std::string_view preset,
                                               std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["range"] = make_range(start_index, end_index, tab_id);
        body["bulletPreset"] = std::string(preset);
        req["createParagraphBullets"] = std::move(body);
        return req;
    }

    static slimr::Json delete_paragraph_bullets(int start_index, int end_index, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["range"] = make_range(start_index, end_index, tab_id);
        req["deleteParagraphBullets"] = std::move(body);
        return req;
    }

    static slimr::Json insert_table(int index, int rows, int columns, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["location"] = make_location(index, tab_id);
        body["rows"] = static_cast<int64_t>(rows);
        body["columns"] = static_cast<int64_t>(columns);
        req["insertTable"] = std::move(body);
        return req;
    }

    static slimr::Json insert_page_break(int index, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["location"] = make_location(index, tab_id);
        req["insertPageBreak"] = std::move(body);
        return req;
    }

    static slimr::Json insert_section_break(int index, std::string_view section_type = "NEXT_PAGE",
                                           std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["location"] = make_location(index, tab_id);
        body["sectionType"] = std::string(section_type);
        req["insertSectionBreak"] = std::move(body);
        return req;
    }

    static slimr::Json insert_inline_image(int index, std::string_view uri, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["location"] = make_location(index, tab_id);
        body["uri"] = std::string(uri);
        req["insertInlineImage"] = std::move(body);
        return req;
    }

    static slimr::Json insert_person(int index, std::string_view email, std::string_view tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["location"] = make_location(index, tab_id);
        body["person"] = slimr::Json::object_t{{"email", std::string(email)}};
        req["insertPerson"] = std::move(body);
        return req;
    }

    static slimr::Json add_document_tab(std::string_view title, int index = -1, std::string_view parent_tab_id = "") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        slimr::Json tp = slimr::Json::object_t{};
        tp["title"] = std::string(title);
        if (index >= 0) tp["index"] = static_cast<int64_t>(index);
        if (!parent_tab_id.empty()) tp["parentTabId"] = std::string(parent_tab_id);
        body["tabProperties"] = std::move(tp);
        req["addDocumentTab"] = std::move(body);
        return req;
    }

    static slimr::Json delete_tab(std::string_view tab_id) {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["tabId"] = std::string(tab_id);
        req["deleteTab"] = std::move(body);
        return req;
    }

    static slimr::Json update_document_tab_properties(std::string_view tab_id, std::string_view title,
                                                     std::string_view fields = "title") {
        slimr::Json req = slimr::Json::object_t{};
        slimr::Json body = slimr::Json::object_t{};
        body["fields"] = std::string(fields);
        slimr::Json tp = slimr::Json::object_t{};
        tp["tabId"] = std::string(tab_id);
        tp["title"] = std::string(title);
        body["tabProperties"] = std::move(tp);
        req["updateDocumentTabProperties"] = std::move(body);
        return req;
    }
};

// ============================================================================
// 4. Parsing Google Docs JSON into DocModel
// ============================================================================

namespace detail {

inline TextStyle parse_text_style(const slimr::Json& ts) {
    TextStyle s;
    if (!ts.is_object()) return s;
    if (ts.contains("bold") && ts["bold"].is_boolean()) s.bold = ts["bold"].get<bool>();
    if (ts.contains("italic") && ts["italic"].is_boolean()) s.italic = ts["italic"].get<bool>();
    if (ts.contains("strikethrough") && ts["strikethrough"].is_boolean()) s.strikethrough = ts["strikethrough"].get<bool>();
    if (ts.contains("underline") && ts["underline"].is_boolean()) s.underline = ts["underline"].get<bool>();

    if (ts.contains("link") && ts["link"].is_object() && ts["link"].contains("url")) {
        if (ts["link"]["url"].is_string()) s.link_url = ts["link"]["url"].get<std::string>();
    }

    if (ts.contains("weightedFontFamily") && ts["weightedFontFamily"].is_object() &&
        ts["weightedFontFamily"].contains("fontFamily")) {
        s.font_family = ts["weightedFontFamily"]["fontFamily"].get<std::string>();
    }
    // Check monospace code font
    std::string lower_font = s.font_family;
    std::transform(lower_font.begin(), lower_font.end(), lower_font.begin(), [](unsigned char c){ return std::tolower(c); });
    if (lower_font.find("consolas") != std::string::npos ||
        lower_font.find("courier") != std::string::npos ||
        lower_font.find("mono") != std::string::npos ||
        lower_font.find("source code") != std::string::npos) {
        s.code = true;
    }

    if (ts.contains("fontSize") && ts["fontSize"].is_object() && ts["fontSize"].contains("magnitude")) {
        if (ts["fontSize"]["magnitude"].is_number()) {
            s.font_size_pt = ts["fontSize"]["magnitude"].is_int64() ?
                static_cast<double>(ts["fontSize"]["magnitude"].get<int64_t>()) :
                ts["fontSize"]["magnitude"].get<double>();
        }
    }
    return s;
}

inline ParagraphBlock parse_paragraph(const slimr::Json& el, int& element_idx) {
    ParagraphBlock p;
    p.key = "p_" + std::to_string(++element_idx);
    if (el.contains("startIndex") && el["startIndex"].is_int64()) p.start_index = static_cast<int>(el["startIndex"].get<int64_t>());
    if (el.contains("endIndex") && el["endIndex"].is_int64()) p.end_index = static_cast<int>(el["endIndex"].get<int64_t>());

    const auto& p_json = el["paragraph"];
    if (p_json.contains("paragraphStyle") && p_json["paragraphStyle"].is_object()) {
        p.raw_style = p_json["paragraphStyle"];
        if (p_json["paragraphStyle"].contains("namedStyleType")) {
            p.named_style_type = p_json["paragraphStyle"]["namedStyleType"].get<std::string>();
            if (p.named_style_type == "HEADING_1") p.heading_level = 1;
            else if (p.named_style_type == "HEADING_2") p.heading_level = 2;
            else if (p.named_style_type == "HEADING_3") p.heading_level = 3;
            else if (p.named_style_type == "HEADING_4") p.heading_level = 4;
            else if (p.named_style_type == "HEADING_5") p.heading_level = 5;
            else if (p.named_style_type == "HEADING_6") p.heading_level = 6;
        }
        if (p_json["paragraphStyle"].contains("headingId")) {
            p.heading_id = p_json["paragraphStyle"]["headingId"].get<std::string>();
        }
        if (p_json["paragraphStyle"].contains("alignment")) {
            p.alignment = p_json["paragraphStyle"]["alignment"].get<std::string>();
        }
    }

    if (p_json.contains("bullet") && p_json["bullet"].is_object()) {
        BulletRef b;
        if (p_json["bullet"].contains("listId")) b.list_id = p_json["bullet"]["listId"].get<std::string>();
        if (p_json["bullet"].contains("nestingLevel") && p_json["bullet"]["nestingLevel"].is_int64()) {
            b.nesting_level = static_cast<int>(p_json["bullet"]["nestingLevel"].get<int64_t>());
        }
        p.bullet = std::move(b);
    }

    if (p_json.contains("elements") && p_json["elements"].is_array()) {
        const auto& elements = p_json["elements"].get<slimr::Json::array_t>();
        for (size_t i = 0; i < elements.size(); ++i) {
            const auto& pe = elements[i];
            bool is_last = (i == elements.size() - 1);

            if (pe.contains("textRun") && pe["textRun"].is_object()) {
                const auto& tr = pe["textRun"];
                std::string content;
                if (tr.contains("content") && tr["content"].is_string()) {
                    content = tr["content"].get<std::string>();
                }
                TextStyle style = parse_text_style(tr.contains("textStyle") ? tr["textStyle"] : slimr::Json{nullptr});
                if (is_last && content.ends_with('\n')) {
                    p.newline_style = style;
                    content.pop_back();
                }
                if (!content.empty()) {
                    Inline in = Inline::text(std::move(content), std::move(style));
                    if (tr.contains("suggestedInsertionIds") && tr["suggestedInsertionIds"].is_array()) {
                        for (const auto& id : tr["suggestedInsertionIds"].get<slimr::Json::array_t>()) {
                            if (id.is_string()) in.text_run.suggested_insertion_ids.push_back(id.get<std::string>());
                        }
                    }
                    if (tr.contains("suggestedDeletionIds") && tr["suggestedDeletionIds"].is_array()) {
                        for (const auto& id : tr["suggestedDeletionIds"].get<slimr::Json::array_t>()) {
                            if (id.is_string()) in.text_run.suggested_deletion_ids.push_back(id.get<std::string>());
                        }
                    }
                    if (tr.contains("suggestedTextStyleChanges")) {
                        in.text_run.suggested_text_style_changes = tr["suggestedTextStyleChanges"];
                    }
                    p.inlines.push_back(std::move(in));
                }
            } else if (pe.contains("inlineObjectElement")) {
                Atom a;
                a.type = AtomType::Image;
                a.type_name = "image";
                a.key = "o" + std::to_string(++element_idx);
                if (pe["inlineObjectElement"].contains("inlineObjectId")) {
                    a.uri = pe["inlineObjectElement"]["inlineObjectId"].get<std::string>();
                }
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("person")) {
                Atom a;
                a.type = AtomType::Person;
                a.type_name = "person";
                a.key = "o" + std::to_string(++element_idx);
                if (pe["person"].contains("personProperties") && pe["person"]["personProperties"].contains("email")) {
                    a.email = pe["person"]["personProperties"]["email"].get<std::string>();
                }
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("dateElement")) {
                Atom a;
                a.type = AtomType::Date;
                a.type_name = "date";
                a.key = "o" + std::to_string(++element_idx);
                if (pe["dateElement"].contains("dateProperties") && pe["dateElement"]["dateProperties"].contains("timestamp")) {
                    a.timestamp = pe["dateElement"]["dateProperties"]["timestamp"].get<std::string>();
                }
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("richLink")) {
                Atom a;
                a.type = AtomType::RichLink;
                a.type_name = "richLink";
                a.key = "o" + std::to_string(++element_idx);
                if (pe["richLink"].contains("richLinkProperties") && pe["richLink"]["richLinkProperties"].contains("uri")) {
                    a.uri = pe["richLink"]["richLinkProperties"]["uri"].get<std::string>();
                }
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("dropdown")) {
                Atom a;
                a.type = AtomType::Dropdown;
                a.type_name = "dropdown";
                a.key = "o" + std::to_string(++element_idx);
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("horizontalRule")) {
                Atom a;
                a.type = AtomType::HorizontalRule;
                a.type_name = "horizontalRule";
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("pageBreak")) {
                Atom a;
                a.type = AtomType::PageBreak;
                a.type_name = "pageBreak";
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            } else if (pe.contains("footnoteReference")) {
                Atom a;
                a.type = AtomType::FootnoteRef;
                a.type_name = "footnoteRef";
                if (pe["footnoteReference"].contains("footnoteId")) {
                    a.footnote_id = pe["footnoteReference"]["footnoteId"].get<std::string>();
                }
                if (pe["footnoteReference"].contains("footnoteNumber")) {
                    a.footnote_number = pe["footnoteReference"]["footnoteNumber"].get<std::string>();
                }
                p.inlines.push_back(Inline::make_atom(std::move(a)));
            }
        }
    }
    return p;
}

inline TableBlock parse_table(const slimr::Json& el, int& element_idx) {
    TableBlock t;
    t.key = "tbl_" + std::to_string(++element_idx);
    if (el.contains("startIndex") && el["startIndex"].is_int64()) t.start_index = static_cast<int>(el["startIndex"].get<int64_t>());
    if (el.contains("endIndex") && el["endIndex"].is_int64()) t.end_index = static_cast<int>(el["endIndex"].get<int64_t>());

    const auto& tbl = el["table"];
    if (tbl.contains("rows") && tbl["rows"].is_int64()) t.rows = static_cast<int>(tbl["rows"].get<int64_t>());
    if (tbl.contains("columns") && tbl["columns"].is_int64()) t.columns = static_cast<int>(tbl["columns"].get<int64_t>());

    if (tbl.contains("tableRows") && tbl["tableRows"].is_array()) {
        for (const auto& r_json : tbl["tableRows"].get<slimr::Json::array_t>()) {
            std::vector<TableCell> row_cells;
            if (r_json.contains("tableCells") && r_json["tableCells"].is_array()) {
                for (const auto& c_json : r_json["tableCells"].get<slimr::Json::array_t>()) {
                    TableCell cell;
                    if (c_json.contains("content") && c_json["content"].is_array()) {
                        for (const auto& ce : c_json["content"].get<slimr::Json::array_t>()) {
                            if (ce.contains("paragraph")) {
                                cell.paragraphs.push_back(parse_paragraph(ce, element_idx));
                            }
                        }
                    }
                    row_cells.push_back(std::move(cell));
                }
            }
            t.cells.push_back(std::move(row_cells));
        }
    }
    return t;
}

inline std::vector<Block> parse_content(const slimr::Json& content, int& element_idx) {
    std::vector<Block> blocks;
    if (!content.is_array()) return blocks;
    const auto& arr = content.get<slimr::Json::array_t>();

    for (size_t i = 0; i < arr.size(); ++i) {
        const auto& el = arr[i];
        if (el.contains("sectionBreak")) {
            // First section break is the leading section break at index 0
            if (i == 0 && el.contains("startIndex") && el["startIndex"].get<int64_t>() == 0) {
                continue;
            }
            SectionBreakBlock sb;
            sb.key = "sb_" + std::to_string(++element_idx);
            if (el.contains("startIndex") && el["startIndex"].is_int64()) sb.start_index = static_cast<int>(el["startIndex"].get<int64_t>());
            if (el.contains("endIndex") && el["endIndex"].is_int64()) sb.end_index = static_cast<int>(el["endIndex"].get<int64_t>());
            if (el["sectionBreak"].contains("sectionStyle")) sb.section_style = el["sectionBreak"]["sectionStyle"];
            blocks.push_back(Block::make_section_break(std::move(sb)));
        } else if (el.contains("paragraph")) {
            blocks.push_back(Block::make_paragraph(parse_paragraph(el, element_idx)));
        } else if (el.contains("table")) {
            blocks.push_back(Block::make_table(parse_table(el, element_idx)));
        } else if (el.contains("horizontalRule")) {
            HorizontalRuleBlock hr;
            hr.key = "hr_" + std::to_string(++element_idx);
            if (el.contains("startIndex") && el["startIndex"].is_int64()) hr.start_index = static_cast<int>(el["startIndex"].get<int64_t>());
            if (el.contains("endIndex") && el["endIndex"].is_int64()) hr.end_index = static_cast<int>(el["endIndex"].get<int64_t>());
            blocks.push_back(Block::make_hr(std::move(hr)));
        }
    }
    return blocks;
}

inline void flatten_tabs_dfs(const slimr::Json& raw_tab, std::vector<TabModel>& out_tabs, int& element_idx) {
    TabModel tm;
    if (raw_tab.contains("tabProperties") && raw_tab["tabProperties"].is_object()) {
        const auto& tp = raw_tab["tabProperties"];
        if (tp.contains("tabId") && tp["tabId"].is_string()) tm.tab_id = tp["tabId"].get<std::string>();
        if (tp.contains("title") && tp["title"].is_string()) tm.title = tp["title"].get<std::string>();
        if (tp.contains("parentTabId") && tp["parentTabId"].is_string()) tm.parent_tab_id = tp["parentTabId"].get<std::string>();
        if (tp.contains("index") && tp["index"].is_int64()) tm.index = static_cast<int>(tp["index"].get<int64_t>());
    }

    const auto& dt = raw_tab.contains("documentTab") ? raw_tab["documentTab"] : raw_tab;
    if (dt.contains("body") && dt["body"].contains("content")) {
        tm.blocks = parse_content(dt["body"]["content"], element_idx);
    }
    if (dt.contains("lists")) tm.lists = dt["lists"];
    if (dt.contains("documentStyle")) tm.document_style = dt["documentStyle"];
    if (dt.contains("namedStyles")) tm.named_styles = dt["namedStyles"];

    out_tabs.push_back(std::move(tm));

    if (raw_tab.contains("childTabs") && raw_tab["childTabs"].is_array()) {
        for (const auto& child : raw_tab["childTabs"].get<slimr::Json::array_t>()) {
            flatten_tabs_dfs(child, out_tabs, element_idx);
        }
    }
}

} // namespace detail

inline DocModel parse_doc_json(const slimr::Json& json, const std::string& doc_id_override = "") {
    DocModel doc;
    doc.raw_doc = json;
    if (!doc_id_override.empty()) {
        doc.doc_id = doc_id_override;
    } else if (json.contains("documentId") && json["documentId"].is_string()) {
        doc.doc_id = json["documentId"].get<std::string>();
    }

    if (json.contains("title") && json["title"].is_string()) {
        doc.title = json["title"].get<std::string>();
    }
    if (json.contains("revisionId") && json["revisionId"].is_string()) {
        doc.revision_id = json["revisionId"].get<std::string>();
    }

    int element_idx = 0;
    if (json.contains("tabs") && json["tabs"].is_array() && !json["tabs"].get<slimr::Json::array_t>().empty()) {
        for (const auto& t : json["tabs"].get<slimr::Json::array_t>()) {
            detail::flatten_tabs_dfs(t, doc.tabs, element_idx);
        }
    } else {
        TabModel tm;
        tm.tab_id = "t.0";
        tm.title = doc.title;
        if (json.contains("body") && json["body"].contains("content")) {
            tm.blocks = detail::parse_content(json["body"]["content"], element_idx);
        }
        if (json.contains("lists")) tm.lists = json["lists"];
        if (json.contains("documentStyle")) tm.document_style = json["documentStyle"];
        if (json.contains("namedStyles")) tm.named_styles = json["namedStyles"];
        doc.tabs.push_back(std::move(tm));
    }

    // Determine list presets and numbering
    for (auto& tab : doc.tabs) {
        if (!tab.lists.is_object()) continue;
        for (auto& b : tab.blocks) {
            if (b.kind != BlockKind::Paragraph || !b.paragraph.bullet) continue;
            auto& b_ref = *b.paragraph.bullet;
            if (tab.lists.contains(b_ref.list_id)) {
                const auto& l_def = tab.lists[b_ref.list_id];
                if (l_def.contains("listProperties") && l_def["listProperties"].contains("nestingLevels")) {
                    const auto& nls = l_def["listProperties"]["nestingLevels"];
                    if (nls.is_array() && b_ref.nesting_level < static_cast<int>(nls.get<slimr::Json::array_t>().size())) {
                        const auto& nl = nls.get<slimr::Json::array_t>()[b_ref.nesting_level];
                        if (nl.contains("glyphType")) {
                            std::string gt = nl["glyphType"].get<std::string>();
                            if (gt.find("DECIMAL") != std::string::npos || gt.find("ROMAN") != std::string::npos || gt.find("ALPHA") != std::string::npos) {
                                b_ref.is_numbered = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // Extract suggestions from blocks
    for (const auto& tab : doc.tabs) {
        for (const auto& b : tab.blocks) {
            if (b.kind != BlockKind::Paragraph) continue;
            for (const auto& in : b.paragraph.inlines) {
                if (!in.is_text()) continue;
                for (const auto& sid : in.text_run.suggested_insertion_ids) {
                    doc.suggestions.push_back(Suggestion{
                        .id = sid,
                        .tab_id = tab.tab_id,
                        .kind = "insertion",
                        .text = in.text_run.text
                    });
                }
                for (const auto& sid : in.text_run.suggested_deletion_ids) {
                    doc.suggestions.push_back(Suggestion{
                        .id = sid,
                        .tab_id = tab.tab_id,
                        .kind = "deletion",
                        .text = in.text_run.text
                    });
                }
            }
        }
    }

    // Parse comments if present in json or raw_doc
    if (json.contains("comments") && json["comments"].is_array()) {
        for (const auto& c_json : json["comments"].get<slimr::Json::array_t>()) {
            Comment c;
            if (c_json.contains("id")) c.id = c_json["id"].get<std::string>();
            if (c_json.contains("author") && c_json["author"].contains("displayName")) {
                c.author = c_json["author"]["displayName"].get<std::string>();
            } else if (c_json.contains("author") && c_json["author"].is_string()) {
                c.author = c_json["author"].get<std::string>();
            }
            if (c_json.contains("content")) c.content = c_json["content"].get<std::string>();
            if (c_json.contains("quotedFileContent")) {
                if (c_json["quotedFileContent"].is_object() && c_json["quotedFileContent"].contains("value")) {
                    c.quoted_file_content = c_json["quotedFileContent"]["value"].get<std::string>();
                } else if (c_json["quotedFileContent"].is_string()) {
                    c.quoted_file_content = c_json["quotedFileContent"].get<std::string>();
                }
            }
            if (c_json.contains("resolved") && c_json["resolved"].is_boolean()) {
                c.resolved = c_json["resolved"].get<bool>();
            }

            if (c_json.contains("replies") && c_json["replies"].is_array()) {
                for (const auto& r_json : c_json["replies"].get<slimr::Json::array_t>()) {
                    CommentReply r;
                    if (r_json.contains("id")) r.id = r_json["id"].get<std::string>();
                    if (r_json.contains("author") && r_json["author"].contains("displayName")) {
                        r.author = r_json["author"]["displayName"].get<std::string>();
                    } else if (r_json.contains("author") && r_json["author"].is_string()) {
                        r.author = r_json["author"].get<std::string>();
                    }
                    if (r_json.contains("content")) r.content = r_json["content"].get<std::string>();
                    c.replies.push_back(std::move(r));
                }
            }

            // Match quote to document text
            if (!c.quoted_file_content.empty()) {
                int match_count = 0;
                for (const auto& tab : doc.tabs) {
                    for (const auto& b : tab.blocks) {
                        if (b.kind != BlockKind::Paragraph) continue;
                        std::string pt = b.paragraph.plain_text();
                        auto pos = pt.find(c.quoted_file_content);
                        if (pos != std::string::npos) {
                            match_count++;
                            if (c.anchor_start < 0) {
                                c.tab_id = tab.tab_id;
                                c.anchor_start = (b.paragraph.start_index >= 0) ? (b.paragraph.start_index + static_cast<int>(pos)) : static_cast<int>(pos);
                                c.anchor_end = c.anchor_start + static_cast<int>(c.quoted_file_content.size());
                            }
                        }
                    }
                }
                if (match_count > 1) c.ambiguous = true;
            }
            doc.comments.push_back(std::move(c));
        }
    }
    return doc;
}

inline DocModel parse_doc_json(std::string_view json_str, const std::string& doc_id_override = "") {
    auto parsed = slimr::parse_json<slimr::Json>(json_str);
    if (!parsed) {
        DocModel empty;
        empty.doc_id = doc_id_override;
        return empty;
    }
    return parse_doc_json(*parsed, doc_id_override);
}

// ============================================================================
// 5. Exporting to Markdown and YAML
// ============================================================================

struct ExportResult {
    std::string base_md;
    std::string next_md;
    std::optional<std::string> comments_yaml;
    std::optional<std::string> suggestions_yaml;
    std::vector<std::string> warnings;
};

inline std::string render_inlines(const std::vector<Inline>& inlines) {
    std::string out;
    for (const auto& in : inlines) {
        if (in.is_text()) {
            std::string t = in.text_run.text;
            if (t.empty()) continue;
            const auto& s = in.text_run.style;
            if (s.code) t = "`" + t + "`";
            if (s.strikethrough) t = "~~" + t + "~~";
            if (s.italic) t = "*" + t + "*";
            if (s.bold) t = "**" + t + "**";
            if (!s.link_url.empty()) t = "[" + t + "](" + s.link_url + ")";
            out += t;
        } else {
            const auto& a = in.atom;
            if (a.type == AtomType::Image) {
                out += "![" + a.alt + "](" + a.uri + ")";
            } else if (a.type == AtomType::Person) {
                out += ":person{email=\"" + a.email + "\"}";
            } else if (a.type == AtomType::Date) {
                out += ":date{timestamp=\"" + a.timestamp + "\"}";
            } else if (a.type == AtomType::RichLink) {
                out += ":richLink{uri=\"" + a.uri + "\"}";
            } else if (a.type == AtomType::Dropdown) {
                out += ":dropdown{id=\"" + a.dropdown_id + "\"}";
            } else if (a.type == AtomType::FootnoteRef) {
                out += "[^" + a.footnote_number + "]";
            } else if (a.type == AtomType::HorizontalRule) {
                out += "---";
            } else if (a.type == AtomType::PageBreak) {
                out += "::pagebreak{}";
            }
        }
    }
    return out;
}

inline std::string render_table(const TableBlock& table) {
    if (table.cells.empty() || table.cells[0].empty()) return "";
    std::ostringstream ss;
    size_t cols = table.cells[0].size();

    // Row 0: Header
    ss << "|";
    for (size_t c = 0; c < cols; ++c) {
        std::string cell_text;
        for (const auto& p : table.cells[0][c].paragraphs) {
            if (!cell_text.empty()) cell_text += "<br>";
            cell_text += render_inlines(p.inlines);
        }
        if (cell_text.empty()) cell_text = " ";
        ss << " " << cell_text << " |";
    }
    ss << "\n|";

    // Separator
    for (size_t c = 0; c < cols; ++c) {
        std::string align = (c < table.alignments.size()) ? table.alignments[c] : "START";
        if (align == "CENTER") ss << " :-: |";
        else if (align == "END") ss << " --: |";
        else ss << " --- |";
    }
    ss << "\n";

    // Data rows
    for (size_t r = 1; r < table.cells.size(); ++r) {
        ss << "|";
        for (size_t c = 0; c < cols; ++c) {
            std::string cell_text;
            if (c < table.cells[r].size()) {
                for (const auto& p : table.cells[r][c].paragraphs) {
                    if (!cell_text.empty()) cell_text += "<br>";
                    cell_text += render_inlines(p.inlines);
                }
            }
            if (cell_text.empty()) cell_text = " ";
            ss << " " << cell_text << " |";
        }
        ss << "\n";
    }
    return ss.str();
}

inline std::string render_blocks(const std::vector<Block>& blocks) {
    std::ostringstream ss;
    bool first = true;
    for (const auto& b : blocks) {
        if (!first) ss << "\n\n";
        first = false;

        if (b.kind == BlockKind::Paragraph) {
            const auto& p = b.paragraph;
            std::string text = render_inlines(p.inlines);
            if (p.heading_level >= 1 && p.heading_level <= 6) {
                ss << std::string(p.heading_level, '#') << " " << text;
            } else if (p.bullet) {
                std::string indent(p.bullet->nesting_level * 2, ' ');
                std::string marker = p.bullet->is_numbered ? "1. " : "- ";
                ss << indent << marker << text;
            } else {
                ss << text;
            }
        } else if (b.kind == BlockKind::Table) {
            ss << render_table(b.table);
        } else if (b.kind == BlockKind::HorizontalRule) {
            ss << "---";
        } else if (b.kind == BlockKind::PageBreak) {
            ss << "::pagebreak{}";
        } else if (b.kind == BlockKind::SectionBreak) {
            ss << "::section{type=\"" << b.section_break.section_type << "\"}";
        } else if (b.kind == BlockKind::CodeBlock) {
            ss << "```" << b.code_fence_info << "\n";
            for (const auto& l : b.code_lines) ss << l << "\n";
            ss << "```";
        }
    }
    return ss.str();
}

inline ExportResult export_artifacts(const DocModel& doc) {
    ExportResult res;

    // Build frontmatter
    slimr::Json fm = slimr::Json::object_t{};
    fm["gdocsmith"] = std::string(GDOCSMITH_VERSION);
    if (!doc.doc_id.empty()) {
        fm["doc"] = doc.doc_id;
    } else {
        fm["doc"] = nullptr;
    }
    if (!doc.revision_id.empty()) {
        fm["revision"] = doc.revision_id;
    } else {
        fm["revision"] = nullptr;
    }
    fm["title"] = doc.title;

    std::string fm_yaml = slimr::to_yaml(fm);
    std::ostringstream body;
    body << "---\n" << fm_yaml << "---\n\n";

    bool multi_tab = doc.tabs.size() > 1;
    for (size_t i = 0; i < doc.tabs.size(); ++i) {
        const auto& tab = doc.tabs[i];
        if (multi_tab) {
            body << "::tab{id=" << tab.tab_id << " title=\"" << tab.title << "\"";
            if (tab.parent_tab_id && !tab.parent_tab_id->empty()) {
                body << " parent=\"" << *tab.parent_tab_id << "\"";
            }
            body << "}\n\n";
        }
        body << render_blocks(tab.blocks);
        if (i + 1 < doc.tabs.size()) {
            body << "\n\n";
        }
    }
    body << "\n";

    res.base_md = body.str();
    res.next_md = res.base_md;

    // Export comments.yaml if any
    if (!doc.comments.empty()) {
        slimr::Json::array_t carr;
        for (const auto& c : doc.comments) {
            slimr::Json cj = slimr::Json::object_t{};
            cj["id"] = c.id;
            cj["author"] = c.author;
            cj["content"] = c.content;
            if (!c.quoted_file_content.empty()) cj["quotedFileContent"] = c.quoted_file_content;
            cj["resolved"] = c.resolved;
            if (!c.tab_id.empty()) cj["tab"] = c.tab_id;
            if (!c.replies.empty()) {
                slimr::Json::array_t rarr;
                for (const auto& r : c.replies) {
                    slimr::Json rj = slimr::Json::object_t{};
                    rj["id"] = r.id;
                    rj["author"] = r.author;
                    rj["content"] = r.content;
                    rarr.push_back(std::move(rj));
                }
                cj["replies"] = std::move(rarr);
            }
            carr.push_back(std::move(cj));
        }
        res.comments_yaml = slimr::to_yaml(slimr::Json{std::move(carr)});
    }

    // Export suggestions.yaml if any
    if (!doc.suggestions.empty()) {
        slimr::Json::array_t sarr;
        for (const auto& s : doc.suggestions) {
            slimr::Json sj = slimr::Json::object_t{};
            sj["id"] = s.id;
            sj["tab"] = s.tab_id;
            sj["kind"] = s.kind;
            sj["text"] = s.text;
            sarr.push_back(std::move(sj));
        }
        res.suggestions_yaml = slimr::to_yaml(slimr::Json{std::move(sarr)});
    }

    return res;
}

// ============================================================================
// 6. Parsing Edited Markdown into Structured Blocks
// ============================================================================

namespace detail {

inline std::vector<Inline> parse_markdown_inlines(std::string_view text) {
    std::vector<Inline> inlines;
    size_t i = 0;
    std::string current_text;

    auto flush_text = [&]() {
        if (!current_text.empty()) {
            inlines.push_back(Inline::text(std::move(current_text)));
            current_text.clear();
        }
    };

    while (i < text.size()) {
        // Check image: ![alt](url)
        if (text[i] == '!' && i + 1 < text.size() && text[i + 1] == '[') {
            size_t close_bracket = text.find(']', i + 2);
            if (close_bracket != std::string_view::npos && close_bracket + 1 < text.size() && text[close_bracket + 1] == '(') {
                size_t close_paren = text.find(')', close_bracket + 2);
                if (close_paren != std::string_view::npos) {
                    flush_text();
                    Atom a;
                    a.type = AtomType::Image;
                    a.type_name = "image";
                    a.alt = std::string(text.substr(i + 2, close_bracket - (i + 2)));
                    a.uri = std::string(text.substr(close_bracket + 2, close_paren - (close_bracket + 2)));
                    inlines.push_back(Inline::make_atom(std::move(a)));
                    i = close_paren + 1;
                    continue;
                }
            }
        }

        // Check directives: :person{email="..."}
        if (text[i] == ':' && text.substr(i).starts_with(":person{")) {
            size_t close = text.find('}', i + 8);
            if (close != std::string_view::npos) {
                flush_text();
                std::string attrs(text.substr(i + 8, close - (i + 8)));
                Atom a;
                a.type = AtomType::Person;
                a.type_name = "person";
                std::regex re(R"(email=["']?([^"'\s}]+)["']?)");
                std::smatch m;
                if (std::regex_search(attrs, m, re)) a.email = m[1].str();
                inlines.push_back(Inline::make_atom(std::move(a)));
                i = close + 1;
                continue;
            }
        }
        // Check date: :date{timestamp="..."}
        if (text[i] == ':' && text.substr(i).starts_with(":date{")) {
            size_t close = text.find('}', i + 6);
            if (close != std::string_view::npos) {
                flush_text();
                std::string attrs(text.substr(i + 6, close - (i + 6)));
                Atom a;
                a.type = AtomType::Date;
                a.type_name = "date";
                std::regex re(R"((?:timestamp|value)=["']?([^"'\s}]+)["']?)");
                std::smatch m;
                if (std::regex_search(attrs, m, re)) a.timestamp = m[1].str();
                inlines.push_back(Inline::make_atom(std::move(a)));
                i = close + 1;
                continue;
            }
        }
        // Check richLink: :richLink{uri="..."}
        if (text[i] == ':' && (text.substr(i).starts_with(":richLink{") || text.substr(i).starts_with(":richlink{"))) {
            size_t open_pos = text.find('{', i);
            size_t close = text.find('}', open_pos);
            if (close != std::string_view::npos) {
                flush_text();
                std::string attrs(text.substr(open_pos + 1, close - open_pos - 1));
                Atom a;
                a.type = AtomType::RichLink;
                a.type_name = "richLink";
                std::regex re(R"((?:uri|url)=["']?([^"'\s}]+)["']?)");
                std::smatch m;
                if (std::regex_search(attrs, m, re)) a.uri = m[1].str();
                inlines.push_back(Inline::make_atom(std::move(a)));
                i = close + 1;
                continue;
            }
        }

        // Check footnote reference: [^1]
        if (text[i] == '[' && i + 2 < text.size() && text[i + 1] == '^') {
            size_t close = text.find(']', i + 2);
            if (close != std::string_view::npos) {
                flush_text();
                Atom a;
                a.type = AtomType::FootnoteRef;
                a.type_name = "footnoteRef";
                a.footnote_number = std::string(text.substr(i + 2, close - (i + 2)));
                inlines.push_back(Inline::make_atom(std::move(a)));
                i = close + 1;
                continue;
            }
        }

        // Check link: [text](url)
        if (text[i] == '[') {
            size_t close_bracket = text.find(']', i + 1);
            if (close_bracket != std::string_view::npos && close_bracket + 1 < text.size() && text[close_bracket + 1] == '(') {
                size_t close_paren = text.find(')', close_bracket + 2);
                if (close_paren != std::string_view::npos) {
                    flush_text();
                    std::string label(text.substr(i + 1, close_bracket - (i + 1)));
                    std::string url(text.substr(close_bracket + 2, close_paren - (close_bracket + 2)));
                    TextStyle s;
                    s.link_url = url;
                    inlines.push_back(Inline::text(std::move(label), std::move(s)));
                    i = close_paren + 1;
                    continue;
                }
            }
        }

        // Check bold: **text**
        if (text[i] == '*' && i + 1 < text.size() && text[i + 1] == '*') {
            size_t close = text.find("**", i + 2);
            if (close != std::string_view::npos) {
                flush_text();
                std::string inner(text.substr(i + 2, close - (i + 2)));
                TextStyle s;
                s.bold = true;
                inlines.push_back(Inline::text(std::move(inner), std::move(s)));
                i = close + 2;
                continue;
            }
        }

        // Check strikethrough: ~~text~~
        if (text[i] == '~' && i + 1 < text.size() && text[i + 1] == '~') {
            size_t close = text.find("~~", i + 2);
            if (close != std::string_view::npos) {
                flush_text();
                std::string inner(text.substr(i + 2, close - (i + 2)));
                TextStyle s;
                s.strikethrough = true;
                inlines.push_back(Inline::text(std::move(inner), std::move(s)));
                i = close + 2;
                continue;
            }
        }

        // Check italic: *text* (single *)
        if (text[i] == '*') {
            size_t close = text.find('*', i + 1);
            if (close != std::string_view::npos) {
                flush_text();
                std::string inner(text.substr(i + 1, close - (i + 1)));
                TextStyle s;
                s.italic = true;
                inlines.push_back(Inline::text(std::move(inner), std::move(s)));
                i = close + 1;
                continue;
            }
        }

        // Check code: `text`
        if (text[i] == '`') {
            size_t close = text.find('`', i + 1);
            if (close != std::string_view::npos) {
                flush_text();
                std::string inner(text.substr(i + 1, close - (i + 1)));
                TextStyle s;
                s.code = true;
                inlines.push_back(Inline::text(std::move(inner), std::move(s)));
                i = close + 1;
                continue;
            }
        }

        current_text += text[i];
        i++;
    }
    flush_text();
    return inlines;
}

inline std::vector<Block> parse_markdown_blocks(std::string_view markdown) {
    std::vector<Block> blocks;
    std::istringstream stream{std::string(markdown)};
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }

    size_t i = 0;
    while (i < lines.size()) {
        const std::string& l = lines[i];

        // Skip blank lines
        if (l.empty() || l.find_first_not_of(" \t") == std::string::npos) {
            i++;
            continue;
        }

        // Check code fence: ```
        if (l.starts_with("```")) {
            std::string info = l.substr(3);
            std::vector<std::string> code_lines;
            i++;
            while (i < lines.size() && !lines[i].starts_with("```")) {
                code_lines.push_back(lines[i]);
                i++;
            }
            if (i < lines.size() && lines[i].starts_with("```")) i++;
            blocks.push_back(Block::make_code_block(std::move(info), std::move(code_lines)));
            continue;
        }

        // Check GFM table: starts with '|'
        if (l.starts_with('|') && l.ends_with('|') && i + 1 < lines.size() && lines[i + 1].starts_with('|') &&
            lines[i + 1].find("---") != std::string::npos) {
            TableBlock tbl;
            std::vector<std::vector<TableCell>> rows;

            // Row 0: Headers
            auto parse_row = [](const std::string& r_str) {
                std::vector<TableCell> cells;
                size_t p = 1;
                while (p < r_str.size()) {
                    size_t next_bar = r_str.find('|', p);
                    if (next_bar == std::string::npos) break;
                    std::string c_str = r_str.substr(p, next_bar - p);
                    // Trim
                    size_t first = c_str.find_first_not_of(" \t");
                    size_t last = c_str.find_last_not_of(" \t");
                    if (first != std::string::npos) c_str = c_str.substr(first, last - first + 1);
                    else c_str = "";

                    TableCell cell;
                    ParagraphBlock cp;
                    cp.inlines = parse_markdown_inlines(c_str);
                    cell.paragraphs.push_back(std::move(cp));
                    cells.push_back(std::move(cell));
                    p = next_bar + 1;
                }
                return cells;
            };

            rows.push_back(parse_row(lines[i]));
            // Parse alignment row
            const std::string& sep = lines[i + 1];
            size_t p = 1;
            while (p < sep.size()) {
                size_t next_bar = sep.find('|', p);
                if (next_bar == std::string::npos) break;
                std::string col_sep = sep.substr(p, next_bar - p);
                if (col_sep.starts_with(':') && col_sep.ends_with(':')) tbl.alignments.push_back("CENTER");
                else if (col_sep.ends_with(':')) tbl.alignments.push_back("END");
                else tbl.alignments.push_back("START");
                p = next_bar + 1;
            }
            i += 2;

            // Parse body rows
            while (i < lines.size() && lines[i].starts_with('|') && lines[i].ends_with('|')) {
                rows.push_back(parse_row(lines[i]));
                i++;
            }
            tbl.rows = static_cast<int>(rows.size());
            tbl.columns = rows.empty() ? 0 : static_cast<int>(rows[0].size());
            tbl.cells = std::move(rows);
            blocks.push_back(Block::make_table(std::move(tbl)));
            continue;
        }

        // Check horizontal rule: ---, ***, ___
        if (l == "---" || l == "***" || l == "___") {
            blocks.push_back(Block::make_hr(HorizontalRuleBlock{}));
            i++;
            continue;
        }

        // Check pagebreak
        if (l.starts_with("::pagebreak")) {
            blocks.push_back(Block::make_page_break(PageBreakBlock{}));
            i++;
            continue;
        }

        // Check headings: # ... ######
        if (l.starts_with('#')) {
            size_t hashes = 0;
            while (hashes < l.size() && l[hashes] == '#' && hashes < 6) hashes++;
            if (hashes > 0 && hashes < l.size() && l[hashes] == ' ') {
                std::string text = l.substr(hashes + 1);
                ParagraphBlock p;
                p.heading_level = static_cast<int>(hashes);
                p.named_style_type = "HEADING_" + std::to_string(hashes);
                p.inlines = parse_markdown_inlines(text);
                blocks.push_back(Block::make_paragraph(std::move(p)));
                i++;
                continue;
            }
        }

        // Check lists: bullet (- , * ) or numbered (1. , 1) )
        size_t indent = 0;
        while (indent < l.size() && l[indent] == ' ') indent++;
        std::string_view trimmed(l.data() + indent, l.size() - indent);

        bool is_bullet = (trimmed.starts_with("- ") || trimmed.starts_with("* "));
        bool is_num = false;
        size_t num_prefix_len = 0;
        if (!is_bullet && !trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0]))) {
            size_t dot_pos = trimmed.find_first_of(".)");
            if (dot_pos != std::string_view::npos && dot_pos + 1 < trimmed.size() && trimmed[dot_pos + 1] == ' ') {
                is_num = true;
                num_prefix_len = dot_pos + 2;
            }
        }

        if (is_bullet || is_num) {
            ParagraphBlock p;
            BulletRef b;
            if (is_bullet) {
                b.nesting_level = static_cast<int>(indent / 2);
                b.is_numbered = false;
                std::string item_text(trimmed.substr(2));
                if (item_text.starts_with("[ ] ") || item_text.starts_with("[x] ")) {
                    b.is_checkbox = true;
                    item_text = item_text.substr(4);
                }
                p.inlines = parse_markdown_inlines(item_text);
            } else {
                b.nesting_level = static_cast<int>(indent / 3);
                b.is_numbered = true;
                b.preset = "NUMBERED_DECIMAL_ALPHA_ROMAN";
                p.inlines = parse_markdown_inlines(trimmed.substr(num_prefix_len));
            }
            p.bullet = std::move(b);
            blocks.push_back(Block::make_paragraph(std::move(p)));
            i++;
            continue;
        }

        // Regular paragraph (can span until empty line)
        std::string p_text = l;
        i++;
        while (i < lines.size() && !lines[i].empty() && lines[i].find_first_not_of(" \t") != std::string::npos &&
               !lines[i].starts_with('#') && !lines[i].starts_with("```") && !lines[i].starts_with('|') &&
               lines[i] != "---" && !lines[i].starts_with("::")) {
            p_text += " " + lines[i];
            i++;
        }
        ParagraphBlock p;
        p.named_style_type = "NORMAL_TEXT";
        p.inlines = parse_markdown_inlines(p_text);
        blocks.push_back(Block::make_paragraph(std::move(p)));
    }
    return blocks;
}

} // namespace detail

inline std::expected<DocModel, std::string> parse_markdown(std::string_view md,
                                                           const std::optional<DocModel>& base_doc = std::nullopt) {
    auto split = slimr::parse_frontmatter(md);
    if (!split) {
        return std::unexpected("Invalid markdown: missing or malformed YAML frontmatter (" + split.error() + ")");
    }
    if (!split->frontmatter) {
        return std::unexpected("A v3 document file must start with YAML frontmatter");
    }

    const auto& fm = *split->frontmatter;
    if (!fm.is_object()) {
        return std::unexpected("Invalid frontmatter: expected YAML mapping");
    }

    // Check version
    if (fm.contains("gdocsmith")) {
        std::string v;
        if (fm["gdocsmith"].is_string()) v = fm["gdocsmith"].get<std::string>();
        else if (fm["gdocsmith"].is_int64()) v = std::to_string(fm["gdocsmith"].get<int64_t>());
        std::string_view major = v;
        if (const auto dot = major.find('.'); dot != std::string_view::npos) {
            major = major.substr(0, dot);
        }
        if (major != "3" && major != "4") {
            return std::unexpected(std::format("This file is for gdocsmith {}; this is gdocsmith {}", v, GDOCSMITH_VERSION));
        }
    }

    DocModel doc;
    if (fm.contains("doc")) {
        if (fm["doc"].is_string()) doc.doc_id = fm["doc"].get<std::string>();
    }
    if (fm.contains("revision")) {
        if (fm["revision"].is_string()) doc.revision_id = fm["revision"].get<std::string>();
    }
    if (fm.contains("title") && fm["title"].is_string()) {
        doc.title = fm["title"].get<std::string>();
    }

    // Validate against base doc if provided
    if (base_doc) {
        if (!base_doc->doc_id.empty() && !doc.doc_id.empty() && doc.doc_id != base_doc->doc_id) {
            return std::unexpected(std::format("Frontmatter doc mismatch: expected {}, got {}", base_doc->doc_id, doc.doc_id));
        }
        if (!base_doc->revision_id.empty()) {
            if (doc.revision_id.empty() || doc.revision_id != base_doc->revision_id) {
                return std::unexpected(std::format("staleBase: the cloud revision changed; expected {}, got {}",
                                                    base_doc->revision_id, doc.revision_id));
            }
        }
    }

    // Split tabs
    std::string_view body = split->body;
    std::regex tab_regex(R"(::tab\{([^}]+)\})");
    std::string body_str(body);
    std::sregex_iterator it(body_str.begin(), body_str.end(), tab_regex);
    std::sregex_iterator end;

    if (it == end) {
        // Single tab
        TabModel tm;
        tm.tab_id = base_doc && !base_doc->tabs.empty() ? base_doc->tabs[0].tab_id : "t.0";
        tm.title = doc.title;
        tm.blocks = detail::parse_markdown_blocks(body);
        doc.tabs.push_back(std::move(tm));
    } else {
        std::set<std::string> seen_tab_ids;
        std::vector<std::pair<TabModel, size_t>> parsed_tab_sections;

        for (; it != end; ++it) {
            std::string attrs = (*it)[1].str();
            TabModel tm;
            std::regex id_re(R"(id=["']?([^"'\s}]+)["']?)");
            std::regex title_re(R"(title=["']([^"']+)["']|title=([^\s}]+))");
            std::regex parent_re(R"(parent=["']?([^"'\s}]+)["']?)");
            std::smatch m;

            if (std::regex_search(attrs, m, id_re)) tm.tab_id = m[1].str();
            if (std::regex_search(attrs, m, title_re)) tm.title = m[1].matched ? m[1].str() : m[2].str();
            if (std::regex_search(attrs, m, parent_re)) tm.parent_tab_id = m[1].str();
            if (attrs.find("delete") != std::string::npos) tm.is_deleted = true;

            if (!tm.tab_id.empty()) {
                if (seen_tab_ids.contains(tm.tab_id)) {
                    return std::unexpected(std::format("tab \"{}\" appears more than once", tm.tab_id));
                }
                seen_tab_ids.insert(tm.tab_id);
            }
            size_t match_end = it->position() + it->length();
            parsed_tab_sections.push_back({std::move(tm), match_end});
        }

        for (size_t t = 0; t < parsed_tab_sections.size(); ++t) {
            size_t start_pos = parsed_tab_sections[t].second;
            size_t end_pos = (t + 1 < parsed_tab_sections.size()) ?
                parsed_tab_sections[t + 1].second - 1 - (*std::next(body_str.begin(), parsed_tab_sections[t + 1].second) == '\n') :
                body_str.size();
            std::string_view tab_body(body_str.data() + start_pos, end_pos > start_pos ? (end_pos - start_pos) : 0);
            parsed_tab_sections[t].first.blocks = detail::parse_markdown_blocks(tab_body);
            doc.tabs.push_back(std::move(parsed_tab_sections[t].first));
        }
    }

    return doc;
}

// ============================================================================
// 7. Reconciling Base -> Next into Google Docs batchUpdate Requests
// ============================================================================

struct DocPlan {
    std::string doc_id;
    std::vector<slimr::Json> content_requests;
    std::vector<slimr::Json> tab_ops;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    bool success = true;

    int content_request_count() const { return static_cast<int>(content_requests.size()); }
    int tab_ops_count() const { return static_cast<int>(tab_ops.size()); }
};

inline void reconcile_paragraph(const ParagraphBlock& base_p, const ParagraphBlock& final_p,
                                std::string_view tab_id, std::vector<slimr::Json>& requests) {
    std::string o_text = base_p.plain_text();
    std::string f_text = final_p.plain_text();
    int base_idx = (base_p.start_index >= 0) ? base_p.start_index : 1;

    if (o_text != f_text) {
        auto hunks = diff_myers_string(o_text, f_text);
        // Process hunks in reverse (back-to-front) to preserve indexing
        for (const auto& h : std::ranges::reverse_view(hunks)) {
            int pos = base_idx + h.a_start;
            int del_len = h.a_end - h.a_start;
            std::string ins_text = f_text.substr(h.b_start, h.b_end - h.b_start);
            if (del_len > 0) {
                requests.push_back(RequestBuilder::delete_content_range(pos, pos + del_len, tab_id));
            }
            if (!ins_text.empty()) {
                requests.push_back(RequestBuilder::insert_text(pos, ins_text, tab_id));
            }
        }
    }

    int final_len = static_cast<int>(f_text.size());
    int end_idx = base_idx + final_len + 1; // including trailing \n

    // Paragraph style (heading change)
    if (base_p.named_style_type != final_p.named_style_type) {
        requests.push_back(RequestBuilder::update_paragraph_style(
            base_idx, end_idx, final_p.named_style_type, "namedStyleType", tab_id));
    }

    // Bullets (list changes)
    if (!base_p.bullet && final_p.bullet) {
        std::string preset = final_p.bullet->preset.empty() ? "BULLET_DISC_CIRCLE_SQUARE" : final_p.bullet->preset;
        requests.push_back(RequestBuilder::create_paragraph_bullets(base_idx, end_idx, preset, tab_id));
    } else if (base_p.bullet && !final_p.bullet) {
        requests.push_back(RequestBuilder::delete_paragraph_bullets(base_idx, end_idx, tab_id));
    }

    // Text formatting changes (bold, italic, strike, underline, link)
    // Compare aggregate styles or inlines
    auto get_style = [](const ParagraphBlock& p) {
        TextStyle s;
        for (const auto& in : p.inlines) {
            if (in.is_text()) {
                if (in.text_run.style.bold) s.bold = true;
                if (in.text_run.style.italic) s.italic = true;
                if (in.text_run.style.strikethrough) s.strikethrough = true;
                if (in.text_run.style.underline) s.underline = true;
                if (!in.text_run.style.link_url.empty()) s.link_url = in.text_run.style.link_url;
            }
        }
        return s;
    };

    TextStyle bs = get_style(base_p);
    TextStyle fs = get_style(final_p);

    std::vector<std::string> changed_fields;
    if (bs.bold != fs.bold) changed_fields.push_back("bold");
    if (bs.italic != fs.italic) changed_fields.push_back("italic");
    if (bs.strikethrough != fs.strikethrough) changed_fields.push_back("strikethrough");
    if (bs.underline != fs.underline) changed_fields.push_back("underline");
    if (bs.link_url != fs.link_url) changed_fields.push_back("link");

    if (!changed_fields.empty()) {
        std::ostringstream fields_ss;
        for (size_t f = 0; f < changed_fields.size(); ++f) {
            if (f > 0) fields_ss << ",";
            fields_ss << changed_fields[f];
        }
        requests.push_back(RequestBuilder::update_text_style(
            base_idx, base_idx + final_len, fs, fields_ss.str(), tab_id));
    }
}

inline std::expected<DocPlan, std::string> reconcile(const DocModel& base, const DocModel& final_doc) {
    DocPlan plan;
    plan.doc_id = final_doc.doc_id.empty() ? base.doc_id : final_doc.doc_id;

    // 1. Tab operations
    std::unordered_map<std::string, const TabModel*> base_tabs;
    for (const auto& t : base.tabs) base_tabs[t.tab_id] = &t;

    std::unordered_map<std::string, const TabModel*> final_tabs;
    for (const auto& t : final_doc.tabs) final_tabs[t.tab_id] = &t;

    // Handle deleted tabs
    for (const auto& bt : base.tabs) {
        auto it = final_tabs.find(bt.tab_id);
        if (it == final_tabs.end() || it->second->is_deleted) {
            plan.tab_ops.push_back(RequestBuilder::delete_tab(bt.tab_id));
        }
    }

    // Handle added and renamed tabs
    for (size_t idx = 0; idx < final_doc.tabs.size(); ++idx) {
        const auto& ft = final_doc.tabs[idx];
        if (ft.is_deleted) continue;
        auto it = base_tabs.find(ft.tab_id);
        if (it == base_tabs.end() || ft.tab_id.empty() || ft.tab_id.starts_with("new:")) {
            plan.tab_ops.push_back(RequestBuilder::add_document_tab(
                ft.title, static_cast<int>(idx), ft.parent_tab_id.value_or("")));
        } else {
            const auto* bt = it->second;
            if (ft.title != bt->title) {
                plan.tab_ops.push_back(RequestBuilder::update_document_tab_properties(
                    ft.tab_id, ft.title, "title"));
            }
        }
    }

    // 2. Content requests per tab
    for (const auto& ft : final_doc.tabs) {
        if (ft.is_deleted) continue;
        auto it = base_tabs.find(ft.tab_id);
        if (it == base_tabs.end()) {
            // Brand new tab content: all blocks inserted
            int cur_idx = 1;
            for (const auto& b : ft.blocks) {
                if (b.kind == BlockKind::Paragraph) {
                    std::string text = b.paragraph.plain_text() + "\n";
                    plan.content_requests.push_back(RequestBuilder::insert_text(cur_idx, text, ft.tab_id));
                    if (b.paragraph.heading_level > 0) {
                        plan.content_requests.push_back(RequestBuilder::update_paragraph_style(
                            cur_idx, cur_idx + static_cast<int>(text.size()),
                            b.paragraph.named_style_type, "namedStyleType", ft.tab_id));
                    }
                    if (b.paragraph.bullet) {
                        plan.content_requests.push_back(RequestBuilder::create_paragraph_bullets(
                            cur_idx, cur_idx + static_cast<int>(text.size()),
                            b.paragraph.bullet->preset, ft.tab_id));
                    }
                    cur_idx += static_cast<int>(text.size());
                } else if (b.kind == BlockKind::Table) {
                    plan.content_requests.push_back(RequestBuilder::insert_table(
                        cur_idx, b.table.rows, b.table.columns, ft.tab_id));
                    cur_idx += 10;
                }
            }
            continue;
        }

        const auto* bt = it->second;
        // Reconcile blocks within existing tab
        // Filter paragraphs for matching
        std::vector<const ParagraphBlock*> base_paras;
        for (const auto& b : bt->blocks) {
            if (b.kind == BlockKind::Paragraph) base_paras.push_back(&b.paragraph);
        }
        std::vector<const ParagraphBlock*> final_paras;
        for (const auto& b : ft.blocks) {
            if (b.kind == BlockKind::Paragraph) final_paras.push_back(&b.paragraph);
        }

        if (base_paras.size() == final_paras.size()) {
            // 1-to-1 paragraph matching, processed in reverse order
            for (int p_i = static_cast<int>(base_paras.size()) - 1; p_i >= 0; --p_i) {
                reconcile_paragraph(*base_paras[p_i], *final_paras[p_i], ft.tab_id, plan.content_requests);
            }
        } else {
            // Block diff: Myers diff on paragraph texts
            std::vector<std::string> base_texts;
            for (const auto* p : base_paras) base_texts.push_back(p->plain_text());
            std::vector<std::string> final_texts;
            for (const auto* p : final_paras) final_texts.push_back(p->plain_text());

            auto hunks = diff_myers(base_texts, final_texts);
            // Process backwards
            for (const auto& h : std::ranges::reverse_view(hunks)) {
                int del_count = h.a_end - h.a_start;
                int ins_count = h.b_end - h.b_start;

                if (del_count > 0 && ins_count == 0) {
                    // Paragraphs deleted
                    int start = base_paras[h.a_start]->start_index;
                    int end = base_paras[h.a_end - 1]->end_index;
                    if (start >= 0 && end > start) {
                        plan.content_requests.push_back(RequestBuilder::delete_content_range(start, end, ft.tab_id));
                    }
                } else if (ins_count > 0 && del_count == 0) {
                    // Paragraphs inserted
                    int insert_pos = (h.a_start < static_cast<int>(base_paras.size())) ?
                        base_paras[h.a_start]->start_index :
                        (!base_paras.empty() ? base_paras.back()->end_index : 1);
                    for (int bi = h.b_start; bi < h.b_end; ++bi) {
                        std::string ins_text = final_paras[bi]->plain_text() + "\n";
                        plan.content_requests.push_back(RequestBuilder::insert_text(insert_pos, ins_text, ft.tab_id));
                        if (final_paras[bi]->heading_level > 0) {
                            plan.content_requests.push_back(RequestBuilder::update_paragraph_style(
                                insert_pos, insert_pos + static_cast<int>(ins_text.size()),
                                final_paras[bi]->named_style_type, "namedStyleType", ft.tab_id));
                        }
                    }
                } else {
                    // Replaced paragraphs
                    for (int i = 0; i < std::min(del_count, ins_count); ++i) {
                        reconcile_paragraph(*base_paras[h.a_start + i], *final_paras[h.b_start + i], ft.tab_id, plan.content_requests);
                    }
                }
            }
        }

        // Table insertion check
        for (const auto& fb : ft.blocks) {
            if (fb.kind == BlockKind::Table) {
                bool exists = false;
                for (const auto& bb : bt->blocks) {
                    if (bb.kind == BlockKind::Table) { exists = true; break; }
                }
                if (!exists) {
                    plan.content_requests.push_back(RequestBuilder::insert_table(
                        1, fb.table.rows, fb.table.columns, ft.tab_id));
                }
            }
        }
    }

    return plan;
}

inline std::expected<DocPlan, std::string> reconcile(const DocModel& base, std::string_view next_md) {
    auto parsed = parse_markdown(next_md, base);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    return reconcile(base, *parsed);
}

inline std::expected<DocPlan, std::string> reconcile_json(const slimr::Json& base_doc_json, std::string_view next_md) {
    DocModel base = parse_doc_json(base_doc_json);
    return reconcile(base, next_md);
}

} // namespace gdocsmith
