#pragma once

#include "model/effective_style.hpp"
#include "model/errors.hpp"
#include "model/invariants.hpp"
#include "model/keys.hpp"
#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/symbols.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model {

inline constexpr std::string_view IMAGE_UPLOAD_SCHEME = "gdocsmith-upload:";

struct EditContext {
    KeyAllocator keys;
    EditStamp stamp;
    std::vector<Tombstone> tombstones;
};

struct EditTarget {
    EditContext& ctx;
    TabModel& tab;
};

struct ContainerRef {
    std::string kind = "body"; // "body" | "cell"
    std::string cellKey;

    bool is_body() const { return kind == "body"; }
};

struct SymSpec {
    std::optional<std::string> atomKey;
    std::string ch;
    std::optional<JsonObject> style;
    std::optional<AtomCreate> create;
};

struct EditParagraphSpec {
    std::optional<BulletRef> bullet;
    std::optional<JsonObject> newlineStyle;
    std::optional<JsonObject> style;
    std::vector<SymSpec> syms;
};

inline CellModel* cellFindInBlocks(std::vector<Block>& blocks, std::string_view cellKey) {
    for (auto& b : blocks) {
        if (b.is_table()) {
            for (auto& row : b.as_table().rows) {
                for (auto& cell : row.cells) {
                    if (cell.key == cellKey) return &cell;
                    auto* found = cellFindInBlocks(cell.blocks, cellKey);
                    if (found) return found;
                }
            }
        }
    }
    return nullptr;
}

inline CellModel* cellFind(TabModel& tab, std::string_view cellKey) {
    return cellFindInBlocks(tab.blocks, cellKey);
}

inline std::vector<Block>& containerOf(TabModel& tab, const ContainerRef& ref) {
    if (ref.is_body()) return tab.blocks;
    auto* cell = cellFind(tab, ref.cellKey);
    if (!cell) throw CoreError(std::string(error_codes::INTERNAL), std::format("no cell with key {}", ref.cellKey));
    return cell->blocks;
}

struct BlockLocation {
    Block* block = nullptr;
    ContainerRef container;
    int index = -1;
};

inline std::optional<BlockLocation> blockFindInBlocks(std::vector<Block>& blocks, std::string_view key, const ContainerRef& parentContainer) {
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].key == key) {
            return BlockLocation{.block = &blocks[i], .container = parentContainer, .index = static_cast<int>(i)};
        }
        if (blocks[i].is_table()) {
            for (auto& row : blocks[i].as_table().rows) {
                for (auto& cell : row.cells) {
                    auto sub = blockFindInBlocks(cell.blocks, key, ContainerRef{.kind = "cell", .cellKey = cell.key});
                    if (sub) return sub;
                }
            }
        }
    }
    return std::nullopt;
}

inline std::optional<BlockLocation> blockFind(TabModel& tab, std::string_view key) {
    return blockFindInBlocks(tab.blocks, key, ContainerRef{.kind = "body"});
}

inline std::vector<Sym> symbolsFromSpecs(
    EditContext& ctx,
    const std::vector<SymSpec>& specs,
    const JsonObject& inheritedStyle
) {
    std::vector<Sym> out;
    for (const auto& s : specs) {
        if (s.atomKey.has_value()) {
            throw CoreError(std::string(error_codes::ATOM_MOVE), "atoms cannot be moved between paragraphs");
        }
        if (s.create.has_value()) {
            Atom a;
            a.kind = "atom";
            a.key = ctx.keys.next("n");
            a.type = s.create->type;
            a.length = (a.type == "equation") ? 3 : 1;
            a.create = *s.create;
            if (s.style.has_value()) a.style = styleCanonical(*s.style);
            out.push_back(Sym(std::move(a)));
            continue;
        }
        for (const auto& cp : utf8CodePoints(s.ch)) {
            CharSym cs;
            cs.ch = cp;
            cs.style = s.style ? styleCanonical(*s.style) : inheritedStyle;
            out.push_back(Sym(std::move(cs)));
        }
    }
    return out;
}

inline void containerRenormalize(EditTarget& target, const ContainerRef& ref) {
    auto& blocks = containerOf(target.tab, ref);
    blocks = containerNormalize(blocks, target.ctx.keys, target.ctx.stamp);
}

