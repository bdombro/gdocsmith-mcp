#pragma once

#include "emulator/google_docs_emulator.hpp"
#include "emulator/types.hpp"
#include "engine/guard.hpp"
#include "engine/load.hpp"
#include "engine/suggestion_rule.hpp"
#include "lens/cell_styles.hpp"
#include "lens/document.hpp"
#include "lens/paragraph_styles.hpp"
#include "lens/parse.hpp"
#include "lens/put.hpp"
#include "lens/tokens.hpp"
#include "model/edit.hpp"
#include "model/errors.hpp"
#include "model/from_json.hpp"
#include "model/invariants.hpp"
#include "model/keys.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

struct Changed {
    bool changed = false;

    bool operator==(const Changed &) const = default;
};

struct TabPosition {
    std::optional<std::string> afterTab;
    std::optional<std::string> beforeTab;
    std::optional<int> index;
};

struct TabInfo {
    std::optional<std::string> parentTabId;
    std::string tabId;
    std::string title;

    bool operator==(const TabInfo &) const = default;
};

struct PageSetupMargins {
    std::optional<double> bottom;
    std::optional<double> left;
    std::optional<double> right;
    std::optional<double> top;
};

struct PageSetupPatch {
    std::optional<double> heightPt;
    std::optional<PageSetupMargins> margins;
    std::optional<std::string> orientation; // "LANDSCAPE" | "PORTRAIT"
    std::optional<bool> pageless;
    std::optional<std::string> size; // "A3" | "A4" | "A5" | "LEGAL" | "LETTER" | "TABLOID"
    std::optional<double> widthPt;
};

struct Ledger {
    std::map<std::string, std::string> docs;
    std::map<std::string, std::vector<std::string>> tabs;
    std::map<std::string, std::string> revisions;
};

inline Ledger ledgerCreate() {
    return Ledger{};
}

struct DocState {
    std::optional<std::string> alias;
    std::optional<std::vector<DriveComment>> comments;
    std::optional<std::vector<CommentAnchor>> commentAnchors;
    std::optional<std::string> exportText;
    std::optional<std::string> exportTextFailed;
    struct CreateInfo {
        std::string title;
    };
    std::optional<CreateInfo> create;
    model::DocModel current;
    std::string docId;
    bool isNew = false;
    slimr::Json json;
    model::DocModel original;
    std::optional<std::string> createdAs;
    std::vector<model::TabModel> hiddenTabs;
    int tabCounter = 0;
    std::vector<model::Tombstone> tombstones;
    std::set<std::string> managedNamedRangeIds;
    std::vector<lens::ImageSource> imageRecreations;
};

struct WriteReport {
    bool changed = false;
    std::vector<std::string> notes;
    std::vector<std::string> recreated;
    std::vector<std::string> rejected;
};

class CoreSession;
class TabHandle;

class DocHandle {
  public:
    virtual ~DocHandle() = default;
    [[nodiscard]] virtual std::optional<std::string> alias() const = 0;
    [[nodiscard]] virtual std::string docId() const = 0;
    [[nodiscard]] virtual bool isNew() const = 0;
    virtual TabHandle tab(const std::string &ref = "") = 0;
    virtual TabHandle tabCreate(const std::string &title,
                                const std::optional<std::string> &parentTab = std::nullopt,
                                const std::optional<TabPosition> &position = std::nullopt) = 0;
    virtual std::vector<TabInfo> tabs() = 0;
    virtual std::string title() = 0;
};

class TabHandle {
  public:
    std::string tabId;
    CoreSession *session = nullptr;
    DocState *state = nullptr;

    TabHandle(CoreSession *s, DocState *st, std::string tid)
        : tabId(std::move(tid)), session(s), state(st) {}

    model::TabModel &model();
    Changed deleteTab();
    Changed move(const TabPosition &p);
    Changed pageSetupSet(const PageSetupPatch &p);
    Changed settingsSet(const lens::TabSettings &settings);
    Changed segmentFlagsSet(bool evenPages, bool firstPage);
    Changed segmentCreate(const std::string &kind, std::optional<int> section = std::nullopt);
    Changed segmentDelete(const std::string &kind, const std::string &type = "default",
                          std::optional<int> section = std::nullopt);
    WriteReport segmentWriteMarkdown(const std::string &kind, const std::string &type,
                                     const std::string &md, std::optional<int> section = std::nullopt);
    WriteReport footnoteWriteMarkdown(const std::string &id, const std::string &md);
    WriteReport writeSegment(model::TabModel &tab, const std::string &md);
    Changed remove();
    Changed rename(const std::string &title);
    WriteReport writeMarkdown(const std::string &md);
};

