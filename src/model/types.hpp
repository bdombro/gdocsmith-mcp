#pragma once

#include "model/raw_json.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gdocsmith::model {

using BulletPreset = std::string;

struct Range {
    int end = 0;
    int start = 0;

    bool operator==(const Range&) const = default;
};

struct EditStamp {
    bool force = false;
    int tabIndex = 0;

    bool operator==(const EditStamp&) const = default;
};

struct Tombstone {
    std::string key;
    std::string kind; // "atom" | "block"
    EditStamp stamp;
    std::string tabId;

    bool operator==(const Tombstone&) const = default;
};

struct BulletRef {
    std::string listId;
    int nestingLevel = 0;
    std::optional<JsonObject> textStyle;

    bool operator==(const BulletRef&) const = default;
};

struct TextRun {
    std::string kind = "text";
    std::string text;
    JsonObject style;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedTextStyleChanges;

    bool operator==(const TextRun&) const = default;
};

using AtomType = std::string;

namespace atom_types {
inline constexpr std::string_view AUTO_TEXT = "autoText";
inline constexpr std::string_view COLUMN_BREAK = "columnBreak";
inline constexpr std::string_view DATE = "date";
inline constexpr std::string_view DROPDOWN = "dropdown";
inline constexpr std::string_view EQUATION = "equation";
inline constexpr std::string_view FOOTNOTE_REF = "footnoteRef";
inline constexpr std::string_view HORIZONTAL_RULE = "horizontalRule";
inline constexpr std::string_view IMAGE = "image";
inline constexpr std::string_view PAGE_BREAK = "pageBreak";
inline constexpr std::string_view PERSON = "person";
inline constexpr std::string_view RICH_LINK = "richLink";
} // namespace atom_types

struct AtomCreate {
    std::string type;
    std::optional<std::string> dateFormat;
    std::optional<std::string> locale;
    std::optional<std::string> timeFormat;
    std::optional<std::string> timeZoneId;
    std::optional<std::string> timestamp;
    std::optional<std::string> definitionId;
    std::optional<std::string> optionId;
    std::optional<std::string> label;
    std::optional<std::string> alt;
    std::optional<double> heightPt;
    std::optional<std::string> uri;
    std::optional<double> widthPt;
    std::optional<std::string> email;

    bool operator==(const AtomCreate&) const = default;
};

struct Atom {
    std::string kind = "atom";
    std::string key;
    std::string type; // AtomType
    int length = 1;
    std::optional<AtomCreate> create;
    std::optional<JsonObject> raw;
    std::optional<std::string> replaceUri;
    std::optional<JsonObject> style;

    bool operator==(const Atom&) const = default;
};

struct Inline {
    std::string kind = "text"; // "text" | "atom"

    // TextRun fields:
    std::string text;
    JsonObject style;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedTextStyleChanges;

    // Atom fields (when kind == "atom"):
    std::string key;
    std::string type;
    int length = 1;
    std::optional<AtomCreate> create;
    std::optional<JsonObject> raw;
    std::optional<std::string> replaceUri;

    Inline() = default;

    Inline(TextRun tr)
        : kind("text"),
          text(std::move(tr.text)),
          style(std::move(tr.style)),
          suggestedDeletionIds(std::move(tr.suggestedDeletionIds)),
          suggestedInsertionIds(std::move(tr.suggestedInsertionIds)),
          suggestedTextStyleChanges(std::move(tr.suggestedTextStyleChanges)) {}

    Inline(Atom a)
        : kind("atom"),
          style(a.style ? std::move(*a.style) : make_json_object()),
          key(std::move(a.key)),
          type(std::move(a.type)),
          length(a.length),
          create(std::move(a.create)),
          raw(std::move(a.raw)),
          replaceUri(std::move(a.replaceUri)) {}

    static Inline from_text(std::string txt, JsonObject st = make_json_object()) {
        Inline in;
        in.kind = "text";
        in.text = std::move(txt);
        in.style = std::move(st);
        return in;
    }

