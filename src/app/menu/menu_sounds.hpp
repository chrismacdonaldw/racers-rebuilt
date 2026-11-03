#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "app/menu/menu_files.hpp"
#include "app/sound_banks.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"

namespace rr::app {

inline constexpr std::string_view kMenuSoundBank = "MENUDATA/GENC0R0.SBK";

/// Ids count from 1 in bank order; others are silent. The mixer and bank must outlive it.
class MenuSounds {
public:
    MenuSounds() = default;
    MenuSounds(audio::Mixer& mixer, std::span<const audio::SoundId> bank)
        : mixer_(&mixer), bank_(bank) {}

    void Play(std::int32_t id, float volume = 1.0F) const;

    /// The old control's deselect sound, then the new one's select sound.
    void MoveHighlight(const assets::SoundIds& from, const assets::SoundIds& to) const;

    /// The press and release sounds together, as the port acts on the key going down.
    void Confirm(const assets::SoundIds& control) const;

    /// `step` is +1 or -1, on a list that wraps.
    void Scroll(const assets::CarouselSounds& carousel, int step) const;

private:
    audio::Mixer* mixer_ = nullptr;
    std::span<const audio::SoundId> bank_;
};

/// A missing style has no sounds.
[[nodiscard]] assets::SoundIds GameButtonSounds(const MenuFiles& files, std::string_view style);

[[nodiscard]] assets::SoundIds SelectorSounds(const MenuFiles& files, std::string_view style);

[[nodiscard]] assets::CarouselSounds ScrollSounds(const MenuFiles& files, std::string_view style);

/// Silent, with a warning, when the bank won't load.
[[nodiscard]] MenuSounds LoadMenuSounds(audio::Mixer& mixer, SoundBanks& banks,
                                        const assets::Vfs& vfs);

}
