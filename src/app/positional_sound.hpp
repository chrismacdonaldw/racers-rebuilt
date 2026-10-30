#pragma once

#include "audio/voice_mix.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// In engine space.
struct Listener {
    Vector3 position;
    Vector3 right{.x = 1.0F};
};

/// A sound plays at full volume within `min` units and falls silent at `max`.
struct SoundRange {
    float min = 30.0F;
    float max = 300.0F;
};

[[nodiscard]] Listener ListenerAt(const Transform& camera);

/// `source` is in game space.
[[nodiscard]] audio::VoiceMix PlaceSound(const Listener& listener, const Vector3& source,
                                         const audio::VoiceMix& mix, SoundRange range = {});

}
