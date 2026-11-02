#include "app/positional_sound.hpp"

#include <doctest/doctest.h>

#include <array>
#include <numbers>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

TEST_CASE("a sound near the listener plays at full volume, panned to its side") {
    const Listener listener = ListenerAt(Transform{});

    const audio::VoiceMix right = PlaceSound(listener, {.x = 10.0F}, {.volume = 0.5F, .pitch = 2});
    const audio::VoiceMix left = PlaceSound(listener, {.x = -20.0F}, {});

    CHECK(right.volume == 0.5F);
    CHECK(right.pitch == 2.0F);
    CHECK(right.pan == doctest::Approx(0.7F));
    CHECK(left.pan == doctest::Approx(-0.7F));
}

TEST_CASE("a sound fades with squared distance to silence at the range's end") {
    const Listener listener = ListenerAt(Transform{});

    const audio::VoiceMix ahead = PlaceSound(listener, {.y = -150.0F}, {});
    const audio::VoiceMix far = PlaceSound(listener, {.y = -300.0F}, {});
    const audio::VoiceMix nearer = PlaceSound(listener, {.y = -150.0F}, {}, {.max = 150.0F});

    CHECK(ahead.volume == doctest::Approx(1.0F - (21600.0F / 89100.0F)));
    CHECK(ahead.pan == doctest::Approx(0.0F));
    CHECK(far.volume == 0.0F);
    CHECK(nearer.volume == 0.0F);
}

TEST_CASE("with two ears, a sound plays at the louder volume, panned by each ear's volume") {
    const std::array cameras{Transform{}, Transform{.position = {.x = 200.0F}}};
    const Listener listener = ListenerAt(cameras);

    const audio::VoiceMix near = PlaceSound(listener, {.x = 10.0F}, {});
    const audio::VoiceMix between = PlaceSound(listener, {.x = 100.0F}, {});
    const float farEar = 1.0F - ((36100.0F - 900.0F) / (90000.0F - 900.0F));
    const float bothEars = 1.0F - ((10000.0F - 900.0F) / (90000.0F - 900.0F));

    CHECK(near.volume == 1.0F);
    CHECK(near.pan == doctest::Approx(0.7F * (1.0F - farEar) / (1.0F + farEar)));
    CHECK(between.volume == doctest::Approx(bothEars));
    CHECK(between.pan == doctest::Approx(0.0F));
}

TEST_CASE("the nearest ear sets the distance") {
    const std::array cameras{Transform{}, Transform{.position = {.x = 200.0F}}};

    CHECK(DistanceSquaredToNearestEar(ListenerAt(cameras), {.x = 190.0F}) ==
          doctest::Approx(100.0F));
}

TEST_CASE("the listener's right follows the camera's turn") {
    const Listener turned =
        ListenerAt({.rotation = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float>)});

    CHECK(PlaceSound(turned, {.x = 10.0F}, {}).pan == doctest::Approx(-0.7F));
}

}
}
