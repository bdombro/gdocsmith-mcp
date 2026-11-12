#pragma once

#include "diff/myers.hpp"
#include "model/from_json.hpp"
#include "model/keys.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"
#include "model/walk.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

struct DriveCommentQuoted {
    std::string value;
};

struct DriveCommentAuthor {
    std::optional<std::string> displayName;
    std::optional<std::string> emailAddress;
    std::optional<bool> me;
};

struct DriveCommentReply {
    std::string id;
    std::optional<std::string> content;
    std::optional<bool> deleted;
    std::optional<std::string> action;
    std::optional<DriveCommentAuthor> author;
    std::optional<std::string> createdTime;
    std::optional<std::string> modifiedTime;
};

struct DriveComment {
    std::string id;
    std::optional<std::string> content;
    std::optional<DriveCommentAuthor> author;
    std::optional<std::string> createdTime;
    std::optional<std::string> modifiedTime;
    std::optional<bool> deleted;
    std::optional<bool> resolved;
    std::optional<std::string> quotedText;
    std::optional<DriveCommentQuoted> quotedFileContent;
    std::vector<DriveCommentReply> replies;

    [[nodiscard]] std::string quote() const {
        if (quotedFileContent.has_value()) return quotedFileContent->value;
        if (quotedText.has_value()) return *quotedText;
        return "";
    }
};

struct LoadedDoc {
    slimr::Json json;
    model::DocModel model;
};

struct CommentOccurrence {
    model::Range range;
    std::string tabId;

    bool operator==(const CommentOccurrence&) const = default;
};

struct CommentAnchor {
    std::string commentId;
    bool ambiguous = false;
    std::vector<CommentOccurrence> occurrences;
    std::string quote;
    bool resolved = false;

    bool operator==(const CommentAnchor&) const = default;
};

struct ExportTextMarker {
    int at = 0;
    std::string label;

    bool operator==(const ExportTextMarker&) const = default;
};

struct ExportTextParts {
    std::string body;
    std::map<std::string, std::string> entries;
    std::vector<ExportTextMarker> markers;
};