namespace detail {

inline model::DocModel documentEditCopy(const model::DocModel &source) {
    auto copy = source;
    auto detach = [&](auto &self, model::TabModel &tab) -> void {
        for (auto &segment : tab.segments) {
            if (segment.tab) {
                segment.tab = std::make_shared<model::TabModel>(*segment.tab);
                self(self, *segment.tab);
            }
            if (segment.original) {
                segment.original = std::make_shared<model::TabModel>(*segment.original);
                self(self, *segment.original);
            }
        }
    };
    for (auto &tab : copy.tabs)
        detach(detach, tab);
    return copy;
}

inline std::string trimLower(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

inline model::TabModel &tabResolveRef(model::DocModel &doc, const std::string &ref) {
    if (ref.empty()) {
        if (doc.tabs.size() == 1)
            return doc.tabs[0];
        std::string titles;
        for (const auto &t : doc.tabs) {
            if (!titles.empty())
                titles += ", ";
            titles += "\"" + t.title + "\"";
        }
        throw model::CoreError(std::string(model::error_codes::TAB_REQUIRED),
                               "this document has " + std::to_string(doc.tabs.size()) + " tabs; name one: " + titles);
    }
    for (auto &t : doc.tabs) {
        if (t.tabId == ref)
            return t;
    }
    std::string wanted = trimLower(ref);
    std::vector<model::TabModel *> byTitle;
    for (auto &t : doc.tabs) {
        if (trimLower(t.title) == wanted) {
            byTitle.push_back(&t);
        }
    }
    if (byTitle.size() == 1)
        return *byTitle[0];
    if (byTitle.size() > 1) {
        throw model::CoreError(std::string(model::error_codes::TAB_REQUIRED),
                               std::to_string(byTitle.size()) + " tabs are titled \"" + ref + "\"; use a tab id");
    }
    std::string titles;
    for (const auto &t : doc.tabs) {
        if (!titles.empty())
            titles += ", ";
        titles += "\"" + t.title + "\"";
    }
    throw model::CoreError(std::string(model::error_codes::TAB_NOT_FOUND),
                           "no tab \"" + ref + "\" (tabs: " + titles + ")");
}

inline void tabInsert(model::DocModel &doc, model::TabModel tab, const std::optional<TabPosition> &position) {
    std::vector<std::string> siblingIds;
    for (const auto &t : doc.tabs) {
        if (t.parentTabId == tab.parentTabId)
            siblingIds.push_back(t.tabId);
    }

    int index = static_cast<int>(siblingIds.size());
    if (position.has_value()) {
        if (position->index.has_value()) {
            index = std::max(0, std::min(*position->index, static_cast<int>(siblingIds.size())));
        } else if (position->afterTab.has_value() || position->beforeTab.has_value()) {
            std::string refName = position->afterTab.value_or(position->beforeTab.value_or(""));
            auto &refTab = tabResolveRef(doc, refName);
            auto it = std::find(siblingIds.begin(), siblingIds.end(), refTab.tabId);
            if (it == siblingIds.end()) {
                throw model::CoreError(std::string(model::error_codes::INVALID_PLACEMENT),
                                       "\"" + refTab.title + "\" isn't a sibling of that tab");
            }
            int at = static_cast<int>(std::distance(siblingIds.begin(), it));
            index = position->afterTab.has_value() ? at + 1 : at;
        }
    }

    auto subtreeEnd = [&](const model::TabModel &t) -> int {
        int end = 0;
        for (size_t i = 0; i < doc.tabs.size(); ++i) {
            if (doc.tabs[i].tabId == t.tabId) {
                end = static_cast<int>(i + 1);
                break;
            }
        }
        auto inSubtree = [&](auto &self, const model::TabModel &x) -> bool {
            if (!x.parentTabId.has_value())
                return false;
            if (*x.parentTabId == t.tabId)
                return true;
            for (const auto &y : doc.tabs) {
                if (y.tabId == *x.parentTabId)
                    return self(self, y);
            }
            return false;
        };
        while (end < static_cast<int>(doc.tabs.size()) && inSubtree(inSubtree, doc.tabs[end])) {
            end++;
        }
        return end;
    };

    int at = static_cast<int>(doc.tabs.size());
    if (index < static_cast<int>(siblingIds.size())) {
        for (size_t i = 0; i < doc.tabs.size(); ++i) {
            if (doc.tabs[i].tabId == siblingIds[index]) {
                at = static_cast<int>(i);
                break;
            }
        }
    } else if (!siblingIds.empty()) {
        for (const auto &t : doc.tabs) {
            if (t.tabId == siblingIds.back()) {
                at = subtreeEnd(t);
                break;
            }
        }
    } else if (tab.parentTabId.has_value()) {
        for (const auto &t : doc.tabs) {
            if (t.tabId == *tab.parentTabId) {
                at = subtreeEnd(t);
                break;
            }
        }
    }
    doc.tabs.insert(doc.tabs.begin() + at, std::move(tab));
}

/** Keys of paragraphs (including table cells) that hold at least one inline. */
inline std::set<std::string> paragraphKeysWithInlines(const std::vector<model::Block> &blocks) {
    std::set<std::string> keys;
    for (const auto &paragraph : model::paragraphsDeep(blocks)) {
        if (!paragraph.inlines.empty())
            keys.insert(paragraph.key);
    }
    return keys;
}

/** Keys from `hadInlines` whose paragraph in `after` now has no inlines. */
inline std::set<std::string> rejectionEmptiedKeys(const std::set<std::string> &hadInlines,
                                                  const std::vector<model::Block> &after) {
    std::set<std::string> keys;
    for (const auto &paragraph : model::paragraphsDeep(after)) {
        if (paragraph.inlines.empty() && hadInlines.contains(paragraph.key))
            keys.insert(paragraph.key);
    }
    return keys;
}

} // namespace detail

class DocHandleImpl : public DocHandle {
  public:
    CoreSession *session = nullptr;
    DocState *state = nullptr;

