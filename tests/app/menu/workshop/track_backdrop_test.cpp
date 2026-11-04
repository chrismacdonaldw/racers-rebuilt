#include "app/menu/workshop/track_backdrop.hpp"

#include <doctest/doctest.h>

#include "app/track_space.hpp"
#include "assets/vfs.hpp"

namespace rr::app {
namespace {

bool IsNear(const Vector3& a, const Vector3& b) {
    return a.x == doctest::Approx(b.x) && a.y == doctest::Approx(b.y) &&
           a.z == doctest::Approx(b.z);
}

TEST_CASE("the shift moves a track's first grid spot onto the bench") {
    const Vector3 first{.x = 120.0F, .y = -40.0F, .z = 3.0F};
    const Vector3 spot{.x = 16.0F, .y = 16.0F};

    CHECK(IsNear(ToEngine(first) + BackdropShift(first, spot), ToEngine(spot)));
    CHECK(IsNear(BackdropShift(spot, spot), {}));
}

TEST_CASE("a track that can't be read gives an error") {
    const assets::Vfs vfs;

    CHECK_FALSE(ReadBackdropFiles(vfs, "RACEC0R0").has_value());
}

}
}
