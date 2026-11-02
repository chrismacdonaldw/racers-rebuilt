#pragma once

#include <string>
#include <string_view>

namespace rr {

/// Unpaired surrogates become U+FFFD.
[[nodiscard]] std::string Utf8FromUtf16(std::u16string_view text);

/// Each byte that doesn't start a whole, shortest-form sequence becomes U+FFFD.
[[nodiscard]] std::u16string Utf16FromUtf8(std::string_view text);

}
