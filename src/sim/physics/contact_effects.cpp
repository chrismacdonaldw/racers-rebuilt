#include "sim/physics/contact_effects.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rr::sim {
namespace {

constexpr float kShoveAlignment = 0.7F;
constexpr float kShoveForce = 200.0F;
constexpr float kShieldSpinRate = 0.009F;
constexpr std::uint32_t kScrapeSounds = 2;

void ScrapeOrShrink(Racer& first, Racer& second, const Vector3& point, Random& random) {
    if (first.Route() && second.Route()) {
        first.ShrinkBox();
        second.ShrinkBox();
        return;
    }
    const bool isSecondSlower = second.Body().derived.speed < first.Body().derived.speed;
    if (isSecondSlower && first.IsScrapeReady() && second.IsScrapeReady()) {
        first.Scrape(static_cast<std::uint8_t>(random.Below(kScrapeSounds)));
        second.HoldScrape();
    }
    if (isSecondSlower) {
        first.Spark(point);
    }
    (isSecondSlower ? second : first).React(VoiceReaction::Negative);
}

void ShieldActsOn(std::uint8_t level, Racer& other, const Vector3& point) {
    switch (level) {
        case 1: {
            const Vector3 away = Normalized(other.CentreOfMass() - point, {});
            if (std::abs(Dot(other.Body().motion.basis.forward, away)) < kShoveAlignment) {
                other.Shove(away * kShoveForce);
            }
            break;
        }
        case 2:
            other.StartSpin(1.0F, kShieldSpinRate);
            break;
        case 3:
            other.StartSpin(2.0F, kShieldSpinRate);
            break;
        default:
            break;
    }
}

bool CanPassCurse(const Racer& from, const Racer& to) {
    return from.Status().isCursed && from.Status().curseMs > 0 && !to.Status().isCursed &&
           !to.IsProtected();
}

void PassCurse(std::span<Racer> racers, std::size_t from, std::size_t to) {
    racers[to].AttachCurse(Milliseconds{racers[from].Status().curseMs},
                           {.giver = static_cast<std::uint8_t>(from)});
    racers[from].RemoveCurse();
}

}

void ApplyContactEffects(std::span<Racer> racers, const RacerContact& contact, Random& random) {
    Racer& first = racers[contact.first];
    Racer& second = racers[contact.second];
    const Vector3& point = contact.point;
    ScrapeOrShrink(first, second, point, random);
    if (const std::optional<std::uint8_t> level = first.Status().shield.level;
        level && !second.IsProtected()) {
        ShieldActsOn(*level, second, point);
    } else if (const std::optional<std::uint8_t> other = second.Status().shield.level;
               other && !first.IsProtected()) {
        ShieldActsOn(*other, first, point);
    }
    if (CanPassCurse(first, second)) {
        PassCurse(racers, contact.first, contact.second);
    } else if (CanPassCurse(second, first)) {
        PassCurse(racers, contact.second, contact.first);
    }
    first.MarkTouching();
    second.MarkTouching();
}

}
