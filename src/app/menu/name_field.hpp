#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace rr::app {

enum class NameEdit : std::uint8_t { Typed, Erased, Refused };

class NameField {
public:
    explicit NameField(std::size_t maxLength) : maxLength_(maxLength) {}

    [[nodiscard]] const std::u16string& Text() const { return text_; }
    [[nodiscard]] bool IsFull() const { return text_.size() >= maxLength_; }
    [[nodiscard]] const std::u16string& Allowed() const { return allowed_; }

    void Allow(std::u16string allowed) { allowed_ = std::move(allowed); }
    void SetText(std::u16string_view text);
    NameEdit Type(char16_t typed);
    NameEdit Erase();

private:
    std::u16string allowed_;
    std::size_t maxLength_;
    std::u16string text_;
};

/// As the licence turns a typed key into a character: Latin-1 letters from `0xE1` up lose `0x20`.
[[nodiscard]] char16_t LicenceUpper(char16_t typed);

/// Latin-1, as the original narrows a name before checking it for cheat plates.
[[nodiscard]] std::string PlateText(std::u16string_view name);

}