namespace detail {

constexpr int QUOTE_SEARCH_WINDOW = 5000;
constexpr int QUOTE_TRUNCATED_MIN = 990;

inline const std::set<std::string>& chip_types() {
    static const std::set<std::string> types = {"date", "dropdown", "person", "richLink"};
    return types;
}

inline std::optional<std::string> atomLabel(const model::TabModel& tab, const model::Inline& inlineItem) {
    if (inlineItem.create.has_value()) {
        const auto& c = *inlineItem.create;
        if (c.type == "person") return c.email;
        if (c.type == "date") return c.timestamp;
        if (c.type == "richLink") return c.uri;
        if (c.type == "image") return c.alt;
        return std::nullopt;
    }
    if (!inlineItem.raw.has_value()) return std::nullopt;
    const auto& raw = *inlineItem.raw;
    if (inlineItem.type == "person") {
        if (raw.contains("person") && raw["person"].is_object()) {
            const auto& p = raw["person"];
            if (p.contains("personProperties") && p["personProperties"].is_object()) {
                const auto& props = p["personProperties"];
                if (props.contains("name") && props["name"].is_string()) return props["name"].get<std::string>();
                if (props.contains("email") && props["email"].is_string()) return props["email"].get<std::string>();
            }
        }
    } else if (inlineItem.type == "date") {
        if (raw.contains("dateElement") && raw["dateElement"].is_object()) {
            const auto& de = raw["dateElement"];
            if (de.contains("dateElementProperties") && de["dateElementProperties"].is_object()) {
                const auto& props = de["dateElementProperties"];
                if (props.contains("displayText") && props["displayText"].is_string()) return props["displayText"].get<std::string>();
            }
        }
    } else if (inlineItem.type == "dropdown") {
        if (raw.contains("dropdown") && raw["dropdown"].is_object()) {
            const auto& dd = raw["dropdown"];
            if (dd.contains("dropdownProperties") && dd["dropdownProperties"].is_object()) {
                const auto& props = dd["dropdownProperties"];
                if (props.contains("displayValue") && props["displayValue"].is_string()) return props["displayValue"].get<std::string>();
            }
        }
    } else if (inlineItem.type == "richLink") {
        if (raw.contains("richLink") && raw["richLink"].is_object()) {
            const auto& rl = raw["richLink"];
            if (rl.contains("richLinkProperties") && rl["richLinkProperties"].is_object()) {
                const auto& props = rl["richLinkProperties"];
                if (props.contains("title") && props["title"].is_string()) return props["title"].get<std::string>();
            }
        }
    } else if (inlineItem.type == "image") {
        std::string objId;
        if (raw.contains("inlineObjectElement") && raw["inlineObjectElement"].is_object()) {
            const auto& ioe = raw["inlineObjectElement"];
            if (ioe.contains("inlineObjectId") && ioe["inlineObjectId"].is_string()) {
                objId = ioe["inlineObjectId"].get<std::string>();
            }
        }
        if (!objId.empty()) {
            auto it = tab.inlineObjects.find(objId);
            if (it != tab.inlineObjects.end() && it->second.is_object()) {
                const auto& obj = it->second;
                if (obj.contains("inlineObjectProperties") && obj["inlineObjectProperties"].is_object()) {
                    const auto& props = obj["inlineObjectProperties"];
                    if (props.contains("embeddedObject") && props["embeddedObject"].is_object()) {
                        const auto& emb = props["embeddedObject"];
                        if (emb.contains("title") && emb["title"].is_string()) return emb["title"].get<std::string>();
                        if (emb.contains("description") && emb["description"].is_string()) return emb["description"].get<std::string>();
                    }
                }
            }
        }
    }
    return std::nullopt;
}

struct TabTextResult {
    std::vector<int> index;
    std::string text;
};

inline TabTextResult tabText(const model::TabModel& tab) {
    std::string text;
    std::vector<int> index;
    std::optional<int> lastNewline;

    auto handleParagraph = [&](const model::ParagraphBlock& p) {
        if (!p.origin.has_value()) return;
        if (lastNewline.has_value()) {
            text += '\n';
            index.push_back(*lastNewline);
        }
        lastNewline = p.origin->end - 1;
        int pos = p.origin->start;
        for (const auto& inlineItem : p.inlines) {
            if (inlineItem.kind == "atom") {
                std::optional<std::string> label;
                if (chip_types().contains(inlineItem.type)) {
                    label = atomLabel(tab, inlineItem);
                }
                std::string s = label.value_or("\xef\xbf\xbc"); // U+FFFC in UTF-8
                for (size_t chIdx = 0; chIdx < s.size(); ++chIdx) {
                    text += s[chIdx];
                    index.push_back(pos);
                }
                pos += inlineItem.length;
                continue;
            }
            for (size_t i = 0; i < inlineItem.text.size(); ++i) {
                text += inlineItem.text[i];
                index.push_back(pos + static_cast<int>(i));
            }
            pos += static_cast<int>(inlineItem.text.size());
        }
    };

    for (const auto& p : model::paragraphsDeep(tab.blocks)) {
        handleParagraph(p);
    }
    return TabTextResult{.index = std::move(index), .text = std::move(text)};
}

inline std::string firstLineNormalize(std::string_view text) {
    size_t nl = text.find('\n');
    std::string_view first = (nl == std::string_view::npos) ? text : text.substr(0, nl);
    // replace \s+ with single space and trim
    std::string res;
    bool inWs = false;
    for (char c : first) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!inWs && !res.empty()) {
                res += ' ';
                inWs = true;
            }
        } else {
            res += c;
            inWs = false;
        }
    }
    while (!res.empty() && res.back() == ' ') res.pop_back();
    if (res.size() > 40) res.resize(40);
    return res;
}

struct QuoteStartResult {
    int at = 0;
    bool matched = false;
};

inline QuoteStartResult quoteStart(std::string_view text, int end, std::string_view quote) {
    int window = std::max(0, end - QUOTE_SEARCH_WINDOW);
    auto occurs = [&](int length) -> int {
        std::string_view prefix = quote.substr(0, length);
        size_t searchLimit = end - length;
        size_t found = text.substr(0, searchLimit + prefix.size()).rfind(prefix, searchLimit);
        if (found != std::string_view::npos && static_cast<int>(found) >= window) {
            return static_cast<int>(found);
        }
        return -1;
    };

    int low = std::min(20, static_cast<int>((quote.size() + 1) / 2));
    int high = std::min(static_cast<int>(quote.size()), end - window);
    if (low < 1 || low > high || occurs(low) < 0) {
        size_t lastNl = (end > 0) ? text.substr(0, end).rfind('\n') : std::string_view::npos;
        int at = (lastNl == std::string_view::npos) ? 0 : static_cast<int>(lastNl + 1);
        return QuoteStartResult{.at = at, .matched = false};
    }

    while (low < high) {
        int mid = (low + high + 1) >> 1;
        if (occurs(mid) >= 0) low = mid;
        else high = mid - 1;
    }
    return QuoteStartResult{.at = occurs(low), .matched = true};
}

} // namespace detail

