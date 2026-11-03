#include "app/hud/race_map_view.hpp"

#include "app/race_step.hpp"
#include "app/time_trial/ghost_cars.hpp"
#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kChampionSlot = 1;

}

MapView PlayerMapView(const sim::RenderSnapshot& snapshot, std::size_t own, std::size_t players,
                      const Vector3& viewDirection, std::vector<MapMarker>& markers) {
    markers.clear();
    for (std::size_t slot = snapshot.racers.size(); slot-- > 0;) {
        if (slot != own) {
            markers.push_back(
                {.position = ToGame(snapshot.entities[slot].transform.position),
                 .isChampion = slot == kChampionSlot && slot >= kPlayerSlot + players});
        }
    }
    AppendGhostMarkers(snapshot.ghosts, markers);
    const Transform racer = TransformToGame(snapshot.entities[own].transform);
    return {.player = racer.position,
            .playerForward = Rotate(racer.rotation, {.x = 1.0F}),
            .viewDirection = viewDirection,
            .markers = markers};
}

}