    DocHandleImpl(CoreSession *s, DocState *st) : session(s), state(st) {}

    [[nodiscard]] std::optional<std::string> alias() const override { return state->alias; }
    [[nodiscard]] std::string docId() const override { return state->docId; }
    [[nodiscard]] bool isNew() const override { return state->isNew; }

    TabHandle tab(const std::string &ref = "") override;
    TabHandle tabCreate(const std::string &title,
                        const std::optional<std::string> &parentTab = std::nullopt,
                        const std::optional<TabPosition> &position = std::nullopt) override;
    std::vector<TabInfo> tabs() override {
        std::vector<TabInfo> res;
        for (const auto &t : state->current.tabs) {
            res.push_back(TabInfo{.parentTabId = t.parentTabId, .tabId = t.tabId, .title = t.title});
        }
        return res;
    }
    std::string title() override {
        return state->current.title;
    }
};

class CoreSession {
  public:
    std::map<std::string, DocState> docs;
    model::KeyAllocator keys;
    int tabIndex = 0;
    std::map<int, bool> tabForces;
    std::vector<std::string> allow;
    std::set<std::string> rejectedSuggestions;
    lens::AtomPutOptions atomOptions;
    bool allowTokenOmission = false;
    std::vector<GuardFinding> findings;
    Ledger *ledger = nullptr;

    // Optional client / drive callbacks
    std::function<slimr::Json(const std::string &)> getDocumentFn;
    std::function<std::vector<DriveComment>(const std::string &)> listCommentsFn;
    std::function<std::string(const std::string &)> textExportFn;

    CoreSession() = default;

    template <typename ClientType, typename DriveType>
    CoreSession(ClientType *client, DriveType *drive, Ledger *led = nullptr, std::vector<std::string> al = {})
        : allow(std::move(al)), ledger(led) {
        if (client) {
            getDocumentFn = [client](const std::string &id) { return client->getDocument(id); };
        }
        if (drive) {
            listCommentsFn = [drive](const std::string &id) { return drive->commentsList(id); };
            textExportFn = [drive](const std::string &id) { return drive->textExport(id); };
        }
    }

    std::shared_ptr<DocHandle> docOpen(const std::string &docId, const std::string &alias = "") {
        auto it = docs.find(docId);
        if (it != docs.end()) {
            return std::make_shared<DocHandleImpl>(this, &it->second);
        }

        slimr::Json json;
        if (getDocumentFn) {
            json = getDocumentFn(docId);
        } else {
            json = emulator::blank_doc_template("Untitled", docId);
        }
        if (ledger && ledger->revisions.contains(docId)) {
            if (!json.is_object() || !json.contains("revisionId") || !json["revisionId"].is_string() ||
                json["revisionId"].get<std::string>() != ledger->revisions.at(docId)) {
                throw model::CoreError("staleBase", "The document changed after creation; outstanding edits were not replayed.");
            }
        }

        auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = docId, .keys = keys});
        auto current = detail::documentEditCopy(original);

        std::set<std::string> createdTabSet;
        if (ledger && ledger->tabs.contains(docId)) {
            for (const auto &tid : ledger->tabs[docId]) {
                createdTabSet.insert(tid);
            }
        }

