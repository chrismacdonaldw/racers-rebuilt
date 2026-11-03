#include "app/racer/champion_car.hpp"

#include <utility>

#include "app/racer/racer_lookup.hpp"

namespace rr::app {
namespace {

template <typename T>
Result<std::optional<T>> NoneIfMissing(Result<T> found) {
    if (!found && found.error().code == ErrorCode::NotFound) {
        return std::nullopt;
    }
    return std::move(found).transform([](T value) { return std::optional(std::move(value)); });
}

}

Result<std::optional<assets::ChampionEntry>> QuickBuiltChampion(const assets::Vfs& vfs,
                                                                const assets::RacerCatalog& catalog,
                                                                const settings::Racer& racer) {
    return NoneIfMissing(FindChampionCar(vfs, racer.chassis))
        .and_then([&](const std::optional<settings::Racer>& build)
                      -> Result<std::optional<assets::ChampionEntry>> {
            if (!build || build->car != racer.car) {
                return std::nullopt;
            }
            return NoneIfMissing(catalog.FindChampionOn(racer.chassis));
        });
}

Result<std::optional<assets::ChampionEntry>> ChampionBodyOf(const assets::Vfs& vfs,
                                                            const assets::RacerCatalog& catalog,
                                                            const settings::Racer& racer,
                                                            CarBody body) {
    if (body == CarBody::Bricks) {
        return std::nullopt;
    }
    return QuickBuiltChampion(vfs, catalog, racer);
}

}
