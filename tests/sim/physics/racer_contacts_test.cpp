#include "sim/physics/racer_contacts.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <numbers>

#include "plane_world.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

struct TestCar {
    BodyState body;
    const BodyConstants* constants = nullptr;
};

const BodyConstants& Constants() {
    static const BodyConstants constants = MakeBodyConstants({}, {});
    return constants;
}

TestCar CarAt(const Vector3& position, float headingRadians, const Vector3& velocity) {
    const Basis basis = OrthonormalBasis(
        {.x = std::cos(headingRadians), .y = std::sin(headingRadians)}, {.z = 1.0F});
    TestCar car{.body = MakeBody(Constants(), position, basis), .constants = &Constants()};
    car.body.motion.velocity = velocity;
    return car;
}

void Resolve(std::array<TestCar*, 2> cars, const CollisionQuery& world) {
    const std::array<ContactBody, 2> bodies{
        {{.body = &cars[0]->body, .constants = cars[0]->constants, .world = &world},
         {.body = &cars[1]->body, .constants = cars[1]->constants, .world = &world}}};
    RacerContacts contacts;
    contacts.Resolve(bodies);
}

TEST_CASE("a head-on contact separates the cars at three quarters of their closing speed") {
    const test::PlaneWorld world({});
    TestCar east = CarAt({.x = 6.0F}, std::numbers::pi_v<float>, {.x = -0.1F});
    TestCar west = CarAt({.x = 1.0F}, 0.0F, {.x = 0.1F});

    Resolve({&east, &west}, world);

    CHECK(east.body.motion.velocity.x == doctest::Approx(0.075F));
    CHECK(west.body.motion.velocity.x == doctest::Approx(-0.075F));
    CHECK(east.body.motion.position.x == 6.0F);
    CHECK(west.body.motion.position.x == doctest::Approx(0.0F));
}

TEST_CASE("a side contact pushes only the first racer, the lower index on a tie") {
    const test::PlaneWorld world({});
    TestCar struck = CarAt({}, 0.0F, {});
    TestCar striker = CarAt({.y = -4.5F}, 0.0F, {.y = 0.1F});

    Resolve({&struck, &striker}, world);

    CHECK(struck.body.motion.position.y == doctest::Approx(0.5F));
    CHECK(striker.body.motion.position.y == -4.5F);
    CHECK(struck.body.motion.velocity.y == doctest::Approx(0.0875F));
    CHECK(striker.body.motion.velocity.y == doctest::Approx(0.0125F));
}

TEST_CASE("a push that would cross a wall is undone, but the impulse still applies") {
    const test::PlaneWorld world({{.normal = {.y = -1.0F}, .offset = -4.3F}});
    TestCar struck = CarAt({}, 0.0F, {});
    TestCar striker = CarAt({.y = -4.5F}, 0.0F, {.y = 0.1F});

    Resolve({&struck, &striker}, world);

    CHECK(struck.body.motion.position.y == 0.0F);
    CHECK(struck.body.motion.velocity.y == doctest::Approx(0.0875F));
}

TEST_CASE("each resolved pair is reported with its first racer and contact point") {
    const test::PlaneWorld world({});
    TestCar struck = CarAt({}, 0.0F, {});
    TestCar striker = CarAt({.y = -4.5F}, 0.0F, {.y = 0.1F});
    TestCar distant = CarAt({.x = 40.0F}, 0.0F, {});
    const std::array<ContactBody, 3> bodies{
        {{.body = &distant.body, .constants = distant.constants, .world = &world},
         {.body = &striker.body, .constants = striker.constants, .world = &world},
         {.body = &struck.body, .constants = struck.constants, .world = &world}}};
    RacerContacts contacts;

    contacts.Resolve(bodies);

    REQUIRE(contacts.Contacts().size() == 1);
    CHECK(contacts.Contacts()[0].first == 1);
    CHECK(contacts.Contacts()[0].second == 2);
    CHECK(contacts.Contacts()[0].point.y == doctest::Approx(-2.25F).epsilon(0.1));
}

TEST_CASE("cars out of reach are left alone") {
    const test::PlaneWorld world({});
    TestCar first = CarAt({}, 0.0F, {.x = 0.1F});
    TestCar second = CarAt({.x = 20.0F}, 0.0F, {});

    Resolve({&first, &second}, world);

    CHECK(first.body.motion.velocity.x == 0.1F);
    CHECK(second.body.motion.velocity.x == 0.0F);
}

}
}
