#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <map>
#include "directives.hpp"
#include "tables.hpp"

namespace gdocsmith::lens {

struct Marks {
    bool bold{false};
    bool italic{false};
    bool underline{false};
    bool strikethrough{false};
    bool smallCaps{false};
    bool code{false};
    std::optional<std::string> link;

    bool operator==(const Marks& other) const = default;
    bool empty() const {
        return !bold && !italic && !underline && !strikethrough && !smallCaps && !code && !link;
    }
};

struct TokenRef {
    std::string kind;
    DirectiveAttrs attrs;
    std::optional<std::string> label;
    int ordinal{0};

    bool operator==(const TokenRef& other) const = default;
};

struct ProjSpan {
    std::string kind{"text"}; // "text" or "token"
    std::string text;
    Marks marks;
    std::optional<std::string> directive; // Style class
    std::optional<TokenRef> token;

    bool operator==(const ProjSpan& other) const = default;
};

struct ProjBlock;

struct ProjCell {
    std::vector<ProjSpan> spans;
    std::optional<std::string> cellStyle;
    std::optional<std::string> paragraphStyle;
    std::vector<ProjBlock> blocks; // For referenced multi-block cells

    bool operator==(const ProjCell& other) const = default;
};

struct ProjTable {
    std::vector<std::vector<std::vector<ProjSpan>>> rows;
    std::vector<std::string> alignments; // "LEFT", "CENTER", "RIGHT"
    std::vector<std::vector<std::optional<std::string>>> cellStyles;
    std::vector<std::vector<std::optional<std::string>>> paragraphStyles;
    std::vector<TableMerge> merges;
    std::vector<std::vector<std::vector<ProjBlock>>> cells; // Referenced cell blocks

    bool operator==(const ProjTable& other) const = default;
};

struct ProjBlock {
    std::string kind{"paragraph"}; // "paragraph", "heading", "listItem", "codeLine", "table", "token"
    std::vector<ProjSpan> spans;
    std::optional<int> headingLevel;
    std::optional<std::string> paragraphStyle;

    // List properties
    std::optional<std::string> listKind; // "bullet" or "numbered"
    int listDepth{0};
    std::optional<std::string> listPreset;

    // Table properties
    std::optional<ProjTable> table;

    // Token block properties
    std::optional<TokenRef> token;

    bool operator==(const ProjBlock& other) const = default;
};

struct Projection {
    std::vector<ProjBlock> blocks;
    std::map<std::string, DirectiveAttrs> styles;
    std::map<std::string, ParagraphDeclaration> paragraphStyles;
    std::map<std::string, CellDeclaration> cellStyles;

    bool operator==(const Projection& other) const = default;
};

} // namespace gdocsmith::lens
