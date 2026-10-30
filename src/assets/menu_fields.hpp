#pragma once

#include "assets/menu_layout.hpp"
#include "assets/token_reader.hpp"
#include "core/result.hpp"

namespace rr::assets {

[[nodiscard]] Result<void> ReadValue(TokenReader& reader, MenuRect& rect);
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, MenuColor& color);

}
