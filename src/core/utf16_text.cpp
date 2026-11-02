#include "core/utf16_text.hpp"

#include <array>
#include <cstddef>

namespace rr {
namespace {

constexpr char32_t kReplacement = 0xFFFD;
constexpr char16_t kHighSurrogate = 0xD800;
constexpr char16_t kLowSurrogate = 0xDC00;
constexpr char16_t kSurrogateEnd = 0xE000;
constexpr std::array<unsigned, 4> kUtf8Lead{0x00, 0xC0, 0xE0, 0xF0};

unsigned ContinuationBytes(char32_t point) {
    if (point < 0x800) {
        return 1;
    }
    return point < 0x10000 ? 2 : 3;
}

void AppendUtf8(std::string& utf8, char32_t point) {
    if (point < 0x80) {
        utf8.push_back(static_cast<char>(point));
        return;
    }
    const unsigned extra = ContinuationBytes(point);
    utf8.push_back(static_cast<char>(kUtf8Lead.at(extra) | (point >> (6U * extra))));
    for (unsigned byte = extra; byte > 0; --byte) {
        utf8.push_back(static_cast<char>(0x80U | ((point >> (6U * (byte - 1))) & 0x3FU)));
    }
}

bool IsLowSurrogate(char16_t unit) {
    return unit >= kLowSurrogate && unit < kSurrogateEnd;
}

}

std::string Utf8FromUtf16(std::u16string_view text) {
    std::string utf8;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char16_t unit = text[at];
        const bool isHigh = unit >= kHighSurrogate && unit < kLowSurrogate;
        if (isHigh && at + 1 < text.size() && IsLowSurrogate(text[at + 1])) {
            AppendUtf8(
                utf8, 0x10000U + ((unit - kHighSurrogate) << 10U) + (text[at + 1] - kLowSurrogate));
            ++at;
        } else {
            AppendUtf8(utf8,
                       isHigh || IsLowSurrogate(unit) ? kReplacement : static_cast<char32_t>(unit));
        }
    }
    return utf8;
}

}
