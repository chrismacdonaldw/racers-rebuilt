#pragma once

#include <string>
#include <string_view>

#include "core/result.hpp"

namespace rr::mods {

struct ModManifest {
    std::string id;
    std::string name;
    std::string version;
    int priority = 0;
    bool isEnabledByDefault = true;
};

/// Ids are lowercase letters, digits, `_` and `-`, since they become setting keys.
[[nodiscard]] Result<ModManifest> ParseModManifest(std::string_view text,
                                                   std::string_view sourceName);

}
