#pragma once

#include "model/edit.hpp"
#include "model/effective_style.hpp"
#include "model/errors.hpp"
#include "model/lists.hpp"
#include "model/types.hpp"

#include <string>
#include <vector>

namespace gdocsmith::model {

using ListKind = std::string; // "bullet" | "check" | "number"

struct BulletIntent {
    ListKind kind = "bullet";
    int nesting = 0;
    std::optional<BulletPreset> preset;
};

struct ListChangeResult {
    bool lossy = false;
};

} // namespace gdocsmith::model