        std::vector<model::TabModel> hidden;
        std::vector<model::TabModel> kept;
        for (auto &t : current.tabs) {
            if (createdTabSet.contains(t.tabId)) {
                hidden.push_back(std::move(t));
            } else {
                kept.push_back(std::move(t));
            }
        }
        current.tabs = std::move(kept);

        DocState st{
            .alias = alias.empty() ? std::nullopt : std::make_optional(alias),
            .current = std::move(current),
            .docId = docId,
            .isNew = false,
            .json = std::move(json),
            .original = std::move(original),
            .createdAs = std::nullopt,
            .hiddenTabs = std::move(hidden),
            .tabCounter = 0,
            .tombstones = {},
        };
        auto [insIt, _] = docs.insert_or_assign(docId, std::move(st));
        return std::make_shared<DocHandleImpl>(this, &insIt->second);
    }

    std::shared_ptr<DocHandle> docCreate(const std::string &alias, const std::string &title) {
        if (ledger && ledger->docs.contains(alias)) {
            std::string realId = ledger->docs[alias];
            auto handle = docOpen(realId, alias);
            docs[realId].createdAs = alias;
            return handle;
        }

        std::string docId = "new_" + alias;
        if (docs.contains(docId)) {
            throw model::CoreError(std::string(model::error_codes::INTERNAL), "alias \"" + alias + "\" is already used");
        }

        slimr::Json json = emulator::blank_doc_template(title, docId);
        auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = docId, .keys = keys});
        original.isNew = true;
        auto current = detail::documentEditCopy(original);

        DocState st{
            .alias = alias,
            .create = DocState::CreateInfo{.title = title},
            .current = std::move(current),
            .docId = docId,
            .isNew = true,
            .json = std::move(json),
            .original = std::move(original),
            .createdAs = std::nullopt,
            .hiddenTabs = {},
            .tabCounter = 0,
            .tombstones = {},
        };
        auto [insIt, _] = docs.insert_or_assign(docId, std::move(st));
        return std::make_shared<DocHandleImpl>(this, &insIt->second);
    }

    void tabBegin(int idx, bool force = false) {
        tabIndex = idx;
        tabForces[idx] = force;
    }

    [[nodiscard]] bool tabForce(int idx) const {
        auto it = tabForces.find(idx);
        return it != tabForces.end() ? it->second : false;
    }

    void findingAdd(GuardFinding f) {
        f.waived = (f.severity == "block" && f.allowKind.has_value() &&
                    std::find(allow.begin(), allow.end(), *f.allowKind) != allow.end());
        findings.push_back(std::move(f));
    }

    std::vector<DriveComment> commentsFor(DocState &st) {
        if (st.isNew)
            return {};
        if (!st.comments.has_value()) {
            if (listCommentsFn) {
                st.comments = listCommentsFn(st.docId);
            } else {
                st.comments = std::vector<DriveComment>{};
            }
        }
        return *st.comments;
    }

    std::optional<std::string> exportTextFor(DocState &st) {
        if (!st.exportText.has_value() && !st.exportTextFailed.has_value()) {
            auto comms = commentsFor(st);
            bool hasQuotes = false;
            for (const auto &c : comms) {
                if (!c.deleted.value_or(false) && !c.quote().empty()) {
                    hasQuotes = true;
                    break;
                }
            }
            if (hasQuotes && textExportFn) {
                try {
                    st.exportText = textExportFn(st.docId);
                } catch (const std::exception &e) {
                    st.exportTextFailed = e.what();
                }
            }
        }
        return st.exportText;
    }

    std::vector<CommentAnchor> commentAnchorsFor(DocState &st) {
        if (st.isNew)
            return {};
        if (!st.commentAnchors.has_value()) {
            auto comms = commentsFor(st);
            auto text = exportTextFor(st);
            st.commentAnchors = commentAnchorsMatch(st.original, comms, text);
        }
        return *st.commentAnchors;
    }
};

inline model::TabModel &TabHandle::model() {
    for (auto &t : state->current.tabs) {
        if (t.tabId == tabId)
            return t;
    }
    throw model::CoreError(std::string(model::error_codes::TAB_NOT_FOUND),
                           "tab " + tabId + " no longer exists");
}

inline Changed TabHandle::deleteTab() {
    auto &doc = state->current;
    std::set<std::string> doomed = {tabId};
    for (bool grew = true; grew;) {
        grew = false;
        for (const auto &t : doc.tabs) {
            if (t.parentTabId.has_value() && doomed.contains(*t.parentTabId) && !doomed.contains(t.tabId)) {
                doomed.insert(t.tabId);
                grew = true;
            }
        }
    }
    std::vector<model::TabModel> kept;
    for (auto &t : doc.tabs) {
        if (!doomed.contains(t.tabId))
            kept.push_back(std::move(t));
    }
    doc.tabs = std::move(kept);
    return Changed{.changed = true};
}

