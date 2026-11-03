#include "sim/powerup/brick_field.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

#include "sim/physics/plane_world.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;

constexpr Milliseconds kTick{10};
constexpr float kReach = 5.0F;
constexpr BoundingSphere kAway{.centre = {.z = 1000.0F}, .radius = kReach};
constexpr int kTicksToIdle = 52;
constexpr int kTicksToHide = 26;

BoundingSphere At(const Vector3& point) {
    return {.centre = point, .radius = kReach};
}

class Field {
public:
    explicit Field(const assets::BrickLayout& layout) : field_(layout) {}

    std::vector<BrickEvent> Run(int ticks, const BoundingSphere& racer = kAway) {
        std::vector<BrickEvent> events;
        for (int i = 0; i < ticks; ++i) {
            field_.Tick(kTick, std::span(&racer, 1), inventory_);
            events.insert(events.end(), field_.Events().begin(), field_.Events().end());
        }
        return events;
    }

    std::vector<BrickEvent> TouchEach(std::initializer_list<float> xs) {
        std::vector<BrickEvent> events;
        for (const float x : xs) {
            const std::vector<BrickEvent> touched = Run(1, At({.x = x}));
            events.insert(events.end(), touched.begin(), touched.end());
        }
        return events;
    }

    int TicksUntilEvent(int limit) {
        for (int ticks = 1; ticks <= limit; ++ticks) {
            if (!Run(1).empty()) {
                return ticks;
            }
        }
        return 0;
    }

    BrickField& Bricks() { return field_; }
    PowerupInventory& Held() { return inventory_[0]; }
    [[nodiscard]] const ColourBrick& Coloured(std::size_t i) const { return field_.Coloured()[i]; }
    [[nodiscard]] const WhiteBrick& White(std::size_t i) const { return field_.White()[i]; }

private:
    BrickField field_;
    std::array<PowerupInventory, 1> inventory_{};
};

assets::BrickLayout OneBrick(BrickColour colour,
                             Milliseconds respawn = assets::kDefaultBrickRespawn) {
    return {.coloured = {{.colour = colour, .respawn = respawn}}};
}

TEST_CASE("coloured bricks grow in when the race starts and become touchable 500 ms later") {
    Field field(OneBrick(BrickColour::Red));
    CHECK(BrickScale(field.Coloured(0).brick) == 0.0F);

    const std::vector<BrickEvent> first = field.Run(1);
    field.Run(kTicksToIdle - 2);
    const BrickState growing = field.Coloured(0).brick.state;
    field.Run(1);

    CHECK(first == std::vector<BrickEvent>{{.kind = BrickEventKind::Respawned}});
    CHECK(growing == BrickState::Active);
    CHECK(field.Coloured(0).brick.state == BrickState::Idle);
}

TEST_CASE("a racer holding nothing takes the colour, and the brick respawns after its time") {
    Field field(OneBrick(BrickColour::Red, Milliseconds{1000}));
    field.Run(kTicksToIdle);

    const std::vector<BrickEvent> touched = field.Run(1, At({}));
    const int ticksToRespawn = field.TicksUntilEvent(1000);

    CHECK(touched == std::vector<BrickEvent>{{.kind = BrickEventKind::Collected}});
    CHECK(field.Held().Colour() == BrickColour::Red);
    CHECK(ticksToRespawn == kTicksToHide + 100);
    CHECK(field.Coloured(0).colour == BrickColour::Red);
}

TEST_CASE("a racer holding a colour swaps it with the brick, which comes straight back") {
    Field field(OneBrick(BrickColour::Red));
    field.Run(kTicksToIdle);
    field.Held().TakeColour(BrickColour::Blue);

    const std::vector<BrickEvent> touched = field.Run(1, At({}));
    const int ticksToReturn = field.TicksUntilEvent(1000);

    CHECK(touched == std::vector<BrickEvent>{{.kind = BrickEventKind::Swapped}});
    CHECK(field.Held().Colour() == BrickColour::Red);
    CHECK(ticksToReturn == kTicksToHide);
    CHECK(field.Coloured(0).colour == BrickColour::Blue);
    CHECK(field.Coloured(0).brick.state == BrickState::Active);
}

