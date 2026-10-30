#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

[[nodiscard]] Result<std::vector<std::u16string>> ParseStringTable(
    std::span<const std::byte> bytes);

}