inline ExportTextParts exportTextParse(const std::string& text) {
    static const std::regex listEntryRegex(R"(^\s*(?:[*•◦▪-]\s+|\d+[.)]\s+)?\[([a-z]+)\](.*)$)");
    std::vector<std::string> lines;
    {
        size_t start = 0;
        while (start <= text.size()) {
            size_t nl = text.find('\n', start);
            if (nl == std::string::npos) {
                lines.push_back(text.substr(start));
                break;
            }
            lines.push_back(text.substr(start, nl - start));
            start = nl + 1;
        }
    }

    int listStart = -1;
    for (int i = static_cast<int>(lines.size()) - 1; i >= 0; --i) {
        std::smatch match;
        if (std::regex_match(lines[i], match, listEntryRegex) && match[1] == "a") {
            listStart = i;
            break;
        }
    }

    std::map<std::string, std::string> entries;
    if (listStart >= 0) {
        for (size_t i = static_cast<size_t>(listStart); i < lines.size(); ++i) {
            std::smatch match;
            if (std::regex_match(lines[i], match, listEntryRegex)) {
                std::string label = match[1];
                std::string commentText = match[2];
                if (!entries.contains(label)) {
                    entries[label] = commentText;
                }
            }
        }
    }

    std::string before;
    if (listStart >= 0) {
        for (int i = 0; i < listStart; ++i) {
            if (i > 0) before += '\n';
            before += lines[i];
        }
    } else {
        before = text;
    }

    if (entries.empty() || before.find("[a]") == std::string::npos) {
        return ExportTextParts{.body = text, .entries = {}, .markers = {}};
    }

    std::vector<ExportTextMarker> markers;
    std::string body;
    size_t from = 0;
    static const std::regex markerRegex(R"(\[([a-z]+)\])");
    auto words_begin = std::sregex_iterator(before.begin(), before.end(), markerRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        const std::smatch& match = *it;
        std::string label = match[1];
        if (!entries.contains(label)) continue;
        size_t matchIndex = static_cast<size_t>(match.position());
        body += before.substr(from, matchIndex - from);
        markers.push_back(ExportTextMarker{
            .at = static_cast<int>(body.size()),
            .label = label,
        });
        from = matchIndex + static_cast<size_t>(match.length());
    }
    body += before.substr(from);
    while (!body.empty() && body.back() == '\n') body.pop_back();

    return ExportTextParts{
        .body = std::move(body),
        .entries = std::move(entries),
        .markers = std::move(markers),
    };
}

inline std::map<std::string, std::optional<CommentOccurrence>> anchorsFromExport(
    const std::vector<std::pair<std::string, detail::TabTextResult>>& tabs,
    const std::vector<DriveComment>& comments,
    const std::string& exportText
) {
    std::map<std::string, std::optional<CommentOccurrence>> out;
    auto parts = exportTextParse(exportText);
    if (parts.markers.empty()) return out;

    std::set<std::string> listed;
    for (const auto& [_, val] : parts.entries) {
        listed.insert(detail::firstLineNormalize(val));
    }

    for (const auto& comment : comments) {
        if (!comment.deleted.value_or(false) && !comment.quote().empty()) {
            std::string key = detail::firstLineNormalize(comment.content.value_or(""));
            if (!listed.contains(key)) {
                out[comment.id] = std::nullopt;
            }
        }
    }

    struct OwnerEntry {
        int offset = 0;
        size_t tabIdx = 0;
    };
    std::vector<OwnerEntry> owner;
    std::string whole;
    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& [_, tabData] = tabs[i];
        if (i > 0) {
            whole += '\n';
            owner.push_back(OwnerEntry{
                .offset = static_cast<int>(tabs[i - 1].second.text.size()),
                .tabIdx = i - 1,
            });
        }
        for (int k = 0; k < static_cast<int>(tabData.text.size()); ++k) {
            owner.push_back(OwnerEntry{.offset = k, .tabIdx = i});
        }
        whole += tabData.text;
    }

    std::vector<int> kept = diff::keptOffsets(whole, parts.body);
    for (const auto& marker : parts.markers) {
        int beforeIdx = marker.at - 1;
        if (beforeIdx < 0 || beforeIdx >= static_cast<int>(kept.size()) || kept[beforeIdx] < 0) continue;
        int keptPos = kept[beforeIdx];
        if (keptPos >= static_cast<int>(owner.size())) continue;
        const auto& at = owner[keptPos];
        const auto& [tabId, tabData] = tabs[at.tabIdx];
        if (at.offset >= static_cast<int>(tabData.text.size()) || tabData.text[at.offset] == '\n') continue;

        std::string markerToken = "[" + marker.label + "]";
        if (at.offset + 1 < static_cast<int>(tabData.text.size())) {
            std::string_view remainder(tabData.text.data() + at.offset + 1, tabData.text.size() - (at.offset + 1));
            if (remainder.starts_with(markerToken)) continue;
        }

        int end = at.offset + 1;
        auto entryIt = parts.entries.find(marker.label);
        std::string key = (entryIt != parts.entries.end()) ? detail::firstLineNormalize(entryIt->second) : "";

        std::vector<const DriveComment*> candidates;
        for (const auto& comment : comments) {
            if (!comment.deleted.value_or(false) && !key.empty() && !out.contains(comment.id) &&
                !comment.quote().empty() && detail::firstLineNormalize(comment.content.value_or("")) == key) {
                candidates.push_back(&comment);
            }
        }

        struct Scored {
            const DriveComment* comment = nullptr;
            detail::QuoteStartResult start;
        };
        std::vector<Scored> scored;
        for (const auto* c : candidates) {
            scored.push_back(Scored{
                .comment = c,
                .start = detail::quoteStart(tabData.text, end, c->quote()),
            });
        }
        std::vector<Scored> matched;
        for (const auto& s : scored) {
            if (s.start.matched) matched.push_back(s);
        }

        const Scored* best = nullptr;
        if (scored.size() == 1) best = &scored[0];
        else if (matched.size() == 1) best = &matched[0];

        if (!best || best->start.at >= end) continue;
        int startDocIdx = tabData.index[best->start.at];
        int endDocIdx = tabData.index[end - 1] + 1;

        out[best->comment->id] = CommentOccurrence{
            .range = model::Range{.end = endDocIdx, .start = startDocIdx},
            .tabId = tabId,
        };
    }
    return out;
}

