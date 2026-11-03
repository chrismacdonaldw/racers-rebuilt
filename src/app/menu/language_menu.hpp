#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "sim/input_frame.hpp"

namespace rr::app {

/// The `MENUDATA` folders in the original's language order.
inline constexpr std::array<std::string_view, 9> kPickableLanguages{
    "ENGLISH", "SPANISH", "FRENCH", "GERMAN", "ITALIAN", "DANISH", "SWEDISH", "NORWEGI", "DUTCH"};

enum class LanguageRequest : std::uint8_t { None, Change, Leave };

/// A language outside `kPickableLanguages` starts on the first.
class LanguageMenu {
public:
    explicit LanguageMenu(std::string_view language);

    [[nodiscard]] LanguageRequest Tick(const sim::InputFrame& input);
    void FocusBack(bool isBackFocused) { isBackFocused_ = isBackFocused; }

    [[nodiscard]] std::size_t Index() const { return index_; }
    [[nodiscard]] std::string_view Language() const { return kPickableLanguages.at(index_); }
    [[nodiscard]] bool IsBackFocused() const { return isBackFocused_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    [[nodiscard]] LanguageRequest Step(std::size_t forward);

    std::size_t index_ = 0;
    bool isBackFocused_ = false;
};

}
