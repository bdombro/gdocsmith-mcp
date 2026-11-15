#pragma once

#include "lens/dropdowns.hpp"
#include "lens/images.hpp"
#include "lens/tokens.hpp"
#include "model/edit.hpp"

#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <regex>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace gdocsmith::lens {

namespace atom_detail {

inline bool personEmailValid(const std::string &email) {
    static const std::regex pattern(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)");
    if (!std::regex_match(email, pattern))
        return false;
    for (const auto &cp : model::utf8CodePoints(email)) {
        if (cp == "\u00A0" || cp == "\u1680" || (cp >= "\u2000" && cp <= "\u200A") ||
            cp == "\u2028" || cp == "\u2029" || cp == "\u202F" || cp == "\u205F" ||
            cp == "\u3000" || cp == "\uFEFF")
            return false;
    }
    return true;
}

inline double zoneOffset(const std::string &name, std::chrono::sys_seconds moment, bool authored = false) {
    if (name == "UTC")
        return 0;
#ifdef __APPLE__
    struct Release {
        void operator()(CFTypeRef value) const { CFRelease(value); }
    };
    using String = std::unique_ptr<std::remove_pointer_t<CFStringRef>, Release>;
    using Zone = std::unique_ptr<std::remove_pointer_t<CFTimeZoneRef>, Release>;
    using Names = std::unique_ptr<std::remove_pointer_t<CFArrayRef>, Release>;
    String identifier(CFStringCreateWithCString(kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8));
    Names known(CFTimeZoneCopyKnownNames());
    const auto slash = name.find('/');
    const auto region = name.substr(0, slash);
    static const std::vector<std::string> regions = {
        "Africa", "America", "Antarctica", "Arctic", "Asia", "Atlantic", "Australia", "Europe", "Indian", "Pacific"};
    if (!identifier || !known || (authored && (slash == std::string::npos || std::find(regions.begin(), regions.end(), region) == regions.end())) ||
        !CFArrayContainsValue(known.get(), CFRangeMake(0, CFArrayGetCount(known.get())), identifier.get())) {
        throw model::CoreError("unsupportedSyntax", "date zone must be an IANA time zone");
    }
    Zone zone(CFTimeZoneCreateWithName(kCFAllocatorDefault, identifier.get(), false));
    if (!zone)
        throw model::CoreError("unsupportedSyntax", "date zone must be an IANA time zone");
    const auto unixSeconds = static_cast<double>(moment.time_since_epoch().count());
    return CFTimeZoneGetSecondsFromGMT(zone.get(), unixSeconds - kCFAbsoluteTimeIntervalSince1970);
#elif defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    try {
        return static_cast<double>(std::chrono::locate_zone(name)->get_info(moment).offset.count());
    } catch (const std::exception &) {
        throw model::CoreError("unsupportedSyntax", "date zone must be an IANA time zone");
    }
#else
    (void)moment;
    throw model::CoreError("unsupportedSyntax", "IANA date timezone conversion is unavailable on this platform");
#endif
}

inline std::chrono::sys_seconds wallTime(const std::string &date, const std::string &time) {
    const auto year = std::chrono::year(std::stoi(date.substr(0, 4)));
    const auto month = std::chrono::month(static_cast<unsigned>(std::stoi(date.substr(5, 2))));
    const auto day = std::chrono::day(static_cast<unsigned>(std::stoi(date.substr(8, 2))));
    return std::chrono::sys_days(year / month / day) +
           std::chrono::hours(std::stoi(time.substr(0, 2))) + std::chrono::minutes(std::stoi(time.substr(3, 2)));
}

inline std::string timestamp(std::chrono::sys_seconds moment) {
    const auto days = std::chrono::floor<std::chrono::days>(moment);
    const std::chrono::year_month_day date(days);
    const std::chrono::hh_mm_ss time(moment - days);
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", static_cast<int>(date.year()),
                       static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                       time.hours().count(), time.minutes().count(), time.seconds().count());
}

