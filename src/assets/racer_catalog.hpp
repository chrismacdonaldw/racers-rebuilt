#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/champion_database.hpp"
#include "assets/chassis_database.hpp"
#include "assets/driver_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct RacerFiles {
    DriverEntry driver;
    ChampionEntry champion;
    ChassisEntry chassis;
};

/// Prebuilt drivers and their cars from `GAMEDATA/COMMON`, merged with the tables mods add.
class RacerCatalog {
public:
    /// Applies every mod's driver table, and logs the changes it skips.
    [[nodiscard]] static Result<RacerCatalog> Load(const Vfs& vfs);
    /// Collects the skipped driver table changes instead of logging them.
    [[nodiscard]] static Result<RacerCatalog> Load(const Vfs& vfs,
                                                   std::vector<std::string>& problems);

    [[nodiscard]] std::span<const DriverEntry> Drivers() const { return drivers_; }

    /// Matches `code` in any case.
    [[nodiscard]] Result<DriverEntry> FindDriver(std::string_view code) const;

    /// Matches `code` in any case. Fails when the driver, its champion or its chassis is missing.
    [[nodiscard]] Result<RacerFiles> Find(std::string_view code) const;

    [[nodiscard]] Result<ChassisEntry> FindChassis(std::string_view name) const;

    /// The first champion whose car sits on `chassis`, matched in any case.
    [[nodiscard]] Result<ChampionEntry> FindChampionOn(std::string_view chassis) const;

private:
    std::vector<DriverEntry> drivers_;
    std::vector<ChampionEntry> champions_;
    std::vector<ChassisEntry> chassis_;
};

}
