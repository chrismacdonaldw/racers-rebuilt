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

constexpr float kChaseDistance = 7.0F;
constexpr float kChaseHeight = 3.0F;
constexpr float kLookHeight = 1.0F;
constexpr float kChaseBlend = 0.1F;

Vector3 Raised(const Vector3& point, float height) {
    return {.x = point.x, .y = point.y + height, .z = point.z};
}

Vector3 ChaseSpot(const Transform& car) {
    return Raised(Apply(car, ToEngine({.y = -kChaseDistance})), kChaseHeight);
}

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

PaceCar::PaceCar(assets::RaceRoute route)
    : playback_(std::move(route)), cameraPosition_(ChaseSpot(Car())) {}

void PaceCar::Tick() {
    playback_.Advance(kSimTick);
    cameraPosition_ = Lerp(cameraPosition_, ChaseSpot(Car()), kChaseBlend);
}

Transform PaceCar::Car() const {
    const Transform game = playback_.Pose();
    const Vector3 right = Apply({.rotation = game.rotation}, {.y = -1.0F});
    const Vector3 up = Apply({.rotation = game.rotation}, {.z = 1.0F});
    return PlacementToEngine(game.position, right, up);
}

Transform PaceCar::Camera() const {
    return LookAt(cameraPosition_, Raised(Car().position, kLookHeight));
}

}