inline Changed TabHandle::move(const TabPosition &p) {
    auto &doc = state->current;
    auto beforeOrder = doc.tabs;
    auto tabM = std::move(model());
    std::vector<model::TabModel> remaining;
    for (auto &t : doc.tabs) {
        if (t.tabId != tabId)
            remaining.push_back(std::move(t));
    }
    doc.tabs = std::move(remaining);
    detail::tabInsert(doc, std::move(tabM), p);
    bool changed = (beforeOrder.size() != doc.tabs.size());
    if (!changed) {
        for (size_t i = 0; i < beforeOrder.size(); ++i) {
            if (beforeOrder[i].tabId != doc.tabs[i].tabId) {
                changed = true;
                break;
            }
        }
    }
    return Changed{.changed = changed};
}

inline Changed TabHandle::rename(const std::string &title) {
    auto &t = model();
    if (t.title == title)
        return Changed{.changed = false};
    t.title = title;
    return Changed{.changed = true};
}

inline Changed TabHandle::remove() {
    auto &t = model();
    t.blocks.clear();
    t.blocks.push_back(model::paragraphEmptyCreate(session->keys, {}, model::EditStamp{
                                                                          .force = session->tabForce(session->tabIndex),
                                                                          .tabIndex = session->tabIndex,
                                                                      }));
    return Changed{.changed = true};
}

inline Changed TabHandle::pageSetupSet(const PageSetupPatch &p) {
    auto &t = model();
    auto style = t.documentStyle;
    if (p.pageless.has_value()) {
        style["documentFormat"] = slimr::Json::object_t{
            {"documentMode", *p.pageless ? "PAGELESS" : "PAGES"}};
    }
    auto positive = [](double number) {
        if (!std::isfinite(number) || number <= 0) {
            throw model::CoreError("unsupportedSyntax", "Page dimensions must be finite positive points.");
        }
        return number;
    };
    std::optional<std::pair<double, double>> size;
    if (p.widthPt || p.heightPt) {
        if (!p.widthPt || !p.heightPt)
            throw model::CoreError("unsupportedSyntax", "Page width and height must be set together.");
        size = {positive(*p.widthPt), positive(*p.heightPt)};
    } else if (p.size) {
        static const std::map<std::string, std::pair<double, double>> sizes = {
            {"LETTER", {612, 792}},
            {"LEGAL", {612, 1008}},
            {"TABLOID", {792, 1224}},
            {"A3", {297 * 72.0 / 25.4, 420 * 72.0 / 25.4}},
            {"A4", {210 * 72.0 / 25.4, 297 * 72.0 / 25.4}},
            {"A5", {148 * 72.0 / 25.4, 210 * 72.0 / 25.4}},
        };
        const auto found = sizes.find(*p.size);
        if (found == sizes.end())
            throw model::CoreError("unsupportedSyntax", "Unknown page size.");
        size = found->second;
    }
    if (p.orientation) {
        if (*p.orientation != "LANDSCAPE" && *p.orientation != "PORTRAIT") {
            throw model::CoreError("unsupportedSyntax", "Invalid page orientation.");
        }
        if (!size) {
            const auto stored = style.contains("pageSize") ? style["pageSize"]
                                                           : (style.contains("documentSize") ? style["documentSize"] : model::make_json_object());
            size = {stored.contains("width") ? positive(model::dimensionPt(stored["width"])) : 612,
                    stored.contains("height") ? positive(model::dimensionPt(stored["height"])) : 792};
        }
        if ((*p.orientation == "LANDSCAPE") != (size->first > size->second))
            std::swap(size->first, size->second);
    }
    if (size)
        style["pageSize"] = slimr::Json::object_t{
            {"width", model::dimensionFromPt(size->first)}, {"height", model::dimensionFromPt(size->second)}};
    if (p.margins) {
        for (const auto &[field, value] : {
                 std::pair{"marginBottom", p.margins->bottom}, {"marginLeft", p.margins->left}, {"marginRight", p.margins->right}, {"marginTop", p.margins->top}}) {
            if (!value)
                continue;
            if (!std::isfinite(*value) || *value < 0)
                throw model::CoreError("unsupportedSyntax", "Invalid page margin.");
            style[field] = model::dimensionFromPt(*value);
        }
    }
    const bool changed = style != t.documentStyle;
    t.documentStyle = std::move(style);
    return {.changed = changed};
}

