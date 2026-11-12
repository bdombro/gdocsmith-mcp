#pragma once

#include "model/errors.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include "directives.hpp"

namespace gdocsmith::lens {

using model::FileProblem;

struct ReviewMarker {
    std::string edge; // "end", "point", "start"
    std::string id;
    int line{0};
    std::string name; // "comment", "suggestion"
    int offset{0};

    bool operator==(const ReviewMarker& other) const = default;
};

struct ReviewMarks {
    std::vector<ReviewMarker> markers;
    std::string text;

    bool operator==(const ReviewMarks& other) const = default;
};

inline ReviewMarks reviewMarkersStrip(
    std::string_view markdown,
    std::vector<FileProblem>& problems,
    int firstLine = 1
) {
    std::vector<ReviewMarker> markers;
    std::string out;
    int length = 0;

    auto emit = [&](std::string_view chunk) {
        out.append(chunk);
        length += static_cast<int>(chunk.size());
    };

    std::istringstream ss{std::string(markdown)};
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    std::optional<std::string> fence;
    static const std::unordered_set<std::string> marker_names = {"comment", "suggestion"};

    for (size_t index = 0; index < lines.size(); index++) {
        const auto& cur_line = lines[index];
        if (index > 0) emit("\n");

        // Check code fence opener: ^\s*(`{3,}|~{3,})
        size_t non_ws = cur_line.find_first_not_of(" \t");
        std::string opener;
        if (non_ws != std::string::npos && (cur_line[non_ws] == '`' || cur_line[non_ws] == '~')) {
            char fc = cur_line[non_ws];
            size_t end_fence = non_ws;
            while (end_fence < cur_line.size() && cur_line[end_fence] == fc) end_fence++;
            if (end_fence - non_ws >= 3) {
                opener = cur_line.substr(non_ws, end_fence - non_ws);
            }
        }

        if (fence.has_value()) {
            if (!opener.empty() && opener[0] == (*fence)[0] && opener.size() >= fence->size()) {
                fence = std::nullopt;
            }
            emit(cur_line);
            continue;
        }

        if (!opener.empty()) {
            fence = opener;
            emit(cur_line);
            continue;
        }

        // Stop at comment or suggestion block line outside code fence: ^::(comment|suggestion)
        if (cur_line.starts_with("::comment") || cur_line.starts_with("::suggestion") ||
            cur_line.starts_with(":::comment") || cur_line.starts_with(":::suggestion")) {
            for (size_t k = index; k < lines.size(); k++) {
                if (k > index) emit("\n");
                emit(lines[k]);
            }
            break;
        }

        size_t at = 0;
        std::string plain;
        while (at < cur_line.size()) {
            char ch = cur_line[at];
            if (ch == '\\') {
                plain += cur_line.substr(at, std::min<size_t>(2, cur_line.size() - at));
                at += 2;
                continue;
            }
            if (ch == '`') {
                size_t run_end = at;
                while (run_end < cur_line.size() && cur_line[run_end] == '`') run_end++;
                std::string run = cur_line.substr(at, run_end - at);
                size_t close = cur_line.find(run, run_end);
                size_t end = (close == std::string::npos) ? run_end : (close + run.size());
                plain += cur_line.substr(at, end - at);
                at = end;
                continue;
            }

            std::optional<InlineDirective> match;
            if (ch == ':') {
                match = inlineDirectiveMatch(cur_line.substr(at), marker_names);
            }

            if (!match.has_value()) {
                plain += ch;
                at++;
                continue;
            }

            emit(plain);
            plain.clear();

            std::string name = match->name;
            const auto& flags = match->attrs.flags;
            auto id_val = match->attrs.get_value("id");

            std::optional<std::string> edge;
            if (flags.size() == 1 && (flags[0] == "start" || flags[0] == "end")) {
                edge = flags[0];
            } else if (flags.empty() && name == "suggestion") {
                edge = "point";
            }

            if (!id_val || !edge || match->attrs.values.size() != 1 ||
                match->label.has_value() || !match->attrs.classes.empty()) {
                problems.push_back(FileProblem{
                    .code = "unsupportedSyntax",
                    .line = firstLine + static_cast<int>(index),
                    .message = "invalid " + name + " marker " + match->raw +
                               "; keep markers exactly as exported (:" + name + "{id=… start} and :" +
                               name + "{id=… end}" + (name == "suggestion" ? ", or :suggestion{id=…}" : "") + ")"
                });
            } else {
                markers.push_back(ReviewMarker{
                    .edge = *edge,
                    .id = *id_val,
                    .line = firstLine + static_cast<int>(index),
                    .name = name,
                    .offset = length
                });
            }
            at += match->raw.size();
        }
        emit(plain);
    }

    return ReviewMarks{
        .markers = std::move(markers),
        .text = std::move(out)
    };
}

} // namespace gdocsmith::lens
