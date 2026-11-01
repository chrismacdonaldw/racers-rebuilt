#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kLavaBlobTrigger = 0;
inline constexpr assets::EventId kLavaBlobEvent = 43;
inline constexpr std::size_t kLavaBlobNode = 0;

/// `start` and `end` are frames of the blob's part. `splash` is in game space.
struct LavaWindow {
    float start = 0.0F;
    float end = 0.0F;
    Vector3 splash;
};

inline constexpr std::array<LavaWindow, 3> kLavaWindows{
    {{.start = 0.0F, .end = 26.0F, .splash = {.x = 577.0F, .y = -444.0F, .z = 7.334F}},
     {.start = 61.0F, .end = 86.0F, .splash = {.x = 605.0F, .y = -500.0F, .z = 19.0F}},
     {.start = 119.0F, .end = 146.0F, .splash = {.x = 525.0F, .y = -505.0F, .z = 4.6F}}}};

struct LavaBlob {
    std::optional<std::size_t> entity;
    BoundingSphere reach;
    Milliseconds eventWait{0};
    bool isBubbling = false;

    void Activate() { eventWait = Milliseconds{0}; }
    void Tick(HazardContext& context);
    void Touch(HazardContext& context) const;
};

[[nodiscard]] LavaBlob LoadLavaBlob(const TrackEntities& entities);

}
