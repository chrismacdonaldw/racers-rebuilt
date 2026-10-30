#include "app/race_audio.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "../audio/alp_builder.hpp"
#include "../sim/physics/plane_world.hpp"
#include "../sim/race/checkpoint_ring.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kGridSize = 6;
constexpr std::size_t kEffectCount = 80;
constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFramesPerTick = 265;
constexpr Milliseconds kTick{12};
constexpr int kTicksPastGo = 600;
constexpr std::array<std::uint8_t, 256> kPayload = [] {
    std::array<std::uint8_t, 256> payload{};
    payload.fill(0x77);
    return payload;
}();

audio::Mixer HeadlessMixer() {
    Result<audio::Mixer> mixer = audio::Mixer::CreateHeadless(kRateHz);
    REQUIRE(mixer.has_value());
    return std::move(mixer).value();
}

std::vector<sim::RacerStart> Grid() {
    std::vector<sim::RacerStart> grid;
    grid.reserve(kGridSize);
    for (std::size_t slot = 0; slot < kGridSize; ++slot) {
        grid.push_back(
            {.position = {.x = -20.0F, .y = static_cast<float>(slot) * -10.0F, .z = 0.2F}});
    }
    return grid;
}

class FullGrid {
public:
    FullGrid()
        : sound_(test::BuildAlp(1, kRateHz, kPayload)),
          mixer_(HeadlessMixer()),
          world_({test::TestPlane{.surface = {.soundId = 3000}}}),
          grid_(Grid()),
          session_(Start()) {
        for (std::size_t i = 0; i <= kEffectCount; ++i) {
            sounds_.push_back(mixer_.AddSound(assets::FileData(sound_)).value_or(audio::SoundId{}));
        }
        audio_ = std::make_unique<RaceAudio>(
            mixer_,
            RaceSoundSet{.effects = std::span(sounds_).first(kEffectCount),
                         .ambient = std::span(sounds_).last(1),
                         .music = {"start", "royal", "lose", "win"}},
            RaceAudioSetup{.racerCount = kGridSize, .seed = 3});
    }

    std::uint64_t TickAndRender() {
        session_.Tick(kTick, controls_);
        const Vector3& player = session_.Racers()[0].Body().motion.position;
        audio_->Hear(session_, {.position = ToEngine(player)}, kTick);
        const std::uint64_t before = mixer_.AllocationCount();
        mixer_.Render(output_);
        return mixer_.AllocationCount() - before;
    }

    [[nodiscard]] bool LastRenderIsSilent() const {
        return std::ranges::all_of(output_, [](float sample) { return sample == 0.0F; });
    }

    [[nodiscard]] const sim::RaceSession& Session() const { return session_; }
    [[nodiscard]] const RaceAudio& Audio() const { return *audio_; }

private:
    sim::RaceSession Start() {
        sim::RaceSetup setup{.racers = std::vector<sim::RacerEntry>(kGridSize)};
        setup.racers[0].isPlayer = true;
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            setup, {.checkpoints = &graph_, .grid = grid_, .world = &world_, .lowestTrackZ = -100});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    std::vector<std::byte> sound_;
    audio::Mixer mixer_;
    std::vector<audio::SoundId> sounds_;
    sim::CheckpointGraph graph_ = test::Ring();
    test::PlaneWorld world_;
    std::vector<sim::RacerStart> grid_;
    sim::RaceSession session_;
    std::unique_ptr<RaceAudio> audio_;
    std::vector<sim::RacerControls> controls_ =
        std::vector<sim::RacerControls>(kGridSize, {.isThrottleHeld = true});
    std::array<float, kFramesPerTick * 2> output_{};
};

TEST_CASE("a full grid's race sounds mix without allocating on the audio thread") {
    FullGrid race;
    std::uint64_t renderAllocations = 0;

    while (race.Session().Clock().Phase() != sim::RacePhase::Racing) {
        renderAllocations += race.TickAndRender();
    }
    for (int tick = 0; tick < kTicksPastGo; ++tick) {
        renderAllocations += race.TickAndRender();
    }

    CHECK(renderAllocations == 0);
    CHECK_FALSE(race.LastRenderIsSilent());
    CHECK(race.Audio().Music().track == "royal");
}

}
}
