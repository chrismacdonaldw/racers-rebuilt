#include "sim/hazard/lava_blob.hpp"

#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr float kEdgeFrames = 10.0F;
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
    reach.centre = context.entities->NodeWorld(*entity, kLavaBlobNode).position;
    if (eventWait > Milliseconds{0}) {
        eventWait = eventWait > context.elapsed ? eventWait - context.elapsed : Milliseconds{0};
        return;
    }
    const float frame = context.entities->Frame(*entity);
    for (const LavaWindow& window : kLavaWindows) {
        for (const float edge : {window.start, window.end}) {
            if (IsJustPast(frame, edge)) {
                context.events->FireStartAndEnd(kLavaBlobEvent, window.splash);
                eventWait = kEventWait;
                isBubbling = edge == window.end;
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
            racer.React(VoiceReaction::Negative);
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