inline std::string zonedTimestamp(const std::string &date, const std::string &time, const std::string &zone) {
    const auto wall = wallTime(date, time);
    const auto first = wall - std::chrono::seconds(static_cast<long long>(zoneOffset(zone, wall, true)));
    return timestamp(wall - std::chrono::seconds(static_cast<long long>(zoneOffset(zone, first, true))));
}

} // namespace atom_detail

struct AtomPutOptions {
    std::optional<std::string> baseDir;
    bool allowLossyImageMove{false};
    std::function<std::optional<ImageSource>(const std::string &)> images;
    std::map<std::string, std::string> createdDropdownIds;
    // Attach the failing top-level block's source line to content-code problems
    bool locateBlockProblems{false};
};

struct AtomPutReport {
    std::vector<ImageSource> recreated;
    std::vector<std::string> notes;
};

inline TokenRef atomTokenProject(const model::Atom &atom, const model::TabModel &tab) {
    TokenRef token;
    token.kind = atom.type == "pageBreak" ? "pagebreak" : atom.type == "richLink" ? "richlink"
                                                                                  : atom.type;
    auto id = model::googleIdOf(atom).value_or(atom.key);
    if (tab.objectIds) {
        const auto found = tab.objectIds->find(atom.key);
        if (found != tab.objectIds->end())
            id = found->second;
    }
    token.attrs.values.emplace_back("id", id);
    const auto raw = atom.raw.value_or(model::make_json_object());
    auto properties = [&](const char *outer, const char *inner) {
        if (raw.contains(outer) && raw[outer].contains(inner))
            return raw[outer][inner];
        return model::make_json_object();
    };
    auto string = [](const slimr::Json &props, const char *field) -> std::optional<std::string> {
        if (props.contains(field) && props[field].is_string())
            return props[field].get<std::string>();
        return std::nullopt;
    };
    auto attr = [&](const char *name, const std::optional<std::string> &value) {
        if (value)
            token.attrs.values.emplace_back(name, *value);
    };
    if (atom.type == "person") {
        const auto props = properties("person", "personProperties");
        const auto email = atom.create ? atom.create->email : string(props, "email");
        attr("email", email);
        token.label = atom.create ? email : string(props, "name").has_value() ? string(props, "name")
                                                                              : email;
    } else if (atom.type == "richLink") {
        const auto props = properties("richLink", "richLinkProperties");
        attr("url", atom.create ? atom.create->uri : string(props, "uri"));
        token.label = atom.create ? atom.create->uri : string(props, "title");
    } else if (atom.type == "dropdown") {
        const auto props = properties("dropdown", "dropdownProperties");
        attr("list", atom.create ? atom.create->definitionId : string(props, "dropdownDefinitionId"));
        token.label = string(props, "displayValue");
    } else if (atom.type == "date") {
        const auto props = properties("dateElement", "dateElementProperties");
        const auto timestamp = atom.create ? atom.create->timestamp : string(props, "timestamp");
        const auto timeFormat = atom.create ? atom.create->timeFormat : string(props, "timeFormat");
        const auto zone = atom.create ? atom.create->timeZoneId : string(props, "timeZoneId");
        const auto format = atom.create ? atom.create->dateFormat : string(props, "dateFormat");
        token.label = atom.create ? timestamp : string(props, "displayText");
        if (timestamp && timestamp->size() >= 10) {
            auto wall = *timestamp;
            if (timeFormat && *timeFormat != "TIME_FORMAT_DISABLED" && zone && timestamp->size() >= 16) {
                const auto moment = atom_detail::wallTime(timestamp->substr(0, 10), timestamp->substr(11, 5));
                wall = atom_detail::timestamp(moment + std::chrono::seconds(static_cast<long long>(atom_detail::zoneOffset(*zone, moment))));
            }
            attr("value", wall.substr(0, 10));
            if (timeFormat && *timeFormat != "TIME_FORMAT_DISABLED" && wall.size() >= 16) {
                attr("time", wall.substr(11, 5));
                attr("zone", zone);
            }
        }
        if (format) {
            if (*format == "DATE_FORMAT_ISO8601")
                attr("format", "iso");
            else if (*format == "DATE_FORMAT_MONTH_DAY_FULL")
                attr("format", "long");
            else if (*format == "DATE_FORMAT_MONTH_DAY_ABBREVIATED")
                attr("format", "short");
        }
        const auto locale = atom.create ? atom.create->locale : string(props, "locale");
        if (locale != "en")
            attr("locale", locale);
    } else if (atom.type == "image") {
        if (atom.create)
            token.label = atom.create->alt;
        else if (const auto provider = model::googleIdOf(atom)) {
            const auto found = tab.inlineObjects.find(*provider);
            if (found != tab.inlineObjects.end() && found->second.contains("inlineObjectProperties") &&
                found->second["inlineObjectProperties"].contains("embeddedObject")) {
                const auto &embedded = found->second["inlineObjectProperties"]["embeddedObject"];
                token.label = string(embedded, "title").has_value() ? string(embedded, "title") : string(embedded, "description");
            }
        }
    }
    return token;
}

