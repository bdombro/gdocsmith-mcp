#pragma once

#include "document_emulator.hpp"

namespace gdocsmith::emulator {

inline EmulateResult requestsEmulate(const slimr::Json& json, const std::vector<slimr::Json>& requests) {
    return requests_emulate(json, requests);
}

} // namespace gdocsmith::emulator
