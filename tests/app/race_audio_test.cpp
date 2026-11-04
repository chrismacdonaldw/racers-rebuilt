#include "app/race_audio.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "../audio/alp_builder.hpp"
#include "../sim/physics/plane_world.hpp"
#include "../sim/race/checkpoint_ring.hpp"
#include "app/track_space.hpp"
#include "sim/powerup/green_effects.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kGridSize = 6;
constexpr std::size_t kEffectCount = 80;
constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFramesPerTick = 265;
constexpr Milliseconds kTick{12};
constexpr int kTicksPastGo = 600;
constexpr std::int32_t kSurfaceSound = 3000;
constexpr std::array kEnginePitches{0.84F, 0.96F};
constexpr std::size_t kStrongTurboHum = 43;
constexpr int kMostTurboTicks = 1000;
constexpr std::array<std::uint8_t, 256> kPayload = [] {
    std::array<std::uint8_t, 256> payload{};
    payload.fill(0x77);
    return payload;
}();
constexpr std::array<std::uint8_t, 256> kSilence{};

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
    explicit FullGrid(std::optional<std::int32_t> surfaceSound = kSurfaceSound,
                      std::optional<std::size_t> onlyAudible = std::nullopt)
        : sound_(test::BuildAlp(1, kRateHz, kPayload)),
          silence_(test::BuildAlp(1, kRateHz, kSilence)),
          mixer_(HeadlessMixer()),
          world_({test::TestPlane{.surface = {.soundId = surfaceSound}}}),
          grid_(Grid()),
          session_(Start()) {
        for (std::size_t i = 0; i <= kEffectCount; ++i) {
            const bool isAudible = !onlyAudible || *onlyAudible == i;
            sounds_.push_back(mixer_.AddSound(assets::FileData(isAudible ? sound_ : silence_))
                                  .value_or(audio::SoundId{}));
        }
        audio_ = std::make_unique<RaceAudio>(
            mixer_,
            RaceSoundSet{.effects = std::span(sounds_).first(kEffectCount),
                         .ambient = std::span(sounds_).last(1),
                         .music = {"start", "royal", "lose", "win"}},
            RaceAudioSetup{.racerCount = kGridSize, .seed = 3, .enginePitches = kEnginePitches});
    }

    void StopLoopsAndRender() {
        audio_->StopLoops();
        mixer_.Render(output_);
    }

    std::uint64_t TickAndRender() {
        session_.Tick(kTick, controls_);
        const Vector3& player = session_.Racers()[0].Body().motion.position;
        audio_->Hear(session_, {.position = ToEngine(player)}, kTick);
        const std::uint64_t before = mixer_.AllocationCount();
        mixer_.Render(output_);
        return mixer_.AllocationCount() - before;
    }

    void Use(assets::BrickColour colour, std::uint8_t level) {
        session_.GivePowerup(0, colour, level);
        controls_[0].isUsePressed = true;
        static_cast<void>(TickAndRender());
        controls_[0].isUsePressed = false;
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
        sim::AddGreenEffects(setup.effects);
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            setup, {.checkpoints = &graph_, .grid = grid_, .world = &world_, .lowestTrackZ = -100});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    std::vector<std::byte> sound_;
    std::vector<std::byte> silence_;
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

TEST_CASE("the nearest opponent's engine waits for the go") {
    FullGrid race(std::nullopt);
    bool isSilent = true;

    static_cast<void>(race.TickAndRender());
    while (race.Session().Clock().Phase() == sim::RacePhase::Intro) {
        isSilent = isSilent && race.LastRenderIsSilent();
        static_cast<void>(race.TickAndRender());
    }

    CHECK(isSilent);
}

TEST_CASE("stopping the loops silences a racing grid") {
    FullGrid race;
    while (race.Session().Clock().Phase() != sim::RacePhase::Racing) {
        static_cast<void>(race.TickAndRender());
    }
    for (int tick = 0; tick < kTicksPastGo; ++tick) {
        static_cast<void>(race.TickAndRender());
    }

    race.StopLoopsAndRender();

    CHECK(race.LastRenderIsSilent());
}

TEST_CASE("a turbo hums at its level until it ends") {
    FullGrid race(std::nullopt, kStrongTurboHum);
    while (race.Session().Clock().Phase() != sim::RacePhase::Racing) {
        static_cast<void>(race.TickAndRender());
    }

    race.Use(assets::BrickColour::Green, 1);
    REQUIRE(race.Session().Racers()[0].ActiveTurbo().has_value());
    bool isHumming = true;
    int ticks = 0;
    while (race.Session().Racers()[0].ActiveTurbo() && ticks++ < kMostTurboTicks) {
        static_cast<void>(race.TickAndRender());
        isHumming =
            isHumming && (!race.Session().Racers()[0].ActiveTurbo() || !race.LastRenderIsSilent());
    }
    static_cast<void>(race.TickAndRender());

    CHECK(isHumming);
    CHECK(race.LastRenderIsSilent());
}

}
}
