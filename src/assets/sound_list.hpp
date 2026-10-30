#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

/// `folder` is relative to the list's own folder, with `/` separators.
struct SoundList {
    std::string folder;
    std::vector<std::string> names;
};

/// Only the first `count` names load, so a longer list is cut short.
[[nodiscard]] Result<SoundList> ParseSoundList(std::span<const std::byte> bytes);

}