inline std::vector<std::string> paragraphsInsert(
    EditTarget& target,
    const ContainerRef& ref,
    int index,
    const std::vector<EditParagraphSpec>& specs
) {
    auto& blocks = containerOf(target.tab, ref);
    if (index < 0 || index > static_cast<int>(blocks.size())) {
        throw CoreError(std::string(error_codes::INTERNAL), std::format("insert index {} is out of range", index));
    }
    std::vector<std::string> createdKeys;
    std::vector<Block> created;
    for (const auto& spec : specs) {
        auto p = paragraphEmptyCreate(target.ctx.keys, spec.style, target.ctx.stamp);
        if (spec.bullet.has_value()) p.bullet = spec.bullet;
        if (spec.newlineStyle.has_value()) p.newline.style = styleCanonical(*spec.newlineStyle);
        paragraphSymbolsSet(p, symbolsFromSpecs(target.ctx, spec.syms, make_json_object()));
        createdKeys.push_back(p.key);
        created.push_back(Block(std::move(p)));
    }
    blocks.insert(blocks.begin() + index, created.begin(), created.end());
    containerRenormalize(target, ref);
    return createdKeys;
}

inline void tombstonesAdd(EditTarget& target, const Block& block) {
    target.ctx.tombstones.push_back(Tombstone{
        .key = block.key,
        .kind = "block",
        .stamp = target.ctx.stamp,
        .tabId = target.tab.tabId,
    });
    if (block.is_paragraph()) {
        for (const auto& inlineItem : block.as_paragraph().inlines) {
            if (inlineItem.is_atom()) {
                target.ctx.tombstones.push_back(Tombstone{
                    .key = inlineItem.key,
                    .kind = "atom",
                    .stamp = target.ctx.stamp,
                    .tabId = target.tab.tabId,
                });
            }
        }
    }
}

inline void blocksDelete(EditTarget& target, const std::vector<std::string>& keys) {
    std::vector<ContainerRef> touched;
    for (const auto& key : keys) {
        auto found = blockFind(target.tab, key);
        if (!found) throw CoreError(std::string(error_codes::INTERNAL), std::format("no block with key {}", key));
        auto& blocks = containerOf(target.tab, found->container);
        auto it = std::find_if(blocks.begin(), blocks.end(), [&](const Block& b) { return b.key == key; });
        if (it != blocks.end()) {
            tombstonesAdd(target, *it);
            blocks.erase(it);
        }
        touched.push_back(found->container);
    }
    for (const auto& ref : touched) {
        containerRenormalize(target, ref);
    }
}

inline void paragraphSplice(
    EditTarget& target,
    std::string_view key,
    int at,
    int deleteCount,
    const std::vector<SymSpec>& insert
) {
    auto found = blockFind(target.tab, key);
    if (!found || !found->block->is_paragraph()) {
        throw CoreError(std::string(error_codes::INTERNAL), std::format("no paragraph with key {}", key));
    }
    auto& paragraph = found->block->as_paragraph();
    auto syms = paragraphSymbols(paragraph);
    if (at < 0 || deleteCount < 0 || at + deleteCount > static_cast<int>(syms.size())) {
        throw CoreError(std::string(error_codes::INTERNAL), std::format("splice [{}, {}) is outside the paragraph", at, at + deleteCount));
    }

    JsonObject inherited = paragraph.newline.style;
    if (at > 0 && at - 1 < static_cast<int>(syms.size())) {
        inherited = syms[at - 1].style;
    } else if (at + deleteCount < static_cast<int>(syms.size())) {
        inherited = syms[at + deleteCount].style;
    }

    auto inserted = symbolsFromSpecs(target.ctx, insert, inherited);
    for (int i = 0; i < deleteCount; ++i) {
        const auto& s = syms[at + i];
        if (s.is_atom() && s.atom.has_value()) {
            target.ctx.tombstones.push_back(Tombstone{
                .key = s.atom->key,
                .kind = "atom",
                .stamp = target.ctx.stamp,
                .tabId = target.tab.tabId,
            });
        }
    }
    syms.erase(syms.begin() + at, syms.begin() + at + deleteCount);
    syms.insert(syms.begin() + at, inserted.begin(), inserted.end());
    paragraphSymbolsSet(paragraph, syms);
    paragraph.stamp = target.ctx.stamp;
}

} // namespace gdocsmith::model
