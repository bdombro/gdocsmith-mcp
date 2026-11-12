#pragma once

#include "emulator/google_docs_emulator.hpp"
#include "emulator/types.hpp"
#include "engine/guard.hpp"
#include "engine/load.hpp"
#include "lens/cell_styles.hpp"
#include "lens/paragraph_styles.hpp"
#include "lens/tokens.hpp"
#include "lens/parse.hpp"
#include "model/edit.hpp"
#include "model/errors.hpp"
#include "model/from_json.hpp"
#include "model/invariants.hpp"
#include "model/keys.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"

#include <algorithm>
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

    bool operator==(const Changed&) const = default;
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

    bool operator==(const TabInfo&) const = default;
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
    virtual TabHandle tab(const std::string& ref = "") = 0;
    virtual TabHandle tabCreate(const std::string& title,
                                const std::optional<std::string>& parentTab = std::nullopt,
                                const std::optional<TabPosition>& position = std::nullopt) = 0;
    virtual std::vector<TabInfo> tabs() = 0;
    virtual std::string title() = 0;
};

class TabHandle {
public:
    std::string tabId;
    CoreSession* session = nullptr;
    DocState* state = nullptr;

    TabHandle(CoreSession* s, DocState* st, std::string tid)
        : tabId(std::move(tid)), session(s), state(st) {}

    model::TabModel& model();
    Changed deleteTab();
    Changed move(const TabPosition& p);
    Changed pageSetupSet(const PageSetupPatch& p);
    Changed remove();
    Changed rename(const std::string& title);
    WriteReport writeMarkdown(const std::string& md);
};

