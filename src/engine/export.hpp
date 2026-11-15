#pragma once

#include "emulator/google_docs_emulator.hpp"
#include "engine/dropdowns.hpp"
#include "engine/artifacts.hpp"
#include "engine/load.hpp"
#include "engine/named_styles.hpp"
#include "lens/document.hpp"
#include "lens/project.hpp"
#include "lens/render.hpp"
#include "lens/suggestion_projection.hpp"
#include "model/document.hpp"
#include "model/errors.hpp"
#include "model/object_ids.hpp"
#include "model/types.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gdocsmith::engine {

struct FileExportResult {
    std::optional<int> commentsLine;
    std::string exportPath;
    std::optional<std::unordered_map<std::string, std::string>> images;
    std::optional<std::string> imagesDir;
    std::optional<int> suggestionsLine;
    std::vector<std::string> warnings;

    bool operator==(const FileExportResult&) const = default;
};

struct ExportOptions {
    bool commentsIncludeResolved{false};
    bool imagesDownload{false};
    std::optional<std::vector<std::string>> imageIds;
    std::optional<std::string> sourceVersion{std::string(lens::GDOCSMITH_NODE_VERSION)};
    std::optional<std::string> apiUser;

    bool operator==(const ExportOptions&) const = default;
};

struct ExportWritten : FileExportResult {
    std::string folderPath;
    std::string markdown;

    bool operator==(const ExportWritten&) const = default;
};

struct DownloadedImage {
    std::string bytes;
    std::string extension;
};

struct FileServices {
    std::string apiUser;
    std::string root;
    std::function<DownloadedImage(const std::string&)> imageDownload;
};

/** Accepts only a document ID or canonical Google Docs URL. */
inline std::string documentId(std::string_view reference) {
    static const std::regex id_re("^[A-Za-z0-9_-]+$");
    if (std::regex_match(reference.begin(), reference.end(), id_re)) {
        return std::string(reference);
    }
    static const std::regex url_re(R"(^https://docs\.google\.com/document/d/([A-Za-z0-9_-]+)(?:/.*)?$)");
    std::cmatch match;
    std::string ref_str(reference);
    if (std::regex_match(ref_str.c_str(), match, url_re)) {
        return match[1].str();
    }
    throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX),
                          "doc must be a document ID or https://docs.google.com/document/d/... URL");
}

/** A thread's replies as comment blocks show them: not deleted, with text. */
inline std::vector<DriveCommentReply> commentReplies(const DriveComment& comment) {
    std::vector<DriveCommentReply> result;
    for (const auto& rep : comment.replies) {
        if (!rep.deleted.value_or(false) && rep.content.has_value()) {
            size_t first = rep.content->find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                result.push_back(rep);
            }
        }
    }
    return result;
}

/**
 * The comment blocks of a document: unresolved threads (resolved ones too when asked) in document
 * order of their quote's first occurrence; threads with no located quote come last.
 */
inline std::vector<lens::CommentBlock> commentBlocksBuild(
    const model::DocModel& model,
    const std::vector<DriveComment>& comments,
    bool includeResolved,
    std::optional<std::string> plainText = std::nullopt
) {
    auto anchors = commentAnchorsMatch(model, comments, plainText);
    std::unordered_map<std::string, const CommentAnchor*> anchor_map;
    for (const auto& a : anchors) {
        anchor_map[a.commentId] = &a;
    }

    std::unordered_map<std::string, int> tab_order;
    for (size_t i = 0; i < model.tabs.size(); i++) {
        tab_order[model.tabs[i].tabId] = static_cast<int>(i);
    }

    struct Item {
        lens::CommentBlock block;
        int tabIndex{1000000};
        int rangeStart{1000000};
        size_t originalOrder{0};
    };

    std::vector<Item> items;

    for (size_t order = 0; order < comments.size(); order++) {
        const auto& comment = comments[order];
        if (comment.deleted.value_or(false)) continue;
        if (!includeResolved && comment.resolved.value_or(false)) continue;

        lens::CommentBlock cb;
        cb.id = comment.id;
        cb.text = comment.content.value_or("");
        cb.resolved = comment.resolved.value_or(false);
        if (comment.author) {
            cb.author = comment.author->displayName;
            cb.mine = comment.author->me.value_or(false);
        }

        auto it = anchor_map.find(comment.id);
        const CommentAnchor* anchor = (it != anchor_map.end()) ? it->second : nullptr;

        int t_idx = 1000000;
        int r_start = 1000000;

        if (comment.quotedFileContent) {
            cb.quote = comment.quotedFileContent->value;
            if (!anchor || anchor->occurrences.empty()) {
                cb.orphaned = true;
            }
        } else if (comment.quotedText) {
            cb.quote = *comment.quotedText;
            if (!anchor || anchor->occurrences.empty()) {
                cb.orphaned = true;
            }
        }

        if (anchor) {
            cb.ambiguous = anchor->ambiguous;
            if (!anchor->occurrences.empty()) {
                const auto& first = anchor->occurrences[0];
                auto t_it = tab_order.find(first.tabId);
                if (t_it != tab_order.end()) t_idx = t_it->second;
                r_start = first.range.start;
            }
        }

        for (const auto& rep : commentReplies(comment)) {
            lens::CommentReply cr;
            cr.id = rep.id;
            cr.text = rep.content.value_or("");
            if (rep.author) {
                cr.author = rep.author->displayName;
                cr.mine = rep.author->me.value_or(false);
            }
            cb.replies.push_back(std::move(cr));
        }

        items.push_back(Item{
            .block = std::move(cb),
            .tabIndex = t_idx,
            .rangeStart = r_start,
            .originalOrder = order,
        });
    }

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.tabIndex != b.tabIndex) return a.tabIndex < b.tabIndex;
        if (a.rangeStart != b.rangeStart) return a.rangeStart < b.rangeStart;
        return a.originalOrder < b.originalOrder;
    });

    std::vector<lens::CommentBlock> result;
    result.reserve(items.size());
    for (auto& item : items) {
        result.push_back(std::move(item.block));
    }
    return result;
}

