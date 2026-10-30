#include "app/pace_car.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/track_space.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

bool IsRoute(const assets::VfsEntry& entry) {
    return entry.kind == assets::EntryKind::File &&
           assets::ToUpperAscii(entry.name).ends_with(".RRB");
}

}

Result<assets::RaceRoute> LoadPaceRoute(const assets::Vfs& vfs, std::string_view folder) {
    Result<std::vector<assets::VfsEntry>> entries = vfs.List(folder);
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    const auto route = std::ranges::find_if(*entries, IsRoute);
    if (route == entries->end()) {
        return Fail(ErrorCode::NotFound, std::format("{} has no race route", folder));
    }
    return vfs.Read(std::format("{}/{}", folder, route->name))
        .and_then(
            [](const assets::FileData& file) { return assets::ParseRaceRoute(file.Bytes()); });
}

PaceCar::PaceCar(assets::RaceRoute route) : playback_(std::move(route)), camera_(Car()) {}

void PaceCar::Tick() {
    playback_.Advance(kSimTick);
    camera_.Follow(Car());
}

Transform PaceCar::Car() const {
    return CarToEngine(playback_.Pose());
}

Transform PaceCar::Camera() const {
    return camera_.Pose(Car());
}

}
