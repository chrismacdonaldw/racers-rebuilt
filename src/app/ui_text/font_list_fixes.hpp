#pragma once

#include <optional>
#include <vector>

#include "assets/font_list.hpp"

namespace rr::app {

/// The corrected list of a retail font whose list skips a glyph in its image, shifting the rest.
[[nodiscard]] std::optional<std::vector<char16_t>> FixedCharacters(
    const assets::FontDefinition& definition);

}
