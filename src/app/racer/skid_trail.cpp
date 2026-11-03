#include "app/racer/skid_trail.hpp"

namespace rr::app {
namespace {

constexpr float kShortestSegment = 0.1F;
constexpr float kSegmentsPerLife = 4.0F;

void AppendSegment(const QuadLook& look, float width, const SkidSegment& segment,
                   GroundDecals& ground, std::vector<render::QuadDraw>& out) {
    const Vector3 along = segment.to - segment.from;
    ground.Append(look,
                  {.centre = (segment.from + segment.to) * 0.5F,
                   .forward = along,
                   .length = Length(along),
                   .width = width},
                  out);
}

}

SkidTrail::SkidTrail(const QuadLook& look, float width, float lifeMs)
    : look_(look), width_(width), lifeMs_(lifeMs) {}

void SkidTrail::Follow(const Vector3& wheel, float elapsedMs) {
    if (fadeMs_) {
        return;
    }
    if (!start_) {
        start_ = wheel;
        return;
    }
    ageMs_ += elapsedMs;
    if (Length(wheel - *start_) < kShortestSegment) {
        return;
    }
    head_ = wheel;
    if (ageMs_ > lifeMs_ / kSegmentsPerLife) {
        settled_[next_] = SkidSegment{.from = *start_, .to = wheel};
        next_ = (next_ + 1) % kSettledCount;
        start_ = wheel;
        head_.reset();
        ageMs_ = 0.0F;
    }
}

void SkidTrail::Release(float fadeMs) {
    if (fadeMs <= 0.0F) {
        isGone_ = true;
        return;
    }
    fadeMs_ = fadeMs;
    fadedMs_ = 0.0F;
}

void SkidTrail::Fade(float elapsedMs) {
    if (!fadeMs_) {
        return;
    }
    fadedMs_ += elapsedMs;
    isGone_ = fadedMs_ >= *fadeMs_;
}

void SkidTrail::Append(GroundDecals& ground, std::vector<render::QuadDraw>& out) const {
    if (isGone_) {
        return;
    }
    QuadLook look = look_;
    if (fadeMs_) {
        look.tint.a *= 1.0F - (fadedMs_ / *fadeMs_);
    }
    for (const std::optional<SkidSegment>& segment : settled_) {
        if (segment) {
            AppendSegment(look, width_, *segment, ground, out);
        }
    }
    if (start_ && head_) {
        AppendSegment(look, width_, {.from = *start_, .to = *head_}, ground, out);
    }
}

}
