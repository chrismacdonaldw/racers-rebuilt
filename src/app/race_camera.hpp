#pragma once

#include <cstdint>
#include <optional>

#include "core/quaternion.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/physics/turbo.hpp"

namespace rr::sim {
class Racer;
}

namespace rr::app {

/// `pose` is in game space with +X forward. Speeds are in units per ms.
struct CameraSubject {
    Transform pose;
    float forwardSpeed = 0.0F;
    float turnRadius = 0.0F;
    bool isSpinning = false;
    std::optional<sim::TurboLevel> turbo;
};

[[nodiscard]] CameraSubject SubjectOf(const sim::Racer& racer);

enum class CameraView : std::uint8_t { Standard, Far, Close, Cockpit, Finish };

/// Follows in game space and eases toward each new view.
class RaceCamera {
public:
    explicit RaceCamera(const CameraSubject& subject);

    /// Eases into `view` from the current pose. Split screen has its own chase views.
    void SetView(CameraView view, bool isSplitScreen);

    /// Skips the finish view and cuts straight to the next one.
    void CycleView();

    /// The next `Follow` snaps instead of easing.
    void Cut();

    void SetLookBack(bool isLookingBack);

    void Follow(const CameraSubject& subject, float elapsedMs);

    [[nodiscard]] CameraView View() const { return view_; }

    /// In engine space.
    [[nodiscard]] Transform Pose() const;

private:
    enum class Mode : std::uint8_t { Chase, Cockpit, Finish };

    struct Shot {
        Vector3 position;
        Quaternion rotation;
    };

    [[nodiscard]] static Mode ModeOf(CameraView view);
    [[nodiscard]] Shot FollowChase(const CameraSubject& subject, float elapsedMs, bool isCut);
    [[nodiscard]] Shot FollowCockpit(const CameraSubject& subject);
    [[nodiscard]] Vector3 SteerView(const CameraSubject& subject, const Vector3& heading,
                                    float elapsedMs);
    [[nodiscard]] float FollowDistance(const CameraSubject& subject, float elapsedMs);
    void Smooth(const Shot& raw, float elapsedMs, bool isCut);

    CameraView view_ = CameraView::Standard;
    Mode mode_ = Mode::Chase;
    bool isSplitScreen_ = false;
    bool isCut_ = true;
    bool isLookingBack_ = false;
    float pitchSine_ = 0.0F;
    float pitchCosine_ = 1.0F;
    float heightSine_ = 0.0F;
    float followDistance_ = 0.0F;
    float positionLag_ = 0.0F;
    float rotationLag_ = 0.0F;
    Vector3 target_;
    Vector3 viewDirection_{.x = 1.0F};
    float turnLead_ = 0.0F;
    bool hadTurbo_ = false;
    float zoom_ = 1.0F;
    float finishMs_ = 0.0F;
    Shot shot_;
};

}
