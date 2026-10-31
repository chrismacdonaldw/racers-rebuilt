#include "sim/powerup/powerup_system.hpp"

#include <algorithm>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kMaxLaunches = 3;
constexpr std::size_t kEventsPerUse = 1 + (2 * kMaxLaunches);

bool OutranksForReclaim(const EffectProgress& candidate, const EffectProgress& best) {
    if (candidate.phase != best.phase) {
        return candidate.phase > best.phase;
    }
    return candidate.phaseTime < best.phaseTime;
}

std::optional<EffectEntities> Claim(const EffectEntities& used, std::size_t count) {
    EffectEntities claimed;
    for (std::size_t i = 0; i < kEffectEntities && claimed.count() < count; ++i) {
        if (!used.test(i)) {
            claimed.set(i);
        }
    }
    return claimed.count() == count ? std::optional(claimed) : std::nullopt;
}

}

PowerupSystem::PowerupSystem(const EffectFactories& factories, std::size_t racerCount) {
    for (std::size_t kind = 0; kind < kPowerupKindCount; ++kind) {
        poolStarts_.at(kind) = slots_.size();
        if (!factories.at(kind)) {
            continue;
        }
        const auto kindId = static_cast<PowerupKind>(kind);
        for (std::size_t i = 0; i < SpecFor(kindId).poolSize; ++i) {
            slots_.push_back({.effect = factories.at(kind)(), .kind = kindId});
        }
    }
    poolStarts_.back() = slots_.size();
    active_.reserve(slots_.size());
    isDone_.resize(slots_.size());
    events_.reserve(slots_.size() + (racerCount * kEventsPerUse));
}

void PowerupSystem::BeginTick() {
    events_.clear();
}

void PowerupSystem::Use(std::size_t racer, PowerupInventory& inventory, BrickField& bricks,
                        PowerupContext& context) {
    const PowerupUse taunt{.racer = static_cast<std::uint8_t>(racer)};
    if (!inventory.Colour()) {
        AddEvent(PowerupEventKind::Taunted, taunt);
        return;
    }
    const PowerupUse use{.racer = taunt.racer,
                         .colour = *inventory.Colour(),
                         .level = static_cast<std::uint8_t>(inventory.WhiteCount())};
    AddEvent(PowerupEventKind::Used, use);
    Fire(use, context);
    bricks.Spend(inventory);
}

void PowerupSystem::Fire(const PowerupUse& use, PowerupContext& context) {
    const PowerupKind kind = KindFor(use.colour, use.level);
    const PowerupKindSpec& spec = SpecFor(kind);
    for (std::size_t shot = 0; shot < spec.launches; ++shot) {
        if (kEffectEntities - EntitiesInUse() < spec.entities) {
            return;
        }
        std::optional<std::size_t> slot = FreeSlot(kind);
        if (!slot) {
            slot = Reclaim(kind, use.level, context);
        }
        if (!slot) {
            return;
        }
        PowerupUse launch = use;
        launch.shot = static_cast<std::uint8_t>(shot);
        Activate(*slot, launch, context);
    }
}

void PowerupSystem::Tick(Milliseconds elapsed, PowerupContext& context) {
    for (const std::uint16_t slot : active_) {
        isDone_[slot] = !slots_[slot].effect->Tick(elapsed, context);
    }
    for (const std::uint16_t slot : active_) {
        if (isDone_[slot]) {
            Finish(slot, context);
        }
    }
    std::erase_if(active_, [this](std::uint16_t slot) { return !slots_[slot].isActive; });
}

bool PowerupSystem::Hit(const PowerupHit& hit, PowerupContext& context) {
    return std::ranges::any_of(active_, [&](std::uint16_t slot) {
        const EffectSlot& owner = slots_[slot];
        return owner.use.racer == hit.racer && owner.effect->Hit(hit, context);
    });
}

std::optional<std::size_t> PowerupSystem::FreeSlot(PowerupKind kind) const {
    const auto pool = static_cast<std::size_t>(kind);
    for (std::size_t slot = poolStarts_.at(pool); slot < poolStarts_.at(pool + 1); ++slot) {
        if (!slots_[slot].isActive) {
            return slot;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> PowerupSystem::Reclaim(PowerupKind kind, std::uint8_t level,
                                                  PowerupContext& context) {
    std::optional<std::size_t> best;
    for (const std::uint16_t slot : active_) {
        const EffectSlot& candidate = slots_[slot];
        if (candidate.kind == kind && candidate.use.level == level &&
            (!best ||
             OutranksForReclaim(candidate.effect->Progress(), slots_[*best].effect->Progress()))) {
            best = slot;
        }
    }
    if (best) {
        Finish(*best, context);
        std::erase(active_, static_cast<std::uint16_t>(*best));
    }
    return best;
}

void PowerupSystem::Activate(std::size_t slot, const PowerupUse& use, PowerupContext& context) {
    EffectSlot& effect = slots_[slot];
    const std::optional<EffectEntities> claimed = Claim(entities_, SpecFor(effect.kind).entities);
    RR_ASSERT(claimed.has_value());
    effect.use = use;
    effect.entities = *claimed;
    effect.isActive = true;
    entities_ |= effect.entities;
    isDone_[slot] = false;
    active_.push_back(static_cast<std::uint16_t>(slot));
    effect.effect->Start(use, context);
    AddEvent(PowerupEventKind::Started, use, slot);
}

void PowerupSystem::Finish(std::size_t slot, PowerupContext& context) {
    EffectSlot& effect = slots_[slot];
    effect.effect->End(context);
    entities_ &= ~effect.entities;
    effect.entities.reset();
    effect.isActive = false;
    AddEvent(PowerupEventKind::Ended, effect.use, slot);
}

void PowerupSystem::AddEvent(PowerupEventKind kind, const PowerupUse& use, std::size_t effect) {
    RR_ASSERT(events_.size() < events_.capacity());
    events_.push_back({.kind = kind, .use = use, .effect = static_cast<std::uint16_t>(effect)});
}

}