inline Changed TabHandle::settingsSet(const lens::TabSettings &settings) {
    auto &t = model();
    auto style = t.documentStyle;
    auto dimension = [&](const char *field, double value, double fallback) {
        if (!std::isfinite(value) || value < 0)
            throw model::CoreError("unsupportedSyntax", "Invalid tab setting dimension.");
        const double current = style.contains(field) ? model::dimensionPt(style[field]) : fallback;
        if (std::abs(current - value) > 0.0001)
            style[field] = model::dimensionFromPt(value);
    };
    dimension("marginHeader", settings.headerMargin, 36);
    dimension("marginFooter", settings.footerMargin, 36);
    const int page = style.contains("pageNumberStart") ? style["pageNumberStart"].as<int>() : 1;
    if (settings.pageNumberStart < 0)
        throw model::CoreError("unsupportedSyntax", "Invalid page number start.");
    if (page != settings.pageNumberStart)
        style["pageNumberStart"] = settings.pageNumberStart;
    if (settings.background.size() != 7 || settings.background.front() != '#' ||
        settings.background.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) {
        throw model::CoreError("unsupportedSyntax", "Invalid tab background color.");
    }
    auto current = style.contains("background") && style["background"].contains("color")
                       ? model::colorHexFromOptional(style["background"]["color"]).value_or("#FFFFFF")
                       : "#FFFFFF";
    if (current != settings.background)
        style["background"] = slimr::Json::object_t{
            {"color", model::colorOptionalFromHex(settings.background)}};
    const bool changed = style != t.documentStyle;
    t.documentStyle = std::move(style);
    return {.changed = changed};
}

inline Changed TabHandle::segmentFlagsSet(bool evenPages, bool firstPage) {
    auto &tab = model();
    auto before = tab.documentStyle;
    for (const auto &[field, value] : {std::pair{"useEvenPageHeaderFooter", evenPages},
                                       {"useFirstPageHeaderFooter", firstPage}}) {
        if (value)
            tab.documentStyle[field] = true;
        else if (tab.documentStyle.is_object())
            tab.documentStyle.get<slimr::Json::object_t>().erase(field);
    }
    return {.changed = before != tab.documentStyle};
}

inline Changed TabHandle::segmentCreate(const std::string &kind, std::optional<int> section) {
    if ((kind != "header" && kind != "footer") || (section && *section < 2)) {
        throw model::CoreError("unsupportedSyntax", "A segment must be a header/footer in an existing section.");
    }
    auto &tab = model();
    if (std::any_of(tab.segments.begin(), tab.segments.end(), [&](const auto &segment) {
            return segment.kind == kind && segment.type == "default" && segment.section == section;
        }))
        return {};
    const auto id = "new:" + tabId + ":" + kind + (section ? ":s" + std::to_string(*section) : "");
    auto blank = model::segmentTabParse(tabId, tab.title, id,
                                        slimr::Json::array_t{slimr::Json::object_t{{"startIndex", 0}, {"endIndex", 1}, {"paragraph", slimr::Json::object_t{{"elements", slimr::Json::array_t{slimr::Json::object_t{{"startIndex", 0}, {"endIndex", 1}, {"textRun", slimr::Json::object_t{{"content", "\n"}}}}}}}}}},
                                        model::make_json_object(), session->keys);
    blank.namedStyles = tab.namedStyles;
    blank.lists = tab.lists;
    blank.inlineObjects = tab.inlineObjects;
    blank.isNew = true;
    tab.segments.push_back({.id = id, .kind = kind, .section = section, .tab = std::make_shared<model::TabModel>(std::move(blank))});
    return {.changed = true};
}

inline Changed TabHandle::segmentDelete(const std::string &kind, const std::string &type,
                                        std::optional<int> section) {
    auto &tab = model();
    const auto found = std::find_if(tab.segments.begin(), tab.segments.end(), [&](const auto &segment) {
        return segment.kind == kind && segment.type == type && segment.section == section;
    });
    if (found == tab.segments.end())
        return {};
    const auto id = found->id;
    tab.segments.erase(found);
    auto unreference = [&](model::JsonObject &style) {
        if (!style.is_object())
            return;
        std::vector<std::string> fields;
        for (const auto &[field, value] : style.get<slimr::Json::object_t>()) {
            if (value == slimr::Json(id))
                fields.push_back(field);
        }
        for (const auto &field : fields)
            style.get<slimr::Json::object_t>().erase(field);
    };
    unreference(tab.documentStyle);
    unreference(tab.leadingSectionStyle);
    for (auto &block : tab.blocks)
        if (block.is_section_break())
            unreference(block.as_section_break().sectionStyle);
    const auto pool = kind == "header" ? "headers" : "footers";
    if (tab.headersFooters.contains(pool) && tab.headersFooters[pool].is_object()) {
        tab.headersFooters[pool].get<slimr::Json::object_t>().erase(id);
    }
    return {.changed = true};
}

