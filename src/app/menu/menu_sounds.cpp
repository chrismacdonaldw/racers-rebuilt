#include "app/menu/menu_sounds.hpp"

#include <array>
#include <cstddef>
#include <span>

#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kSelect = 0;
constexpr std::size_t kDeselect = 1;
constexpr std::size_t kPress = 2;
constexpr std::size_t kRelease = 3;
constexpr std::size_t kNext = 0;
constexpr std::size_t kPrevious = 1;

assets::SoundIds SoundsOf(const assets::IconStyle& icon) {
    return icon.sounds.value_or(assets::SoundIds{});
}

}

void MenuSounds::Play(std::int32_t id, float volume) const {
    if (mixer_ == nullptr || id <= 0 || static_cast<std::size_t>(id) > bank_.size()) {
        return;
    }
    const std::array cue{audio::SoundCue{.sound = bank_[static_cast<std::size_t>(id) - 1],
                                         .mix = {.volume = volume}}};
    mixer_->Play(cue);
}

void MenuSounds::MoveHighlight(const assets::SoundIds& from, const assets::SoundIds& to) const {
    Play(from[kDeselect]);
    Play(to[kSelect]);
}

void MenuSounds::Confirm(const assets::SoundIds& control) const {
    Play(control[kPress]);
    Play(control[kRelease]);
}

void MenuSounds::Scroll(const assets::CarouselSounds& carousel, int step) const {
    Play(carousel[step > 0 ? kNext : kPrevious]);
}

assets::SoundIds GameButtonSounds(const MenuFiles& files, std::string_view style) {
    const assets::GameButtonStyle* found = files.FindGameButtonStyle(style);
    return found != nullptr ? SoundsOf(found->icon) : assets::SoundIds{};
}

assets::SoundIds SelectorSounds(const MenuFiles& files, std::string_view style) {
    const assets::SelectorStyle* found = files.FindSelectorStyle(style);
    return found != nullptr ? SoundsOf(found->icon) : assets::SoundIds{};
}

assets::CarouselSounds ScrollSounds(const MenuFiles& files, std::string_view style) {
    const assets::CarouselStyle* found = files.FindCarouselStyle(style);
    return found != nullptr ? found->sounds : assets::CarouselSounds{};
}

MenuSounds LoadMenuSounds(audio::Mixer& mixer, SoundBanks& banks, const assets::Vfs& vfs) {
    Result<std::span<const audio::SoundId>> bank = banks.Load(mixer, vfs, kMenuSoundBank);
    if (!bank) {
        log::Warn("the menus are silent: {}", bank.error().message);
        return {};
    }
    return {mixer, *bank};
}

}
