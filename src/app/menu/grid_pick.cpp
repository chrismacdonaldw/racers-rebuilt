#include "app/menu/grid_pick.hpp"

#include <algorithm>
#include <cmath>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"

#include <glm/trigonometric.hpp>

namespace rr::app {
namespace {

struct Ray {
    Vector3 origin;
    Vector3 direction;
};

std::optional<Ray> RayInCar(const GridView& at, float x, float y) {
    if (at.view.height <= 0.0F) {
        return std::nullopt;
    }
    const float halfHeight = at.view.height / 2.0F;
    const float scale = halfHeight / std::tan(glm::radians(at.camera.verticalFovDegrees) / 2.0F);
    const Vector3 inView{.x = (x - at.view.x - (at.view.width / 2.0F)) / scale,
                         .y = (at.view.y + halfHeight - y) / scale,
                         .z = -1.0F};
    const Vector3 engine = Rotate(at.camera.transform.rotation, inView);
    const Transform toCar = Inverse(at.car);
    return Ray{.origin = Apply(toCar, ToGame(at.camera.transform.position)),
               .direction = Rotate(toCar.rotation, ToGame(engine))};
}

}

std::optional<GridCellPick> PickGridCell(const GridView& at, const sim::CarGrid& grid, float x,
                                         float y) {
    const std::optional<Ray> ray = RayInCar(at, x, y);
    if (!ray || ray->direction.z >= 0.0F) {
        return std::nullopt;
    }
    std::optional<GridCellPick> nearest;
    float nearestDistance = 0.0F;
    for (int cellX = 0; cellX < sim::kCarGridWidth; ++cellX) {
        for (int cellY = 0; cellY < sim::kCarGridDepth; ++cellY) {
            const float top =
                at.corner.z + (static_cast<float>(grid.At(cellX, cellY).height) * kPlateHeight);
            const float distance = (top - ray->origin.z) / ray->direction.z;
            const Vector3 hit = ray->origin + (ray->direction * distance);
            const bool isInside = std::floor(hit.x - at.corner.x) == static_cast<float>(cellX) &&
                                  std::floor(hit.y - at.corner.y) == static_cast<float>(cellY);
            if (distance > 0.0F && isInside && (!nearest || distance < nearestDistance)) {
                nearest = GridCellPick{.x = cellX, .y = cellY};
                nearestDistance = distance;
            }
        }
    }
    return nearest;
}

std::optional<std::size_t> PickPiece(const GridView& at, const sim::BuiltCar& car, float x,
                                     float y) {
    const std::optional<GridCellPick> cell = PickGridCell(at, car.Grid(), x, y);
    if (!cell) {
        return std::nullopt;
    }
    const int height = car.Grid().At(cell->x, cell->y).height;
    return height > 0 ? car.PieceAt(cell->x, cell->y, height - 1) : std::nullopt;
}

sim::PiecePlacement CentredOn(GridCellPick cell, assets::CellSize size, unsigned rotation) {
    return {.x = std::clamp(cell.x - ((size.width - 1) / 2), 0,
                            std::max(sim::kCarGridWidth - size.width, 0)),
            .y = std::clamp(cell.y - ((size.height - 1) / 2), 0,
                            std::max(sim::kCarGridDepth - size.height, 0)),
            .rotation = rotation};
}

}