inline WriteReport TabHandle::segmentWriteMarkdown(const std::string &kind, const std::string &type,
                                                   const std::string &md, std::optional<int> section) {
    auto &tab = model();
    for (auto &segment : tab.segments) {
        if (segment.kind == kind && segment.type == type && segment.section == section && segment.tab) {
            return writeSegment(*segment.tab, md);
        }
    }
    throw model::CoreError("anchorNotFound", "The requested header/footer does not exist.");
}

inline WriteReport TabHandle::footnoteWriteMarkdown(const std::string &id, const std::string &md) {
    for (auto &segment : model().segments) {
        if (segment.kind == "footnote" && segment.id == id && segment.tab)
            return writeSegment(*segment.tab, md);
    }
    throw model::CoreError("anchorNotFound", "The footnote does not exist.");
}

inline WriteReport TabHandle::writeSegment(model::TabModel &tab, const std::string &md) {
    model::EditContext context{
        .keys = session->keys,
        .stamp = {.force = session->tabForce(session->tabIndex), .tabIndex = session->tabIndex},
        .tombstones = state->tombstones,
    };
    auto candidate = detail::documentEditCopy(model::DocModel{.tabs = {tab}}).tabs.front();
    lens::AtomPutReport recreations;
    std::map<std::string, std::string> atomAliases;
    const auto hadInlines = detail::paragraphKeysWithInlines(candidate.blocks);
    auto rejected = suggestionRejectsApply(model::EditTarget{context, candidate}, session->rejectedSuggestions,
                                           state->current, recreations, &atomAliases);
    auto rejectionEmptied = detail::rejectionEmptiedKeys(hadInlines, candidate.blocks);
    auto parsed = lens::markdownParse(md);
    auto aliasAtoms = [&](auto &self, std::vector<lens::ProjBlock> &blocks) -> void {
        for (auto &block : blocks) {
            for (auto &span : block.spans) {
                if (!span.token || span.token->kind != "image")
                    continue;
                for (auto &[name, id] : span.token->attrs.values) {
                    if (name == "id" && atomAliases.contains(id))
                        id = atomAliases.at(id);
                }
            }
            if (block.table)
                for (auto &row : block.table->cells)
                    for (auto &cell : row)
                        self(self, cell);
        }
    };
    aliasAtoms(aliasAtoms, parsed.blocks);
    std::erase_if(parsed.reviewMarkers, [&](const auto &marker) {
        return marker.name == "suggestion" &&
               std::find(rejected.begin(), rejected.end(), marker.id) != rejected.end();
    });
    auto options = session->atomOptions;
    options.allowLossyImageMove = true;
    auto report = lens::markdownPut(model::EditTarget{context, candidate}, parsed, options,
                                    {.allowTokenOmission = session->allowTokenOmission, .carryReview = suggestionRuleApply, .rejectionEmptied = std::move(rejectionEmptied)});
    const bool changed = candidate != tab;
    tab = std::move(candidate);
    for (const auto &source : report.recreatedImages) {
        if (source.sourceUriFallback || source.lost.empty())
            continue;
        bool retained = source.source.docId != state->current.docId;
        for (const auto &currentTab : state->current.tabs) {
            for (const auto &atom : model::atomsDeep(currentTab.blocks)) {
                if (atom.key == source.source.key)
                    retained = true;
            }
            for (const auto &segment : currentTab.segments)
                if (segment.tab) {
                    for (const auto &atom : model::atomsDeep(segment.tab->blocks)) {
                        if (atom.key == source.source.key)
                            retained = true;
                    }
                }
        }
        if (!retained)
            continue;
        std::string fields;
        for (const auto &field : source.lost)
            fields += (fields.empty() ? "" : ", ") + field;
        const auto name = source.label ? "image " + slimr::write_json(slimr::Json(*source.label)) : "an image";
        report.notes.push_back("Copied " + name + " without its " + fields + "; the original keeps them.");
    }
    state->imageRecreations.insert(state->imageRecreations.end(), recreations.recreated.begin(), recreations.recreated.end());
    state->imageRecreations.insert(state->imageRecreations.end(), report.recreatedImages.begin(), report.recreatedImages.end());
    state->tombstones = std::move(context.tombstones);
    state->managedNamedRangeIds.insert(report.managedNamedRangeIds.begin(), report.managedNamedRangeIds.end());
    return WriteReport{.changed = changed, .notes = std::move(report.notes), .rejected = std::move(rejected)};
}

