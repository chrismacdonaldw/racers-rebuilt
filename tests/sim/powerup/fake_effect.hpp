#pragma once

#include <memory>

#include "sim/powerup/powerup_effect.hpp"

namespace rr::test {

inline constexpr Milliseconds kFakePhaseLength{100};

/// Moves to the next phase every `kFakePhaseLength` and finishes after `lifetime`.
class FakeEffect final : public sim::PowerupEffect {
public:
    FakeEffect(Milliseconds lifetime, bool blocksHits)
        : lifetime_(lifetime), blocksHits_(blocksHits) {}

    void Start(const sim::PowerupUse& use, sim::PowerupContext& /*context*/) override {
        use_ = use;
        age_ = Milliseconds{0};
    }

    [[nodiscard]] bool Tick(Milliseconds elapsed, sim::PowerupContext& /*context*/) override {
        age_ += elapsed;
        return age_ < lifetime_;
    }

    [[nodiscard]] bool Hit(const sim::PowerupHit& /*hit*/,
                           sim::PowerupContext& /*context*/) override {
        return blocksHits_;
    }

    void End(sim::PowerupContext& /*context*/) override {}

    [[nodiscard]] sim::EffectProgress Progress() const override {
        return {.phase = static_cast<std::uint8_t>(age_ / kFakePhaseLength),
                .phaseTime = age_ % kFakePhaseLength};
    }

    [[nodiscard]] Transform Pose() const override {
        return {.position = {.x = static_cast<float>(use_.shot)}};
    }

private:
    Milliseconds lifetime_;
    bool blocksHits_ = false;
    sim::PowerupUse use_;
    Milliseconds age_{0};
};

[[nodiscard]] inline sim::EffectFactories FakeEffects(Milliseconds lifetime,
                                                      bool blocksHits = false) {
    sim::EffectFactories factories;
    for (sim::EffectFactory& factory : factories) {
        factory = [lifetime, blocksHits] {
            return std::make_unique<FakeEffect>(lifetime, blocksHits);
        };
    }
    return factories;
}

}