inline slimr::Json exportStyleObject(slimr::Json object) {
    if (object.is_object()) {
        auto& fields = object.get<slimr::Json::object_t>();
        fields.erase("contentUri");
        fields.erase("sourceUri");
        for (auto& [key, value] : fields) { (void)key; value = exportStyleObject(std::move(value)); }
    } else if (object.is_array()) {
        for (auto& value : object.get<slimr::Json::array_t>()) value = exportStyleObject(std::move(value));
    }
    return object;
}

inline slimr::Json exportStyleSnapshot(const model::TabModel& tab) {
    auto snapshot = slimr::json_obj();
    snapshot["documentStyle"] = tab.documentStyle;
    snapshot["leadingSectionStyle"] = tab.leadingSectionStyle;
    snapshot["namedStyles"] = tab.namedStyles;
    snapshot["headersFooters"] = tab.headersFooters;
    for (const auto& paragraph : model::paragraphsDeep(tab.blocks)) {
        auto& entry = snapshot["paragraphs"][paragraph.key];
        entry["style"] = paragraph.style;
        entry["newlineStyle"] = paragraph.newline.style;
        entry["inlines"] = slimr::json_arr();
        for (const auto& item : paragraph.inlines) {
            auto style = slimr::json_obj();
            style["style"] = item.style;
            if (item.raw) style["atomRaw"] = *item.raw;
            lens::json::push_back(entry["inlines"], std::move(style));
        }
    }
    auto collect = [&](auto&& self, const std::vector<model::Block>& blocks) -> void {
        for (const auto& block : blocks) {
            if (block.is_section_break()) snapshot["sections"][block.key] = block.as_section_break().sectionStyle;
            if (block.is_toc()) snapshot["tablesOfContents"][block.key] = block.as_toc().raw;
            if (!block.is_table()) continue;
            const auto& table = block.as_table();
            for (const auto& column : table.columns) snapshot["columns"][column.key] = column.props;
            for (const auto& row : table.rows) {
                snapshot["rows"][row.key] = row.style;
                for (const auto& cell : row.cells) {
                    snapshot["cells"][cell.key] = cell.style;
                    self(self, cell.blocks);
                }
            }
        }
    };
    collect(collect, tab.blocks);
    for (const auto& [id, value] : tab.inlineObjects) snapshot["inlineObjects"][id] = exportStyleObject(value);
    for (const auto& [id, value] : tab.positionedObjects) snapshot["positionedObjects"][id] = exportStyleObject(value);
    for (const auto& [id, value] : tab.dropdownDefinitions) snapshot["dropdownDefinitions"][id] = value;
    return exportStyleObject(std::move(snapshot));
}

