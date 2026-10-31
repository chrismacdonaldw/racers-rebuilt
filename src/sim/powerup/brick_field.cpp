#include "sim/powerup/brick_field.hpp"

#include <numbers>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr float kSpinPerMs = 0.004F;
constexpr float kFullTurn = 2.0F * std::numbers::pi_v<float>;
constexpr float kIdleScale = 0.8F;
constexpr Milliseconds kStartTime{3000};
constexpr Milliseconds kGrowTime{400};
constexpr Milliseconds kActiveTime{500};
constexpr Milliseconds kTransitionTime{250};
constexpr Milliseconds kDroppedTime{10000};
constexpr float kGrowPerMs = 0.0025F;
constexpr float kSettlePerMs = 0.002F;
constexpr float kShrinkPerMs = 0.4F * kIdleScale * 0.01F;
constexpr Vector3 kDropProbe{.z = 25.0F};
constexpr float kDropHeight = 5.0F;

float Ms(Milliseconds time) {
    return static_cast<float>(time.count());
}

void Advance(Brick& brick, Milliseconds elapsed) {
    brick.stateTime += elapsed;
    brick.wasTouched = brick.isTouched;
    brick.isTouched = false;
}

void Enter(Brick& brick, BrickState state) {
    brick.state = state;
    brick.stateTime = Milliseconds{0};
}

bool Reaches(const BoundingSphere& racer, const Vector3& point) {
    const Vector3 offset = point - racer.centre;
    return Dot(offset, offset) < racer.radius * racer.radius;
}

bool TakesTouch(Brick& brick, const PowerupInventory& inventory) {
    const bool wasTouched = brick.wasTouched;
    brick.isTouched = true;
    return brick.state == BrickState::Idle && (!wasTouched || !inventory.Colour());
}

void GoHome(WhiteBrick& white) {
    white.droppedTime.reset();
    if (white.brick.state == BrickState::Idle) {
        Enter(white.brick, BrickState::Transition);
        white.isGoingHome = true;
        return;
    }
    white.brick.position = white.home;
    Enter(white.brick, BrickState::Active);
}

void UpdateWhite(WhiteBrick& white, Milliseconds elapsed) {
    Brick& brick = white.brick;
    Advance(brick, elapsed);
    if (white.droppedTime) {
        *white.droppedTime += elapsed;
        if (*white.droppedTime > kDroppedTime) {
            GoHome(white);
        }
    }
    if (brick.state == BrickState::Active && brick.stateTime > kActiveTime) {
        Enter(brick, BrickState::Idle);
    } else if (brick.state == BrickState::Transition && brick.stateTime > kTransitionTime) {
        if (white.isGoingHome) {
            white.isGoingHome = false;
            brick.position = white.home;
            Enter(brick, BrickState::Active);
        } else {
            Enter(brick, BrickState::Wait);
        }
    }
}

}

float BrickScale(const Brick& brick) {
    switch (brick.state) {
        case BrickState::Idle:
            return kIdleScale;
        case BrickState::Wait:
            return 0.0F;
        case BrickState::Active:
            if (brick.stateTime < kGrowTime) {
                return Ms(brick.stateTime) * kGrowPerMs;
            }
            if (brick.stateTime < kActiveTime) {
                return 1.0F - (Ms(brick.stateTime - kGrowTime) * kSettlePerMs);
            }
            return kIdleScale;
        case BrickState::Transition:
            return brick.stateTime <= kTransitionTime
                       ? kShrinkPerMs * Ms(kTransitionTime - brick.stateTime)
                       : 0.0F;
    }
    return 0.0F;
}

BrickField::BrickField(const assets::BrickLayout& layout) {
    coloured_.reserve(layout.coloured.size());
    for (const assets::ColourBrickPlacement& placement : layout.coloured) {
        coloured_.push_back({.brick = {.position = placement.position,
                                       .state = BrickState::Wait,
                                       .stateTime = kStartTime},
                             .colour = placement.colour,
                             .homeColour = placement.colour,
                             .nextColour = placement.colour,
                             .respawn = placement.respawn});
    }
    white_.reserve(layout.white.size());
    for (const Vector3& position : layout.white) {
        white_.push_back(
            {.brick = {.position = position, .state = BrickState::Active, .stateTime = kStartTime},
             .home = position});
    }
    events_.reserve(coloured_.size() + white_.size());
}

