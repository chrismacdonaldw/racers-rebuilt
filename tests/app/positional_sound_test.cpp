#include "app/positional_sound.hpp"

#include <doctest/doctest.h>

#include <numbers>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

TEST_CASE("a sound near the listener plays at full volume, panned to its side") {
    const Listener listener = ListenerAt({});

    const audio::VoiceMix right = PlaceSound(listener, {.x = 10.0F}, {.volume = 0.5F, .pitch = 2});
    const audio::VoiceMix left = PlaceSound(listener, {.x = -20.0F}, {});

    CHECK(right.volume == 0.5F);
    CHECK(right.pitch == 2.0F);
    CHECK(right.pan == doctest::Approx(0.7F));
    CHECK(left.pan == doctest::Approx(-0.7F));
}

TEST_CASE("a sound fades with squared distance to silence at the range's end") {
    const Listener listener = ListenerAt({});

    const audio::VoiceMix ahead = PlaceSound(listener, {.y = -150.0F}, {});
    const audio::VoiceMix far = PlaceSound(listener, {.y = -300.0F}, {});
    const audio::VoiceMix nearer = PlaceSound(listener, {.y = -150.0F}, {}, {.max = 150.0F});

    CHECK(ahead.volume == doctest::Approx(1.0F - (21600.0F / 89100.0F)));
    CHECK(ahead.pan == doctest::Approx(0.0F));
    CHECK(far.volume == 0.0F);
    CHECK(nearer.volume == 0.0F);
}

TEST_CASE("the listener's right follows the camera's turn") {
    const Listener turned =
        ListenerAt({.rotation = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float>)});

    CHECK(PlaceSound(turned, {.x = 10.0F}, {}).pan == doctest::Approx(-0.7F));
}

}
}
