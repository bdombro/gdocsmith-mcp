#pragma once

#include "types.hpp"

namespace gdocsmith::emulator {

inline std::vector<std::string> parseFields(std::string_view fields) {
    return parse_fields(fields);
}

inline slimr::Json applyStyleFields(const slimr::Json& base, const slimr::Json& patch,
                                    const std::vector<std::string>& fields) {
    return apply_style_fields(base, patch, fields);
}

} // namespace gdocsmith::emulator
