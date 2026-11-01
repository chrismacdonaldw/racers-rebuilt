#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/powerup_inventory.hpp"
#include "sim/powerup/powerup_kind.hpp"

namespace rr::sim {

inline constexpr std::size_t kEffectEntities = 25;
inline constexpr std::size_t kCrowdedEntities = 20;

using EffectEntities = std::bitset<kEffectEntities>;

struct EffectSlot {
    std::unique_ptr<PowerupEffect> effect;
    PowerupKind kind = PowerupKind::Cannonball;
    PowerupUse use;
    EffectEntities entities;
    bool isActive = false;
};

enum class PowerupEventKind : std::uint8_t {
    Taunted,
    Used,
    Started,
    Ended,
};

struct PowerupEvent {
    PowerupEventKind kind = PowerupEventKind::Used;
    PowerupUse use;
    std::uint16_t effect = 0;

    friend bool operator==(const PowerupEvent&, const PowerupEvent&) = default;
};

/// Every effect is built up front, so using, ticking and ending effects never allocates.
class PowerupSystem {
public:
    PowerupSystem(const EffectFactories& factories, std::size_t usesPerTick);

    void BeginTick();

    /// Fires and spends what `inventory` holds, or taunts when it holds no colour.
    void Use(std::size_t racer, PowerupInventory& inventory, BrickField& bricks,
             PowerupContext& context);

    /// Skips a launch when entities run short, or the pool is full with no effect of this level.
    void Fire(const PowerupUse& use, PowerupContext& context);

    void Tick(Milliseconds elapsed, PowerupContext& context);

    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context);

    [[nodiscard]] std::size_t EntitiesInUse() const { return entities_.count(); }

    /// AI racers hold their power-ups while this is true.
    [[nodiscard]] bool IsCrowded() const { return EntitiesInUse() >= kCrowdedEntities; }

    [[nodiscard]] std::span<const EffectSlot> Slots() const { return slots_; }

    /// Indexes into `Slots`, in the order the effects started.
    [[nodiscard]] std::span<const std::uint16_t> Active() const { return active_; }

    [[nodiscard]] std::span<const PowerupEvent> Events() const { return events_; }

private:
    [[nodiscard]] std::optional<std::size_t> FreeSlot(PowerupKind kind) const;
    [[nodiscard]] std::optional<std::size_t> Reclaim(PowerupKind kind, std::uint8_t level,
                                                     PowerupContext& context);
    void Activate(std::size_t slot, const PowerupUse& use, PowerupContext& context);
    void Finish(std::size_t slot, PowerupContext& context);
    void AddEvent(PowerupEventKind kind, const PowerupUse& use, std::size_t effect = 0);

    std::vector<EffectSlot> slots_;
    std::array<std::size_t, kPowerupKindCount + 1> poolStarts_{};
    std::vector<std::uint16_t> active_;
    std::vector<bool> isDone_;
    std::vector<PowerupEvent> events_;
    EffectEntities entities_;
};

}
