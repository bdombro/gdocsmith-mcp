#pragma once

#include "model/errors.hpp"
#include "lens/json_helper.hpp"
#include "lens/cell_styles.hpp"
#include "lens/paragraph_styles.hpp"
#include "json.hpp"
#include "yaml.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <regex>
#include <algorithm>

namespace gdocsmith::lens {

using gdocsmith::model::CoreError;

constexpr int GDOCSMITH_VERSION = 1;

static const std::unordered_map<std::string, std::string> STYLE_ATTRIBUTES = {
    {"backgroundColor", "color"},
    {"baselineOffset", "string"},
    {"bold", "boolean"},
    {"fontFamily", "string"},
    {"fontSize", "number"},
    {"fontWeight", "number"},
    {"foregroundColor", "color"},
    {"italic", "boolean"},
    {"smallCaps", "boolean"},
    {"strikethrough", "boolean"},
    {"underline", "boolean"}
};

static const std::vector<std::string> NAMED_STYLE_TYPES = {
    "NORMAL_TEXT", "TITLE", "SUBTITLE",
    "HEADING_1", "HEADING_2", "HEADING_3", "HEADING_4", "HEADING_5", "HEADING_6"
};

struct StyleDeclaration {
    std::optional<std::string> backgroundColor;
    std::optional<std::string> baselineOffset;
    std::optional<bool> bold;
    std::optional<std::string> fontFamily;
    std::optional<double> fontSize;
    std::optional<double> fontWeight;
    std::optional<std::string> foregroundColor;
    std::optional<bool> italic;
    std::optional<bool> smallCaps;
    std::optional<bool> strikethrough;
    std::optional<bool> underline;

    bool operator==(const StyleDeclaration& other) const = default;
};



struct DocumentFrontmatter {
    int gdocsmith = GDOCSMITH_VERSION;
    std::optional<std::string> doc;
    std::optional<std::string> revision;
    std::string title;
    std::unordered_map<std::string, StyleDeclaration> textStyles;
    std::unordered_map<std::string, ParagraphDeclaration> paragraphStyles;
    std::unordered_map<std::string, CellDeclaration> cellStyles;
    std::optional<slimr::Json> guards;
    std::optional<slimr::Json> styleBaseline;
    std::unordered_map<std::string, std::string> variables;
    std::optional<int> commentsLine;
    std::optional<int> suggestionsLine;

    bool operator==(const DocumentFrontmatter& other) const = default;
};

struct TabFrontmatter {
    std::optional<int> gdocsmith;
    std::optional<std::string> doc;
    std::optional<std::string> revision;
    std::optional<std::string> title;
    std::optional<std::string> tab;
    std::unordered_map<std::string, StyleDeclaration> textStyles;
    std::unordered_map<std::string, ParagraphDeclaration> paragraphStyles;
    std::unordered_map<std::string, CellDeclaration> cellStyles;
    std::unordered_map<std::string, std::string> variables;
};

inline bool isValidDeclarationName(std::string_view name) {
    if (name.empty()) return false;
    char first = name[0];
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '+' || c == '.' || c == '-')) {
            return false;
        }
    }
    return true;
}

inline bool isValidHexColor(std::string_view hex) {
    if (hex.size() != 7 || hex[0] != '#') return false;
    for (size_t i = 1; i < 7; i++) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

inline std::string uppercaseHex(std::string_view s) {
    std::string out(s);
    for (size_t i = 1; i < out.size(); i++) {
        out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));
    }
    return out;
}

