#include "assets/racer_catalog.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/merged_table.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kTablesFolder = "GAMEDATA/COMMON";

template <typename Entry>
Result<std::vector<Entry>> ReadTable(
    const Vfs& vfs, std::string_view file,
    Result<std::vector<Entry>> (*parse)(std::span<const std::byte>)) {
    return ReadMergedTable(vfs, std::format("{}/{}", kTablesFolder, file), parse);
}

template <typename Entry>
Result<Entry> FindNamed(const std::vector<Entry>& entries, std::string_view name,
                        std::string_view table) {
    const std::string wanted = ToUpperAscii(name);
    const auto entry = std::ranges::find_if(
        entries, [&](const Entry& candidate) { return ToUpperAscii(candidate.name) == wanted; });
    if (entry == entries.end()) {
        return Fail(ErrorCode::NotFound, std::format("{} has no entry named {}", table, name));
    }
    return *entry;
}

}

Result<RacerCatalog> RacerCatalog::Load(const Vfs& vfs) {
    RacerCatalog catalog;
    return ReadTable(vfs, "DRIVERS.DDB", ParseDrivers)
        .and_then([&](std::vector<DriverEntry> drivers) {
            catalog.drivers_ = std::move(drivers);
            return ReadTable(vfs, "CHAMPS.CCB", ParseChampions);
        })
        .and_then([&](std::vector<ChampionEntry> champions) {
            catalog.champions_ = std::move(champions);
            return ReadTable(vfs, "CHASSIS.CMB", ParseChassis);
        })
        .transform([&](std::vector<ChassisEntry> chassis) {
            catalog.chassis_ = std::move(chassis);
            return std::move(catalog);
        });
}

Result<RacerFiles> RacerCatalog::Find(std::string_view code) const {
    RacerFiles files;
    return FindNamed(drivers_, code, "DRIVERS.DDB")
        .and_then([&](DriverEntry driver) {
            files.driver = std::move(driver);
            return FindNamed(champions_, files.driver.champion, "CHAMPS.CCB");
        })
        .and_then([&](ChampionEntry champion) {
            files.champion = std::move(champion);
            return FindChassis(files.champion.chassis);
        })
        .transform([&](ChassisEntry chassis) {
            files.chassis = std::move(chassis);
            return std::move(files);
        });
}

Result<ChassisEntry> RacerCatalog::FindChassis(std::string_view name) const {
    return FindNamed(chassis_, name, "CHASSIS.CMB");
}

}