inline std::string atomTokenIdentity(const TokenRef &token) {
    const auto id = token.attrs.get_value("id");
    if (id) {
        std::string identity = token.kind + ":" + *id;
        std::vector<std::string> names;
        if (token.kind == "person")
            names = {"email"};
        else if (token.kind == "date")
            names = {"format", "locale", "time", "value", "zone"};
        else if (token.kind == "richlink" || token.kind == "richLink")
            names = {"url"};
        else if (token.kind == "dropdown")
            names = {"list"};
        for (const auto &key : names) {
            if (const auto value = token.attrs.get_value(key))
                identity += "\n" + key + "=" + *value;
        }
        return identity;
    }
    if (token.kind == "footnoteRef" && token.label)
        return "footnote:" + *token.label;
    std::string result = token.kind;
    for (const auto &[key, value] : token.attrs.values)
        result += "\n" + key + "=" + value;
    return result;
}

inline model::Atom atomRecreate(model::EditTarget &target, const model::Atom &original,
                                const model::DocModel &document, AtomPutReport &report) {
    model::AtomCreate create;
    if (original.create)
        create = *original.create;
    else {
        const auto raw = original.raw.value_or(model::make_json_object());
        auto properties = [&](const char *outer, const char *inner) {
            if (raw.contains(outer) && raw[outer].contains(inner))
                return raw[outer][inner];
            return model::make_json_object();
        };
        auto string = [](const slimr::Json &props, const char *field) -> std::optional<std::string> {
            if (!props.contains(field) || !props[field].is_string())
                return std::nullopt;
            return props[field].get<std::string>();
        };
        create.type = original.type;
        if (original.type == "person")
            create.email = string(properties("person", "personProperties"), "email");
        else if (original.type == "richLink")
            create.uri = string(properties("richLink", "richLinkProperties"), "uri");
        else if (original.type == "date") {
            const auto props = properties("dateElement", "dateElementProperties");
            create.timestamp = string(props, "timestamp");
            create.dateFormat = string(props, "dateFormat");
            create.timeFormat = string(props, "timeFormat");
            create.timeZoneId = string(props, "timeZoneId");
            create.locale = string(props, "locale");
        } else if (original.type == "dropdown") {
            const auto props = properties("dropdown", "dropdownProperties");
            create.definitionId = string(props, "dropdownDefinitionId");
            create.optionId = string(props, "selectedOptionId");
        } else if (original.type == "image") {
            const auto source = imageRecreationSourceOf(document, target.tab, original);
            if (!source)
                throw model::CoreError("unrecreatableMove", "this image has no known recreatable source");
            create = source->create;
            report.recreated.push_back(*source);
        } else if (original.type == "footnoteRef") {
            std::optional<std::string> id;
            if (raw.contains("footnoteReference"))
                id = string(raw["footnoteReference"], "footnoteId");
            const auto sourceTab = std::find_if(document.tabs.begin(), document.tabs.end(), [&](const auto &tab) {
                return tab.tabId == target.tab.tabId;
            });
            const auto &tab = sourceTab == document.tabs.end() ? target.tab : *sourceTab;
            std::map<std::string, std::string> labels;
            for (const auto &paragraph : model::paragraphsDeep(tab.blocks)) {
                for (const auto &item : paragraph.inlines) {
                    if (!item.is_atom() || item.type != "footnoteRef" || !item.raw ||
                        !item.raw->contains("footnoteReference"))
                        continue;
                    const auto reference = string((*item.raw)["footnoteReference"], "footnoteId");
                    if (reference && !labels.contains(*reference))
                        labels[*reference] = std::to_string(labels.size() + 1);
                }
            }
            if (!id || !labels.contains(*id)) {
                throw model::CoreError("anchorNotFound", "the moved footnote needs its original exported label");
            }
            create.label = labels.at(*id);
        } else if (original.type != "pageBreak") {
            throw model::CoreError("unrecreatableMove", "this object cannot be recreated");
        }
    }
    const bool valid =
        (create.type == "person" && create.email) || (create.type == "richLink" && create.uri) ||
        (create.type == "date" && create.timestamp) ||
        (create.type == "dropdown" && create.definitionId && create.optionId) ||
        (create.type == "image" && create.uri) || create.type == "pageBreak" ||
        (create.type == "footnoteRef" && create.label);
    if (!valid)
        throw model::CoreError("unrecreatableMove", "this object has no known recreatable source");
    return model::Atom{.key = target.ctx.keys.next("n"), .type = create.type, .create = std::move(create), .style = original.style};
}

