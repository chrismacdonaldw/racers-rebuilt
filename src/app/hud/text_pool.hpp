#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "core/duration.hpp"

namespace rr::app {

/// Fixed storage for a frame's generated text. Views stay valid until the next `Clear`.
class TextPool {
public:
    void Clear() { used_ = 0; }

    [[nodiscard]] std::u16string_view Time(Milliseconds time);
    [[nodiscard]] std::u16string_view Number(std::size_t number);

private:
    static constexpr std::size_t kCapacity = 256;

    std::array<char16_t, kCapacity> chars_{};
    std::size_t used_ = 0;
};

}