inline void exportTabSettings(lens::DocumentFileTab& output, const model::TabModel& tab) {
    const auto& style = tab.documentStyle;
    auto& attrs = output.readonlyAttributes;
    if (tab.iconEmoji) attrs.values.emplace_back("emoji", *tab.iconEmoji);
    const bool pageful = style.contains("documentFormat") && style["documentFormat"].contains("documentMode") &&
        style["documentFormat"]["documentMode"] == slimr::Json("PAGES");
    if (pageful) attrs.flags.push_back("pageful");
    const auto pageSettings = lens::sectionSettingsOf(style, style);
    auto section = lens::sectionSettingsAttrs(pageSettings);
    for (const auto& pair : section.values) {
        if ((pair.first == "margins" && pageful && pair.second != "72,72,72,72") ||
            (pair.first == "pageNumberStart" && pair.second != "1")) {
            attrs.values.push_back(pair);
        }
    }
    if ((style.contains("useFirstPageHeaderFooter") && style["useFirstPageHeaderFooter"] == slimr::Json(true)) ||
        lens::sectionSettingsOf(tab.leadingSectionStyle, style).firstPage) attrs.flags.push_back("firstPage");
    if (style.contains("useEvenPageHeaderFooter") && style["useEvenPageHeaderFooter"] == slimr::Json(true)) {
        attrs.flags.push_back("oddEven");
    }
    if (pageful && (style.contains("pageSize") || style.contains("documentSize"))) {
        const auto& size = style.contains("pageSize") ? style["pageSize"] : style["documentSize"];
        if (size.contains("width") && size.contains("height")) {
            const double width = lens::dimensionPt(size["width"]), height = lens::dimensionPt(size["height"]);
            if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) {
                throw model::CoreError("unsupportedSyntax", "Invalid page dimensions.");
            }
            const double portraitWidth = std::min(width, height), portraitHeight = std::max(width, height);
            std::string name;
            const auto near = [](double a, double b) { return std::abs(a - b) <= 0.01; };
            const double mm = 72.0 / 25.4;
            for (const auto& [candidate, points] : std::map<std::string, std::pair<double, double>>{
                     {"A3", {297 * mm, 420 * mm}}, {"A4", {210 * mm, 297 * mm}}, {"A5", {148 * mm, 210 * mm}},
                     {"LETTER", {612, 792}}, {"LEGAL", {612, 1008}}, {"TABLOID", {792, 1224}}}) {
                if (near(portraitWidth, points.first) && near(portraitHeight, points.second)) { name = candidate; break; }
            }
            if (!name.empty()) {
                if (name != "LETTER") attrs.values.emplace_back("size", name);
                if (width > height) attrs.flags.push_back("landscape");
            } else attrs.values.emplace_back("size", std::format("{}x{}", width, height));
        }
    }
    for (const auto& [field, attribute] : {std::pair{"marginHeader", "headerMargin"}, {"marginFooter", "footerMargin"}}) {
        if (style.contains(field)) {
            const double pt = lens::dimensionPt(style[field]);
            if (pt != 36.0) attrs.values.emplace_back(attribute, std::format("{}", pt));
        }
    }
    if (style.contains("background") && style["background"].contains("color")) {
        const auto color = lens::colorHexFromOptional(style["background"]["color"]);
        if (color && *color != "#FFFFFF") attrs.values.emplace_back("background", *color);
    }
}

/** Pure model-to-file projection shared by export and pre-send import validation. */
inline bool footnoteBodyBlank(const model::TabModel& tab) {
    return std::all_of(tab.blocks.begin(), tab.blocks.end(), [](const auto& block) {
        return block.is_paragraph() && std::all_of(block.as_paragraph().inlines.begin(),
            block.as_paragraph().inlines.end(), [](const auto& item) {
                return item.is_text() && item.text.find_first_not_of(" \t\r\n") == std::string::npos;
            });
    });
}