inline model::Atom atomRecreate(model::EditTarget &target, const model::Atom &original, AtomPutReport &report) {
    return atomRecreate(target, original, model::DocModel{}, report);
}

inline void atomTokenKeep(model::Atom &atom, const TokenRef &before, const TokenRef &after,
                          const model::TabModel &tab, const AtomPutOptions &options) {
    if (!after.attrs.flags.empty() || !after.attrs.classes.empty()) {
        throw model::CoreError("unsupportedSyntax", "unexpected object attributes");
    }
    for (const auto &[name, unused] : after.attrs.values) {
        (void)unused;
        const bool allowed = name == "id" ||
                             (atom.type == "person" && name == "email") ||
                             (atom.type == "richLink" && name == "url") ||
                             (atom.type == "dropdown" && (name == "list" || name == "option")) ||
                             (atom.type == "image" && name == "src") ||
                             (atom.type == "date" && (name == "value" || name == "time" || name == "zone" ||
                                                      name == "format" || name == "locale"));
        if (!allowed)
            throw model::CoreError("unsupportedSyntax", "unexpected object attribute: " + name);
    }
    auto label = [](const std::optional<std::string> &text) {
        std::string result;
        bool space = false;
        for (const auto &cp : model::utf8CodePoints(text.value_or(""))) {
            if (cp == "{" || cp == "}" || cp == "|" || cp == "\r" || cp == "\n" ||
                (cp >= "\uE000" && cp <= "\uF8FF") || (cp.size() == 1 && std::isspace(static_cast<unsigned char>(cp[0])))) {
                space = !result.empty();
            } else {
                if (space)
                    result += " ";
                result += cp;
                space = false;
            }
        }
        std::string shortened;
        int units = 0;
        for (const auto &cp : model::utf8CodePoints(result)) {
            const auto length = model::codePointUtf16Units(cp);
            if (units + length > 40)
                break;
            shortened += cp;
            units += length;
        }
        while (!shortened.empty() && shortened.back() == ' ')
            shortened.pop_back();
        return shortened;
    };
    if (after.kind != "dropdown" && after.label && label(before.label) != label(after.label)) {
        const auto name = before.kind + (before.label ? " \"" + *before.label + "\"" : "");
        throw model::CoreError("tokenLabelChanged", "the " + name + " can't be relabeled \"" +
                                                        *after.label + "\"; keep exported objects unchanged");
    }
    if (const auto src = after.attrs.get_value("src"); src && atom.type == "image") {
        atom.replaceUri = imageUriResolve(*src, options.baseDir);
    }
    if (const auto option = after.attrs.get_value("option"); option && atom.type == "dropdown") {
        if (!atom.raw || !atom.raw->contains("dropdown") ||
            !(*atom.raw)["dropdown"].contains("dropdownProperties")) {
            throw model::CoreError("unsupportedSyntax", "this dropdown has no known list");
        }
        auto &props = (*atom.raw)["dropdown"]["dropdownProperties"];
        if (!props.contains("dropdownDefinitionId") || !props["dropdownDefinitionId"].is_string()) {
            throw model::CoreError("unsupportedSyntax", "this dropdown has no known list");
        }
        const auto definition = tab.dropdownDefinitions.find(props["dropdownDefinitionId"].get<std::string>());
        const auto id = definition == tab.dropdownDefinitions.end() ? std::nullopt : optionIdOf(definition->second, *option);
        if (!id)
            throw model::CoreError("unsupportedSyntax", "this dropdown's list has no option \"" + *option + "\"");
        props["selectedOptionId"] = *id;
        props["displayValue"] = *option;
    }
}

