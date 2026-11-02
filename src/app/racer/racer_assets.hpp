#pragma once

#include <optional>
#include <string_view>

#include "assets/animation_database.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::app {

struct JointedAssets {
    assets::SkeletonAsset skeleton;
    assets::AnimationAsset animations;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
struct RacerAssets {
    assets::RacerFiles files;
    assets::TrackAssets models;
    JointedAssets wheels;
    JointedAssets driver;
    std::optional<sim::CarBuild> build;
};

/// Reads `<skeleton>.SDB` and `<animations>.ADB` from `GAMEDATA/COMMON`.
[[nodiscard]] Result<JointedAssets> LoadJointed(const assets::Vfs& vfs, std::string_view skeleton,
                                                std::string_view animations);

/// Loads the skeletons for the wheels `racer.files` names and for the driver.
[[nodiscard]] Result<RacerAssets> LoadRacerJoints(const assets::Vfs& vfs, RacerAssets racer);

/// `code` names a driver in `DRIVERS.DDB`, such as `RR`.
[[nodiscard]] Result<RacerAssets> LoadRacer(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                            const assets::RacerCatalog& catalog,
                                            std::string_view code);

}
