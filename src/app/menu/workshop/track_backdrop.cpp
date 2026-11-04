#include "app/menu/workshop/track_backdrop.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include "app/menu/race_setup_menu.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"
#include "assets/race_catalog.hpp"
#include "assets/start_grid.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kStartGrid = "STARTPOS.SPB";
constexpr std::string_view kSky = "BACKGRND.SKB";
constexpr std::size_t kLoaderWorkers = 2;

template <typename Parse>
auto ReadTrackFile(const assets::Vfs& vfs, std::string_view track, std::string_view file,
                   Parse parse) {
    return vfs.Read(std::format("GAMEDATA/{}/{}", track, file))
        .and_then([&](const assets::FileData& data) { return parse(data.Bytes()); });
}

Result<assets::SkyFile> ReadSky(const assets::Vfs& vfs, std::string_view track) {
    return vfs.List(std::format("GAMEDATA/{}", track))
        .and_then([&](const std::vector<assets::VfsEntry>& folder) -> Result<assets::SkyFile> {
            if (!assets::HasFile(folder, kSky)) {
                return assets::SkyFile{};
            }
            return ReadTrackFile(vfs, track, kSky, assets::ParseSkyFile);
        });
}

Result<Vector3> FirstSpot(const assets::Vfs& vfs, std::string_view track) {
    return ReadTrackFile(vfs, track, kStartGrid, assets::ParseSpb)
        .and_then([&](const assets::StartGridAsset& grid) -> Result<Vector3> {
            if (grid.slots.empty()) {
                return Fail(ErrorCode::InvalidData, std::format("{} has no grid spots", track));
            }
            return grid.slots.front().position;
        });
}

}

std::vector<BackdropTrack> ListBackdropTracks(const assets::Vfs& vfs, std::string_view language) {
    const Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Load(vfs);
    if (!catalog) {
        log::Warn("the workshop offers no tracks: {}", catalog.error().message);
        return {};
    }
    const std::vector<std::u16string> names =
        ReadRaceNames(vfs, language).value_or(std::vector<std::u16string>{});
    std::vector<BackdropTrack> tracks;
    for (const assets::RaceEntry& race : catalog->Races()) {
        std::string folder = assets::ToUpperAscii(race.folder);
        const bool isListed = std::ranges::any_of(
            tracks, [&](const BackdropTrack& track) { return track.folder == folder; });
        if (race.isMirrored || isListed) {
            continue;
        }
        const auto line = static_cast<std::size_t>(race.nameLine.value_or(-1));
        std::u16string name = race.nameLine && line < names.size()
                                  ? names[line]
                                  : std::u16string(folder.begin(), folder.end());
        std::ranges::replace(name, u'\n', u' ');
        tracks.push_back({.folder = std::move(folder), .name = std::move(name)});
    }
    return tracks;
}

Result<BackdropFiles> ReadBackdropFiles(const assets::Vfs& vfs, std::string_view track) {
    assets::LoaderPool pool(kLoaderWorkers);
    assets::TrackLoader loader(vfs, pool);
    Result<Vector3> first = FirstSpot(vfs, track);
    Result<assets::TrackAssets> assets = first.and_then(
        [&](const Vector3&) { return loader.Load(std::format("GAMEDATA/{}", track)); });
    Result<assets::SkyFile> sky =
        assets.and_then([&](const assets::TrackAssets&) { return ReadSky(vfs, track); });
    if (!sky) {
        return std::unexpected(std::move(sky.error()));
    }
    return BackdropFiles{.assets = std::move(*assets), .sky = std::move(*sky), .firstSpot = *first};
}

Vector3 BackdropShift(const Vector3& firstSpot, const Vector3& spot) {
    return ToEngine(spot) - ToEngine(firstSpot);
}

Result<std::unique_ptr<TrackBackdrop>> TrackBackdrop::Upload(render::Renderer& renderer,
                                                             BackdropFiles files,
                                                             const Vector3& spot) {
    Result<TrackModel> model = UploadTrack(renderer, files.assets);
    Result<SkyBackdrop> sky = model.and_then([&](TrackModel& uploaded) {
        return SkyBackdrop::Upload(renderer, std::move(uploaded.skyModels));
    });
    if (!sky) {
        return std::unexpected(std::move(sky.error()));
    }
    const Vector3 shift = BackdropShift(files.firstSpot, spot);
    for (render::MeshDraw& draw : model->draws) {
        draw.transform.position = draw.transform.position + shift;
    }
    const float farPlane = std::max(TrackCamera(model->bounds).farPlane, 1.0F);
    return std::unique_ptr<TrackBackdrop>(new TrackBackdrop(
        std::move(model->draws), std::make_unique<assets::SkyFile>(std::move(files.sky)),
        std::move(*sky), farPlane));
}

render::RenderView TrackBackdrop::View(const render::Camera& camera,
                                       const render::Viewport& viewport) {
    sky_.Place(raceSky_, camera.transform.position, 0.0F, skyDraws_);
    return {.camera = camera, .backdrop = skyDraws_, .meshes = draws_, .viewport = viewport};
}

}
