#include "sim/hazard/lava_blob.hpp"

#include <array>

#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kBlobNode = 0;
constexpr std::array<float, 3> kWindowStarts{0.0F, 61.0F, 119.0F};
constexpr std::array<float, 3> kWindowEnds{26.0F, 86.0F, 146.0F};
constexpr float kEdgeFrames = 10.0F;
constexpr std::array<Vector3, 3> kSplashPoints{{{.x = 577.0F, .y = -444.0F, .z = 7.334F},
                                                {.x = 605.0F, .y = -500.0F, .z = 19.0F},
                                                {.x = 525.0F, .y = -505.0F, .z = 4.6F}}};
constexpr Milliseconds kEventWait{400};
constexpr float kSpinTurns = 1.0F;
constexpr float kSpinRatePerMs = 0.01F;

bool IsJustPast(float frame, float edge) {
    return edge < frame && frame < edge + kEdgeFrames;
}

}

void LavaBlob::Tick(HazardContext& context) {
    if (!entity) {
        return;
    }
    reach.centre = context.entities->NodeWorld(*entity, kBlobNode).position;
    if (eventWait > Milliseconds{0}) {
        eventWait = eventWait > context.elapsed ? eventWait - context.elapsed : Milliseconds{0};
        return;
    }
    const float frame = context.entities->Frame(*entity);
    for (std::size_t i = 0; i < kSplashPoints.size(); ++i) {
        for (const float edge : {kWindowStarts[i], kWindowEnds[i]}) {
            if (IsJustPast(frame, edge)) {
                context.events->FireStartAndEnd(kLavaBlobEvent, kSplashPoints[i]);
                eventWait = kEventWait;
            }
        }
    }
}

void LavaBlob::Touch(HazardContext& context) const {
    if (!entity) {
        return;
    }
    for (std::size_t i = 0; i < context.racers.size(); ++i) {
        Racer& racer = context.racers[i];
        if (Touches(reach, context.reaches[i]) && !racer.IsSpinning() && !racer.IsStopped()) {
            racer.StartSpin(kSpinTurns, kSpinRatePerMs);
            racer.ReactToHit();
        }
    }
}

LavaBlob LoadLavaBlob(const TrackEntities& entities) {
    LavaBlob blob{.entity = entities.Find("mmlavbl")};
    if (blob.entity) {
        blob.reach.radius = entities.Entity(*blob.entity).radius;
    }
    return blob;
}

}
