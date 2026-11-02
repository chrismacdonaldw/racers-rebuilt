#include "app/menu/language_menu.hpp"

#include <algorithm>
#include <iterator>

namespace rr::app {
namespace {

constexpr std::size_t kLanguageCount = kPickableLanguages.size();
constexpr std::uint64_t kHashBase = 32;

}

LanguageMenu::LanguageMenu(std::string_view language) {
    const auto found = std::ranges::find(kPickableLanguages, language);
    if (found != kPickableLanguages.end()) {
        index_ = static_cast<std::size_t>(std::distance(kPickableLanguages.begin(), found));
    }
}

LanguageRequest LanguageMenu::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Up) != input.WasPressed(sim::MenuAction::Down)) {
        isBackFocused_ = !isBackFocused_;
    }
    if (input.WasPressed(sim::MenuAction::Back) ||
        (isBackFocused_ && input.WasPressed(sim::MenuAction::Confirm))) {
        return LanguageRequest::Leave;
    }
    if (isBackFocused_) {
        return LanguageRequest::None;
    }
    if (input.WasPressed(sim::MenuAction::Left)) {
        return Step(kLanguageCount - 1);
    }
    if (input.WasPressed(sim::MenuAction::Right)) {
        return Step(1);
    }
    return LanguageRequest::None;
}

std::uint64_t LanguageMenu::StateHash() const {
    return (index_ * kHashBase) + (isBackFocused_ ? 1U : 0U);
}

LanguageRequest LanguageMenu::Step(std::size_t forward) {
    index_ = (index_ + forward) % kLanguageCount;
    return LanguageRequest::Change;
}

}
