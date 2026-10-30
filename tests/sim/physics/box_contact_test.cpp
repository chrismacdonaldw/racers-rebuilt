#include "sim/physics/box_contact.hpp"

#include <doctest/doctest.h>

#include <optional>

namespace rr::sim {
namespace {

constexpr Vector3 kCarBox{.x = 4.0F, .y = 2.5F, .z = 3.1F};

ContactBox AlignedAt(const Vector3& centre) {
    return {.centre = centre, .halfSize = kCarBox};
}

TEST_CASE("overlapping boxes push apart along the shallowest face axis") {
    const std::optional<BoxContact> found =
        TestBoxes(AlignedAt({.x = 7.0F}), AlignedAt({.z = 1.0F}));

    REQUIRE(found.has_value());
    const BoxContact& contact = *found;
    CHECK(contact.penetration == doctest::Approx(1.0F));
    CHECK(contact.normal == Vector3{.x = 1.0F});
    CHECK(contact.point.x == doctest::Approx(3.5F));
    CHECK(contact.point.z == doctest::Approx(0.5F));
}

TEST_CASE("the normal points from the second box to the first") {
    const std::optional<BoxContact> found = TestBoxes(AlignedAt({}), AlignedAt({.y = 4.5F}));

    REQUIRE(found.has_value());
    const BoxContact& contact = *found;
    CHECK(contact.penetration == doctest::Approx(0.5F));
    CHECK(contact.normal == Vector3{.y = -1.0F});
}

TEST_CASE("a tie between face axes keeps the earlier axis") {
    const std::optional<BoxContact> found =
        TestBoxes(AlignedAt({.x = 7.0F, .y = 4.0F}), AlignedAt({}));

    REQUIRE(found.has_value());
    const BoxContact& contact = *found;
    CHECK(contact.normal == Vector3{.x = 1.0F});
}

TEST_CASE("separated or merely touching boxes don't make contact") {
    CHECK_FALSE(TestBoxes(AlignedAt({.x = 9.0F}), AlignedAt({})).has_value());
    CHECK_FALSE(TestBoxes(AlignedAt({.x = 8.0F}), AlignedAt({})).has_value());
}

}
}
