#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "assets/event_id.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

inline constexpr std::size_t kCodeGates = 3;
inline constexpr assets::EventId kCodeSolvedEvent = 18;
inline constexpr assets::EventId kCodeJingleEvent = 28;

struct CodePuzzle {
    std::array<assets::EventId, kCodeGates> code{};
    std::uint8_t progress = 0;
    Milliseconds revealLeft{0};

    void Tick(HazardContext& context);
    void Hear(const TrackEvent& event, HazardContext& context);

    [[nodiscard]] static std::size_t MostEventsPerTick(std::size_t racerCount);

private:
    void Solve(HazardContext& context);
};

[[nodiscard]] CodePuzzle LoadCodePuzzle(Random& random);

}
