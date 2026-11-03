#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "assets/menu_layout.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// A menu scene element's set, the minifig's spot in it and the animation parts it plays.
struct StandSet {
    std::string_view folder;
    std::string_view element;
    std::string_view animations;
    std::span<const std::string_view> idles;
    std::span<const std::string_view> cheers;
    std::span<const std::string_view> reactions;
    std::string_view turntable;
    Vector3 driverSpot;
    Vector3 driverFacing;
    Vector3 driverUp{.z = 1.0F};
    float ambient = 0.0F;
    float light = 1.0F;
};

extern const StandSet kRacerStandSet;

extern const StandSet kBuilderStandSet;

extern const StandSet kLicenceStandSet;

/// `driver` is an entity added for the racer. The part lists follow the set's names.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct StandScene {
    assets::TrackAssets assets;
    sim::TrackEntitySources sources;
    render::Camera camera;
    std::optional<std::size_t> turntable;
    std::size_t driver = 0;
    std::vector<std::size_t> idles;
    std::vector<std::size_t> cheers;
    std::vector<std::size_t> reactions;
};

struct StandFraming {
    MenuFrameLook look;
    render::Rect frame;
    render::Rect interior;
};

inline constexpr render::Color kMenuSceneAmbient{
    .r = 120.0F / 255.0F, .g = 120.0F / 255.0F, .b = 120.0F / 255.0F};

[[nodiscard]] render::DirectionalLight MenuSceneLight();

/// A scene element's world camera, which wins over the camera its MIB entry holds.
[[nodiscard]] render::Camera MenuSceneCamera(const assets::WorldCamera& camera);

[[nodiscard]] Result<const assets::MenuElement*> FindStandElement(const MenuFiles& files,
                                                                  std::string_view screen,
                                                                  const StandSet& set);

/// The frame the scene element links to, round the element's rectangle.
[[nodiscard]] Result<StandFraming> LoadStandFraming(render::Renderer& renderer,
                                                    const assets::Vfs& vfs, const MenuFiles& files,
                                                    std::string_view screen, const StandSet& set);

/// The worlds that the screen's scene element names, from the set's folder.
[[nodiscard]] Result<StandScene> LoadStandScene(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                                const MenuFiles& files, std::string_view screen,
                                                const StandSet& set);

/// In game space: on the car's spot, turned as the turntable's first node is.
[[nodiscard]] Transform StandCarPlacement(const sim::TrackEntitySources& sources,
                                          std::size_t turntable, const sim::EntityClock& clock);

}
