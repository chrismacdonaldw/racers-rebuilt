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
constexpr std::array<char32_t, 4> kShortestFrom{0x00, 0x80, 0x800, 0x10000};
constexpr char32_t kLastPoint = 0x10FFFF;
constexpr char32_t kPlaneSize = 0x10000;

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

unsigned LeadContinuations(unsigned char lead) {
    if (lead >= 0xC0 && lead < 0xE0) {
        return 1;
    }
    if (lead >= 0xE0 && lead < 0xF0) {
        return 2;
    }
    return lead >= 0xF0 && lead < 0xF8 ? 3 : 0;
}

struct Decoded {
    char32_t point = kReplacement;
    std::size_t length = 1;
};

Decoded DecodeUtf8(std::string_view text, std::size_t at) {
    const auto lead = static_cast<unsigned char>(text[at]);
    if (lead < 0x80) {
        return {.point = lead};
    }
    const unsigned extra = LeadContinuations(lead);
    if (extra == 0 || at + extra >= text.size()) {
        return {};
    }
    char32_t point = lead & (0x3FU >> extra);
    for (unsigned byte = 1; byte <= extra; ++byte) {
        const auto next = static_cast<unsigned char>(text[at + byte]);
        if ((next & 0xC0U) != 0x80U) {
            return {};
        }
        point = (point << 6U) | (next & 0x3FU);
    }
    const bool isSurrogate = point >= char32_t{kHighSurrogate} && point < char32_t{kSurrogateEnd};
    if (point < kShortestFrom.at(extra) || point > kLastPoint || isSurrogate) {
        return {};
    }
    return {.point = point, .length = extra + 1};
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

std::u16string Utf16FromUtf8(std::string_view text) {
    std::u16string utf16;
    for (std::size_t at = 0; at < text.size();) {
        const Decoded decoded = DecodeUtf8(text, at);
        if (decoded.point < kPlaneSize) {
            utf16.push_back(static_cast<char16_t>(decoded.point));
        } else {
            const char32_t offset = decoded.point - kPlaneSize;
            utf16.push_back(static_cast<char16_t>(kHighSurrogate + (offset >> 10U)));
            utf16.push_back(static_cast<char16_t>(kLowSurrogate + (offset & 0x3FFU)));
        }
        at += decoded.length;
    }
    return utf16;
}

}