TEST_CASE("a racer parked on a brick it swapped with doesn't swap again") {
    Field field(OneBrick(BrickColour::Red));
    field.Run(kTicksToIdle);
    field.Held().TakeColour(BrickColour::Blue);

    const std::vector<BrickEvent> events = field.Run(200, At({}));

    CHECK(events.size() == 2);
    CHECK(field.Held().Colour() == BrickColour::Red);
    CHECK(field.Coloured(0).brick.state == BrickState::Idle);
}

TEST_CASE("white bricks stack to three, and a fourth stays on the track") {
    Field field({.white = {{.x = 0.0F}, {.x = 20.0F}, {.x = 40.0F}, {.x = 60.0F}}});

    const std::vector<BrickEvent> events = field.TouchEach({0.0F, 20.0F, 40.0F, 60.0F});
    field.Run(kTicksToHide);

    REQUIRE(events.size() == 3);
    CHECK(events[2] ==
          BrickEvent{.kind = BrickEventKind::WhiteCollected, .brick = 2, .whiteCount = 3});
    CHECK(field.Held().WhiteCount() == 3);
    CHECK(field.White(0).brick.state == BrickState::Wait);
    CHECK(field.White(3).brick.state == BrickState::Idle);
}

TEST_CASE("spending sends held white bricks home, where they grow back in") {
    Field field({.white = {{.x = 0.0F}}});
    field.Run(1, At({}));
    field.Run(kTicksToHide);

    field.Bricks().Spend(field.Held());
    const BrickState returned = field.White(0).brick.state;
    field.Run(kTicksToIdle - 1);

    CHECK(returned == BrickState::Active);
    CHECK(field.Held().WhiteCount() == 0);
    CHECK(field.White(0).brick.state == BrickState::Idle);
}

TEST_CASE("a dropped white brick lands on the ground and goes home after 10 s") {
    Field field({.white = {{.x = 0.0F}}});
    field.Run(1, At({}));
    field.Run(kTicksToHide);
    const test::PlaneWorld ground = test::FlatGround();

    field.Bricks().DropWhite(field.Held(), {.x = 50.0F, .z = 3.0F}, ground);
    const Vector3 dropped = field.White(0).brick.position;
    field.Run(1000);
    const Vector3 beforeHome = field.White(0).brick.position;
    field.Run(1 + kTicksToHide);

    CHECK(dropped == Vector3{.x = 50.0F, .z = 5.0F});
    CHECK(field.Held().WhiteCount() == 0);
    CHECK(beforeHome == dropped);
    CHECK(field.White(0).brick.position == Vector3{});
    CHECK(field.White(0).brick.state == BrickState::Active);
}

TEST_CASE("brick scale grows past full size, settles, and shrinks away when taken") {
    CHECK(BrickScale({.state = BrickState::Active, .stateTime = Milliseconds{200}}) ==
          doctest::Approx(0.5F));
    CHECK(BrickScale({.state = BrickState::Active, .stateTime = Milliseconds{450}}) ==
          doctest::Approx(0.9F));
    CHECK(BrickScale({.state = BrickState::Idle}) == 0.8F);
    CHECK(BrickScale({.state = BrickState::Transition, .stateTime = Milliseconds{125}}) ==
          doctest::Approx(0.4F));
    CHECK(BrickScale({.state = BrickState::Wait}) == 0.0F);
}

TEST_CASE("bricks spin together by 0.004 rad per ms") {
    Field field({});

    field.Run(100);

    CHECK(field.Bricks().SpinRadians() == doctest::Approx(4.0F));
}

TEST_CASE("a brick cheat colour replaces every coloured brick's own") {
    const assets::BrickLayout layout{
        .coloured = {{.colour = BrickColour::Blue}, {.colour = BrickColour::Green}}};

    const BrickField field(layout, BrickColour::Yellow);

    for (const ColourBrick& brick : field.Coloured()) {
        CHECK(brick.colour == BrickColour::Yellow);
        CHECK(brick.homeColour == BrickColour::Yellow);
        CHECK(brick.nextColour == BrickColour::Yellow);
    }
}

}
}
