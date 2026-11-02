#pragma once

#include <array>
#include <optional>
#include <vector>

#include "app/quad_shapes.hpp"
#include "app/racer/skid_trail.hpp"
#include "assets/chassis_database.hpp"
#include "assets/track_loader.hpp"
#include "render/render_view.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

using SkidWheels = std::array<bool, assets::kChassisWheelCount>;

/// One car's marks, which the original leaves only under human drivers.
class SkidMarks {
public:
    SkidMarks(const assets::NameMap<render::Material>& materials, const assets::SkidMarks& wheels);

    /// `pose` is in engine space.
    void Update(const sim::EntitySnapshot& pose, const sim::RacerSnapshot& racer, float elapsedMs);

    void Append(const sim::CollisionQuery& ground, std::vector<render::QuadDraw>& out) const;

    [[nodiscard]] std::optional<SkidWheels> Skidding() const;

private:
    void Follow(const sim::EntitySnapshot& pose, const sim::SkidSnapshot& skid, float elapsedMs);
    void Start(const sim::SkidSnapshot& skid);
    void Stop();
    [[nodiscard]] std::optional<SkidTrail> NewTrail(std::size_t wheel) const;

    std::optional<QuadLook> skidLook_;
    std::optional<QuadLook> turboLook_;
    assets::SkidMarks wheels_;
    sim::SkidSnapshot last_;
    bool wasGhost_ = false;
    bool isSkidding_ = false;
    bool isSpinMarking_ = false;
    bool isTurboMarking_ = false;
    SkidWheels marks_{};
    std::array<std::optional<SkidTrail>, assets::kChassisWheelCount> trails_;
    std::vector<SkidTrail> fading_;
};

}
