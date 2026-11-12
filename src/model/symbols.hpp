#pragma once

#include "model/raw_json.hpp"
#include "model/style_values.hpp"
#include "model/suggestions.hpp"
#include "model/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gdocsmith::model {

struct CharSym {
    std::string ch; // 1 Unicode code point (in UTF-8)
    std::string kind = "char";
    JsonObject style;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedTextStyleChanges;

    bool operator==(const CharSym&) const = default;
};

struct AtomSym {
    Atom atom;
    std::string kind = "atom";

    bool operator==(const AtomSym&) const = default;
};

struct Sym {
    std::string kind = "char"; // "char" | "atom"

    // CharSym fields:
    std::string ch;
    JsonObject style;
    std::optional<std::vector<std::string>> suggestedDeletionIds;
    std::optional<std::vector<std::string>> suggestedInsertionIds;
    std::optional<JsonObject> suggestedTextStyleChanges;

    // AtomSym fields:
    std::optional<Atom> atom;

    Sym() = default;

    Sym(CharSym cs)
        : kind("char"),
          ch(std::move(cs.ch)),
          style(std::move(cs.style)),
          suggestedDeletionIds(std::move(cs.suggestedDeletionIds)),
          suggestedInsertionIds(std::move(cs.suggestedInsertionIds)),
          suggestedTextStyleChanges(std::move(cs.suggestedTextStyleChanges)) {}

    Sym(Atom a)
        : kind("atom"),
          atom(std::move(a)) {}

    bool is_char() const { return kind == "char"; }
    bool is_atom() const { return kind == "atom"; }

    bool operator==(const Sym&) const = default;
};

/** A symbol's suggestion marks. */
inline SuggestionMarks symMarks(const Sym& sym) {
    if (sym.is_atom() && sym.atom.has_value()) {
        return atomSuggestion(*sym.atom);
    }
    return SuggestionMarks{
        .suggestedDeletionIds = sym.suggestedDeletionIds,
        .suggestedInsertionIds = sym.suggestedInsertionIds,
        .suggestedTextStyleChanges = sym.suggestedTextStyleChanges,
    };
}

inline bool symHidden(const Sym& sym) {
    auto m = symMarks(sym);
    return m.suggestedInsertionIds.has_value() && !m.suggestedInsertionIds->empty();
}

/** Splits a UTF-8 string into a vector of UTF-8 code point strings. */
inline std::vector<std::string> utf8CodePoints(std::string_view s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if ((c & 0x80) == 0) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > s.size()) len = s.size() - i;
        out.emplace_back(s.substr(i, len));
        i += len;
    }
    return out;
}

/** UTF-16 code units of a UTF-8 encoded code point string. */
inline int codePointUtf16Units(std::string_view cp) {
    if (cp.empty()) return 0;
    unsigned char c = static_cast<unsigned char>(cp[0]);
    if ((c & 0xF8) == 0xF0) return 2; // 4-byte UTF-8 is astral code point -> 2 UTF-16 units
    return 1;
}

/** UTF-16 code unit length of a UTF-8 string (Docs indices are UTF-16 code units). */
inline int utf16CodeUnitLength(std::string_view s) {
    int n = 0;
    for (const auto& cp : utf8CodePoints(s)) n += codePointUtf16Units(cp);
    return n;
}

/** A paragraph's inlines as symbols, in order. */
inline std::vector<Sym> paragraphSymbols(const ParagraphBlock& paragraph) {
    std::vector<Sym> out;
    for (const auto& inlineItem : paragraph.inlines) {
        if (inlineItem.is_atom()) {
            out.push_back(Sym(inlineItem.to_atom()));
            continue;
        }
        for (const auto& cp : utf8CodePoints(inlineItem.text)) {
            CharSym cs;
            cs.ch = cp;
            cs.style = inlineItem.style;
            cs.suggestedDeletionIds = inlineItem.suggestedDeletionIds;
            cs.suggestedInsertionIds = inlineItem.suggestedInsertionIds;
            cs.suggestedTextStyleChanges = inlineItem.suggestedTextStyleChanges;
            out.push_back(Sym(std::move(cs)));
        }
    }
    return out;
}

