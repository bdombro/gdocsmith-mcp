#pragma once

#include "directives.hpp"
#include "model/object_ids.hpp"
#include "model/raw_json.hpp"
#include "model/types.hpp"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith::lens {

inline constexpr std::string_view IMAGE_UPLOAD_SCHEME = "gdocsmith-upload:";
inline constexpr std::string_view IMAGE_DOC_SCHEME = "gdoc:";

inline std::string imageUriResolve(std::string_view href, const std::optional<std::string> &baseDir) {
    std::string lower(href);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lower.starts_with("https://")) {
        return std::string(href);
    }
    if (lower.starts_with("http://")) {
        return std::string(IMAGE_UPLOAD_SCHEME) + std::string(href);
    }

    auto decode = [](std::string_view input, bool fileUrl) {
        std::string decoded;
        auto digit = [](char ch) -> int {
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            return -1;
        };
        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] != '%') {
                decoded += input[i];
                continue;
            }
            if (i + 2 >= input.size() || digit(input[i + 1]) < 0 || digit(input[i + 2]) < 0) {
                throw CoreError("unsupportedSyntax", "image path has invalid percent encoding");
            }
            const auto ch = static_cast<char>(digit(input[i + 1]) * 16 + digit(input[i + 2]));
            if (ch == '\0' || (fileUrl && ch == '/')) {
                throw CoreError("unsupportedSyntax", "image file URL contains an encoded path separator or null");
            }
            decoded += ch;
            i += 2;
        }
        return decoded;
    };
    std::string path(href);
    if (lower.starts_with("file://")) {
        auto filePath = href.substr(7);
        if (filePath.starts_with("localhost/"))
            filePath.remove_prefix(9);
        if (!filePath.starts_with("/"))
            throw CoreError("unsupportedSyntax", "image file URL must name a local absolute path");
        const auto end = filePath.find_first_of("?#");
        path = decode(filePath.substr(0, end), true);
    } else if (path == "~" || path.starts_with("~/")) {
        const char *home = std::getenv("HOME");
        std::string home_str = home ? home : "";
        path = home_str + path.substr(1);
    } else {
        size_t colon = path.find(':');
        if (colon != std::string::npos && colon > 0 && std::isalpha(static_cast<unsigned char>(path[0]))) {
            bool is_scheme = true;
            for (size_t i = 1; i < colon; i++) {
                char c = path[i];
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '.' && c != '-') {
                    is_scheme = false;
                    break;
                }
            }
            if (is_scheme) {
                throw CoreError("unsupportedSyntax", "image \"" + std::string(href) + "\" is neither a URL nor a local file path");
            }
        }
    }

    std::filesystem::path p(path);
    if (!p.is_absolute()) {
        if (!baseDir.has_value()) {
            throw CoreError("unsupportedSyntax", "a relative image path (\"" + std::string(href) + "\") needs an imported file to resolve against");
        }
        p = (std::filesystem::path(*baseDir) / decode(path, false)).lexically_normal();
    }
    return std::string(IMAGE_UPLOAD_SCHEME) + p.string();
}

struct ImageSource {
    model::AtomCreate create;
    std::optional<std::string> label;
    std::vector<std::string> lost;
    struct SourceLoc {
        std::string docId;
        std::string key;
        std::string tabId;
        bool operator==(const SourceLoc &other) const = default;
    } source;
    bool sourceUriFallback{false};

    bool operator==(const ImageSource &other) const = default;
};

