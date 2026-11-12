#pragma once

#include <cctype>
#include <format>
#include <memory>
#include <string>
#include <string_view>

namespace gdocsmith::model {

/** The identifier-safe suffix a tab's parsed keys carry (`t.abc` -> `-t-abc`). */
inline std::string tabKeySuffix(std::string_view tabId) {
    std::string s = "-";
    for (char c : tabId) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            s += c;
        } else {
            s += '-';
        }
    }
    return s;
}

/**
 * Hands out deterministic, monotonically increasing keys within one parse/edit session:
 * `o<N>` for blocks/atoms read from existing JSON, `n<N>` for ones an edit creates.
 */
class KeyAllocator {
public:
    struct State {
        int n = 0;
        int o = 0;
    };

private:
    std::shared_ptr<int> sharedN;
    int oCounter = 0;
    std::string suffix;

public:
    KeyAllocator()
        : sharedN(std::make_shared<int>(0)), oCounter(0), suffix("") {}

    KeyAllocator(std::shared_ptr<int> nPtr, int oVal, std::string suff)
        : sharedN(std::move(nPtr)), oCounter(oVal), suffix(std::move(suff)) {}

    std::string next(std::string_view prefix) {
        if (prefix == "n") {
            (*sharedN)++;
            return std::format("n{}", *sharedN);
        } else {
            oCounter++;
            return std::format("o{}{}", oCounter, suffix);
        }
    }

    KeyAllocator tabScope(std::string_view tabId) {
        return KeyAllocator(sharedN, 0, tabKeySuffix(tabId));
    }
};

} // namespace gdocsmith::model
