#include "sim/powerup/powerup_system.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "allocation_counter.hpp"
#include "fake_effect.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;

constexpr Milliseconds kTick{10};
constexpr Milliseconds kLongLife{60'000};
constexpr std::size_t kRacers = 6;

PowerupUse Use(BrickColour colour, std::uint8_t level, std::uint8_t racer = 0) {
    return {.racer = racer, .colour = colour, .level = level};
}

class Manager {
public:
    explicit Manager(Milliseconds lifetime = kLongLife, bool blocksHits = false, Cheats cheats = {})
        : system_(test::FakeEffects(lifetime, blocksHits), kRacers, cheats) {}

    PowerupSystem& System() { return system_; }
    PowerupContext& Context() { return context_; }

    std::vector<PowerupEvent> Fire(const PowerupUse& use, int times = 1) {
        system_.BeginTick();
        for (int i = 0; i < times; ++i) {
            system_.Fire(use, context_);
        }
        return {system_.Events().begin(), system_.Events().end()};
    }

    std::vector<PowerupEvent> Run(Milliseconds time) {
        std::vector<PowerupEvent> events;
        for (Milliseconds run{0}; run < time; run += kTick) {
            system_.BeginTick();
            system_.Tick(kTick, context_);
            events.insert(events.end(), system_.Events().begin(), system_.Events().end());
        }
        return events;
    }

    [[nodiscard]] std::vector<PowerupKind> ActiveKinds() const {
        std::vector<PowerupKind> kinds;
        kinds.reserve(system_.Active().size());
        for (const std::uint16_t slot : system_.Active()) {
            kinds.push_back(system_.Slots()[slot].kind);
        }
        return kinds;
    }

private:
    PowerupSystem system_;
    PowerupContext context_;
};

std::vector<PowerupEventKind> Kinds(const std::vector<PowerupEvent>& events) {
    std::vector<PowerupEventKind> kinds;
    kinds.reserve(events.size());
    for (const PowerupEvent& event : events) {
        kinds.push_back(event.kind);
    }
    return kinds;
}

TEST_CASE("each colour and level picks the original's power-up") {
    CHECK(KindFor(BrickColour::Red, 0) == PowerupKind::Cannonball);
    CHECK(KindFor(BrickColour::Red, 3) == PowerupKind::Missile);
    CHECK(KindFor(BrickColour::Yellow, 1) == PowerupKind::Dynamite);
    CHECK(KindFor(BrickColour::Yellow, 3) == PowerupKind::Curse);
    CHECK(KindFor(BrickColour::Blue, 2) == PowerupKind::Shield);
    CHECK(KindFor(BrickColour::Green, 2) == PowerupKind::Turbo);
    CHECK(KindFor(BrickColour::Green, 3) == PowerupKind::Warp);
}

TEST_CASE("using with no colour held taunts and starts nothing") {
    Manager manager;
    PowerupInventory inventory;
    BrickField bricks;
    manager.System().BeginTick();

    manager.System().Use(2, inventory, bricks, manager.Context());

    CHECK(std::vector(manager.System().Events().begin(), manager.System().Events().end()) ==
          std::vector<PowerupEvent>{{.kind = PowerupEventKind::Taunted, .use = {.racer = 2}}});
    CHECK(manager.System().Active().empty());
}

TEST_CASE("using fires the held colour at the white brick level, then spends the inventory") {
    Manager manager;
    PowerupInventory inventory;
    inventory.Grant(BrickColour::Yellow, 2);
    BrickField bricks;
    manager.System().BeginTick();

    manager.System().Use(1, inventory, bricks, manager.Context());

    const PowerupUse magnet = Use(BrickColour::Yellow, 2, 1);
    CHECK(std::vector(manager.System().Events().begin(), manager.System().Events().end()) ==
          std::vector<PowerupEvent>{{.kind = PowerupEventKind::Used, .use = magnet},
                                    {.kind = PowerupEventKind::Started,
                                     .use = magnet,
                                     .effect = manager.System().Active()[0]}});
    CHECK(manager.ActiveKinds() == std::vector{PowerupKind::Magnet});
    CHECK(inventory == PowerupInventory{});
}

TEST_CASE("red level 3 launches three missiles, numbered in order") {
    Manager manager;

    manager.Fire(Use(BrickColour::Red, 3));

    REQUIRE(manager.System().Active().size() == 3);
    for (std::uint8_t shot = 0; shot < 3; ++shot) {
        const EffectSlot& missile = manager.System().Slots()[manager.System().Active()[shot]];
        CHECK(missile.kind == PowerupKind::Missile);
        CHECK(missile.use.shot == shot);
    }
}

TEST_CASE("a kind without effects does nothing when used") {
    EffectFactories factories;
    factories.at(static_cast<std::size_t>(PowerupKind::Shield)) = test::FakeEffects(kLongLife)[0];
    PowerupSystem system(factories, kRacers);
    PowerupContext context;

    system.BeginTick();
    system.Fire(Use(BrickColour::Red, 0), context);
    system.Fire(Use(BrickColour::Blue, 0), context);

    CHECK(system.Active().size() == 1);
    CHECK(system.Slots().size() == SpecFor(PowerupKind::Shield).poolSize);
}

TEST_CASE("a full pool takes back the effect in the highest phase") {
    Manager manager;
    const PowerupUse curse = Use(BrickColour::Yellow, 3);
    manager.Fire(curse);
    manager.Run(Milliseconds{250});
    manager.Fire(curse);
    manager.Run(Milliseconds{120});
    const std::uint16_t oldest = manager.System().Active()[0];

    const std::vector<PowerupEvent> events = manager.Fire(curse);

    CHECK(events == std::vector<PowerupEvent>{
                        {.kind = PowerupEventKind::Ended, .use = curse, .effect = oldest},
                        {.kind = PowerupEventKind::Started, .use = curse, .effect = oldest}});
    CHECK(manager.System().Active().size() == 2);
    CHECK(manager.System().Active()[1] == oldest);
}

TEST_CASE(
    "between effects in the same phase, a full pool takes back the one with less time in it") {
    Manager manager;
    const PowerupUse curse = Use(BrickColour::Yellow, 3);
    manager.Fire(curse);
    manager.Run(Milliseconds{10});
    manager.Fire(curse);
    manager.Run(Milliseconds{50});
    const std::uint16_t newest = manager.System().Active()[1];

    const std::vector<PowerupEvent> events = manager.Fire(curse);

    REQUIRE(events.size() == 2);
    CHECK(events[0].effect == newest);
}

TEST_CASE("a full pool takes back only an effect of the same level") {
    Manager manager;
    const std::size_t turbos = SpecFor(PowerupKind::Turbo).poolSize;
    manager.Fire(Use(BrickColour::Green, 0), static_cast<int>(turbos) - 1);
    manager.Fire(Use(BrickColour::Green, 1));

    CHECK(manager.Fire(Use(BrickColour::Green, 2)).empty());
    CHECK(Kinds(manager.Fire(Use(BrickColour::Green, 1))) ==
          std::vector{PowerupEventKind::Ended, PowerupEventKind::Started});
}

TEST_CASE("a launch needs its effect entities free") {
    Manager manager;
    manager.Fire(Use(BrickColour::Blue, 0), 5);
    manager.Fire(Use(BrickColour::Green, 0), 5);
    REQUIRE(manager.System().EntitiesInUse() == kEffectEntities);

    CHECK(manager.Fire(Use(BrickColour::Green, 0)).empty());
    CHECK(Kinds(manager.Fire(Use(BrickColour::Red, 0))) == std::vector{PowerupEventKind::Started});
}

TEST_CASE("the manager turns crowded once 20 effect entities are in use") {
    Manager manager;
    manager.Fire(Use(BrickColour::Blue, 0), 5);
    manager.Fire(Use(BrickColour::Green, 0), 3);
    CHECK(manager.System().EntitiesInUse() == 19);
    CHECK_FALSE(manager.System().IsCrowded());

    manager.Fire(Use(BrickColour::Green, 0));

    CHECK(manager.System().IsCrowded());
}

TEST_CASE("an ended effect frees its entities and its slot") {
    Manager manager(Milliseconds{100});
    manager.Fire(Use(BrickColour::Yellow, 2));

    const std::vector<PowerupEvent> events = manager.Run(Milliseconds{100});

    CHECK(Kinds(events) == std::vector{PowerupEventKind::Ended});
    CHECK(manager.System().Active().empty());
    CHECK(manager.System().EntitiesInUse() == 0);
}

TEST_CASE("effects that run out on the same tick end in the order they started") {
    Manager manager(Milliseconds{100});
    manager.Fire(Use(BrickColour::Green, 1));
    manager.Fire(Use(BrickColour::Red, 0));
    manager.Fire(Use(BrickColour::Blue, 2));
    const std::vector<std::uint16_t> started(manager.System().Active().begin(),
                                             manager.System().Active().end());

    const std::vector<PowerupEvent> events = manager.Run(Milliseconds{100});

    REQUIRE(events.size() == 3);
    for (std::size_t i = 0; i < events.size(); ++i) {
        CHECK(events[i].kind == PowerupEventKind::Ended);
        CHECK(events[i].effect == started[i]);
    }
}

TEST_CASE("a hit asks only the effects of the racer that was hit") {
    Manager manager(kLongLife, true);
    manager.Fire(Use(BrickColour::Blue, 0, 1));

    CHECK(manager.System().Hit({.racer = 1, .from = 0}, manager.Context()));
    CHECK_FALSE(manager.System().Hit({.racer = 0, .from = 1}, manager.Context()));
}

TEST_CASE("ticking and reclaiming with every pool full allocates nothing") {
    Manager manager;
    for (const BrickColour colour : {BrickColour::Red, BrickColour::Yellow}) {
        for (std::uint8_t level = 0; level <= kMaxPowerupLevel; ++level) {
            manager.Fire(Use(colour, level), 9);
        }
    }
    manager.Fire(Use(BrickColour::Green, 3), 3);
    manager.Fire(Use(BrickColour::Blue, 0), 5);
    const std::uint64_t before = test::AllocationCount();

    manager.System().BeginTick();
    manager.System().Tick(kTick, manager.Context());
    manager.System().Fire(Use(BrickColour::Red, 0), manager.Context());
    manager.System().Fire(Use(BrickColour::Blue, 0), manager.Context());

    CHECK(test::AllocationCount() == before);
}

TEST_CASE("under red only a use fires a level 1 red whatever is held, then spends it") {
    Manager manager(kLongLife, false, Cheats(static_cast<std::uint16_t>(Cheat::RedOnly)));
    PowerupInventory inventory;
    inventory.Grant(BrickColour::Blue, 3);
    BrickField bricks;
    manager.System().BeginTick();

    manager.System().Use(0, inventory, bricks, manager.Context());

    CHECK(manager.ActiveKinds() == std::vector{PowerupKind::GrapplingHook});
    CHECK_FALSE(inventory.Colour().has_value());
}

TEST_CASE("under the hover cheat a green short of three white bricks is spent unfired") {
    Manager manager(kLongLife, false, Cheats(static_cast<std::uint16_t>(Cheat::HoverTurbo)));
    PowerupInventory inventory;
    inventory.Grant(BrickColour::Green, 2);
    BrickField bricks;
    manager.System().BeginTick();

    manager.System().Use(0, inventory, bricks, manager.Context());

    CHECK(Kinds({manager.System().Events().begin(), manager.System().Events().end()}) ==
          std::vector{PowerupEventKind::Used});
    CHECK(manager.System().Active().empty());
    CHECK_FALSE(inventory.Colour().has_value());
}

}
}