    static Inline from_atom(Atom a) {
        return Inline(std::move(a));
    }

    bool is_text() const { return kind == "text"; }
    bool is_atom() const { return kind == "atom"; }

    Atom to_atom() const {
        return Atom{
            .kind = "atom",
            .key = key,
            .type = type,
            .length = length,
            .create = create,
            .raw = raw,
            .replaceUri = replaceUri,
            .style = style,
        };
    }

    TextRun to_text_run() const {
        return TextRun{
            .kind = "text",
            .text = text,
            .style = style,
            .suggestedDeletionIds = suggestedDeletionIds,
            .suggestedInsertionIds = suggestedInsertionIds,
            .suggestedTextStyleChanges = suggestedTextStyleChanges,
        };
    }

    bool operator==(const Inline&) const = default;
};

struct NewlineMark {
    JsonObject style;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedTextStyleChanges;

    bool operator==(const NewlineMark&) const = default;
};

struct ParagraphBlock {
    std::string kind = "paragraph";
    std::string key;
    std::vector<Inline> inlines;
    NewlineMark newline;
    JsonObject style;
    std::optional<BulletRef> bullet;
    std::optional<std::string> headingId;
    std::optional<Range> origin;
    std::optional<std::vector<std::string>> positionedObjectIds;
    bool is_protected = false;
    std::optional<EditStamp> stamp;
    std::optional<JsonObject> suggestionsRaw;

    bool operator==(const ParagraphBlock&) const = default;
};

struct ColumnModel {
    std::string key;
    JsonObject props;

    bool operator==(const ColumnModel&) const = default;
};

struct Block;

struct CellModel {
    std::string key;
    std::vector<Block> blocks;
    JsonObject style;
    std::optional<Range> origin;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedStyleChanges;

    bool operator==(const CellModel& o) const;
};

struct RowModel {
    std::string key;
    std::vector<CellModel> cells;
    JsonObject style;
    std::optional<Range> origin;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedStyleChanges;

    bool operator==(const RowModel& o) const;
};

struct TableBlock {
    std::string kind = "table";
    std::string key;
    std::vector<ColumnModel> columns;
    std::vector<RowModel> rows;
    std::optional<Range> origin;
    bool is_protected = false;
    std::optional<EditStamp> stamp;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;

    bool operator==(const TableBlock& o) const;
};

struct TocBlock {
    std::string kind = "toc";
    std::string key;
    int length = 0;
    Range origin;
    JsonObject raw;

    bool operator==(const TocBlock&) const = default;
};

struct SectionBreakBlock {
    struct Create {
        std::string sectionType = "CONTINUOUS"; // "CONTINUOUS" | "NEXT_PAGE"
        bool operator==(const Create&) const = default;
    };
    std::string kind = "sectionBreak";
    std::string key;
    JsonObject sectionStyle;
    std::optional<Create> create;
    std::optional<Range> origin;

    bool operator==(const SectionBreakBlock&) const = default;
};

struct Block {
    std::string kind = "paragraph";
    std::string key;
    std::optional<Range> origin;
    std::optional<EditStamp> stamp;

    std::shared_ptr<ParagraphBlock> paragraph;
    std::shared_ptr<TableBlock> table;
    std::shared_ptr<SectionBreakBlock> sectionBreak;
    std::shared_ptr<TocBlock> toc;

    Block() : kind("paragraph"), paragraph(std::make_shared<ParagraphBlock>()) {}

    Block(ParagraphBlock p)
        : kind("paragraph"),
          key(p.key),
          origin(p.origin),
          stamp(p.stamp),
          paragraph(std::make_shared<ParagraphBlock>(std::move(p))) {}

    Block(TableBlock t)
        : kind("table"),
          key(t.key),
          origin(t.origin),
          stamp(t.stamp),
          table(std::make_shared<TableBlock>(std::move(t))) {}