void BrickField::Tick(Milliseconds elapsed, std::span<const BoundingSphere> racers,
                      std::span<PowerupInventory> inventories) {
    RR_ASSERT(racers.size() == inventories.size());
    events_.clear();
    spinRadians_ += Ms(elapsed) * kSpinPerMs;
    if (spinRadians_ > kFullTurn) {
        spinRadians_ -= kFullTurn;
    }
    for (std::size_t i = 0; i < coloured_.size(); ++i) {
        UpdateColour(i, elapsed);
    }
    for (WhiteBrick& white : white_) {
        UpdateWhite(white, elapsed);
    }
    for (std::size_t racer = 0; racer < racers.size(); ++racer) {
        for (std::size_t i = 0; i < coloured_.size(); ++i) {
            if (Reaches(racers[racer], coloured_[i].brick.position) &&
                TakesTouch(coloured_[i].brick, inventories[racer])) {
                TouchColour(i, racer, inventories[racer]);
            }
        }
        for (std::size_t i = 0; i < white_.size(); ++i) {
            if (Reaches(racers[racer], white_[i].brick.position) &&
                TakesTouch(white_[i].brick, inventories[racer])) {
                TouchWhite(i, racer, inventories[racer]);
            }
        }
    }
}

void BrickField::Spend(PowerupInventory& inventory) {
    for (const std::optional<std::size_t> white : inventory.Spend()) {
        if (white) {
            GoHome(white_.at(*white));
        }
    }
}

void BrickField::DropWhite(PowerupInventory& inventory, const Vector3& at,
                           const CollisionQuery& world) {
    const std::optional<std::size_t> index = inventory.LoseWhite();
    if (!index) {
        return;
    }
    WhiteBrick& white = white_.at(*index);
    const std::optional<SegmentHit> ground = CastSolid(world, at + kDropProbe, at - kDropProbe);
    white.brick.position = ground ? ground->point : at;
    white.brick.position.z += kDropHeight;
    Enter(white.brick, BrickState::Active);
    white.droppedTime = Milliseconds{0};
}

void BrickField::UpdateColour(std::size_t index, Milliseconds elapsed) {
    ColourBrick& colour = coloured_[index];
    Brick& brick = colour.brick;
    Advance(brick, elapsed);
    switch (brick.state) {
        case BrickState::Idle:
            break;
        case BrickState::Wait:
            if (brick.stateTime >= colour.respawn) {
                Enter(brick, BrickState::Active);
                AddEvent(BrickEventKind::Respawned, index);
            }
            break;
        case BrickState::Active:
            if (brick.stateTime > kActiveTime) {
                Enter(brick, BrickState::Idle);
            }
            break;
        case BrickState::Transition:
            if (brick.stateTime > kTransitionTime) {
                Enter(brick, colour.nextState);
                colour.colour = colour.nextColour;
                if (brick.state == BrickState::Active) {
                    AddEvent(BrickEventKind::Respawned, index);
                }
            }
            break;
    }
}

void BrickField::TouchColour(std::size_t index, std::size_t racer, PowerupInventory& inventory) {
    ColourBrick& colour = coloured_[index];
    const std::optional<assets::BrickColour> held = inventory.TakeColour(colour.colour);
    colour.nextColour = held.value_or(colour.homeColour);
    colour.nextState = held ? BrickState::Active : BrickState::Wait;
    Enter(colour.brick, BrickState::Transition);
    AddEvent(held ? BrickEventKind::Swapped : BrickEventKind::Collected, index, racer);
}

void BrickField::TouchWhite(std::size_t index, std::size_t racer, PowerupInventory& inventory) {
    WhiteBrick& white = white_[index];
    if (!inventory.TakeWhite(index)) {
        return;
    }
    Enter(white.brick, BrickState::Transition);
    white.droppedTime.reset();
    AddEvent(BrickEventKind::WhiteCollected, index, racer, inventory.WhiteCount());
}

void BrickField::AddEvent(BrickEventKind kind, std::size_t brick, std::size_t racer,
                          std::size_t whiteCount) {
    RR_ASSERT(events_.size() < events_.capacity());
    events_.push_back({.kind = kind,
                       .brick = static_cast<std::uint32_t>(brick),
                       .racer = static_cast<std::uint8_t>(racer),
                       .whiteCount = static_cast<std::uint8_t>(whiteCount)});
}

}