inline model::Atom atomTokenCreate(model::EditTarget &target, const TokenRef &token,
                                   const AtomPutOptions &options, AtomPutReport &report) {
    model::AtomCreate create;
    const auto value = [&](const char *name) { return token.attrs.get_value(name); };
    auto only = [&](std::initializer_list<std::string_view> names) {
        if (!token.attrs.flags.empty() || !token.attrs.classes.empty()) {
            throw model::CoreError("unsupportedSyntax", "unexpected object attributes");
        }
        for (const auto &[name, unused] : token.attrs.values) {
            (void)unused;
            if (name != "id" && std::find(names.begin(), names.end(), name) == names.end()) {
                throw model::CoreError("unsupportedSyntax", "unexpected object attribute: " + name);
            }
        }
    };
    if (token.kind == "person" && value("email")) {
        only({"email"});
        create.type = "person";
        create.email = value("email");
        if (!atom_detail::personEmailValid(*create.email)) {
            throw model::CoreError("unsupportedSyntax", "person chip needs an email address, got \"" + *create.email + "\"");
        }
    } else if (token.kind == "date" && value("value")) {
        only({"value", "format", "locale", "time", "zone"});
        const auto date = *value("value");
        static const std::regex datePattern(R"(^\d{4}-\d{2}-\d{2}$)");
        static const std::regex timePattern(R"(^([01]\d|2[0-3]):[0-5]\d$)");
        static const std::regex localePattern(R"(^[a-z]{2,3}(-[A-Za-z0-9]{2,8})*$)");
        if (!std::regex_match(date, datePattern))
            throw model::CoreError("unsupportedSyntax", "date needs ISO value=YYYY-MM-DD");
        const auto m = std::chrono::month(static_cast<unsigned>(std::stoi(date.substr(5, 2))));
        const auto d = std::chrono::day(static_cast<unsigned>(std::stoi(date.substr(8, 2))));
        if (!m.ok() || !d.ok())
            throw model::CoreError("unsupportedSyntax", "invalid date");
        create.type = "date";
        create.locale = value("locale");
        if (create.locale && !std::regex_match(*create.locale, localePattern)) {
            throw model::CoreError("unsupportedSyntax", "date locale must be a language code");
        }
        if (const auto format = value("format")) {
            if (*format == "iso")
                create.dateFormat = "DATE_FORMAT_ISO8601";
            else if (*format == "long")
                create.dateFormat = "DATE_FORMAT_MONTH_DAY_FULL";
            else if (*format == "short")
                create.dateFormat = "DATE_FORMAT_MONTH_DAY_ABBREVIATED";
            else
                throw model::CoreError("unsupportedSyntax", "date format is iso, long or short");
        }
        const auto time = value("time");
        const auto zone = value("zone");
        if (zone && !time)
            throw model::CoreError("unsupportedSyntax", "date zone needs a time");
        if (time && !std::regex_match(*time, timePattern))
            throw model::CoreError("unsupportedSyntax", "date time is 24-hour HH:MM");
        create.timestamp = zone ? atom_detail::zonedTimestamp(date, *time, *zone) : time ? atom_detail::timestamp(atom_detail::wallTime(date, *time))
                                                                                         : date + "T12:00:00Z";
        if (time)
            create.timeFormat = zone ? "TIME_FORMAT_HOUR_MINUTE_TIMEZONE" : "TIME_FORMAT_HOUR_MINUTE";
        create.timeZoneId = zone;
    } else if ((token.kind == "richlink" || token.kind == "richLink") && value("url")) {
        only({"url"});
        create.type = "richLink";
        create.uri = value("url");
    } else if (token.kind == "image" &&
               ((value("src") && value("src")->starts_with(IMAGE_DOC_SCHEME)) || (value("id") && !value("src")))) {
        for (const auto &[name, unused] : token.attrs.values) {
            (void)unused;
            if (name != "id" && name != "src") {
                throw model::CoreError("unsupportedSyntax",
                                       "a copied image takes only its id; insert a new image with :image{src=… width=…} instead");
            }
        }
        only({"src"});
        const auto reference = value("src").value_or(value("id").value_or(""));
        const auto source = options.images ? options.images(reference) : std::nullopt;
        if (!source)
            throw model::CoreError("anchorNotFound",
                                   "no single image with that id in that document (or it couldn't be read); use src=\"gdoc:<doc id>/<image id>\"");
        create = source->create;
        report.recreated.push_back(*source);
    } else if (token.kind == "image" && value("src")) {
        only({"src", "height", "width"});
        create.type = "image";
        create.uri = imageUriResolve(*value("src"), options.baseDir);
        auto dimension = [&](const char *name) -> std::optional<double> {
            const auto text = value(name);
            if (!text)
                return std::nullopt;
            size_t parsed = 0;
            double number = 0;
            try {
                number = std::stod(*text, &parsed);
            } catch (const std::exception &) {
                throw model::CoreError("unsupportedSyntax", "image size must be positive points");
            }
            if (parsed != text->size() || !std::isfinite(number) || number <= 0) {
                throw model::CoreError("unsupportedSyntax", "image size must be positive points");
            }
            return number;
        };
        create.heightPt = dimension("height");
        create.widthPt = dimension("width");
        if (token.label && !token.label->empty()) {
            create.alt = token.label;
            const auto note = "alt text \"" + *token.label + "\" was dropped: Google can't set an inserted image's alt text";
            if (std::find(report.notes.begin(), report.notes.end(), note) == report.notes.end())
                report.notes.push_back(note);
        }
    } else if (token.kind == "dropdown" && value("list") && value("option")) {
        only({"list", "option"});
        create.type = "dropdown";
        const auto resolved = dropdownResolve(target.tab, *value("list"), *value("option"), options.createdDropdownIds);
        create.definitionId = resolved.definitionId;
        create.optionId = resolved.optionId;
    } else if (token.kind == "footnoteRef" && token.label) {
        if (target.tab.tabId.find("::") != std::string::npos) {
            throw model::CoreError("unsupportedSyntax", "footnote [^" + *token.label + "] can only be referenced from a tab body");
        }
        only({});
        create.type = "footnoteRef";
        create.label = token.label;
    } else if (token.kind == "pagebreak") {
        only({});
        create.type = "pageBreak";
    } else {
        throw model::CoreError("unsupportedSyntax", "this object is neither an exported object nor a supported creation");
    }
    return model::Atom{.key = target.ctx.keys.next("n"), .type = create.type, .create = std::move(create)};
}

} // namespace gdocsmith::lens