inline std::optional<ImageSource> imageSourceOf(const model::DocModel &doc, const model::TabModel &tab,
                                                const model::Atom &atom) {
    if (atom.type != "image")
        return std::nullopt;
    const auto id = model::googleIdOf(atom);
    if (!id)
        return std::nullopt;
    const auto found = tab.inlineObjects.find(*id);
    if (found == tab.inlineObjects.end())
        return std::nullopt;
    const auto &object = found->second;
    if (!object.contains("inlineObjectProperties"))
        return std::nullopt;
    const auto &props = object["inlineObjectProperties"];
    if (!props.contains("embeddedObject"))
        return std::nullopt;
    const auto &embedded = props["embeddedObject"];
    if (embedded.contains("embeddedDrawingProperties") || embedded.contains("linkedContentReference") ||
        !embedded.contains("imageProperties"))
        return std::nullopt;
    const auto &image = embedded["imageProperties"];
    if (!image.contains("contentUri") || !image["contentUri"].is_string())
        return std::nullopt;
    const auto contentUri = image["contentUri"].get<std::string>();
    if (!contentUri.starts_with("https://") || contentUri.size() <= 8)
        return std::nullopt;
    ImageSource result;
    result.create.type = "image";
    result.create.uri = contentUri;
    auto size = [&](const char *side) -> std::optional<double> {
        if (!embedded.contains("size") || !embedded["size"].contains(side))
            return std::nullopt;
        const auto &dimension = embedded["size"][side];
        if (!dimension.contains("magnitude") || !dimension["magnitude"].is_number())
            return std::nullopt;
        return dimension["magnitude"].as<double>();
    };
    result.create.heightPt = size("height");
    result.create.widthPt = size("width");
    auto active = [](const slimr::Json &value) {
        return value.is_number() && value.as<double>() != 0.0;
    };
    if (image.contains("cropProperties")) {
        const auto &crop = image["cropProperties"];
        for (const auto *side : {"offsetLeft", "offsetRight", "offsetTop", "offsetBottom"}) {
            if (crop.contains(side) && active(crop[side])) {
                result.lost.emplace_back("crop");
                break;
            }
        }
        if (crop.contains("angle") && active(crop["angle"]))
            result.lost.emplace_back("rotation");
    }
    if (image.contains("angle") && active(image["angle"]) &&
        std::find(result.lost.begin(), result.lost.end(), "rotation") == result.lost.end()) {
        result.lost.emplace_back("rotation");
    }
    for (const auto *adjustment : {"brightness", "contrast", "transparency"}) {
        if (image.contains(adjustment) && active(image[adjustment]))
            result.lost.emplace_back(adjustment);
    }
    if (embedded.contains("embeddedObjectBorder") &&
        embedded["embeddedObjectBorder"].contains("propertyState") &&
        embedded["embeddedObjectBorder"]["propertyState"] == slimr::Json("RENDERED")) {
        result.lost.emplace_back("border");
    }
    for (const auto *field : {"title", "description"}) {
        if (!embedded.contains(field) || !embedded[field].is_string())
            continue;
        const auto text = embedded[field].get<std::string>();
        if (!text.empty()) {
            if (!result.label)
                result.label = text;
            if (std::find(result.lost.begin(), result.lost.end(), "alt text") == result.lost.end()) {
                result.lost.emplace_back("alt text");
            }
        }
    }
    result.source = {.docId = doc.docId, .key = atom.key, .tabId = tab.tabId};
    return result;
}

inline std::optional<ImageSource> imageRecreationSourceOf(const model::DocModel &doc, const model::TabModel &tab,
                                                          const model::Atom &atom) {
    if (const auto source = imageSourceOf(doc, tab, atom))
        return source;
    if (atom.type != "image")
        return std::nullopt;
    const auto id = model::googleIdOf(atom);
    if (!id)
        return std::nullopt;
    const auto found = tab.inlineObjects.find(*id);
    if (found == tab.inlineObjects.end() || !found->second.contains("inlineObjectProperties"))
        return std::nullopt;
    const auto &props = found->second["inlineObjectProperties"];
    if (!props.contains("embeddedObject"))
        return std::nullopt;
    const auto &embedded = props["embeddedObject"];
    if (embedded.contains("embeddedDrawingProperties") || embedded.contains("linkedContentReference") ||
        !embedded.contains("imageProperties"))
        return std::nullopt;
    const auto &image = embedded["imageProperties"];
    if (image.contains("contentUri") && image["contentUri"].is_string()) {
        throw CoreError("invalidAtom", "an image's URI must be a nonempty HTTPS URI");
    }
    if (!image.contains("sourceUri") || !image["sourceUri"].is_string())
        return std::nullopt;
    const auto uri = image["sourceUri"].get<std::string>();
    if (!uri.starts_with("https://") || uri.size() <= 8)
        return std::nullopt;
    ImageSource result;
    result.create = {.type = "image", .uri = uri};
    result.source = {.docId = doc.docId, .key = atom.key, .tabId = tab.tabId};
    result.sourceUriFallback = true;
    return result;
}

