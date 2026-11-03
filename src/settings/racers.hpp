#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/car_data.hpp"
#include "assets/driver_look.hpp"
#include "core/result.hpp"
#include "settings/progress.hpp"

namespace rr::settings {

// NOLINTNEXTLINE(bugprone-exception-escape)
struct Racer {
    std::string name;
    assets::DriverLook look;
    std::string chassis;
    assets::CarData car{};
    Trophies trophies;

    friend bool operator==(const Racer&, const Racer&) = default;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
struct RacerStore {
    std::vector<Racer> racers;
    std::optional<std::size_t> selected;

    friend bool operator==(const RacerStore&, const RacerStore&) = default;
};

/// The selected racer's, or `progress`'s own while no racer is selected.
[[nodiscard]] Trophies& TrophiesOf(Progress& progress, RacerStore& store);

/// Follows `LoadProgress`, but fails on a racer that doesn't parse so a save can't drop it.
[[nodiscard]] Result<RacerStore> LoadRacers(const std::filesystem::path& path);

[[nodiscard]] Result<RacerStore> ParseRacers(std::string_view text, std::string_view sourceName);

/// Follows `SaveProgress`. Unknown keys inside a racer are lost.
[[nodiscard]] Result<void> SaveRacers(const RacerStore& store, const std::filesystem::path& path);

}
