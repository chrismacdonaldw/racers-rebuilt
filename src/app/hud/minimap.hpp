#pragma once

#include <optional>
#include <span>
#include <vector>

#include "app/ui_text/ui_image.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Bounds are in game space, as the race file gives them. A mirrored track keeps them unmirrored.
struct TrackMap {
    float minX = 0.0F;
    float maxY = 0.0F;
    float maxX = 0.0F;
    float minY = 0.0F;
    std::optional<render::TextureId> image;
    bool isMirrored = false;
};

struct MapMarker {
    Vector3 position;
    bool isChampion = false;
};

/// In game space. `viewDirection` is the race camera's.
struct MapView {
    Vector3 player;
    Vector3 playerForward;
    Vector3 viewDirection;
    std::span<const MapMarker> markers;
};

void DrawFixedMap(const TrackMap& map, const MapView& view, const UiImage& markers,
                  std::vector<render::SpriteDraw>& out);

void DrawRotatingMap(const TrackMap& map, const MapView& view, const UiImage& markers,
                     std::vector<render::SpriteDraw>& out);

}
