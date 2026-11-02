#pragma once

#include <string>
#include <string_view>

namespace rr {

/// Unpaired surrogates become U+FFFD.
[[nodiscard]] std::string Utf8FromUtf16(std::u16string_view text);

}