inline std::vector<CommentAnchor> commentAnchorsMatch(
    const model::DocModel& model,
    const std::vector<DriveComment>& comments,
    const std::optional<std::string>& exportText = std::nullopt
) {
    std::vector<std::pair<std::string, detail::TabTextResult>> tabs;
    for (const auto& tab : model.tabs) {
        tabs.push_back({tab.tabId, detail::tabText(tab)});
    }

    std::optional<std::map<std::string, std::optional<CommentOccurrence>>> pinned;
    if (exportText.has_value()) {
        pinned = anchorsFromExport(tabs, comments, *exportText);
    }

    std::vector<CommentAnchor> result;
    for (const auto& comment : comments) {
        std::string quote = comment.quote();
        if (quote.empty() || comment.deleted.value_or(false)) continue;

        if (pinned.has_value()) {
            auto it = pinned->find(comment.id);
            if (it != pinned->end()) {
                std::vector<CommentOccurrence> occ;
                if (it->second.has_value()) {
                    occ.push_back(*it->second);
                }
                result.push_back(CommentAnchor{
                    .commentId = comment.id,
                    .ambiguous = false,
                    .occurrences = std::move(occ),
                    .quote = quote,
                    .resolved = comment.resolved.value_or(false),
                });
                continue;
            }
        }

        std::vector<CommentOccurrence> occurrences;
        std::string trimmed = quote;
        // Trim trailing \s*@\s*$
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
        if (!trimmed.empty() && trimmed.back() == '@') {
            trimmed.pop_back();
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
        }

        bool hasFullQuote = false;
        for (const auto& [_, tabData] : tabs) {
            if (tabData.text.find(quote) != std::string::npos) {
                hasFullQuote = true;
                break;
            }
        }

        std::string search = (!trimmed.empty() && trimmed != quote && !hasFullQuote) ? trimmed : quote;

        for (const auto& [tabId, tabData] : tabs) {
            size_t at = tabData.text.find(search);
            while (at != std::string::npos) {
                int start = tabData.index[at];
                size_t last = at + search.size() - 1;
                if (search.size() >= detail::QUOTE_TRUNCATED_MIN) {
                    size_t newline = tabData.text.find('\n', last + 1);
                    last = (newline == std::string::npos ? tabData.text.size() : newline) - 1;
                }
                int end = tabData.index[last] + 1;
                occurrences.push_back(CommentOccurrence{
                    .range = model::Range{.end = end, .start = start},
                    .tabId = tabId,
                });
                at = tabData.text.find(search, at + 1);
            }
        }

        result.push_back(CommentAnchor{
            .commentId = comment.id,
            .ambiguous = occurrences.size() > 1,
            .occurrences = std::move(occurrences),
            .quote = quote,
            .resolved = comment.resolved.value_or(false),
        });
    }
    return result;
}

template <typename ClientType>
inline LoadedDoc docLoad(
    const std::string& docId,
    ClientType& client,
    model::KeyAllocator& keys
) {
    slimr::Json json = client.getDocument(docId);
    model::DocModel mod = model::docModelParse(json, model::DocModelParseOptions{.docId = docId, .keys = keys});
    return LoadedDoc{.json = std::move(json), .model = std::move(mod)};
}

} // namespace gdocsmith::engine
