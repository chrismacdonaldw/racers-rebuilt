#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "app/ground_decal.hpp"
#include "app/quad_shapes.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct SkidSegment {
    Vector3 from;
    Vector3 to;
};

/// A growing segment that settles every quarter of `lifeMs`, behind up to three settled ones.
class SkidTrail {
public:
    SkidTrail(const QuadLook& look, float width, float lifeMs);

    [[nodiscard]] float LifeMs() const { return lifeMs_; }
    [[nodiscard]] bool IsGone() const { return isGone_; }

    /// `wheel` is in game space.
    void Follow(const Vector3& wheel, float elapsedMs);

    /// Stops following and fades the whole mark out over `fadeMs`, or removes it at once for 0.
    void Release(float fadeMs);
    void Fade(float elapsedMs);

    void Append(GroundDecals& ground, std::vector<render::QuadDraw>& out) const;

private:
    static constexpr std::size_t kSettledCount = 3;

    QuadLook look_;
    float width_ = 0.0F;
    float lifeMs_ = 0.0F;
    std::optional<Vector3> start_;
    std::optional<Vector3> head_;
    std::array<std::optional<SkidSegment>, kSettledCount> settled_{};
    std::size_t next_ = 0;
    float ageMs_ = 0.0F;
    std::optional<float> fadeMs_;
    float fadedMs_ = 0.0F;
    bool isGone_ = false;
};

}
