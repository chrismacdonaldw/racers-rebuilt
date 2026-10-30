#include "app/positional_sound.hpp"

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

Listener ListenerAt(const Transform& camera) {
    return {.position = camera.position, .right = Rotate(camera.rotation, {.x = 1.0F})};
}

audio::VoiceMix PlaceSound(const Listener& listener, const Vector3& source,
                           const audio::VoiceMix& mix, SoundRange range) {
    const Vector3 offset = ToEngine(source) - listener.position;
    const float distanceSquared = Dot(offset, offset);
    const float side = Dot(offset, listener.right);
    const float pan = distanceSquared > 0.0F
                          ? kPanScale * side * side / distanceSquared * (side < 0.0F ? -1.0F : 1.0F)
                          : 0.0F;
    return {.volume = mix.volume * Falloff(distanceSquared, range), .pitch = mix.pitch, .pan = pan};
}

}
