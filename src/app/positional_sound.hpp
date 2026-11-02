#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "audio/voice_mix.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// In engine space.
struct Ear {
    Vector3 position;
    Vector3 right{.x = 1.0F};
};

/// A split-screen race hears from every player's camera.
struct Listener {
    std::array<Ear, sim::kMaxPlayers> ears{};
    std::size_t earCount = 1;

    [[nodiscard]] std::span<const Ear> Ears() const { return std::span(ears).first(earCount); }
};

/// A sound plays at full volume within `min` units and falls silent at `max`.
struct SoundRange {
    float min = 30.0F;
    float max = 300.0F;
};

/// Cameras past `sim::kMaxPlayers` go unheard.
[[nodiscard]] Listener ListenerAt(std::span<const Transform> cameras);
[[nodiscard]] Listener ListenerAt(const Transform& camera);

/// `source` is in game space.
[[nodiscard]] float DistanceSquaredToNearestEar(const Listener& listener, const Vector3& source);

/// `source` is in game space. Loudest ear's volume, with each ear's pan weighted by its volume.
[[nodiscard]] audio::VoiceMix PlaceSound(const Listener& listener, const Vector3& source,
                                         const audio::VoiceMix& mix, SoundRange range = {});

}