inline DocumentFrontmatter frontmatterValidate(const slimr::Json& root, std::string_view kind = "document") {
    std::vector<std::string> problems;

    if (!root.is_object()) {
        problems.push_back("frontmatter must be a mapping");
        throw CoreError("unsupportedSyntax", problems[0]);
    }

    static const std::unordered_set<std::string> doc_allowed_keys = {
        "apiUser", "cellStyles", "commentsLine", "doc", "dropdowns", "gdocsmith",
        "guards", "namedStyleSets", "paragraphStyles", "revision", "styleBaseline",
        "styles", "suggestionsLine", "textStyles", "title", "variables"
    };

    static const std::unordered_set<std::string> tab_allowed_keys = {
        "cellStyles", "doc", "gdocsmith", "paragraphStyles", "revision", "styles", "tab", "title", "variables"
    };

    const auto& allowed = (kind == "document") ? doc_allowed_keys : tab_allowed_keys;
    for (const auto& [key, _] : root.get<slimr::Json::object_t>()) {
        if (!allowed.contains(key)) {
            if (kind == "document") {
                problems.push_back("unknown frontmatter field \"" + key + "\"");
            } else {
                problems.push_back("unknown frontmatter key \"" + key + "\" (allowed: doc, revision, styles, tab, title)");
            }
        }
    }

    DocumentFrontmatter df;

    // gdocsmith
    if (auto* g_val = json::get(root, "gdocsmith")) {
        if (g_val->is_number()) {
            if (g_val->as<int64_t>() != GDOCSMITH_VERSION) {
                problems.push_back("frontmatter gdocsmith must be " + std::to_string(GDOCSMITH_VERSION));
            } else {
                df.gdocsmith = GDOCSMITH_VERSION;
            }
        } else if (kind == "document") {
            problems.push_back("frontmatter gdocsmith must be " + std::to_string(GDOCSMITH_VERSION));
        }
    } else if (kind == "document") {
        problems.push_back("frontmatter gdocsmith must be " + std::to_string(GDOCSMITH_VERSION));
    }

    // doc
    if (auto* doc_val = json::get(root, "doc")) {
        if (!doc_val->is_null()) {
            if (doc_val->is_string() && !doc_val->get<std::string>().empty() &&
                doc_val->get<std::string>().find_first_not_of(" \t\r\n") != std::string::npos) {
                df.doc = doc_val->get<std::string>();
            } else {
                problems.push_back("doc must be a document ID or explicit null to create a document");
            }
        }
    }

    // revision
    if (auto* rev_val = json::get(root, "revision")) {
        if (!rev_val->is_null()) {
            if (rev_val->is_string() && !rev_val->get<std::string>().empty() &&
                rev_val->get<std::string>().find_first_not_of(" \t\r\n") != std::string::npos) {
                df.revision = rev_val->get<std::string>();
            } else {
                problems.push_back("revision must be a nonempty string or explicit null");
            }
        }
    }

    // title
    if (auto* title_val = json::get(root, "title")) {
        if (title_val->is_string() && !title_val->get<std::string>().empty() &&
            title_val->get<std::string>().find_first_not_of(" \t\r\n") != std::string::npos) {
            df.title = title_val->get<std::string>();
        } else if (kind == "document") {
            problems.push_back("title must be a nonempty string");
        }
    } else if (kind == "document") {
        problems.push_back("title must be a nonempty string");
    }

    // textStyles / styles
    auto parse_styles = [&](const std::string& sec_name, const slimr::Json& styles_obj) {
        if (!styles_obj.is_object()) {
            problems.push_back("frontmatter " + sec_name + " must be a mapping of name → attributes");
            return;
        }
        for (const auto& [decl_name, decl_obj] : styles_obj.get<slimr::Json::object_t>()) {
            if (!isValidDeclarationName(decl_name)) {
                problems.push_back("style name \"" + decl_name + "\" must match ^[A-Za-z0-9][A-Za-z0-9_+.-]*$");
                continue;
            }
            if (!decl_obj.is_object()) {
                problems.push_back("style \"" + decl_name + "\" must be a mapping of attributes");
                continue;
            }
            StyleDeclaration sd;
            for (const auto& [attr_name, attr_val] : decl_obj.get<slimr::Json::object_t>()) {
                auto it = STYLE_ATTRIBUTES.find(attr_name);
                if (it == STYLE_ATTRIBUTES.end()) {
                    problems.push_back("style \"" + decl_name + "\" has unknown attribute \"" + attr_name +
                                      "\" (allowed: backgroundColor, baselineOffset, bold, fontFamily, fontSize, fontWeight, foregroundColor, italic, smallCaps, strikethrough, underline)");
                    continue;
                }
                const std::string& type = it->second;
                if (type == "color") {
                    if (attr_val.is_string() && isValidHexColor(attr_val.get<std::string>())) {
                        std::string up = uppercaseHex(attr_val.get<std::string>());
                        if (attr_name == "backgroundColor") sd.backgroundColor = up;
                        else if (attr_name == "foregroundColor") sd.foregroundColor = up;
                    } else if (attr_val.is_null()) {
                        // Explicit reset null is allowed
                    } else {
                        problems.push_back("style \"" + decl_name + "\" attribute \"" + attr_name + "\" must be #RRGGBB");
                    }
                } else if (type == "number") {
                    if (attr_val.is_number() && attr_val.as<double>() > 0) {
                        double val = attr_val.as<double>();
                        if (attr_name == "fontSize") sd.fontSize = val;
                        else if (attr_name == "fontWeight") sd.fontWeight = val;
                    } else if (attr_val.is_null()) {
                        // Explicit null
                    } else {
                        problems.push_back("style \"" + decl_name + "\" attribute \"" + attr_name + "\" must be a number");
                    }
                } else if (type == "boolean") {
                    if (attr_val.is_boolean()) {
                        bool b = attr_val.get<bool>();
                        if (attr_name == "bold") sd.bold = b;
                        else if (attr_name == "italic") sd.italic = b;
                        else if (attr_name == "smallCaps") sd.smallCaps = b;
                        else if (attr_name == "strikethrough") sd.strikethrough = b;
                        else if (attr_name == "underline") sd.underline = b;
                    } else if (attr_val.is_null()) {
                        // Explicit null
                    } else {
                        problems.push_back("style \"" + decl_name + "\" attribute \"" + attr_name + "\" must be a boolean");
                    }
                } else if (type == "string") {
                    if (attr_val.is_string() && !attr_val.get<std::string>().empty()) {
                        if (attr_name == "baselineOffset") sd.baselineOffset = attr_val.get<std::string>();
                        else if (attr_name == "fontFamily") sd.fontFamily = attr_val.get<std::string>();
                    } else if (attr_val.is_null()) {
                        // Explicit null
                    } else {
                        problems.push_back("style \"" + decl_name + "\" attribute \"" + attr_name + "\" must be a string");
                    }
                }
            }
            df.textStyles[decl_name] = sd;
        }
    };

    if (auto* ts = json::get(root, "textStyles")) parse_styles("textStyles", *ts);
    else if (auto* s = json::get(root, "styles")) parse_styles("styles", *s);

    // paragraphStyles
    if (auto* ps_val = json::get(root, "paragraphStyles")) {
        if (!ps_val->is_object()) {
            problems.push_back("paragraphStyles must be a mapping");
        } else {
            for (const auto& [decl_name, decl_obj] : ps_val->get<slimr::Json::object_t>()) {
                if (!isValidDeclarationName(decl_name)) {
                    problems.push_back("invalid paragraph style name \"" + decl_name + "\"");
                    continue;
                }
                if (!decl_obj.is_object()) {
                    problems.push_back("paragraph style must be a mapping");
                    continue;
                }
                ParagraphDeclaration pd;
                for (const auto& [prop, val] : decl_obj.get<slimr::Json::object_t>()) {
                    if (prop == "namedStyle") {
                        if (val.is_null()) {
                            problems.push_back("namedStyle cannot be null; use NORMAL_TEXT");
                        } else if (val.is_string()) {
                            std::string s = val.get<std::string>();
                            if (std::find(NAMED_STYLE_TYPES.begin(), NAMED_STYLE_TYPES.end(), s) != NAMED_STYLE_TYPES.end()) {
                                pd.namedStyle = s;
                            } else {
                                problems.push_back("invalid namedStyle: " + s);
                            }
                        } else {
                            problems.push_back("invalid namedStyle");
                        }
                    } else if (prop == "alignment") {
                        if (val.is_string()) {
                            std::string s = val.get<std::string>();
                            if (s == "CENTER" || s == "END" || s == "JUSTIFIED" || s == "START") {
                                pd.alignment = s;
                            } else {
                                problems.push_back("invalid alignment: " + s);
                            }
                        } else if (!val.is_null()) {
                            problems.push_back("invalid alignment: " + slimr::to_json(val));
                        }
                    } else if (prop == "direction") {
                        if (val.is_string()) {
                            std::string s = val.get<std::string>();
                            if (s == "LEFT_TO_RIGHT" || s == "RIGHT_TO_LEFT") pd.direction = s;
                            else problems.push_back("invalid direction: " + s);
                        } else if (!val.is_null()) {
                            problems.push_back("invalid direction: " + slimr::to_json(val));
                        }
                    } else if (prop == "spacingMode") {
                        if (val.is_string()) {
                            std::string s = val.get<std::string>();
                            if (s == "COLLAPSE_LISTS" || s == "NEVER_COLLAPSE") pd.spacingMode = s;
                            else problems.push_back("invalid spacingMode: " + s);
                        } else if (!val.is_null()) {
                            problems.push_back("invalid spacingMode: " + slimr::to_json(val));
                        }
                    } else if (prop == "shading") {
                        if (val.is_string() && isValidHexColor(val.get<std::string>())) {
                            pd.shading = uppercaseHex(val.get<std::string>());
                        } else if (!val.is_null()) {
                            problems.push_back("shading must be #RRGGBB");
                        }
                    } else if (prop == "avoidWidowAndOrphan" || prop == "keepLinesTogether" ||
                               prop == "keepWithNext" || prop == "pageBreakBefore") {
                        if (val.is_boolean()) {
                            bool b = val.get<bool>();
                            if (prop == "avoidWidowAndOrphan") pd.avoidWidowAndOrphan = b;
                            else if (prop == "keepLinesTogether") pd.keepLinesTogether = b;
                            else if (prop == "keepWithNext") pd.keepWithNext = b;
                            else if (prop == "pageBreakBefore") pd.pageBreakBefore = b;
                        }
                    } else if (prop == "lineSpacing" || prop == "indentEnd" || prop == "indentFirstLine" ||
                               prop == "indentStart" || prop == "spaceAbove" || prop == "spaceBelow") {
                        if (val.is_number()) {
                            double num = val.as<double>();
                            if (prop == "lineSpacing") pd.lineSpacing = num;
                            else if (prop == "indentEnd") pd.indentEnd = num;
                            else if (prop == "indentFirstLine") pd.indentFirstLine = num;
                            else if (prop == "indentStart") pd.indentStart = num;
                            else if (prop == "spaceAbove") pd.spaceAbove = num;
                            else if (prop == "spaceBelow") pd.spaceBelow = num;
                        } else if (!val.is_null()) {
                            problems.push_back(prop + " must be a finite valid point value");
                        }
                    } else if (prop.starts_with("border")) {
                        if (val.is_object()) {
                            ParagraphBorder pb;
                            if (auto* col = json::get(val, "color")) {
                                if (col->is_string() && isValidHexColor(col->get<std::string>())) {
                                    pb.color = uppercaseHex(col->get<std::string>());
                                }
                            }
                            if (auto* dash = json::get(val, "dashStyle")) {
                                if (dash->is_string()) pb.dashStyle = dash->get<std::string>();
                            }
                            if (auto* pad = json::get(val, "padding")) {
                                if (pad->is_number()) pb.padding = pad->as<double>();
                            }
                            if (auto* wid = json::get(val, "width")) {
                                if (wid->is_number()) pb.width = wid->as<double>();
                            }
                            if (prop == "borderBetween") pd.borderBetween = pb;
                            else if (prop == "borderBottom") pd.borderBottom = pb;
                            else if (prop == "borderLeft") pd.borderLeft = pb;
                            else if (prop == "borderRight") pd.borderRight = pb;
                            else if (prop == "borderTop") pd.borderTop = pb;
                        }
                    } else {
                        problems.push_back("unknown paragraph style field \"" + prop + "\"");
                    }
                }
                df.paragraphStyles[decl_name] = pd;
            }
        }
    }

    // cellStyles
    if (auto* cs_val = json::get(root, "cellStyles")) {
        if (!cs_val->is_object()) {
            problems.push_back("cellStyles must be a mapping");
        } else {
            for (const auto& [decl_name, decl_obj] : cs_val->get<slimr::Json::object_t>()) {
                if (!isValidDeclarationName(decl_name)) {
                    problems.push_back("invalid cell style name \"" + decl_name + "\"");
                    continue;
                }
                if (!decl_obj.is_object()) {
                    problems.push_back("cell style must be a mapping");
                    continue;
                }
                CellDeclaration cd;
                for (const auto& [prop, val] : decl_obj.get<slimr::Json::object_t>()) {
                    if (prop == "backgroundColor") {
                        if (val.is_string() && isValidHexColor(val.get<std::string>())) {
                            cd.backgroundColor = uppercaseHex(val.get<std::string>());
                        } else if (!val.is_null()) {
                            problems.push_back("backgroundColor must be #RRGGBB");
                        }
                    } else if (prop == "contentAlignment") {
                        if (val.is_string()) {
                            std::string s = val.get<std::string>();
                            if (s == "TOP" || s == "MIDDLE" || s == "BOTTOM") cd.contentAlignment = s;
                            else problems.push_back("contentAlignment must be TOP, MIDDLE, BOTTOM");
                        }
                    } else if (prop.starts_with("padding")) {
                        if (val.is_number()) {
                            double pt = val.as<double>();
                            if (prop == "paddingTop") cd.paddingTop = pt;
                            else if (prop == "paddingRight") cd.paddingRight = pt;
                            else if (prop == "paddingBottom") cd.paddingBottom = pt;
                            else if (prop == "paddingLeft") cd.paddingLeft = pt;
                        } else if (!val.is_null()) {
                            problems.push_back(prop + " must be a point value");
                        }
                    } else if (prop.starts_with("border")) {
                        if (val.is_object()) {
                            CellBorder cb;
                            if (auto* col = json::get(val, "color")) {
                                if (col->is_string() && isValidHexColor(col->get<std::string>())) cb.color = uppercaseHex(col->get<std::string>());
                            }
                            if (auto* dash = json::get(val, "dashStyle")) {
                                if (dash->is_string()) cb.dashStyle = dash->get<std::string>();
                            }
                            if (auto* wid = json::get(val, "width")) {
                                if (wid->is_number()) cb.width = wid->as<double>();
                            }
                            if (prop == "borderTop") cd.borderTop = cb;
                            else if (prop == "borderRight") cd.borderRight = cb;
                            else if (prop == "borderBottom") cd.borderBottom = cb;
                            else if (prop == "borderLeft") cd.borderLeft = cb;
                        } else if (!val.is_null()) {
                            problems.push_back(prop + " must be { color: \"#RRGGBB\", width: <points>, dashStyle: SOLID | DOT | DASH }");
                        }
                    } else {
                        problems.push_back("unknown cell style field \"" + prop + "\" (allowed: backgroundColor, borderBottom, borderLeft, borderRight, borderTop, contentAlignment, paddingBottom, paddingLeft, paddingRight, paddingTop)");
                    }
                }
                df.cellStyles[decl_name] = cd;
            }
        }
    }

    if (!problems.empty()) {
        throw CoreError("unsupportedSyntax", problems[0]);
    }
    return df;
}

inline ParagraphDeclaration paragraphDeclarationValidate(const slimr::Json& value) {
    slimr::Json root = slimr::json_obj();
    slimr::Json ps = slimr::json_obj();
    ps["decl"] = value;
    root["paragraphStyles"] = std::move(ps);
    auto df = frontmatterValidate(root, "tab");
    return df.paragraphStyles["decl"];
}

} // namespace gdocsmith::lens
