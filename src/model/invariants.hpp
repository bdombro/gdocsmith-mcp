#pragma once

#include "model/keys.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model {

/** True for a paragraph holding only a page break. */
inline bool pageBreakParagraphIs(const Block& block) {
    if (!block.is_paragraph()) return false;
    const auto& p = block.as_paragraph();
    return p.inlines.size() == 1 && p.inlines[0].is_atom() && p.inlines[0].type == "pageBreak";
}

/** Creates a brand-new empty paragraph (no bullet, no inline content). */
inline ParagraphBlock paragraphEmptyCreate(
    KeyAllocator& keys,
    std::optional<JsonObject> style = std::nullopt,
    std::optional<EditStamp> stamp = std::nullopt
) {
    ParagraphBlock p;
    p.kind = "paragraph";
    p.key = keys.next("n");
    p.newline = NewlineMark{.style = make_json_object()};
    p.style = style.value_or(make_json_object());
    p.is_protected = false;
    p.stamp = stamp;
    return p;
}

/** True for an "invisible" paragraph: empty, NORMAL_TEXT (or unset), and not a list item. */
inline bool blockInvisibleIs(const Block& block) {
    if (!block.is_paragraph()) return false;
    const auto& p = block.as_paragraph();
    if (p.bullet.has_value()) return false;
    if (!p.inlines.empty()) return false;
    if (p.style.contains("namedStyleType") && p.style["namedStyleType"].holds<std::string>()) {
        std::string nst = p.style["namedStyleType"].get<std::string>();
        return nst == "NORMAL_TEXT";
    }
    return true;
}

struct ContainerValidateOptions {
    bool inCell = false;
};

inline std::vector<std::string> containerValidate(
    const std::vector<Block>& blocks,
    ContainerValidateOptions opts = {}
) {
    std::vector<std::string> problems;
    if (blocks.empty() || !blocks.back().is_paragraph()) {
        problems.push_back(opts.inCell ? "cell does not end with a paragraph (V1)" : "container does not end with a paragraph (V1)");
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        if (block.is_table() || block.is_toc() || block.is_section_break()) {
            if (i == 0 || !blocks[i - 1].is_paragraph()) {
                problems.push_back(std::format("block {} ({}) is not directly preceded by a paragraph (V2)", i, block.kind));
            }
        }
    }
    return problems;
}

inline std::vector<Block> containerNormalize(
    const std::vector<Block>& blocks,
    KeyAllocator& keys,
    EditStamp stamp
) {
    std::vector<Block> out;
    for (const auto& block : blocks) {
        bool structure = block.is_table() || block.is_toc() || block.is_section_break();
        bool needsNewBefore = structure && !block.is_toc() && block.key.starts_with("n");

        bool prevOk = false;
        if (!out.empty() && out.back().is_paragraph()) {
            const auto& prev = out.back();
            if (needsNewBefore) {
                prevOk = prev.key.starts_with("n") && !pageBreakParagraphIs(prev);
            } else {
                prevOk = !(structure && pageBreakParagraphIs(prev) && prev.key.starts_with("n"));
            }
        }
        if (structure && !prevOk) {
            out.push_back(Block(paragraphEmptyCreate(keys, std::nullopt, stamp)));
        }
        out.push_back(block);
    }
    if (out.empty() || !out.back().is_paragraph() || (pageBreakParagraphIs(out.back()) && out.back().key.starts_with("n"))) {
        out.push_back(Block(paragraphEmptyCreate(keys, std::nullopt, stamp)));
    }
    return out;
}

} // namespace gdocsmith::model
