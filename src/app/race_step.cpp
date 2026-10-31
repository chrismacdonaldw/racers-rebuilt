#include "app/race_step.hpp"

#include <cstdint>
#include <optional>

#include "app/frame_loop.hpp"
#include "app/track_space.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::app {
namespace {

void AddBrick(const sim::Brick& brick, const Quaternion& spin,
              std::optional<assets::BrickColour> colour, sim::RenderSnapshot& out) {
    const float scale = sim::BrickScale(brick);
    if (scale > 0.0F) {
        out.bricks.push_back(
            {.transform = TransformToEngine({.position = brick.position, .rotation = spin}),
             .scale = scale,
             .colour = colour});
    }
}

void SnapshotBricks(const sim::BrickField& bricks, sim::RenderSnapshot& out) {
    out.bricks.clear();
    const Quaternion spin = FromAxisAngle({.z = 1.0F}, bricks.SpinRadians());
    for (const sim::ColourBrick& coloured : bricks.Coloured()) {
        AddBrick(coloured.brick, spin, coloured.colour, out);
    }
    for (const sim::WhiteBrick& white : bricks.White()) {
        AddBrick(white.brick, spin, std::nullopt, out);
    }
}

void SnapshotEffects(const sim::PowerupSystem& powerups, sim::RenderSnapshot& out) {
    out.effects.clear();
    for (const std::uint16_t slot : powerups.Active()) {
        const sim::EffectSlot& effect = powerups.Slots()[slot];
        out.effects.push_back({.slot = slot,
                               .kind = effect.kind,
                               .use = effect.use,
                               .transform = TransformToEngine(effect.effect->Pose())});
    }
}

}

void StepRace(sim::RaceSession& session, const sim::InputFrame& input,
              std::span<sim::RacerControls> controls) {
    controls[kPlayerSlot] = sim::ControlsFrom(input);
    session.Tick(kSimTick, controls);
}

void SnapshotRace(const sim::RaceSession& session, sim::RenderSnapshot& out) {
    out.entities.clear();
    out.racers.clear();
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        const sim::Racer& racer = session.Racers()[i];
        const sim::EntityId id{static_cast<std::uint32_t>(i + 1)};
        out.entities.push_back({.id = id,
                                .transform = TransformToEngine(racer.Pose()),
                                .flags = session.WasRespawned(i) ? sim::SnapshotFlags::Teleported
                                                                 : sim::SnapshotFlags::None});
        out.racers.push_back({.id = id,
                              .forwardSpeed = racer.Body().derived.forwardSpeed,
                              .turnRadius = racer.Body().turnRadius,
                              .hitReactions = racer.Status().hitReactions});
    }
    SnapshotBricks(session.Bricks(), out);
    SnapshotEffects(session.Powerups(), out);
    out.simStateHash = sim::HashRaceSession(session);
}

}
