#include "app/hud/race_time_text.hpp"

#include <cstdint>

namespace rr::app {
namespace {

constexpr std::int64_t kMsPerCentisecond = 10;
constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int64_t kMsPerMinute = 60 * kMsPerSecond;
constexpr std::int64_t kMsPerHour = 60 * kMsPerMinute;
constexpr std::int64_t kTwoDigitMinutes = 10;

std::size_t WriteDigits(std::span<char16_t> out, std::size_t at, std::int64_t value, int digits) {
    for (int i = digits - 1; i >= 0; --i) {
        out[at + static_cast<std::size_t>(i)] = static_cast<char16_t>(u'0' + (value % 10));
        value /= 10;
    }
    return at + static_cast<std::size_t>(digits);
}

}

std::u16string_view WriteRaceTime(Milliseconds time, std::span<char16_t, kRaceTimeLength> out) {
    const std::int64_t ms = ((time.count() % kMsPerHour) + kMsPerHour) % kMsPerHour;
    const std::int64_t minutes = ms / kMsPerMinute;
    std::size_t at = WriteDigits(out, 0, minutes, minutes < kTwoDigitMinutes ? 1 : 2);
    out[at++] = u':';
    at = WriteDigits(out, at, (ms % kMsPerMinute) / kMsPerSecond, 2);
    out[at++] = u':';
    at = WriteDigits(out, at, (ms % kMsPerSecond) / kMsPerCentisecond, 2);
    return {out.data(), at};
}

}
