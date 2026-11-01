#include "sim/physics/racer_contacts.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <numbers>

#include "plane_world.hpp"
#include "sim/ai/straight_route.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/racer.hpp"
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

std::size_t Resolve(std::array<TestCar*, 2> cars, const CollisionQuery& world,
                    const ContactBody& firstTraits = {}) {
    const std::array<ContactBody, 2> bodies{
        {{.body = &cars[0]->body,
          .constants = cars[0]->constants,
          .world = &world,
          .isSolid = firstTraits.isSolid,
          .boxScale = firstTraits.boxScale},
         {.body = &cars[1]->body, .constants = cars[1]->constants, .world = &world}}};
    RacerContacts contacts;
    contacts.Resolve(bodies);
    return contacts.Contacts().size();
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

TEST_CASE("a racer that isn't solid, or whose box has shrunk away, touches nothing") {
    const test::PlaneWorld world({});
    TestCar struck = CarAt({}, 0.0F, {});
    TestCar striker = CarAt({.y = -4.5F}, 0.0F, {.y = 0.1F});

    CHECK(Resolve({&struck, &striker}, world, {.isSolid = false}) == 0);
    CHECK(Resolve({&struck, &striker}, world, {.boxScale = 0.0F}) == 0);

    CHECK(struck.body.motion.position.y == 0.0F);
    CHECK(striker.body.motion.velocity.y == 0.1F);
}

TEST_CASE("a half-size box touches only once the cars are closer") {
    const test::PlaneWorld world({});
    TestCar struck = CarAt({}, 0.0F, {});
    TestCar striker = CarAt({.y = -4.5F}, 0.0F, {.y = 0.1F});

    CHECK(Resolve({&struck, &striker}, world, {.boxScale = 0.5F}) == 0);
}

assets::RaceRoute LaneWideOnTheRight() {
    assets::RaceRoute lane = test::StraightRoute({.z = 0.2F});
    for (assets::RoutePoint& point : lane.points) {
        point.widthRight = 2.0F;
    }
    return lane;
}

float SideOffsetOf(const Racer& racer) {
    return racer.Route()
        .transform([](const AiRacer& route) { return route.SideOffset(); })
        .value_or(0.0F);
}

TEST_CASE("a computer racer first in a contact is pushed sideways along its route") {
    const test::PlaneWorld world = test::FlatGround();
    constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
    Racer computer(kChassis, {}, {});
    computer.FollowRoute(LaneWideOnTheRight());
    Racer player(kChassis, {}, {.position = {.x = 0.5F, .y = -4.5F, .z = 0.2F}});
    const std::array bodies{computer.ForContacts(world), player.ForContacts(world)};
    RacerContacts contacts;

    contacts.Resolve(bodies);

    REQUIRE(contacts.Contacts().size() == 1);
    CHECK(contacts.Contacts()[0].first == 0);
    const float offset = SideOffsetOf(computer);
    CHECK(offset == doctest::Approx(0.5F).epsilon(0.1));
    CHECK(computer.Body().motion.position.y == doctest::Approx(offset));
    CHECK(player.Body().motion.position.y == -4.5F);
}

TEST_CASE("a computer racer hit from behind speeds up") {
    const test::PlaneWorld world = test::FlatGround();
    constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
    Racer computer(kChassis, {}, {});
    computer.FollowRoute(test::StraightRoute({.x = 7.5F, .z = 0.2F}));
    Racer player(kChassis, {}, {.position = {.z = 0.2F}});
    const ContactBody playerBody = player.ForContacts(world);
    playerBody.body->motion.velocity = {.x = 0.1F};
    const std::array bodies{playerBody, computer.ForContacts(world)};
    RacerContacts contacts;

    contacts.Resolve(bodies);

    REQUIRE(contacts.Contacts().size() == 1);
    CHECK(computer.Route()
              .transform([](const AiRacer& route) { return route.PlaybackSpeed(); })
              .value_or(0.0F) > 1.0F);
    CHECK(player.Body().motion.velocity.x < 0.1F);
}

}
}
