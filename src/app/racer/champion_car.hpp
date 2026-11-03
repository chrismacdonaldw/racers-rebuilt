#pragma once

#include <cstdint>
#include <optional>

#include "assets/champion_database.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"

namespace rr::app {

enum class CarBody : std::uint8_t { Bricks, Champion };

/// The champion whose unchanged `CHAMP` quick build `racer` drives, else none.
[[nodiscard]] Result<std::optional<assets::ChampionEntry>> QuickBuiltChampion(
    const assets::Vfs& vfs, const assets::RacerCatalog& catalog, const settings::Racer& racer);

/// None for `CarBody::Bricks`, else `QuickBuiltChampion`.
[[nodiscard]] Result<std::optional<assets::ChampionEntry>> ChampionBodyOf(
    const assets::Vfs& vfs, const assets::RacerCatalog& catalog, const settings::Racer& racer,
    CarBody body);

}
