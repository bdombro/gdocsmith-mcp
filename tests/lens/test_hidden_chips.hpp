#pragma once

#include "lens/hidden_chips.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("hiddenChipKindOf identifies kinds") {
    CHECK(hiddenChipKindOf("⏱ 01:23") == "stopwatch");
    CHECK(hiddenChipKindOf("⌛ 05:00") == "timer");
    CHECK(hiddenChipKindOf("Person") == "placeholder");
    CHECK(hiddenChipKindOf("Date") == "placeholder");
    CHECK(hiddenChipKindOf("Place") == "placeholder");
    CHECK(hiddenChipKindOf("File") == "placeholder");
    CHECK(hiddenChipKindOf("Calendar event") == "placeholder");
    CHECK(hiddenChipKindOf("Listen to this tab") == "listen");
    CHECK(hiddenChipKindOf("(👍 3) (👎 1)") == "vote");
    CHECK(hiddenChipKindOf("random prose") == "unknown");
}

} // namespace gdocsmith::lens::test
