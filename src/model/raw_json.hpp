#pragma once

#include "json.hpp"
#include "reflect.hpp"

namespace glz {
template <num_mode Mode, template <class> class MapType>
inline bool operator==(const generic_json<Mode, MapType>& a, const generic_json<Mode, MapType>& b) {
    return a.data == b.data;
}
}

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace gdocsmith::model {

using JsonObject = slimr::Json;

inline JsonObject make_json_object() {
    return slimr::json_obj();
}

/** Docs API rgbColor 0-1 channels. */
struct RawColor {
    std::optional<double> blue;
    std::optional<double> green;
    std::optional<double> red;
};

/** Docs API OptionalColor wrapper. */
struct RawOptionalColor {
    struct Inner {
        std::optional<RawColor> rgbColor;
    };
    std::optional<Inner> color;
};

/** Docs API Dimension (PT unless stated otherwise). */
struct RawDimension {
    std::optional<double> magnitude;
    std::optional<std::string> unit;
};

/** One named range instance (Docs API NamedRange). */
struct RawNamedRange {
    struct RangePart {
        std::optional<int> endIndex;
        std::optional<int> startIndex;
        std::optional<std::string> tabId;
    };
    std::optional<std::string> name;
    std::optional<std::string> namedRangeId;
    std::optional<std::vector<RangePart>> ranges;
};

/** One entry in a document's namedRanges map. */
struct RawNamedRanges {
    std::optional<std::string> name;
    std::optional<std::vector<RawNamedRange>> namedRanges;
};

using RawListNestingLevel = JsonObject;

struct RawList {
    struct Props {
        std::optional<std::vector<RawListNestingLevel>> nestingLevels;
    };
    std::optional<Props> listProperties;
};

struct RawSuggested {
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
};

struct RawTextRun : RawSuggested {
    std::optional<std::string> content;
    std::optional<JsonObject> suggestedTextStyleChanges;
    std::optional<JsonObject> textStyle;
};

struct RawPerson : RawSuggested {
    std::optional<std::string> personId;
    std::optional<JsonObject> personProperties;
    std::optional<JsonObject> textStyle;
};

struct RawRichLink : RawSuggested {
    std::optional<std::string> richLinkId;
    std::optional<JsonObject> richLinkProperties;
    std::optional<JsonObject> textStyle;
};

struct RawDropdown : RawSuggested {
    std::optional<std::string> dropdownId;
    std::optional<JsonObject> dropdownProperties;
    std::optional<JsonObject> suggestedDropdownPropertiesChanges;
    std::optional<JsonObject> suggestedTextStyleChanges;
    std::optional<JsonObject> textStyle;
};

struct RawDate : RawSuggested {
    std::optional<JsonObject> dateElementProperties;
    std::optional<std::string> dateId;
    std::optional<JsonObject> textStyle;
};

struct RawInlineObjectElement : RawSuggested {
    std::optional<std::string> inlineObjectId;
    std::optional<JsonObject> textStyle;
};

struct RawFootnoteReference : RawSuggested {
    std::optional<std::string> footnoteId;
    std::optional<std::string> footnoteNumber;
    std::optional<JsonObject> textStyle;
};

struct RawHorizontalRule : RawSuggested {
    std::optional<JsonObject> textStyle;
};

struct RawPageBreak : RawSuggested {
    std::optional<JsonObject> textStyle;
};

struct RawColumnBreak : RawSuggested {
    std::optional<JsonObject> textStyle;
};

using RawEquation = JsonObject;

struct RawAutoText : RawSuggested {
    std::optional<JsonObject> textStyle;
    std::optional<std::string> type;
};

struct RawParagraphElement {
    std::optional<RawAutoText> autoText;
    std::optional<RawColumnBreak> columnBreak;
    std::optional<RawDate> dateElement;
    std::optional<RawDropdown> dropdown;
    std::optional<int> endIndex;
    std::optional<RawEquation> equation;
    std::optional<RawFootnoteReference> footnoteReference;
    std::optional<RawHorizontalRule> horizontalRule;
    std::optional<RawInlineObjectElement> inlineObjectElement;
    std::optional<RawPageBreak> pageBreak;
    std::optional<RawPerson> person;
    std::optional<RawRichLink> richLink;
    std::optional<int> startIndex;
    std::optional<RawTextRun> textRun;
};

struct RawBullet {
    std::optional<std::string> listId;
    std::optional<int> nestingLevel;
    std::optional<JsonObject> textStyle;
};

struct RawParagraph {
    std::optional<RawBullet> bullet;
    std::optional<std::vector<RawParagraphElement>> elements;
    std::optional<JsonObject> paragraphStyle;
    std::optional<std::vector<std::string>> positionedObjectIds;
    std::optional<JsonObject> suggestedBulletChanges;
    std::optional<JsonObject> suggestedParagraphStyleChanges;
    std::optional<JsonObject> suggestedPositionedObjectIds;
};

struct RawStructuralElement;

struct RawTableCell : RawSuggested {
    std::optional<std::vector<RawStructuralElement>> content;
    std::optional<int> endIndex;
    std::optional<int> startIndex;
    std::optional<JsonObject> suggestedTableCellStyleChanges;
    std::optional<JsonObject> tableCellStyle;
};

struct RawTableRow : RawSuggested {
    std::optional<int> endIndex;
    std::optional<int> startIndex;
    std::optional<JsonObject> suggestedTableRowStyleChanges;
    std::optional<std::vector<RawTableCell>> tableCells;
    std::optional<JsonObject> tableRowStyle;
};

struct RawTable : RawSuggested {
    std::optional<int> columns;
    std::optional<int> rows;
    std::optional<std::vector<RawTableRow>> tableRows;
    std::optional<JsonObject> tableStyle;
};

struct RawTableOfContents {
    std::optional<std::vector<RawStructuralElement>> content;
};

struct RawSectionBreak {
    std::optional<JsonObject> sectionStyle;
};

struct RawStructuralElement {
    std::optional<int> endIndex;
    std::optional<RawParagraph> paragraph;
    std::optional<RawSectionBreak> sectionBreak;
    std::optional<int> startIndex;
    std::optional<RawTable> table;
    std::optional<RawTableOfContents> tableOfContents;
};

struct RawDocumentTab {
    struct Body {
        std::optional<std::vector<RawStructuralElement>> content;
    };
    std::optional<Body> body;
    std::optional<JsonObject> documentStyle;
    std::optional<JsonObject> dropdownDefinitions;
    std::optional<JsonObject> footers;
    std::optional<JsonObject> footnotes;
    std::optional<JsonObject> headers;
    std::optional<JsonObject> inlineObjects;
    std::optional<JsonObject> lists;
    std::optional<JsonObject> namedRanges;
    std::optional<JsonObject> namedStyles;
    std::optional<JsonObject> positionedObjects;
    std::optional<JsonObject> suggestedDocumentStyleChanges;
    std::optional<JsonObject> suggestedNamedStylesChanges;
};

struct RawTab {
    std::optional<std::vector<RawTab>> childTabs;
    std::optional<RawDocumentTab> documentTab;
    struct Props {
        std::optional<int> index;
        std::optional<std::string> parentTabId;
        std::optional<std::string> tabId;
        std::optional<std::string> title;
    };
    std::optional<Props> tabProperties;
};

struct RawDocument {
    std::optional<RawDocumentTab::Body> body;
    std::optional<std::string> documentId;
    std::optional<JsonObject> documentStyle;
    std::optional<JsonObject> dropdownDefinitions;
    std::optional<JsonObject> footers;
    std::optional<JsonObject> footnotes;
    std::optional<JsonObject> headers;
    std::optional<JsonObject> inlineObjects;
    std::optional<JsonObject> lists;
    std::optional<JsonObject> namedRanges;
    std::optional<JsonObject> namedStyles;
    std::optional<JsonObject> positionedObjects;
    std::optional<std::string> revisionId;
    std::optional<std::vector<RawTab>> tabs;
    std::optional<std::string> title;
};

/**
 * Validates a fetched Google Doc JSON has `tabs` or legacy `body`.
 */
inline const slimr::Json& rawDocumentOf(const slimr::Json& json) {
    if (!json.contains("tabs") && !json.contains("body")) {
        throw std::runtime_error("rawDocumentOf: expected a document with `tabs` or a legacy `body`, got neither");
    }
    return json;
}

} // namespace gdocsmith::model
