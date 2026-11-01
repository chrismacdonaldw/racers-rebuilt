#pragma once

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/race_step.hpp"
#include "app/scene.hpp"
#include "assets/brick_layout.hpp"
#include "core/duration.hpp"
#include "sim/input_frame.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/race/race_session.hpp"
#include "sim/replay/replay_recorder.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::test {

struct PowerupGift {
    std::uint64_t tick = 0;
    assets::BrickColour colour = assets::BrickColour::Green;
    std::uint8_t level = 0;
};

/// A race with the player in slot 0, without drawing anything.
class RaceDriveScene final : public app::Scene {
public:
    RaceDriveScene(const sim::RaceSetup& setup, const sim::RaceCourse& course,
                   std::span<const PowerupGift> gifts)
        : session_(Start(setup, course)),
          controls_(session_.Racers().size()),
          gifts_(gifts.begin(), gifts.end()),
          uses_(session_.Racers().size()) {}

    void Tick(const sim::InputFrame& input) override {
        for (const PowerupGift& gift : gifts_) {
            if (gift.tick == ticks_) {
                session_.GivePowerup(app::kPlayerSlot, gift.colour, gift.level);
            }
        }
        std::vector<bool> held(controls_.size());
        for (std::size_t i = 0; i < controls_.size(); ++i) {
            held[i] = session_.Inventories()[i].Colour().has_value();
        }
        app::StepRace(session_, input, controls_);
        for (std::size_t i = 0; i < controls_.size(); ++i) {
            respawns_ += session_.WasRespawned(i) ? 1 : 0;
            mostLaps_ = std::max(mostLaps_, session_.Progress().OfRacer(i).laps.CompletedLaps());
            uses_[i] += held[i] && !session_.Inventories()[i].Colour().has_value() ? 1 : 0;
        }
        ++ticks_;
    }

    void Snapshot(sim::RenderSnapshot& out) const override { app::SnapshotRace(session_, out); }

    void Render(const sim::RenderSnapshot& /*interpolated*/) override {}

    [[nodiscard]] int Respawns() const { return respawns_; }
    [[nodiscard]] int MostLaps() const { return mostLaps_; }
    [[nodiscard]] const std::vector<int>& PowerupUses() const { return uses_; }

private:
    static sim::RaceSession Start(const sim::RaceSetup& setup, const sim::RaceCourse& course) {
        Result<sim::RaceSession> session = sim::RaceSession::Create(setup, course);
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    sim::RaceSession session_;
    std::vector<sim::RacerControls> controls_;
    std::vector<PowerupGift> gifts_;
    std::vector<int> uses_;
    std::uint64_t ticks_ = 0;
    int respawns_ = 0;
    int mostLaps_ = 0;
};

/// Feeds a replay's input to the scene and keeps every tick's snapshot hash.
class ReplayDriver final : public app::TickTap {
public:
    explicit ReplayDriver(sim::ReplayPlayer& player) : player_(&player) {}

    void SceneStarted() override {}

    void BeforeTick(sim::InputFrame& input) override {
        isReplaying_ = !player_->IsFinished();
        if (isReplaying_) {
            input = player_->Next();
        }
    }

    void AfterTick(const sim::InputFrame& /*input*/, const sim::RenderSnapshot& after) override {
        if (isReplaying_) {
            hashes.push_back(sim::HashSnapshot(after));
            player_->Check(after);
        }
    }

    std::vector<std::uint64_t> hashes;

private:
    sim::ReplayPlayer* player_;
    bool isReplaying_ = false;
};

/// The scene that ran, for checking what the race went through.
struct DriveResult {
    std::vector<std::uint64_t> hashes;
    std::size_t checkedTicks = 0;
    std::optional<std::uint64_t> firstMismatch;
    int respawns = 0;
    int mostLaps = 0;
    std::vector<int> powerupUses;
};

/// Plays `replay` through a frame loop that renders at `frameTimes`.
[[nodiscard]] inline DriveResult Drive(const sim::Replay& replay,
                                       std::vector<std::uint64_t> expected,
                                       const sim::RaceSetup& setup, const sim::RaceCourse& course,
                                       std::span<const Nanoseconds> frameTimes,
                                       std::span<const PowerupGift> gifts = {}) {
    sim::ReplayPlayer player(replay, std::move(expected));
    ReplayDriver driver(player);
    app::FrameLoop loop;
    loop.SetTickTap(&driver);
    auto owned = std::make_unique<RaceDriveScene>(setup, course, gifts);
    const RaceDriveScene& scene = *owned;
    loop.Scenes().Push(std::move(owned));
    for (const Nanoseconds now : frameTimes) {
        static_cast<void>(loop.RunFrame(now));
    }
    return {.hashes = std::move(driver.hashes),
            .checkedTicks = player.CheckedTicks(),
            .firstMismatch = player.FirstMismatch(),
            .respawns = scene.Respawns(),
            .mostLaps = scene.MostLaps(),
            .powerupUses = scene.PowerupUses()};
}

}
