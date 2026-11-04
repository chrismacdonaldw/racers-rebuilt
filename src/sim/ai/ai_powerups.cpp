#include "sim/ai/ai_powerups.hpp"

#include "core/assert.hpp"
#include "sim/powerup/aim_cones.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;

constexpr AimCone kRedCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 62'500.0F, .cosine = 0.96F};
constexpr AimCone kDropCone{
    .minDistanceSquared = 25.0F, .maxDistanceSquared = 10'000.0F, .cosine = 0.96F};
constexpr std::uint32_t kByteRange = 256;

bool Rolls(std::uint8_t chance, Random& random) {
    return random.Below(kByteRange) < chance;
}

bool HasRacerAhead(const AiPowerupView& view) {
    const BodyState& body = view.racers[view.racer].Body();
    return FindRacerInCone(view.racers, body.motion.position, body.motion.basis.forward, kRedCone)
        .has_value();
}

bool HasRacerBehind(const AiPowerupView& view) {
    const BodyState& body = view.racers[view.racer].Body();
    return FindRacerInCone(view.racers, body.motion.position, -body.motion.basis.forward, kDropCone)
        .has_value();
}

bool IsUnaimed(BrickColour colour, const AiPowerupView& view) {
    if (colour == BrickColour::Red) {
        return !HasRacerAhead(view);
    }
    return colour == BrickColour::Yellow && view.isLiningUp && !HasRacerBehind(view);
}

bool HoldsGreenBack(const AiPowerupView& view) {
    return view.racers[view.racer].IsSpinning() ||
           (view.hasFinished && view.inventory->WhiteCount() == kMaxWhiteBricks);
}

}

bool AiPowerupUser::Tick(Milliseconds elapsed, const AiPowerupView& view, Random& random) {
    RR_ASSERT(view.inventory != nullptr && view.racer < view.racers.size());
    sinceCheck_ += elapsed;
    if (sinceCheck_ <= wait_) {
        return false;
    }
    sinceCheck_ = Milliseconds{0};
    wait_ = kAiUseCheck;
    if (!view.inventory->Colour()) {
        return false;
    }
    const Choice choice = Choose(view, random);
    if (choice == Choice::Hold) {
        wait_ = kAiUseHold;
    }
    return choice == Choice::Use;
}

bool AiPowerupUser::RollsStartBoost(Random& random) const {
    if (habits_.startBoost) {
        return *habits_.startBoost;
    }
    return Rolls(habits_.UseChance(BrickColour::Green), random);
}

AiPowerupUser::Choice AiPowerupUser::Choose(const AiPowerupView& view, Random& random) const {
    if (view.isCrowded || IsCharging(*view.inventory)) {
        return Choice::Hold;
    }
    const BrickColour colour = *view.inventory->Colour();
    if (IsUnaimed(colour, view)) {
        return Choice::Recheck;
    }
    if (!Rolls(habits_.UseChance(colour), random)) {
        return Choice::Hold;
    }
    if (colour == BrickColour::Green && HoldsGreenBack(view)) {
        return Choice::Recheck;
    }
    return Choice::Use;
}

bool AiPowerupUser::IsCharging(const PowerupInventory& inventory) const {
    return inventory.Colour() == habits_.chargeColour &&
           inventory.WhiteCount() < habits_.chargeWhites;
}

}
