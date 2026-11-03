#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "core/vector3.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

enum class DriverPart : std::uint8_t {
    Default,
    SteerRight,
    SteerLeft,
    Hit,
    SuddenStop,
    Reverse,
    ReverseHold,
    ReverseBack,
    SideLookRight,
    SideLookLeft,
    RearLookRight,
    RearLookLeft,
    Winner,
    WinnerStart,
    WinnerLoop,
    Loser,
    LoserLoop,
};

inline constexpr std::size_t kDriverPartCount = 17;

inline constexpr std::array<std::string_view, kDriverPartCount> kDriverPartNames{
    "default",  "steer-r",  "steer-l",  "power1",   "velostop", "reverse",
    "rev-hold", "rev-back", "s-lean-r", "s-lean-l", "l-lean-r", "l-lean-l",
    "winner1",  "winner2",  "win2_lp",  "loser",    "loser_lp"};

/// `nearestRacer` is a unit vector in the driver's frame: x forward, y to the left.
struct DriverSense {
    float turnRadius = 0.0F;
    bool isReversing = false;
    bool wasHit = false;
    bool hasStopped = false;
    std::optional<int> finishPlace;
    std::optional<Vector3> nearestRacer;
};

enum class DriverCueKind : std::uint8_t { Once, Loop, Blend };

struct DriverCue {
    DriverPart part = DriverPart::Default;
    DriverCueKind kind = DriverCueKind::Loop;
    DriverPart then = DriverPart::Default;

    friend constexpr bool operator==(const DriverCue&, const DriverCue&) = default;
};

/// Picks the driver's part each frame as the original does (`0043ec10`).
class DriverMoves {
public:
    explicit DriverMoves(sim::Random random);

    /// `isPlayingOnce` says whether the last `Once` part is still short of its end.
    [[nodiscard]] std::optional<DriverCue> Next(const DriverSense& sense, bool isPlayingOnce,
                                                float elapsedMs);

    [[nodiscard]] DriverPart Current() const { return current_; }

private:
    [[nodiscard]] std::optional<DriverCue> Decide(const DriverSense& sense);
    [[nodiscard]] std::optional<DriverCue> Finish(int place);
    [[nodiscard]] std::optional<DriverCue> Look(const Vector3& nearest);
    [[nodiscard]] std::optional<DriverCue> Steer(float turnRadius) const;
    [[nodiscard]] bool IsHeld() const;
    void Take(const DriverCue& cue);

    sim::Random random_;
    DriverPart current_ = DriverPart::Default;
    std::optional<DriverPart> queued_;
    float lookCooldownMs_ = 0.0F;
};

}
