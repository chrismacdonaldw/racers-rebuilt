#include "app/racer/skid_marks.hpp"

#include <cstddef>
#include <string_view>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kSkidMaterial = "SKID";
constexpr std::string_view kTurboSkidMaterial = "TRBSKID";
constexpr float kMarkMs = 1000.0F;
constexpr float kSpinMarkMs = 250.0F;
constexpr float kStopFadeMs = 1000.0F;
constexpr std::size_t kFrontWheels = 2;
constexpr std::size_t kFadingStops = 2;
constexpr SkidWheels kRearWheels{false, false, true, true};
constexpr SkidWheels kLeftWheels{false, true, false, true};
constexpr SkidWheels kRightWheels{true, false, true, false};

std::optional<QuadLook> Find(const assets::NameMap<render::Material>& materials,
                             std::string_view name) {
    const auto found = materials.find(name);
    return found == materials.end() ? std::nullopt : std::optional(LookOf(found->second));
}

bool Rose(bool was, bool is) {
    return is && !was;
}

bool Fell(bool was, bool is) {
    return was && !is;
}

}

SkidMarks::SkidMarks(const assets::NameMap<render::Material>& materials,
                     const assets::SkidMarks& wheels)
    : skidLook_(Find(materials, kSkidMaterial)),
      turboLook_(Find(materials, kTurboSkidMaterial)),
      wheels_(wheels) {
    fading_.reserve(kFadingStops * assets::kChassisWheelCount);
}

void SkidMarks::Update(const sim::EntitySnapshot& pose, const sim::RacerSnapshot& racer,
                       float elapsedMs) {
    const sim::SkidSnapshot& skid = racer.skid;
    if (Rose(wasGhost_, racer.isGhost)) {
        Stop();
    }
    if (skid.turboCount != last_.turboCount && skid.turbo && *skid.turbo != sim::TurboLevel::Weak) {
        isTurboMarking_ = true;
        Start(skid);
    }
    if (Fell(last_.turbo.has_value(), skid.turbo.has_value())) {
        Stop();
        isTurboMarking_ = false;
    }
    if (Rose(last_.isDrifting, skid.isDrifting) || Rose(last_.isMagnetHeld, skid.isMagnetHeld)) {
        Start(skid);
    }
    if ((Fell(last_.isDrifting, skid.isDrifting) && !skid.turbo) ||
        Fell(last_.isMagnetHeld, skid.isMagnetHeld)) {
        Stop();
    }
    if (skid.isSpinning != isSpinMarking_) {
        isSpinMarking_ = skid.isSpinning;
        if (isSpinMarking_) {
            Start(skid);
        } else {
            Stop();
        }
    }
    if (skid.isHalted) {
        Stop();
    }
    if (isSkidding_) {
        Follow(pose, skid, elapsedMs);
    }
    for (SkidTrail& trail : fading_) {
        trail.Fade(elapsedMs);
    }
    std::erase_if(fading_, [](const SkidTrail& trail) { return trail.IsGone(); });
    last_ = skid;
    wasGhost_ = racer.isGhost;
}

void SkidMarks::Append(const sim::CollisionQuery& ground,
                       std::vector<render::QuadDraw>& out) const {
    if (wasGhost_) {
        return;
    }
    for (const std::optional<SkidTrail>& trail : trails_) {
        if (trail) {
            trail->Append(ground, out);
        }
    }
    for (const SkidTrail& trail : fading_) {
        trail.Append(ground, out);
    }
}

std::optional<SkidWheels> SkidMarks::Skidding() const {
    return isSkidding_ ? std::optional(marks_) : std::nullopt;
}

void SkidMarks::Follow(const sim::EntitySnapshot& pose, const sim::SkidSnapshot& skid,
                       float elapsedMs) {
    const Transform car = TransformToGame(pose.transform);
    for (std::size_t i = 0; i < trails_.size(); ++i) {
        if (std::optional<SkidTrail>& trail = trails_[i]) {
            trail->Follow(Apply(car, wheels_.wheelOffsets[i]), elapsedMs);
        }
    }
    if (!skid.isSpinning) {
        if (skid.turbo) {
            marks_ = kRearWheels;
        } else {
            marks_ = skid.leftwardSpeed > 0.0F ? kLeftWheels : kRightWheels;
        }
    }
    const float lifeMs = isSpinMarking_ ? kSpinMarkMs : kMarkMs;
    for (std::size_t i = 0; i < trails_.size(); ++i) {
        std::optional<SkidTrail>& trail = trails_[i];
        if (trail && (!marks_[i] || trail->LifeMs() != lifeMs)) {
            trail.reset();
        }
        if (marks_[i] && !trail) {
            trail = NewTrail(i);
        }
    }
}

void SkidMarks::Start(const sim::SkidSnapshot& skid) {
    if (skid.isHalted) {
        return;
    }
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        marks_[i] = skid.isSpinning || i >= kFrontWheels;
    }
    isSkidding_ = true;
}

void SkidMarks::Stop() {
    for (std::optional<SkidTrail>& trail : trails_) {
        if (trail) {
            trail->Release(kStopFadeMs);
            fading_.push_back(*trail);
            trail.reset();
        }
    }
    isSkidding_ = false;
}

std::optional<SkidTrail> SkidMarks::NewTrail(std::size_t wheel) const {
    const std::optional<QuadLook>& look = isTurboMarking_ ? turboLook_ : skidLook_;
    if (!look) {
        return std::nullopt;
    }
    const float width = wheel < kFrontWheels ? wheels_.frontWidth : wheels_.rearWidth;
    return SkidTrail(*look, width, isSpinMarking_ ? kSpinMarkMs : kMarkMs);
}

}
