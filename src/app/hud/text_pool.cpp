#include "app/hud/text_pool.hpp"

#include <span>

#include "app/hud/race_time_text.hpp"
#include "core/assert.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kMaxDigits = 20;

}

std::u16string_view TextPool::Time(Milliseconds time) {
    RR_ASSERT(used_ + kRaceTimeLength <= kCapacity);
    const std::u16string_view text =
        WriteRaceTime(time, std::span(chars_).subspan(used_).first<kRaceTimeLength>());
    used_ += text.size();
    return text;
}

std::u16string_view TextPool::Number(std::size_t number) {
    std::array<char16_t, kMaxDigits> digits{};
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<char16_t>(u'0' + (number % 10));
        number /= 10;
    } while (number > 0);
    RR_ASSERT(used_ + count <= kCapacity);
    const std::size_t start = used_;
    while (count > 0) {
        chars_[used_++] = digits[--count];
    }
    return {&chars_[start], used_ - start};
}

}