    Block(SectionBreakBlock s)
        : kind("sectionBreak"),
          key(s.key),
          origin(s.origin),
          sectionBreak(std::make_shared<SectionBreakBlock>(std::move(s))) {}

    Block(TocBlock tc)
        : kind("toc"),
          key(tc.key),
          origin(tc.origin),
          toc(std::make_shared<TocBlock>(std::move(tc))) {}

    Block(const Block& other)
        : kind(other.kind),
          key(other.key),
          origin(other.origin),
          stamp(other.stamp),
          paragraph(other.paragraph ? std::make_shared<ParagraphBlock>(*other.paragraph) : nullptr),
          table(other.table ? std::make_shared<TableBlock>(*other.table) : nullptr),
          sectionBreak(other.sectionBreak ? std::make_shared<SectionBreakBlock>(*other.sectionBreak) : nullptr),
          toc(other.toc ? std::make_shared<TocBlock>(*other.toc) : nullptr) {}

    Block(Block&& other) noexcept = default;

    Block& operator=(const Block& other) {
        if (this == &other) return *this;
        kind = other.kind;
        key = other.key;
        origin = other.origin;
        stamp = other.stamp;
        paragraph = other.paragraph ? std::make_shared<ParagraphBlock>(*other.paragraph) : nullptr;
        table = other.table ? std::make_shared<TableBlock>(*other.table) : nullptr;
        sectionBreak = other.sectionBreak ? std::make_shared<SectionBreakBlock>(*other.sectionBreak) : nullptr;
        toc = other.toc ? std::make_shared<TocBlock>(*other.toc) : nullptr;
        return *this;
    }

    Block& operator=(Block&& other) noexcept = default;

    bool is_paragraph() const { return kind == "paragraph" && paragraph != nullptr; }
    bool is_table() const { return kind == "table" && table != nullptr; }
    bool is_section_break() const { return kind == "sectionBreak" && sectionBreak != nullptr; }
    bool is_toc() const { return kind == "toc" && toc != nullptr; }

    ParagraphBlock& as_paragraph() { return *paragraph; }
    const ParagraphBlock& as_paragraph() const { return *paragraph; }

    TableBlock& as_table() { return *table; }
    const TableBlock& as_table() const { return *table; }

    SectionBreakBlock& as_section_break() { return *sectionBreak; }
    const SectionBreakBlock& as_section_break() const { return *sectionBreak; }

    TocBlock& as_toc() { return *toc; }
    const TocBlock& as_toc() const { return *toc; }

    bool operator==(const Block& o) const {
        if (kind != o.kind || key != o.key || origin != o.origin || stamp != o.stamp) return false;
        if (is_paragraph()) return o.is_paragraph() && *paragraph == *o.paragraph;
        if (is_table()) return o.is_table() && *table == *o.table;
        if (is_section_break()) return o.is_section_break() && *sectionBreak == *o.sectionBreak;
        if (is_toc()) return o.is_toc() && *toc == *o.toc;
        return true;
    }
};

inline bool CellModel::operator==(const CellModel& o) const {
    return key == o.key && style == o.style && origin == o.origin &&
           suggestedDeletionIds == o.suggestedDeletionIds &&
           suggestedInsertionIds == o.suggestedInsertionIds &&
           suggestedStyleChanges == o.suggestedStyleChanges &&
           blocks == o.blocks;
}

inline bool RowModel::operator==(const RowModel& o) const {
    return key == o.key && style == o.style && origin == o.origin &&
           suggestedDeletionIds == o.suggestedDeletionIds &&
           suggestedInsertionIds == o.suggestedInsertionIds &&
           suggestedStyleChanges == o.suggestedStyleChanges &&
           cells == o.cells;
}