inline WriteReport TabHandle::writeMarkdown(const std::string &md) {
    auto report = writeSegment(model(), md);
    auto &tab = model();
    std::set<std::string> kept;
    for (const auto &paragraph : model::paragraphsDeep(tab.blocks)) {
        for (const auto &item : paragraph.inlines) {
            if (!item.is_atom() || item.type != "footnoteRef")
                continue;
            std::string id;
            if (item.create)
                id = "new:fn:" + item.key;
            else if (item.raw && item.raw->contains("footnoteReference") &&
                     (*item.raw)["footnoteReference"].contains("footnoteId")) {
                id = (*item.raw)["footnoteReference"]["footnoteId"].get<std::string>();
            }
            if (id.empty())
                continue;
            kept.insert(id);
            if (!item.create || std::any_of(tab.segments.begin(), tab.segments.end(),
                                            [&](const auto &segment) { return segment.kind == "footnote" && segment.id == id; }))
                continue;
            auto blank = model::docModelParse(reconcile::footnoteBlankJson(tabId, tab.title, tab.namedStyles),
                                              model::DocModelParseOptions{.docId = state->docId, .keys = session->keys})
                             .tabs.front();
            blank.tabId = model::segmentTabId(tabId, id);
            blank.inlineObjects = tab.inlineObjects;
            tab.segments.push_back({.id = id, .kind = "footnote", .original = std::make_shared<model::TabModel>(blank), .tab = std::make_shared<model::TabModel>(std::move(blank))});
        }
    }
    std::erase_if(tab.segments, [&](const auto &segment) {
        return segment.kind == "footnote" && !kept.contains(segment.id);
    });
    for (auto it = tab.footnotes.begin(); it != tab.footnotes.end();) {
        if (!kept.contains(it->first))
            it = tab.footnotes.erase(it);
        else
            ++it;
    }
    return report;
}

inline TabHandle DocHandleImpl::tab(const std::string &ref) {
    auto &resolved = detail::tabResolveRef(state->current, ref);
    return TabHandle(session, state, resolved.tabId);
}

inline TabHandle DocHandleImpl::tabCreate(
    const std::string &title,
    const std::optional<std::string> &parentTab,
    const std::optional<TabPosition> &position) {
    auto &doc = state->current;
    if (session->ledger && session->ledger->tabs.contains(docId())) {
        const auto &ids = session->ledger->tabs[docId()];
        if (state->tabCounter < static_cast<int>(ids.size())) {
            std::string bound = ids[state->tabCounter++];
            for (auto it = state->hiddenTabs.begin(); it != state->hiddenTabs.end(); ++it) {
                if (it->tabId == bound) {
                    it->title = title;
                    model::TabModel reclaimed = std::move(*it);
                    state->hiddenTabs.erase(it);
                    reclaimed.parentTabId = parentTab
                                                ? std::make_optional(detail::tabResolveRef(doc, *parentTab).tabId)
                                                : std::nullopt;
                    detail::tabInsert(doc, std::move(reclaimed), position);
                    return TabHandle(session, state, bound);
                }
            }
            throw model::CoreError("internal", "A created tab was not present in the authoritative document readback.");
        }
    }

    std::optional<std::string> parentTabId;
    if (parentTab.has_value()) {
        parentTabId = detail::tabResolveRef(doc, *parentTab).tabId;
    }

    state->tabCounter++;
    std::string tabId = "new_tab_" + std::to_string(state->tabCounter);
    model::TabModel tab{
        .tabId = tabId,
        .title = title,
        .isNew = true,
        .parentTabId = parentTabId,
        .iconEmoji = std::nullopt,
        .blocks = {
            model::paragraphEmptyCreate(session->keys, {}, model::EditStamp{
                                                               .force = session->tabForce(session->tabIndex),
                                                               .tabIndex = session->tabIndex,
                                                           }),
        },
        .segments = {},
        .documentStyle = doc.tabs.empty() ? model::make_json_object() : doc.tabs[0].documentStyle,
        .leadingSectionStyle = doc.tabs.empty() ? model::make_json_object() : doc.tabs[0].leadingSectionStyle,
        .namedStyles = doc.tabs.empty() ? model::make_json_object() : doc.tabs[0].namedStyles,
        .headersFooters = model::make_json_object(),
        .dropdownDefinitions = {},
        .footnotes = {},
        .inlineObjects = {},
        .lists = {},
        .namedRanges = {},
        .positionedObjects = {},
        .objectIds = std::nullopt,
        .suggestedDocumentStyleChanges = std::nullopt,
        .suggestedNamedStylesChanges = std::nullopt,
    };

    detail::tabInsert(doc, std::move(tab), position);
    return TabHandle(session, state, tabId);
}

} // namespace gdocsmith::engine
