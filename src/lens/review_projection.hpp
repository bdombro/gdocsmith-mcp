#pragma once

#include "lens/tokens.hpp"
#include "model/suggestions.hpp"
#include "model/errors.hpp"

#include <utility>
#include <vector>
#include <algorithm>

namespace gdocsmith::lens {

inline bool paragraphContentHidden(const model::ParagraphBlock& paragraph) {
    if (paragraph.bullet || paragraph.inlines.empty()) return false;
    return std::all_of(paragraph.inlines.begin(), paragraph.inlines.end(), [](const auto& item) {
        const auto marks = item.is_text() ? model::SuggestionMarks{
            .suggestedInsertionIds = item.suggestedInsertionIds} : model::atomSuggestion(item.to_atom());
        return marks.suggestedInsertionIds && !marks.suggestedInsertionIds->empty();
    });
}

/** Suggested insertions are hidden in the editable view, including paragraph breaks. */
inline void reviewProjectionNormalize(Projection& projection, const std::vector<model::Block>& original) {
    if (projection.blocks.size() != original.size()) {
        throw model::CoreError("internal", "Review projection lost its source block correspondence.");
    }
    std::vector<ProjBlock> visible;
    bool joinNext = false;
    for (size_t i = 0; i < original.size(); ++i) {
        const auto& block = original[i];
        auto projected = std::move(projection.blocks[i]);
        if (block.is_paragraph()) {
            const auto& paragraph=block.as_paragraph();
            const bool hiddenContent=paragraphContentHidden(paragraph);
            if (model::paragraphInserted(paragraph) || hiddenContent) {
                if (!visible.empty() &&
                    visible.back().kind!="token" && visible.back().kind!="table") {
                    std::vector<std::string> ids;
                    for (const auto& item : paragraph.inlines) {
                        const auto marks=item.is_text() ? model::SuggestionMarks{
                            .suggestedInsertionIds=item.suggestedInsertionIds} : model::atomSuggestion(item.to_atom());
                        if (marks.suggestedInsertionIds)
                            for (const auto& id : *marks.suggestedInsertionIds)
                                if (std::find(ids.begin(),ids.end(),id)==ids.end()) ids.push_back(id);
                    }
                    if (paragraph.newline.suggestedInsertionIds)
                        for (const auto& id : *paragraph.newline.suggestedInsertionIds)
                            if (std::find(ids.begin(),ids.end(),id)==ids.end()) ids.push_back(id);
                    for (const auto& id : ids) {
                        ProjSpan point;
                        point.kind="token";
                        point.token=TokenRef{};
                        point.token->kind="suggestion";
                        point.token->attrs.values.emplace_back("id",id);
                        visible.back().spans.push_back(std::move(point));
                    }
                }
                continue;
            }
        }
        if (joinNext && block.is_paragraph() && !visible.empty() && visible.back().kind != "token" &&
            visible.back().kind != "table") {
            auto& spans = visible.back().spans;
            spans.insert(spans.end(), projected.spans.begin(), projected.spans.end());
        } else visible.push_back(std::move(projected));
        joinNext = block.is_paragraph() && model::newlineInserted(block.as_paragraph().newline);
    }
    projection.blocks = std::move(visible);
}

} // namespace gdocsmith::lens
