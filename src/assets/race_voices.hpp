#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include "core/result.hpp"

namespace rr::assets {

/// `voiceC1` names `GAMEDATA/VOICES/VOICEC1.SBK`. The test track has none.
[[nodiscard]] Result<std::optional<std::string>> ReadOpponentVoices(std::span<const std::byte> rab);

}
