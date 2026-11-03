#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/vector3.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxSurfaceTouches = 4;

struct SurfaceTouch {
    std::int32_t event = 0;
    Vector3 point;
};

/// Keeps the first touch of each event and drops touches past `kMaxSurfaceTouches`.
class SurfaceTouches {
public:
    void Add(std::int32_t event, const Vector3& point) {
        for (const SurfaceTouch& touch : Touches()) {
            if (touch.event == event) {
                return;
            }
        }
        if (count_ < touches_.size()) {
            touches_.at(count_++) = {.event = event, .point = point};
        }
    }

    void ReachFinish() { hasReachedFinish_ = true; }

    [[nodiscard]] std::span<const SurfaceTouch> Touches() const {
        return std::span(touches_).first(count_);
    }

    [[nodiscard]] bool HasReachedFinish() const { return hasReachedFinish_; }

private:
    std::array<SurfaceTouch, kMaxSurfaceTouches> touches_{};
    std::size_t count_ = 0;
    bool hasReachedFinish_ = false;
};

}