inline lens::DocumentFile exportDocumentFile(
    const model::DocModel& model,
    const std::vector<DriveComment>& comments = {},
    const ExportOptions& options = {},
    std::optional<std::string> plainText = std::nullopt
) {
    lens::DocumentFile docFile;
    docFile.frontmatter.doc = model.docId;
    docFile.frontmatter.revision = model.revisionId;
    docFile.frontmatter.title = model.title;
    if (options.apiUser && !options.apiUser->empty()) docFile.frontmatter.apiUser = options.apiUser;
    docFile.frontmatter.gdocsmith = lens::GDOCSMITH_VERSION;
    docFile.frontmatter.sourceVersion = options.sourceVersion;

    auto baseline = slimr::json_obj();
    for (const auto& tab : model.tabs) {
        lens::DocumentFileTab dft;
        dft.id = tab.tabId;
        dft.title = tab.title;
        dft.parent = tab.parentTabId;
        auto projection = lens::projectionBuild(tab, &model);
        if (options.sourceVersion) {
            if (projection.blocks.empty() || !projection.blocks.front().token ||
                projection.blocks.front().token->kind != "section" ||
                projection.blocks.front().token->attrs.get_value("id") != std::optional<std::string>{"s1"}) {
                lens::ProjBlock opening;
                opening.kind = "token";
                opening.token = lens::TokenRef{.kind = "section"};
                opening.token->attrs.values.emplace_back("id", "s1");
                projection.blocks.insert(projection.blocks.begin(), std::move(opening));
            }
            auto& attrs = projection.blocks.front().token->attrs;
            std::erase_if(attrs.flags, [](const auto& flag) { return flag != "columnSeparator" && flag != "rtl"; });
            std::erase_if(attrs.values, [](const auto& entry) {
                return entry.first != "id" && entry.first != "columns" && entry.first != "columnGap";
            });
        }
        dft.markdown = lens::markdownRender(projection);
        docFile.frontmatter.paragraphStyles.insert(projection.paragraphStyles.begin(), projection.paragraphStyles.end());
        docFile.frontmatter.cellStyles.insert(projection.cellStyles.begin(), projection.cellStyles.end());
        exportTabSettings(dft, tab);
        const auto dropdowns = dropdownListsExport(tab);
        if (!dropdowns.get<slimr::Json::object_t>().empty()) docFile.frontmatter.dropdowns[tab.tabId] = dropdowns;
        baseline["tabs"][tab.tabId] = exportStyleSnapshot(tab);
        static const auto defaultNamedStyles = []() {
            auto blank = model::docModelParse(emulator::blank_doc_template("", ""), {});
            return namedStyleSetOf(blank.tabs.at(0));
        }();
        const auto currentNamedStyles = namedStyleSetOf(tab);
        auto diffNamedStyles = slimr::json_obj();
        for (const auto& [type, currentDecl] : currentNamedStyles.get<slimr::Json::object_t>()) {
            const auto defaultDecl = defaultNamedStyles.contains(type) ? defaultNamedStyles[type] : slimr::json_obj();
            auto diffDecl = slimr::json_obj();
            std::set<std::string> fields;
            for (const auto& [field, val] : currentDecl.get<slimr::Json::object_t>()) {
                (void)val;
                fields.insert(field);
            }
            if (defaultDecl.is_object()) {
                for (const auto& [field, val] : defaultDecl.get<slimr::Json::object_t>()) {
                    (void)val;
                    fields.insert(field);
                }
            }
            for (const auto& field : fields) {
                const bool inCur = currentDecl.contains(field);
                const bool inDef = defaultDecl.is_object() && defaultDecl.contains(field);
                if (inCur && inDef && currentDecl[field] == defaultDecl[field]) continue;
                diffDecl[field] = inCur ? currentDecl[field] : slimr::Json(nullptr);
            }
            if (!diffDecl.get<slimr::Json::object_t>().empty()) {
                diffNamedStyles[type] = std::move(diffDecl);
            }
        }
        if (!diffNamedStyles.get<slimr::Json::object_t>().empty()) {
            std::optional<std::string> existingName;
            for (const auto& [key, val] : docFile.frontmatter.namedStyleSets.get<slimr::Json::object_t>()) {
                if (val == diffNamedStyles) {
                    existingName = key;
                    break;
                }
            }
            if (existingName) {
                dft.readonlyAttributes.values.emplace_back("namedStyleSet", *existingName);
            } else {
                const auto name = std::to_string(docFile.frontmatter.namedStyleSets.get<slimr::Json::object_t>().size() + 1);
                docFile.frontmatter.namedStyleSets[name] = std::move(diffNamedStyles);
                dft.readonlyAttributes.values.emplace_back("namedStyleSet", name);
            }
        }
        const auto labels = lens::projectionFootnotes(tab);
        for (const auto& segment : tab.segments) {
            if (!segment.tab) throw model::CoreError("unsupportedSyntax", "Cannot export an unloaded document segment.");
            auto content = lens::projectionBuild(*segment.tab);
            docFile.frontmatter.paragraphStyles.insert(content.paragraphStyles.begin(), content.paragraphStyles.end());
            docFile.frontmatter.cellStyles.insert(content.cellStyles.begin(), content.cellStyles.end());
            const auto markdown = lens::markdownRender(content);
            baseline["segments"][tab.tabId][segment.id] = exportStyleSnapshot(*segment.tab);
            if (segment.kind == "footnote") {
                dft.footnotes.push_back({labels.at(segment.id), footnoteBodyBlank(*segment.tab) ? "" : markdown});
            } else if (segment.kind == "header" || segment.kind == "footer") {
                dft.segments.push_back({segment.kind, segment.type, segment.section, markdown, false});
            } else throw model::CoreError("unsupportedSyntax", "Unsupported document segment kind.");
        }
        docFile.tabs.push_back(std::move(dft));
    }
    if (!options.sourceVersion) docFile.frontmatter.styleBaseline = std::move(baseline);

    docFile.comments = commentBlocksBuild(model, comments, options.commentsIncludeResolved, plainText);
    docFile.suggestions = lens::suggestionBlocksBuild(model);
    return docFile;
}

