#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/sky_backdrop.hpp"
#include "assets/sky_file.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/track/race_sky.hpp"

namespace rr::app {

struct BackdropTrack {
    std::string folder;
    std::u16string name;
};

/// Every unmirrored race track once, in race order, named in `language`.
[[nodiscard]] std::vector<BackdropTrack> ListBackdropTracks(const assets::Vfs& vfs,
                                                            std::string_view language);

/// `firstSpot` is the track's first grid spot, in game space.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct BackdropFiles {
    assets::TrackAssets assets;
    assets::SkyFile sky;
    Vector3 firstSpot;
};

/// Safe on any thread, as it reads through a loader of its own.
[[nodiscard]] Result<BackdropFiles> ReadBackdropFiles(const assets::Vfs& vfs,
                                                      std::string_view track);

/// In engine space. Both spots are in game space.
[[nodiscard]] Vector3 BackdropShift(const Vector3& firstSpot, const Vector3& spot);

/// A race track's scenery and sky, moved so its first grid spot sits under the workshop's car.
class TrackBackdrop {
public:
    /// Main thread only. `spot` is where the first grid spot goes, in game space.
    [[nodiscard]] static Result<std::unique_ptr<TrackBackdrop>> Upload(render::Renderer& renderer,
                                                                       BackdropFiles files,
                                                                       const Vector3& spot);

    [[nodiscard]] float FarPlane() const { return farPlane_; }

    /// The view's spans point into the backdrop until the next call.
    [[nodiscard]] render::RenderView View(const render::Camera& camera,
                                          const render::Viewport& viewport);

private:
    TrackBackdrop(std::vector<render::MeshDraw> draws, std::unique_ptr<assets::SkyFile> skyFile,
                  SkyBackdrop sky, float farPlane)
        : draws_(std::move(draws)),
          skyFile_(std::move(skyFile)),
          raceSky_(*skyFile_),
          sky_(std::move(sky)),
          farPlane_(farPlane) {}

    std::vector<render::MeshDraw> draws_;
    std::unique_ptr<assets::SkyFile> skyFile_;
    sim::RaceSky raceSky_;
    SkyBackdrop sky_;
    std::vector<render::MeshDraw> skyDraws_;
    float farPlane_;
};

}