using ImageResolver = std::function<std::optional<ImageSource>(const std::string &)>;

inline ImageResolver imageResolverBuild(const std::vector<model::DocModel> &documents) {
    std::map<std::string, std::optional<ImageSource>> sources;
    auto add = [&](const std::string &id, const ImageSource &source) {
        if (sources.contains(id))
            sources[id] = std::nullopt;
        else
            sources[id] = source;
    };
    for (size_t index = 0; index < documents.size(); ++index) {
        const auto &document = documents[index];
        for (const auto &tab : document.tabs) {
            for (const auto &paragraph : model::paragraphsDeep(tab.blocks)) {
                for (const auto &item : paragraph.inlines) {
                    if (!item.is_atom() || item.type != "image")
                        continue;
                    const auto source = imageSourceOf(document, tab, item.to_atom());
                    if (!source)
                        continue;
                    auto id = model::googleIdOf(item.to_atom()).value_or(item.key);
                    if (tab.objectIds) {
                        const auto found = tab.objectIds->find(item.key);
                        if (found != tab.objectIds->end())
                            id = found->second;
                    }
                    add(std::string(IMAGE_DOC_SCHEME) + document.docId + "/" + id, *source);
                    if (index == 0)
                        add(id, *source);
                }
            }
        }
    }
    return [sources = std::move(sources)](const std::string &id) -> std::optional<ImageSource> {
        const auto found = sources.find(id);
        return found == sources.end() ? std::nullopt : found->second;
    };
}

inline std::vector<std::string> imageRecreationLosses(const model::JsonObject &embedded) {
    std::vector<std::string> lost;
    auto nonzero = [&](auto &self, const slimr::Json &value) -> bool {
        if (value.is_number())
            return value.as<double>() != 0.0;
        if (value.is_boolean())
            return value.get<bool>();
        if (value.is_string())
            return !value.get<std::string>().empty();
        if (value.is_object()) {
            for (const auto &[_, child] : value.get<slimr::Json::object_t>()) {
                if (self(self, child))
                    return true;
            }
        }
        return false;
    };
    for (const auto &field : {"title", "description", "rotationAngle"}) {
        if (embedded.contains(field) && nonzero(nonzero, embedded[field]))
            lost.emplace_back(field);
    }
    if (embedded.contains("embeddedObjectBorder")) {
        const auto &border = embedded["embeddedObjectBorder"];
        const bool rendered = border.contains("propertyState") && border["propertyState"].is_string() &&
                              border["propertyState"].get<std::string>() == "RENDERED";
        if (rendered || (border.contains("width") && border["width"].contains("magnitude") &&
                         nonzero(nonzero, border["width"]["magnitude"]))) {
            lost.emplace_back("embeddedObjectBorder");
        }
    }
    if (embedded.contains("imageProperties") && embedded["imageProperties"].is_object()) {
        for (const auto &[field, value] : embedded["imageProperties"].get<slimr::Json::object_t>()) {
            if (field == "contentUri" || field == "sourceUri")
                continue;
            if (nonzero(nonzero, value))
                lost.push_back(field);
        }
    }
    return lost;
}

} // namespace gdocsmith::lens
