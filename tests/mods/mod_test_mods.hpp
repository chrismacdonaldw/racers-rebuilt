#pragma once

#include <doctest/doctest.h>

#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mods/mod.hpp"
#include "mods/mod_manifest.hpp"

namespace rr::mods::test {

inline Mod MakeMod(std::string_view id, std::string_view tables = "",
                   std::string_view version = "1.0.0") {
    const Result<ModManifest> manifest = ParseModManifest(
        std::format("id = \"{}\"\nname = \"{}\"\nversion = \"{}\"\n{}", id, id, version, tables),
        "mod.toml");
    REQUIRE(manifest.has_value());
    return {manifest.value_or(ModManifest{}), std::string(id)};
}

inline std::vector<std::string> IdsOf(std::span<const Mod> mods,
                                      std::span<const std::size_t> indices) {
    std::vector<std::string> ids;
    for (const std::size_t index : indices) {
        ids.push_back(mods[index].Manifest().id);
    }
    return ids;
}

}