inline bool TableBlock::operator==(const TableBlock& o) const {
    return kind == o.kind && key == o.key && origin == o.origin &&
           is_protected == o.is_protected && stamp == o.stamp &&
           suggestedDeletionIds == o.suggestedDeletionIds &&
           suggestedInsertionIds == o.suggestedInsertionIds &&
           columns == o.columns && rows == o.rows;
}

struct ListDef {
    bool isNew = false;
    std::vector<JsonObject> nestingLevels;
    std::optional<BulletPreset> preset;

    bool operator==(const ListDef&) const = default;
};

using SegmentKind = std::string;
using SegmentType = std::string;

struct TabModel;

struct SegmentModel {
    std::string id;
    SegmentKind kind; // "footer" | "footnote" | "header"
    SegmentType type = "default"; // "default" | "even" | "first"
    std::optional<int> section;
    std::shared_ptr<TabModel> original;
    std::shared_ptr<TabModel> tab;

    bool operator==(const SegmentModel& o) const;
};

struct NamedRangeDef {
    struct Edge {
        std::string key;
        int offset = 0;
        bool operator==(const Edge&) const = default;
    };
    struct EdgePair {
        Edge end;
        Edge start;
        bool operator==(const EdgePair&) const = default;
    };
    struct RangePart : Range {
        std::optional<std::string> segmentId;
        bool operator==(const RangePart&) const = default;
    };

    std::string name;
    std::string namedRangeId;
    std::vector<RangePart> ranges;
    std::optional<EdgePair> create;
    std::optional<EdgePair> expect;

    bool operator==(const NamedRangeDef&) const = default;
};

struct TabModel {
    std::string tabId;
    std::string title;
    bool isNew = false;
    std::optional<std::string> parentTabId;
    std::optional<std::string> iconEmoji;

    std::vector<Block> blocks;
    std::vector<SegmentModel> segments;

    JsonObject documentStyle;
    JsonObject leadingSectionStyle;
    JsonObject namedStyles;
    JsonObject headersFooters;

    std::unordered_map<std::string, JsonObject> dropdownDefinitions;
    std::unordered_map<std::string, JsonObject> footnotes;
    std::unordered_map<std::string, JsonObject> inlineObjects;
    std::unordered_map<std::string, ListDef> lists;
    std::vector<NamedRangeDef> namedRanges;
    std::unordered_map<std::string, JsonObject> positionedObjects;

    std::optional<std::unordered_map<std::string, std::string>> objectIds;
    std::optional<JsonObject> suggestedDocumentStyleChanges;
    std::optional<JsonObject> suggestedNamedStylesChanges;

    bool operator==(const TabModel& o) const;
};

inline bool SegmentModel::operator==(const SegmentModel& o) const {
    if (id != o.id || kind != o.kind || type != o.type || section != o.section) return false;
    if (tab && o.tab) return *tab == *o.tab;
    return tab == o.tab;
}

inline bool TabModel::operator==(const TabModel& o) const {
    return tabId == o.tabId && title == o.title && isNew == o.isNew &&
           parentTabId == o.parentTabId && iconEmoji == o.iconEmoji &&
           blocks == o.blocks && segments == o.segments &&
           documentStyle == o.documentStyle && leadingSectionStyle == o.leadingSectionStyle &&
           namedStyles == o.namedStyles && lists == o.lists &&
           namedRanges == o.namedRanges &&
           suggestedDocumentStyleChanges == o.suggestedDocumentStyleChanges &&
           suggestedNamedStylesChanges == o.suggestedNamedStylesChanges;
}

struct DocModel {
    std::string docId;
    std::string title;
    bool isNew = false;
    std::optional<std::string> revisionId;
    std::optional<std::string> suggestionsViewMode;
    std::vector<TabModel> tabs;

    bool operator==(const DocModel&) const = default;
};

struct IdentityTransfer {
    std::optional<std::string> fromKey;
    std::optional<std::string> headingId;
    std::string tabId;
    std::optional<std::string> toKey;

    bool operator==(const IdentityTransfer&) const = default;
};

} // namespace gdocsmith::model
