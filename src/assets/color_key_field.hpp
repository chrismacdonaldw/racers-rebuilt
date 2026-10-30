#pragma once

#include "assets/texture_database.hpp"
#include "assets/token_reader.hpp"
#include "core/result.hpp"

namespace rr::assets {

[[nodiscard]] Result<void> ReadValue(TokenReader& reader, ColorKey& key);

}
