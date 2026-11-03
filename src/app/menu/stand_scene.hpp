#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "assets/menu_layout.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// `driver` is an entity added for the racer, posed by `RSANIM`'s `idles` and `cheers` parts.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct StandScene {
    assets::TrackAssets assets;
    sim::TrackEntitySources sources;
    render::Camera camera;
    std::size_t turntable = 0;
    std::size_t driver = 0;
    std::vector<std::size_t> idles;
    std::vector<std::size_t> cheers;
};

inline constexpr render::Color kMenuSceneAmbient{
    .r = 120.0F / 255.0F, .g = 120.0F / 255.0F, .b = 120.0F / 255.0F};

[[nodiscard]] render::DirectionalLight MenuSceneLight();

/// A scene element's world camera, which wins over the camera its MIB entry holds.
[[nodiscard]] render::Camera MenuSceneCamera(const assets::WorldCamera& camera);

[[nodiscard]] Result<const assets::MenuElement*> FindShowcase(const MenuFiles& files,
                                                              std::string_view screen);

/// The `RS_SET` worlds that the screen's `showcase` element names.
[[nodiscard]] Result<StandScene> LoadStandScene(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                                const MenuFiles& files, std::string_view screen);

/// In game space: on the car's spot, turned as the turntable's first node is.
[[nodiscard]] Transform StandCarPlacement(const sim::TrackEntitySources& sources,
                                          std::size_t turntable, const sim::EntityClock& clock);

}
