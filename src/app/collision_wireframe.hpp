#pragma once

#include <vector>

#include "render/render_view.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/collision/track_collision.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kCollisionWireframe{.key = "dev.collision_wireframe",
                                                                 .defaultValue = false,
                                                                 .label = "Collision wireframe",
                                                                 .category = "Developer"};

void DeclareDeveloperSettings(settings::Registry& registry);

/// In engine space. An edge shared by two triangles of one world is drawn once.
[[nodiscard]] std::vector<render::DebugLine> BuildCollisionWireframe(
    const sim::TrackCollision& collision);

}
