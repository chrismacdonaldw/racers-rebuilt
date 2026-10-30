#pragma once

#include <string_view>

#include "app/chase_camera.hpp"
#include "assets/race_route.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "sim/route_playback.hpp"

namespace rr::app {

/// The first race route in `folder`, which by name is a fast driver's.
[[nodiscard]] Result<assets::RaceRoute> LoadPaceRoute(const assets::Vfs& vfs,
                                                      std::string_view folder);

/// Poses are in engine space, for a car model that faces game +Y as the racer models do.
class PaceCar {
public:
    explicit PaceCar(assets::RaceRoute route);

    void Tick();

    [[nodiscard]] Transform Car() const;
    [[nodiscard]] Transform GamePose() const { return playback_.Pose(); }
    [[nodiscard]] Transform Camera() const;

private:
    sim::RoutePlayback playback_;
    ChaseCamera camera_;
};

}
