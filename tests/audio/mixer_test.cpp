#include "audio/mixer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "audio/alp_builder.hpp"
#include "audio/decoded_file.hpp"

namespace rr::audio {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFramesPerRender = 1024;
constexpr float kHeadroom = 0.5F;
constexpr float kHalfVolumeGain = 0.17783F * kHeadroom;
constexpr float kPannedAwayGain = 0.13335F * kHeadroom;
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

    std::array<float, 2> RenderPeaks() {
        mixer_.Render(output_);
        std::array<float, 2> peaks{};
        for (std::size_t sample = 0; sample < output_.size(); ++sample) {
            peaks.at(sample % 2) = std::max(peaks.at(sample % 2), std::abs(output_.at(sample)));
        }
        return peaks;
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

TEST_CASE_FIXTURE(MixerFixture, "a started effect plays until it finishes or stops") {
    const std::optional<VoiceId> finishing = mixer_.Start({.sound = effectId_});
    REQUIRE(finishing.has_value());
    CHECK(mixer_.IsPlaying(finishing.value_or(VoiceId{})));
    CHECK_FALSE(RenderIsSilent());
    CHECK(RenderIsSilent());
    CHECK_FALSE(mixer_.IsPlaying(finishing.value_or(VoiceId{})));

    const std::optional<VoiceId> stopped = mixer_.Start({.sound = effectId_});
    REQUIRE(stopped.has_value());
    mixer_.Stop(stopped.value_or(VoiceId{}));
    CHECK_FALSE(mixer_.IsPlaying(stopped.value_or(VoiceId{})));
    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "a mix that isn't finite plays as the default mix") {
    const float nan = std::nanf("");
    const std::optional<VoiceId> voice =
        mixer_.Loop({.sound = effectId_, .mix = {.volume = nan, .pitch = nan, .pan = nan}});
    REQUIRE(voice.has_value());

    const std::array<float, 2> peaks = RenderPeaks();
    CHECK(std::isfinite(peaks[0]));
    CHECK(peaks[0] > 0.0F);
    CHECK(peaks[0] == doctest::Approx(peaks[1]));

    mixer_.Mix(voice.value_or(VoiceId{}),
               {.volume = std::numeric_limits<float>::infinity(), .pitch = -nan});
    const std::array<float, 2> mixed = RenderPeaks();
    CHECK(std::isfinite(mixed[0]));
    CHECK(std::isfinite(mixed[1]));
}

TEST_CASE_FIXTURE(MixerFixture, "music keeps looping past the end of its track") {
    REQUIRE(mixer_.PlayMusic(trackId_).has_value());

    for (int render = 0; render < 4; ++render) {
        CHECK_FALSE(RenderIsSilent());
    }

    mixer_.StopMusic();
    CHECK(RenderIsSilent());
}

TEST_CASE_FIXTURE(MixerFixture, "music time counts what has played and stops with the track") {
    constexpr std::size_t kTrackFrames = 4410;
    REQUIRE(mixer_
                .PlayMusic(SampleStream({.channelCount = 2, .sampleRateHz = kRateHz},
                                        std::vector<std::int16_t>(kTrackFrames * 2, 1000)),
                           Milliseconds{0})
                .has_value());

    CHECK(mixer_.MusicTime() == Milliseconds{0});
    mixer_.Render(output_);
    mixer_.Render(output_);
    CHECK(mixer_.MusicTime() ==
          Milliseconds{static_cast<std::int64_t>(kFramesPerRender * 2 * 1000 / kRateHz)});

    for (int render = 0; render < 3; ++render) {
        mixer_.Render(output_);
    }
    CHECK_FALSE(mixer_.MusicTime().has_value());
}

TEST_CASE_FIXTURE(MixerFixture, "headless music time stands still until the mixer renders") {
    REQUIRE(mixer_
                .PlayMusic(SampleStream({.channelCount = 2, .sampleRateHz = kRateHz},
                                        std::vector<std::int16_t>(kFramesPerRender * 2, 1000)),
                           Milliseconds{0})
                .has_value());

    CHECK_FALSE(mixer_.HasDevice());
    CHECK(mixer_.IsMusicPlaying());
    CHECK(mixer_.MusicTime() == Milliseconds{0});
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

TEST_CASE_FIXTURE(MixerFixture, "a voice at half volume plays 15 dB down") {
    static_cast<void>(mixer_.Loop({.sound = effectId_, .mix = {.volume = 0.5F}}));

    const std::array<float, 2> peaks = RenderPeaks();

    CHECK(peaks[0] == doctest::Approx(kHalfVolumeGain).epsilon(0.01));
    CHECK(peaks[1] == doctest::Approx(kHalfVolumeGain).epsilon(0.01));
}

TEST_CASE_FIXTURE(MixerFixture, "a bus volume scales its voices before the volume curve") {
    mixer_.SetVolume(Bus::Effects, 0.5F);
    static_cast<void>(mixer_.Loop({.sound = effectId_}));

    CHECK(RenderPeaks()[0] == doctest::Approx(kHalfVolumeGain).epsilon(0.01));

    mixer_.SetVolume(Bus::Effects, 1.0F);

    CHECK(RenderPeaks()[0] == doctest::Approx(kHeadroom).epsilon(0.01));
}

TEST_CASE_FIXTURE(MixerFixture, "panning right cuts the left channel and keeps the right") {
    static_cast<void>(mixer_.Loop({.sound = effectId_, .mix = {.pan = 0.7F}}));

    const std::array<float, 2> peaks = RenderPeaks();

    CHECK(peaks[0] == doctest::Approx(kPannedAwayGain).epsilon(0.01));
    CHECK(peaks[1] == doctest::Approx(kHeadroom).epsilon(0.01));
}

TEST_CASE_FIXTURE(MixerFixture, "in mono a panned voice plays evenly on both channels") {
    static_cast<void>(mixer_.Loop({.sound = effectId_, .mix = {.pan = 0.7F}}));

    mixer_.SetStereo(false);
    const std::array<float, 2> peaks = RenderPeaks();

    CHECK(peaks[0] == doctest::Approx((kPannedAwayGain + kHeadroom) / 2.0F).epsilon(0.01));
    CHECK(peaks[1] == doctest::Approx(peaks[0]));
}

TEST_CASE_FIXTURE(MixerFixture, "a stack of full-volume voices stays within full scale") {
    for (int voice = 0; voice < 8; ++voice) {
        static_cast<void>(mixer_.Loop({.sound = effectId_}));
    }

    const std::array<float, 2> peaks = RenderPeaks();

    CHECK(peaks[0] <= 1.0F);
    CHECK(peaks[1] <= 1.0F);
    CHECK(peaks[0] == doctest::Approx(1.0F).epsilon(0.01));
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
