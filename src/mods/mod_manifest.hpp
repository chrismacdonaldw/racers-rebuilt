#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "mods/mod_version.hpp"

namespace rr::mods {

struct ModReference {
    std::string id;
    VersionRange range;
};

struct ModManifest {
    std::string id;
    std::string name;
    std::string version;
    int priority = 0;
    bool isEnabledByDefault = true;
    std::vector<ModReference> requirements;
    std::vector<ModReference> loadsAfter;
    std::vector<ModReference> conflicts;
};

/// Lowercase letters, digits, `_` and `-`, since ids become setting keys.
[[nodiscard]] bool IsValidModId(std::string_view id);

[[nodiscard]] Result<ModManifest> ParseModManifest(std::string_view text,
                                                   std::string_view sourceName);

}
