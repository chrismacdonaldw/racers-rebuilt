#include "app/race_step.hpp"

#include <cstdint>

#include "app/frame_loop.hpp"
#include "app/track_space.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::app {

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
                              .turnRadius = racer.Body().turnRadius});
    }
    out.simStateHash = sim::HashRaceSession(session);
}

}