inline bool charJoinsRun(const TextRun& run, const Sym& sym) {
    return styleEqual(run.style, sym.style) &&
           suggestionIdsEqual(run.suggestedDeletionIds, sym.suggestedDeletionIds) &&
           suggestionIdsEqual(run.suggestedInsertionIds, sym.suggestedInsertionIds) &&
           styleEqual(run.suggestedTextStyleChanges.value_or(make_json_object()),
                      sym.suggestedTextStyleChanges.value_or(make_json_object()));
}

/** Builds canonical inlines (maximal equal-style runs) from symbols. */
inline std::vector<Inline> inlinesFromSymbols(const std::vector<Sym>& syms) {
    std::vector<Inline> out;
    for (const auto& sym : syms) {
        if (sym.is_atom() && sym.atom.has_value()) {
            out.push_back(Inline(*sym.atom));
            continue;
        }
        if (!out.empty() && out.back().is_text()) {
            TextRun tr = out.back().to_text_run();
            if (charJoinsRun(tr, sym)) {
                out.back().text += sym.ch;
                continue;
            }
        }
        TextRun run;
        run.kind = "text";
        run.style = sym.style;
        run.text = sym.ch;
        run.suggestedDeletionIds = sym.suggestedDeletionIds;
        run.suggestedInsertionIds = sym.suggestedInsertionIds;
        run.suggestedTextStyleChanges = sym.suggestedTextStyleChanges;
        out.push_back(Inline(std::move(run)));
    }
    return out;
}

/** Replaces a paragraph's inlines with `syms`, merging adjacent characters with equal style. */
inline void paragraphSymbolsSet(ParagraphBlock& paragraph, const std::vector<Sym>& syms) {
    paragraph.inlines = inlinesFromSymbols(syms);
}

/** UTF-16 length of a symbol sequence (the index span it occupies in the document). */
inline int symbolsUtf16Length(const std::vector<Sym>& syms) {
    int length = 0;
    for (const auto& sym : syms) {
        if (sym.is_atom() && sym.atom.has_value()) {
            length += sym.atom->length;
        } else {
            length += codePointUtf16Units(sym.ch);
        }
    }
    return length;
}

/**
 * Identity used when diffing symbol sequences: character (plus suggestion marks) or "\0" + key for atom.
 */
inline std::string symbolId(const Sym& sym) {
    if (sym.is_atom() && sym.atom.has_value()) {
        return std::string(1, '\0') + sym.atom->key;
    }
    std::string marks = marksKey(symMarks(sym));
    if (!marks.empty()) {
        return sym.ch + "\u0001" + marks;
    }
    return sym.ch;
}

/**
 * The marks text inserted at symbol offset `at` of a paragraph joins.
 */
inline std::optional<SuggestionMarks> joinMarks(
    const std::vector<Sym>& syms,
    const NewlineMark& newline,
    int at
) {
    auto marksAt = [&](int i) -> SuggestionMarks {
        if (i < static_cast<int>(syms.size())) return symMarks(syms[i]);
        return SuggestionMarks{
            .suggestedDeletionIds = newline.suggestedDeletionIds,
            .suggestedInsertionIds = newline.suggestedInsertionIds,
            .suggestedTextStyleChanges = newline.suggestedTextStyleChanges,
        };
    };
    std::optional<SuggestionMarks> previous;
    if (at > 0) previous = marksAt(at - 1);
    std::string key = previous ? marksKey(*previous) : "";
    if (!key.empty() && key == marksKey(marksAt(at))) {
        return marksCopy(*previous);
    }
    std::string first = marksKey(marksAt(0));
    if (!first.empty()) {
        bool all_same = true;
        for (size_t i = 0; i < syms.size(); ++i) {
            if (marksKey(marksAt(static_cast<int>(i))) != first) {
                all_same = false;
                break;
            }
        }
        if (all_same && marksKey(newline) == first) {
            return marksCopy(marksAt(0));
        }
    }
    return std::nullopt;
}

} // namespace gdocsmith::model
