#include "app/powerup/powerup_sounds.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <ranges>
#include <vector>

#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/dynamite_effect.hpp"
#include "sim/powerup/grapple_effect.hpp"
#include "sim/powerup/lightning_effect.hpp"
#include "sim/powerup/magnet_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};

sim::EffectProgress At(auto phase, Milliseconds left = Milliseconds{0}) {
    return {.phase = static_cast<std::uint8_t>(phase), .phaseTime = left};
}

std::optional<std::size_t> LoopSound(sim::PowerupKind kind, const sim::EffectProgress& progress) {
    return LoopOf(kind, progress).transform([](const EffectLoop& loop) { return loop.effect; });
}

float LightningVolume(sim::LightningPhase phase, Milliseconds left) {
    return LoopOf(sim::PowerupKind::Lightning, At(phase, left))
        .transform([](const EffectLoop& loop) { return loop.volume; })
        .value_or(0.0F);
}

struct Crackles {
    std::vector<int> ticks;
    std::vector<float> reaches;
};

Crackles CrackleFor(int ticks, Crackle& crackle, sim::Random& random) {
    Crackles crackles;
    for (int tick = 0; tick < ticks; ++tick) {
        if (const std::optional<float> reach = crackle.Tick(true, kTick, random)) {
            crackles.ticks.push_back(tick);
            crackles.reaches.push_back(*reach);
        }
    }
    return crackles;
}

std::ranges::minmax_result<int> GapRange(const std::vector<int>& ticks) {
    std::vector<int> gaps(ticks.size());
    std::adjacent_difference(ticks.begin(), ticks.end(), gaps.begin());
    return std::ranges::minmax(gaps | std::views::drop(1));
}

bool IsCrackleReach(float reach) {
    return reach >= 0.0F && reach < 50.0F;
}

}

TEST_CASE("the player taunts with its own sound and computer racers with their voice bank's") {
    CHECK(TauntSound(true, 4) == 0x1A);
    CHECK(TauntSound(false, 0) == 0x1C);
    CHECK(TauntSound(false, 1) == 0x1B);
    CHECK(TauntSound(false, 2) == 0x1D);
    CHECK(TauntSound(false, 3) == 0x1E);
    CHECK(TauntSound(false, 4) == 0x1F);
    CHECK(TauntSound(false, 5) == 0x1F);
    CHECK_FALSE(TauntSound(false, 6));
    CHECK_FALSE(TauntSound(false, -1));
}

TEST_CASE("dropped traps loop once they land, and a magnet from the start") {
    CHECK_FALSE(LoopSound(sim::PowerupKind::OilSlick, At(sim::TrapPhase::Dropping)));
    CHECK(LoopSound(sim::PowerupKind::OilSlick, At(sim::TrapPhase::Armed)) == 0x30);
    CHECK(LoopSound(sim::PowerupKind::Curse, At(sim::TrapPhase::Fading)) == 9);
    CHECK_FALSE(LoopSound(sim::PowerupKind::Dynamite, At(sim::DynamitePhase::Lit)));
    CHECK(LoopSound(sim::PowerupKind::Dynamite, At(sim::DynamitePhase::Flying)) == 0x11);
    CHECK(LoopSound(sim::PowerupKind::Magnet, At(sim::MagnetPhase::Armed)) == 0x22);
    CHECK_FALSE(LoopSound(sim::PowerupKind::Shield, At(0)));
}

TEST_CASE("the lightning loop swells in and fades out by a tenth") {
    CHECK(LightningVolume(sim::LightningPhase::RampIn, Milliseconds{500}) == doctest::Approx(0.9F));
    CHECK(LightningVolume(sim::LightningPhase::RampIn, Milliseconds{0}) == doctest::Approx(1.0F));
    CHECK(LightningVolume(sim::LightningPhase::Sustain, Milliseconds{3000}) == 1.0F);
    CHECK(LightningVolume(sim::LightningPhase::Fade, Milliseconds{500}) == doctest::Approx(1.0F));
    CHECK(LightningVolume(sim::LightningPhase::Fade, Milliseconds{0}) == doctest::Approx(0.9F));
}

TEST_CASE("projectiles loop only while they fly, and a grapple changes loop once it holds") {
    CHECK_FALSE(FlightLoopOf(sim::PowerupKind::Cannonball, At(sim::ShotPhase::Armed)));
    CHECK(FlightLoopOf(sim::PowerupKind::Cannonball, At(sim::ShotPhase::Flying)) == 7);
    CHECK(FlightLoopOf(sim::PowerupKind::Missile, At(sim::ShotPhase::Flying)) == 0x34);
    CHECK(FlightLoopOf(sim::PowerupKind::GrapplingHook, At(sim::GrapplePhase::Flying)) == 0x17);
    CHECK(FlightLoopOf(sim::PowerupKind::GrapplingHook, At(sim::GrapplePhase::Pulling)) == 0x13);
    CHECK_FALSE(FlightLoopOf(sim::PowerupKind::GrapplingHook, At(sim::GrapplePhase::Retracting)));
    CHECK_FALSE(FlightLoopOf(sim::PowerupKind::OilSlick, At(sim::TrapPhase::Armed)));
}

TEST_CASE("a sustained bolt crackles at once, then every 200 to 499 ms") {
    Crackle crackle;
    sim::Random random(5);
    CHECK_FALSE(crackle.Tick(false, kTick, random));

    const auto [ticks, reaches] = CrackleFor(1000, crackle, random);

    REQUIRE(ticks.size() > 20);
    CHECK(ticks.front() == 0);
    const auto [shortest, longest] = GapRange(ticks);
    CHECK(shortest * kTick.count() > 200);
    CHECK(longest * kTick.count() <= 499 + kTick.count());
    CHECK(std::ranges::all_of(reaches, IsCrackleReach));
}

}
