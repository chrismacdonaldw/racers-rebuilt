#include "audio/mixer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "audio/alp_builder.hpp"

namespace rr::audio {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFramesPerRender = 1024;
constexpr std::array<std::uint8_t, 256> kLoudPayload = [] {
    std::array<std::uint8_t, 256> payload{};
    payload.fill(0x77);
    return payload;
}();

Mixer CreateMixer() {
    Result<Mixer> mixer = Mixer::CreateHeadless(kRateHz);
    REQUIRE(mixer.has_value());
    return std::move(mixer).value();
}

class MixerFixture {
public:
    MixerFixture()
        : effect_(test::BuildAlp(1, kRateHz, kLoudPayload)),
          track_(test::BuildAlp(2, std::nullopt, kLoudPayload)),
          mixer_(CreateMixer()),
          effectId_(Add(effect_)),
          trackId_(Add(track_)) {}

    bool RenderIsSilent() {
        std::ranges::fill(output_, 1.0F);
        mixer_.Render(output_);
        return std::ranges::all_of(output_, [](float sample) { return sample == 0.0F; });
    }

protected:
    SoundId Add(const std::vector<std::byte>& file) {
        const Result<SoundId> sound = mixer_.AddSound(assets::FileData(file));
        REQUIRE(sound.has_value());
        return sound.value_or(SoundId{});
    }

    std::vector<std::byte> effect_;
    std::vector<std::byte> track_;
    Mixer mixer_;
    SoundId effectId_;
    SoundId trackId_;
    std::array<float, kFramesPerRender * 2> output_{};
};

TEST_CASE_FIXTURE(MixerFixture, "an effect plays once and then goes silent") {
    const std::array cues{SoundCue{.sound = effectId_}};

    mixer_.Play(cues);

    CHECK_FALSE(RenderIsSilent());
    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "music keeps looping past the end of its track") {
    REQUIRE(mixer_.PlayMusic(trackId_).has_value());

    for (int render = 0; render < 4; ++render) {
        CHECK_FALSE(RenderIsSilent());
    }

    mixer_.StopMusic();
    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "a bus at zero volume is silent") {
    const std::array cues{SoundCue{.sound = effectId_, .bus = Bus::Voice}};
    mixer_.SetVolume(Bus::Voice, 0.0F);

    mixer_.Play(cues);

    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "mixing allocates nothing") {
    const std::array cues{SoundCue{.sound = effectId_}, SoundCue{.sound = effectId_}};
    REQUIRE(mixer_.PlayMusic(trackId_).has_value());
    mixer_.Play(cues);
    const std::uint64_t allocations = mixer_.AllocationCount();
    REQUIRE(allocations > 0);

    for (int render = 0; render < 8; ++render) {
        mixer_.Render(output_);
    }

    CHECK(mixer_.AllocationCount() == allocations);
}

}
}
