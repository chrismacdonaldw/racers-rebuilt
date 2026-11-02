#include "app/menu/menu_sounds.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "../../audio/alp_builder.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFrames = 4096;
constexpr std::array<std::uint8_t, 256> kPayload = [] {
    std::array<std::uint8_t, 256> payload{};
    payload.fill(0x77);
    return payload;
}();

std::span<const std::byte> Alp() {
    static const std::vector<std::byte> alp = test::BuildAlp(1, kRateHz, kPayload);
    return alp;
}

class Speaker {
public:
    Speaker() : mixer_(Headless()) {
        for (audio::SoundId& sound : bank_) {
            sound = mixer_.AddSound(assets::FileData(Alp())).value_or(audio::SoundId{});
        }
    }

    [[nodiscard]] MenuSounds Sounds() { return {mixer_, bank_}; }

    bool Heard() {
        std::array<float, kFrames * 2> output{};
        mixer_.Render(output);
        return std::ranges::any_of(output, [](float sample) { return sample != 0.0F; });
    }

private:
    static audio::Mixer Headless() {
        Result<audio::Mixer> mixer = audio::Mixer::CreateHeadless(kRateHz);
        REQUIRE(mixer.has_value());
        return std::move(mixer).value();
    }

    audio::Mixer mixer_;
    std::array<audio::SoundId, 2> bank_{};
};

}

TEST_CASE("menu sound ids count from 1 and stay silent outside the bank") {
    Speaker speaker;
    const MenuSounds sounds = speaker.Sounds();

    sounds.Play(0);
    CHECK_FALSE(speaker.Heard());
    sounds.Play(3);
    CHECK_FALSE(speaker.Heard());
    sounds.Play(2);
    CHECK(speaker.Heard());
}

TEST_CASE("menu sounds without a bank play nothing") {
    MenuSounds{}.Play(1);
    MenuSounds{}.Confirm({1, 1, 1, 1, 1});
}

TEST_CASE("confirming plays a control's release sound") {
    Speaker speaker;
    const MenuSounds sounds = speaker.Sounds();

    sounds.Confirm({1, 1, 0, 0, 1});
    CHECK_FALSE(speaker.Heard());
    sounds.Confirm({0, 0, 0, 1, 0});
    CHECK(speaker.Heard());
}

TEST_CASE("moving the highlight plays the old control's deselect and the new one's select") {
    Speaker speaker;
    const MenuSounds sounds = speaker.Sounds();

    sounds.MoveHighlight({1, 0, 0, 0, 0}, {0, 1, 0, 0, 0});
    CHECK_FALSE(speaker.Heard());
    sounds.MoveHighlight({0, 1, 0, 0, 0}, {});
    CHECK(speaker.Heard());
    sounds.MoveHighlight({}, {2, 0, 0, 0, 0});
    CHECK(speaker.Heard());
}

}