namespace detail {

inline std::string trimLower(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    std::string out;
    out.reserve(s.size());
    for (char c : s) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

inline model::TabModel& tabResolveRef(model::DocModel& doc, const std::string& ref) {
    if (ref.empty()) {
        if (doc.tabs.size() == 1) return doc.tabs[0];
        std::string titles;
        for (const auto& t : doc.tabs) {
            if (!titles.empty()) titles += ", ";
            titles += "\"" + t.title + "\"";
        }
        throw model::CoreError(std::string(model::error_codes::TAB_REQUIRED),
                               "this document has " + std::to_string(doc.tabs.size()) + " tabs; name one: " + titles);
    }
    for (auto& t : doc.tabs) {
        if (t.tabId == ref) return t;
    }
    std::string wanted = trimLower(ref);
    std::vector<model::TabModel*> byTitle;
    for (auto& t : doc.tabs) {
        if (trimLower(t.title) == wanted) {
            byTitle.push_back(&t);
        }
    }
    if (byTitle.size() == 1) return *byTitle[0];
    if (byTitle.size() > 1) {
        throw model::CoreError(std::string(model::error_codes::TAB_REQUIRED),
                               std::to_string(byTitle.size()) + " tabs are titled \"" + ref + "\"; use a tab id");
    }
    std::string titles;
    for (const auto& t : doc.tabs) {
        if (!titles.empty()) titles += ", ";
        titles += "\"" + t.title + "\"";
    }
    throw model::CoreError(std::string(model::error_codes::TAB_NOT_FOUND),
                           "no tab \"" + ref + "\" (tabs: " + titles + ")");
}

inline void tabInsert(model::DocModel& doc, model::TabModel tab, const std::optional<TabPosition>& position) {
    std::vector<std::string> siblingIds;
    for (const auto& t : doc.tabs) {
        if (t.parentTabId == tab.parentTabId) siblingIds.push_back(t.tabId);
    }

    int index = static_cast<int>(siblingIds.size());
    if (position.has_value()) {
        if (position->index.has_value()) {
            index = std::max(0, std::min(*position->index, static_cast<int>(siblingIds.size())));
        } else if (position->afterTab.has_value() || position->beforeTab.has_value()) {
            std::string refName = position->afterTab.value_or(position->beforeTab.value_or(""));
            auto& refTab = tabResolveRef(doc, refName);
            auto it = std::find(siblingIds.begin(), siblingIds.end(), refTab.tabId);
            if (it == siblingIds.end()) {
                throw model::CoreError(std::string(model::error_codes::INVALID_PLACEMENT),
                                       "\"" + refTab.title + "\" isn't a sibling of that tab");
            }
            int at = static_cast<int>(std::distance(siblingIds.begin(), it));
            index = position->afterTab.has_value() ? at + 1 : at;
        }
    }

    auto subtreeEnd = [&](const model::TabModel& t) -> int {
        int end = 0;
        for (size_t i = 0; i < doc.tabs.size(); ++i) {
            if (doc.tabs[i].tabId == t.tabId) {
                end = static_cast<int>(i + 1);
                break;
            }
        }
        auto inSubtree = [&](auto& self, const model::TabModel& x) -> bool {
            if (!x.parentTabId.has_value()) return false;
            if (*x.parentTabId == t.tabId) return true;
            for (const auto& y : doc.tabs) {
                if (y.tabId == *x.parentTabId) return self(self, y);
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
        for (const auto& t : doc.tabs) {
            if (t.tabId == siblingIds.back()) {
                at = subtreeEnd(t);
                break;
            }
        }
    } else if (tab.parentTabId.has_value()) {
        for (const auto& t : doc.tabs) {
            if (t.tabId == *tab.parentTabId) {
                at = subtreeEnd(t);
                break;
            }
        }
    }
    doc.tabs.insert(doc.tabs.begin() + at, std::move(tab));
}

} // namespace detail

class DocHandleImpl : public DocHandle {
public:
    CoreSession* session = nullptr;
    DocState* state = nullptr;

    DocHandleImpl(CoreSession* s, DocState* st) : session(s), state(st) {}

    [[nodiscard]] std::optional<std::string> alias() const override { return state->alias; }
    [[nodiscard]] std::string docId() const override { return state->docId; }
    [[nodiscard]] bool isNew() const override { return state->isNew; }

    TabHandle tab(const std::string& ref = "") override;
    TabHandle tabCreate(const std::string& title,
                        const std::optional<std::string>& parentTab = std::nullopt,
                        const std::optional<TabPosition>& position = std::nullopt) override;
    std::vector<TabInfo> tabs() override {
        std::vector<TabInfo> res;
        for (const auto& t : state->current.tabs) {
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
    std::vector<GuardFinding> findings;
    Ledger* ledger = nullptr;

    // Optional client / drive callbacks
    std::function<slimr::Json(const std::string&)> getDocumentFn;
    std::function<std::vector<DriveComment>(const std::string&)> listCommentsFn;
    std::function<std::string(const std::string&)> textExportFn;

    CoreSession() = default;

    template <typename ClientType, typename DriveType>
    CoreSession(ClientType* client, DriveType* drive, Ledger* led = nullptr, std::vector<std::string> al = {})
        : allow(std::move(al)), ledger(led) {
        if (client) {
            getDocumentFn = [client](const std::string& id) { return client->getDocument(id); };
        }
        if (drive) {
            listCommentsFn = [drive](const std::string& id) { return drive->commentsList(id); };
            textExportFn = [drive](const std::string& id) { return drive->textExport(id); };
        }
    }

    std::shared_ptr<DocHandle> docOpen(const std::string& docId, const std::string& alias = "") {
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

        auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = docId, .keys = keys});
        auto current = original;

        std::set<std::string> createdTabSet;
        if (ledger && ledger->tabs.contains(docId)) {
            for (const auto& tid : ledger->tabs[docId]) {
                createdTabSet.insert(tid);
            }
        }

        std::vector<model::TabModel> hidden;
        std::vector<model::TabModel> kept;
        for (auto& t : current.tabs) {
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

    std::shared_ptr<DocHandle> docCreate(const std::string& alias, const std::string& title) {
        if (ledger && ledger->docs.contains(alias)) {
            std::string realId = ledger->docs[alias];
            auto handle = docOpen(realId, alias);
            docs[realId].createdAs = alias;
            return handle;
        }

        std::string docId = "new:" + alias;
        if (docs.contains(docId)) {
            throw model::CoreError(std::string(model::error_codes::INTERNAL), "alias \"" + alias + "\" is already used");
        }

        slimr::Json json = emulator::blank_doc_template(title, docId);
        auto original = model::docModelParse(json, model::DocModelParseOptions{.docId = docId, .keys = keys});
        original.isNew = true;
        auto current = original;

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

    std::vector<DriveComment> commentsFor(DocState& st) {
        if (st.isNew) return {};
        if (!st.comments.has_value()) {
            if (listCommentsFn) {
                st.comments = listCommentsFn(st.docId);
            } else {
                st.comments = std::vector<DriveComment>{};
            }
        }
        return *st.comments;
    }

    std::optional<std::string> exportTextFor(DocState& st) {
        if (!st.exportText.has_value() && !st.exportTextFailed.has_value()) {
            auto comms = commentsFor(st);
            bool hasQuotes = false;
            for (const auto& c : comms) {
                if (!c.deleted.value_or(false) && !c.quote().empty()) {
                    hasQuotes = true;
                    break;
                }
            }
            if (hasQuotes && textExportFn) {
                try {
                    st.exportText = textExportFn(st.docId);
                } catch (const std::exception& e) {
                    st.exportTextFailed = e.what();
                }
            }
        }
        return st.exportText;
    }

    std::vector<CommentAnchor> commentAnchorsFor(DocState& st) {
        if (st.isNew) return {};
        if (!st.commentAnchors.has_value()) {
            auto comms = commentsFor(st);
            auto text = exportTextFor(st);
            st.commentAnchors = commentAnchorsMatch(st.original, comms, text);
        }
        return *st.commentAnchors;
    }
};

inline model::TabModel& TabHandle::model() {
    for (auto& t : state->current.tabs) {
        if (t.tabId == tabId) return t;
    }
    throw model::CoreError(std::string(model::error_codes::TAB_NOT_FOUND),
                           "tab " + tabId + " no longer exists");
}

inline Changed TabHandle::deleteTab() {
    auto& doc = state->current;
    std::set<std::string> doomed = {tabId};
    for (bool grew = true; grew;) {
        grew = false;
        for (const auto& t : doc.tabs) {
            if (t.parentTabId.has_value() && doomed.contains(*t.parentTabId) && !doomed.contains(t.tabId)) {
                doomed.insert(t.tabId);
                grew = true;
            }
        }
    }
    std::vector<model::TabModel> kept;
    for (auto& t : doc.tabs) {
        if (!doomed.contains(t.tabId)) kept.push_back(std::move(t));
    }
    doc.tabs = std::move(kept);
    return Changed{.changed = true};
}

inline Changed TabHandle::move(const TabPosition& p) {
    auto& doc = state->current;
    auto beforeOrder = doc.tabs;
    auto tabM = std::move(model());
    std::vector<model::TabModel> remaining;
    for (auto& t : doc.tabs) {
        if (t.tabId != tabId) remaining.push_back(std::move(t));
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

inline Changed TabHandle::rename(const std::string& title) {
    auto& t = model();
    if (t.title == title) return Changed{.changed = false};
    t.title = title;
    return Changed{.changed = true};
}

inline Changed TabHandle::remove() {
    auto& t = model();
    t.blocks.clear();
    t.blocks.push_back(model::paragraphEmptyCreate(session->keys, {}, model::EditStamp{
        .force = session->tabForce(session->tabIndex),
        .tabIndex = session->tabIndex,
    }));
    return Changed{.changed = true};
}

inline Changed TabHandle::pageSetupSet(const PageSetupPatch& p) {
    auto& t = model();
    if (p.pageless.has_value()) {
        t.documentStyle["documentFormat"] = slimr::Json::object_t{
            {"documentMode", *p.pageless ? "PAGELESS" : "PAGES"}
        };
    }
    return Changed{.changed = true};
}

inline WriteReport TabHandle::writeMarkdown(const std::string& md) {
    auto& t = model();
    auto proj = lens::markdownParse(md);
    t.blocks.clear();

    for (const auto& b : proj.blocks) {
        if (b.kind == "paragraph" || b.kind == "heading" || b.kind == "listItem") {
            model::ParagraphBlock pb;
            pb.key = session->keys.next("p");
            if (b.kind == "heading" && b.headingLevel.has_value()) {
                pb.style["namedStyleType"] = "HEADING_" + std::to_string(*b.headingLevel);
            } else {
                pb.style["namedStyleType"] = "NORMAL_TEXT";
            }
            for (const auto& span : b.spans) {
                model::Inline inl;
                inl.kind = "text";
                inl.text = span.text;
                if (span.marks.bold) inl.style["bold"] = true;
                if (span.marks.italic) inl.style["italic"] = true;
                if (span.marks.underline) inl.style["underline"] = true;
                if (span.marks.strikethrough) inl.style["strikethrough"] = true;
                if (span.marks.link.has_value()) {
                    inl.style["link"] = slimr::Json::object_t{{"url", *span.marks.link}};
                }
                pb.inlines.push_back(std::move(inl));
            }
            t.blocks.push_back(model::Block(std::move(pb)));
        }
    }
    if (t.blocks.empty()) {
        t.blocks.push_back(model::paragraphEmptyCreate(session->keys, {}, model::EditStamp{
            .force = session->tabForce(session->tabIndex),
            .tabIndex = session->tabIndex,
        }));
    }
    return WriteReport{.changed = true};
}

inline TabHandle DocHandleImpl::tab(const std::string& ref) {
    auto& resolved = detail::tabResolveRef(state->current, ref);
    return TabHandle(session, state, resolved.tabId);
}

inline TabHandle DocHandleImpl::tabCreate(
    const std::string& title,
    const std::optional<std::string>& parentTab,
    const std::optional<TabPosition>& position
) {
    auto& doc = state->current;
    if (session->ledger && session->ledger->tabs.contains(docId())) {
        const auto& ids = session->ledger->tabs[docId()];
        if (state->tabCounter < static_cast<int>(ids.size())) {
            std::string bound = ids[state->tabCounter++];
            for (auto it = state->hiddenTabs.begin(); it != state->hiddenTabs.end(); ++it) {
                if (it->tabId == bound) {
                    it->title = title;
                    model::TabModel reclaimed = std::move(*it);
                    state->hiddenTabs.erase(it);
                    detail::tabInsert(doc, std::move(reclaimed), position);
                    return TabHandle(session, state, bound);
                }
            }
            return TabHandle(session, state, bound);
        }
    }

    std::optional<std::string> parentTabId;
    if (parentTab.has_value()) {
        parentTabId = detail::tabResolveRef(doc, *parentTab).tabId;
    }

    state->tabCounter++;
    std::string tabId = "new:tab:" + std::to_string(state->tabCounter);
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
