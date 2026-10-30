#include "audio/mixer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <optional>
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

TEST_CASE_FIXTURE(MixerFixture, "a looping effect plays until it stops") {
    const std::optional<VoiceId> voice = mixer_.Loop({.sound = effectId_});
    REQUIRE(voice.has_value());

    for (int render = 0; render < 4; ++render) {
        CHECK_FALSE(RenderIsSilent());
    }

    mixer_.Stop(voice.value_or(VoiceId{}));
    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "mixing a voice to zero volume silences it") {
    const std::optional<VoiceId> voice = mixer_.Loop({.sound = effectId_});
    REQUIRE(voice.has_value());

    mixer_.Mix(voice.value_or(VoiceId{}), {.volume = 0.0F, .pitch = 2.0F, .pan = 1.0F});

    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "a stopped voice's id leaves the next voice in its slot alone") {
    const std::optional<VoiceId> stopped = mixer_.Loop({.sound = effectId_});
    REQUIRE(stopped.has_value());
    mixer_.Stop(stopped.value_or(VoiceId{}));

    const std::optional<VoiceId> next = mixer_.Loop({.sound = effectId_});
    mixer_.Mix(stopped.value_or(VoiceId{}), {.volume = 0.0F});
    mixer_.Stop(stopped.value_or(VoiceId{}));

    REQUIRE(next.has_value());
    CHECK(next.transform([](VoiceId id) { return id.slot; }) ==
          stopped.transform([](VoiceId id) { return id.slot; }));
    CHECK_FALSE(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "every effect voice can loop at once and the next is refused") {
    constexpr int kEffectVoices = 32;
    for (int voice = 0; voice < kEffectVoices; ++voice) {
        CHECK(mixer_.Loop({.sound = effectId_}).has_value());
    }

    CHECK_FALSE(mixer_.Loop({.sound = effectId_}).has_value());
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

TEST_CASE_FIXTURE(MixerFixture, "changing looping voices as they mix allocates nothing") {
    std::vector<VoiceId> voices;
    voices.reserve(12);
    for (int voice = 0; voice < 12; ++voice) {
        voices.push_back(mixer_.Loop({.sound = effectId_}).value_or(VoiceId{}));
    }
    const std::uint64_t allocations = mixer_.AllocationCount();

    for (int render = 0; render < 16; ++render) {
        const float shift = static_cast<float>(render) / 16.0F;
        for (const VoiceId voice : voices) {
            mixer_.Mix(voice, {.volume = 1.0F - shift, .pitch = 0.5F + shift, .pan = shift});
        }
        mixer_.Render(output_);
    }

    CHECK(mixer_.AllocationCount() == allocations);
}

}
}
