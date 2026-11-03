#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "assets/vfs_layer.hpp"

namespace rr::assets {

enum class GameVersion : std::uint8_t {
    Release1999,
    Release2001,
};

inline constexpr std::array<std::string_view, 2> kGameVersionNames{"1999", "2001"};

[[nodiscard]] std::string_view NameOf(GameVersion version);

[[nodiscard]] std::optional<GameVersion> ParseGameVersion(std::string_view name);

/// Anything without the 2001 console screens in `MENUDATA/MENUNAME.SRF` counts as 1999.
[[nodiscard]] GameVersion IdentifyRelease(const VfsLayer& archive);

}
