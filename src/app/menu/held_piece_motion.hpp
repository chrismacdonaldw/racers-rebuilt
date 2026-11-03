#pragma once

#include <cstdint>
#include <optional>

#include "core/duration.hpp"

namespace rr::app {

inline constexpr float kHoverHeight = 8.4F;

struct PieceLayer {
    float height = 0.0F;
    float alpha = 1.0F;

    friend bool operator==(const PieceLayer&, const PieceLayer&) = default;
};

/// Heights are world units above the car's base.
struct HeldPieceLayers {
    std::optional<float> solid;
    std::optional<PieceLayer> trail;
    std::optional<PieceLayer> ghost;
};

enum class PieceMotionEnd : std::uint8_t { None, Dropped, Lifted };

class HeldPieceMotion {
public:
    /// `rest` is where the held piece would land.
    PieceMotionEnd Tick(Milliseconds elapsed, bool fits, float rest);

    void Drop();

    void Lift(float rest);

    [[nodiscard]] bool IsBusy() const { return busy_ != Busy::None; }

    [[nodiscard]] HeldPieceLayers Layers(bool isOverhead) const;

private:
    enum class Trail : std::uint8_t { Off, Lowering, Holding, Raising };
    enum class Busy : std::uint8_t { None, Dropping, Lifting };

    void MoveTrail(Milliseconds elapsed, bool fits, float rest);
    void StepTrail(Milliseconds elapsed);
    PieceMotionEnd MoveBusy(Milliseconds elapsed);
    [[nodiscard]] float GhostAlpha(bool isOverhead) const;

    Trail trail_ = Trail::Off;
    Milliseconds trailLeft_{0};
    float trailHeight_ = 0.0F;
    float rest_ = 0.0F;
    Busy busy_ = Busy::None;
    Milliseconds busyLeft_{0};
    float busyHeight_ = 0.0F;
    Milliseconds bob_{0};
};

}