/** Writes an already fetched snapshot without introducing an extra cloud read. */
inline ExportWritten exportModel(
    const model::DocModel& model,
    const std::vector<DriveComment>& comments,
    const FileServices& services,
    const ExportOptions& options = {},
    std::optional<std::string> plainText = std::nullopt
) {
    auto opts = options;
    if (!services.apiUser.empty()) opts.apiUser = services.apiUser;
    auto docFile = exportDocumentFile(model, comments, opts, plainText);
    std::string markdown = lens::documentFileExport(docFile);

    std::string rootDir = services.root.empty() ? artifactRoot() : services.root;
    auto folder = exportFolderWrite(rootDir, model.docId, model.revisionId.value_or(""), markdown);

    std::vector<std::string> warnings;
    warnings.insert(warnings.end(), folder.warnings.begin(), folder.warnings.end());
    if (!model.revisionId.has_value() || model.revisionId->empty()) {
        warnings.push_back("This export has no writable revision; it is read-only for this document.");
    }
    ExportWritten written;
    written.exportPath = folder.exportFilePath;
    written.folderPath = std::filesystem::path(folder.exportFilePath).parent_path().string();
    written.markdown = markdown;
    if (options.imagesDownload) {
        if (!services.imageDownload) {
            throw model::CoreError("operationFailed", "Image downloads require a configured image downloader.");
        }
        std::map<std::string, std::string> sources;
        for (const auto& tab : model.tabs) {
            auto collect = [&](const model::TabModel& source) {
                for (const auto& [id, object] : source.inlineObjects) {
                    if (!object.contains("inlineObjectProperties")) continue;
                    const auto& properties = object["inlineObjectProperties"];
                    if (!properties.contains("embeddedObject")) continue;
                    const auto& embedded = properties["embeddedObject"];
                    if (!embedded.contains("imageProperties")) continue;
                    const auto& image = embedded["imageProperties"];
                    if (image.contains("contentUri") && image["contentUri"].is_string()) {
                        sources[id] = image["contentUri"].get<std::string>();
                    }
                }
            };
            collect(tab);
            for (const auto& segment : tab.segments) {
                if (segment.tab) collect(*segment.tab);
            }
        }
        std::vector<std::string> selected;
        if (options.imageIds) {
            selected = *options.imageIds;
        } else {
            for (const auto& [id, _] : sources) selected.push_back(id);
        }
        for (const auto& id : selected) {
            if (!sources.contains(id)) {
                throw model::CoreError("unsupportedSyntax", "Requested image ID was not found: " + id);
            }
        }
        written.images.emplace();
        if (!selected.empty()) {
            const auto directory = std::filesystem::path(written.folderPath) / "images";
            std::filesystem::create_directories(directory);
            struct stat directoryStat{};
            if (lstat(directory.c_str(), &directoryStat) != 0 || !S_ISDIR(directoryStat.st_mode) ||
                directoryStat.st_uid != getuid()) {
                throw model::CoreError("operationFailed", "Image output must be a directory owned by this user, not a symlink.");
            }
            std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
            written.imagesDir = directory.string();
            for (const auto& id : selected) {
                if (written.images->contains(id)) continue;
                auto image = services.imageDownload(sources.at(id));
                if (image.bytes.empty() ||
                    (image.extension != "png" && image.extension != "jpeg" && image.extension != "gif")) {
                    throw model::CoreError("operationFailed", "Image downloader returned an unsupported image for " + id);
                }
                const auto path = directory / (detail::sha256_hex(id) + "." + image.extension);
                detail::atomic_write(path.string(), image.bytes);
                written.images->emplace(id, path.string());
            }
        }
    }
    written.warnings = std::move(warnings);

    std::istringstream ss{markdown};
    std::string line;
    int line_num = 1;
    while (std::getline(ss, line)) {
        if ((line.starts_with("::comment{") || line.starts_with(":::comment{")) && !written.commentsLine) {
            written.commentsLine = line_num;
        } else if ((line.starts_with("::suggestion{") || line.starts_with(":::suggestion{")) && !written.suggestionsLine) {
            written.suggestionsLine = line_num;
        }
        line_num++;
    }

    return written;
}

} // namespace gdocsmith::engine
