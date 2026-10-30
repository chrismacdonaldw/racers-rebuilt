#include "app/hud/race_time_text.hpp"

#include <cstdint>

namespace rr::app {
namespace {

constexpr std::int64_t kMsPerCentisecond = 10;
constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int64_t kMsPerMinute = 60 * kMsPerSecond;
constexpr std::int64_t kMsPerHour = 60 * kMsPerMinute;
constexpr std::int64_t kTwoDigitMinutes = 10;

void AppendDigits(std::u16string& out, std::int64_t value, int digits) {
    std::int64_t place = 1;
    for (int i = 1; i < digits; ++i) {
        place *= 10;
    }
    for (; place > 0; place /= 10) {
        out += static_cast<char16_t>(u'0' + ((value / place) % 10));
    }
}

}

std::u16string RaceTimeText(Milliseconds time) {
    const std::int64_t ms = ((time.count() % kMsPerHour) + kMsPerHour) % kMsPerHour;
    const std::int64_t minutes = ms / kMsPerMinute;
    std::u16string text;
    AppendDigits(text, minutes, minutes < kTwoDigitMinutes ? 1 : 2);
    text += u':';
    AppendDigits(text, (ms % kMsPerMinute) / kMsPerSecond, 2);
    text += u':';
    AppendDigits(text, (ms % kMsPerSecond) / kMsPerCentisecond, 2);
    return text;
}

}
