#include "app/positional_sound.hpp"

#include <algorithm>
#include <limits>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr float kPanScale = 0.7F;

float Falloff(float distanceSquared, SoundRange range) {
    const float min = range.min * range.min;
    const float max = range.max * range.max;
    if (distanceSquared <= min) {
        return 1.0F;
    }
    if (distanceSquared >= max) {
        return 0.0F;
    }
    return 1.0F - ((distanceSquared - min) / (max - min));
}

}

Listener ListenerAt(std::span<const Transform> cameras) {
    Listener listener{.earCount = std::min(cameras.size(), sim::kMaxPlayers)};
    for (std::size_t i = 0; i < listener.earCount; ++i) {
        listener.ears.at(i) = {.position = cameras[i].position,
                               .right = Rotate(cameras[i].rotation, {.x = 1.0F})};
    }
    return listener;
}

Listener ListenerAt(const Transform& camera) {
    return ListenerAt(std::span(&camera, 1));
}

float DistanceSquaredToNearestEar(const Listener& listener, const Vector3& source) {
    float nearest = std::numeric_limits<float>::max();
    for (const Ear& ear : listener.Ears()) {
        const Vector3 offset = ToEngine(source) - ear.position;
        nearest = std::min(nearest, Dot(offset, offset));
    }
    return nearest;
}

audio::VoiceMix PlaceSound(const Listener& listener, const Vector3& source,
                           const audio::VoiceMix& mix, SoundRange range) {
    float loudest = 0.0F;
    float heard = 0.0F;
    float weightedPan = 0.0F;
    for (const Ear& ear : listener.Ears()) {
        const Vector3 offset = ToEngine(source) - ear.position;
        const float distanceSquared = Dot(offset, offset);
        const float side = Dot(offset, ear.right);
        const float pan = distanceSquared > 0.0F ? kPanScale * side * side / distanceSquared *
                                                       (side < 0.0F ? -1.0F : 1.0F)
                                                 : 0.0F;
        const float falloff = Falloff(distanceSquared, range);
        loudest = std::max(loudest, falloff);
        heard += falloff;
        weightedPan += falloff * pan;
    }
    return {.volume = mix.volume * loudest,
            .pitch = mix.pitch,
            .pan = heard > 0.0F ? weightedPan / heard : 0.0F};
}

}
