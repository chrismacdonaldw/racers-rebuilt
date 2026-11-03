#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "assets/brick_layout.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/powerup/powerup_inventory.hpp"

namespace rr::sim {

enum class BrickState : std::uint8_t {
    Idle,
    Wait,
    Active,
    Transition,
};

struct Brick {
    Vector3 position;
    BrickState state = BrickState::Idle;
    Milliseconds stateTime{0};
    bool isTouched = false;
    bool wasTouched = false;
};

struct ColourBrick {
    Brick brick;
    assets::BrickColour colour = assets::BrickColour::Green;
    assets::BrickColour homeColour = assets::BrickColour::Green;
    assets::BrickColour nextColour = assets::BrickColour::Green;
    BrickState nextState = BrickState::Wait;
    Milliseconds respawn = assets::kDefaultBrickRespawn;
};

struct WhiteBrick {
    Brick brick;
    Vector3 home;
    std::optional<Milliseconds> droppedTime;
    bool isGoingHome = false;
};

enum class BrickEventKind : std::uint8_t {
    Respawned,
    Collected,
    Swapped,
    WhiteCollected,
};

/// `brick` indexes the white bricks for `WhiteCollected` and the coloured ones otherwise.
struct BrickEvent {
    BrickEventKind kind = BrickEventKind::Respawned;
    std::uint32_t brick = 0;
    std::uint8_t racer = 0;
    std::uint8_t whiteCount = 0;

    friend bool operator==(const BrickEvent&, const BrickEvent&) = default;
};

/// 0 while hidden.
[[nodiscard]] float BrickScale(const Brick& brick);

class BrickField {
public:
    BrickField() = default;
    explicit BrickField(const assets::BrickLayout& layout,
                        std::optional<assets::BrickColour> everyBrick = std::nullopt);

    /// `racers` and `inventories` have one entry per racer.
    void Tick(Milliseconds elapsed, std::span<const BoundingSphere> racers,
              std::span<PowerupInventory> inventories);

    /// Empties `inventory` and sends its white bricks home.
    void Spend(PowerupInventory& inventory);

    /// Puts the first white brick `inventory` holds on the ground below `at`.
    void DropWhite(PowerupInventory& inventory, const Vector3& at, const CollisionQuery& world);

    [[nodiscard]] bool IsEmpty() const { return coloured_.empty() && white_.empty(); }
    [[nodiscard]] std::span<const ColourBrick> Coloured() const { return coloured_; }
    [[nodiscard]] std::span<const WhiteBrick> White() const { return white_; }
    [[nodiscard]] float SpinRadians() const { return spinRadians_; }

    [[nodiscard]] std::span<const BrickEvent> Events() const { return events_; }

private:
    void UpdateColour(std::size_t index, Milliseconds elapsed);
    void TouchColour(std::size_t index, std::size_t racer, PowerupInventory& inventory);
    void TouchWhite(std::size_t index, std::size_t racer, PowerupInventory& inventory);
    void AddEvent(BrickEventKind kind, std::size_t brick, std::size_t racer = 0,
                  std::size_t whiteCount = 0);

    std::vector<ColourBrick> coloured_;
    std::vector<WhiteBrick> white_;
    std::vector<BrickEvent> events_;
    float spinRadians_ = 0.0F;
};

}
