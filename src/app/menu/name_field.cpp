#include "app/menu/name_field.hpp"

namespace rr::app {
namespace {

constexpr char16_t kCaseOffset = 0x20;
constexpr char16_t kFirstLatinLower = 0xE1;
constexpr char16_t kLastLatin1 = 0xFF;

}

void NameField::SetText(std::u16string_view text) {
    text_ = text.substr(0, maxLength_);
}

NameEdit NameField::Type(char16_t typed) {
    if (IsFull() || !allowed_.contains(typed)) {
        return NameEdit::Refused;
    }
    text_.push_back(typed);
    return NameEdit::Typed;
}

NameEdit NameField::Erase() {
    if (text_.empty()) {
        return NameEdit::Refused;
    }
    text_.pop_back();
    return NameEdit::Erased;
}

char16_t LicenceUpper(char16_t typed) {
    const bool isAsciiLower = typed >= u'a' && typed <= u'z';
    const bool isLatinLower = typed >= kFirstLatinLower && typed <= kLastLatin1;
    return isAsciiLower || isLatinLower ? static_cast<char16_t>(typed - kCaseOffset) : typed;
}

std::string PlateText(std::u16string_view name) {
    std::string plate;
    plate.reserve(name.size());
    for (const char16_t unit : name) {
        plate.push_back(static_cast<char>(static_cast<unsigned char>(unit)));
    }
    return plate;
}

}
