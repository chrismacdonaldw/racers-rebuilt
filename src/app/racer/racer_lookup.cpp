#include "app/racer/racer_lookup.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/merged_table.hpp"
#include "core/log.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kQuickBuildFile = "MENUDATA/QBUILD.LRS";
constexpr std::string_view kDefaultFile = "MENUDATA/DEFAULT.LRS";
constexpr std::string_view kModRacersFile = "MENUDATA/RACERS.TOML";
constexpr std::string_view kChampionCarName = "CHAMP";

bool IsNamed(std::string_view name, std::string_view wanted) {
    return assets::ToUpperAscii(name) == assets::ToUpperAscii(wanted);
}

Result<std::vector<assets::SavedRacer>> ReadRacerFile(const assets::Vfs& vfs,
                                                      std::string_view file) {
    return vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseRacerFile(data.Bytes()); });
}

std::vector<settings::Racer> RacersFromSaves(const std::vector<assets::SavedRacer>& saved) {
    std::vector<settings::Racer> racers;
    racers.reserve(saved.size());
    for (const assets::SavedRacer& racer : saved) {
        racers.push_back(RacerFromSave(racer));
    }
    return racers;
}

Result<std::vector<settings::Racer>> ReadGameRacers(const assets::Vfs& vfs, std::string_view file) {
    return ReadRacerFile(vfs, file).transform(RacersFromSaves);
}

std::optional<std::vector<settings::Racer>> ParseModRacers(const assets::FileData& file) {
    const std::span<const std::byte> bytes = file.Bytes();
    Result<settings::RacerStore> store = settings::ParseRacers(
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
        kModRacersFile);
    if (!store) {
        log::Warn("{}, skipping a mod's racers", store.error().message);
        return std::nullopt;
    }
    for (settings::Racer& racer : store->racers) {
        racer.trophies = {};
    }
    return std::move(store->racers);
}

void AppendModRacers(const assets::Vfs& vfs, std::vector<std::vector<settings::Racer>>& tables) {
    Result<std::vector<assets::FileData>> files = vfs.ReadEveryLayer(kModRacersFile);
    if (!files) {
        log::Warn("{}, the racer list has no mod racers", files.error().message);
        return;
    }
    for (const assets::FileData& file : *files) {
        if (std::optional<std::vector<settings::Racer>> racers = ParseModRacers(file)) {
            tables.push_back(std::move(*racers));
        }
    }
}

}

settings::Racer RacerFromSave(const assets::SavedRacer& saved) {
    return {.name = Utf8FromUtf16(saved.name),
            .look = saved.look,
            .chassis = saved.chassis,
            .car = saved.car,
            .trophies = {}};
}

Result<std::optional<settings::Racer>> FindDriverRacer(const assets::Vfs& vfs,
                                                       const assets::DriverEntry& driver) {
    if (driver.racer.empty()) {
        return std::optional<settings::Racer>();
    }
    return FindGameRacer(vfs, {.name = driver.racer}).transform([](settings::Racer racer) {
        return std::optional(std::move(racer));
    });
}

Result<settings::Racer> FindGameRacer(const assets::Vfs& vfs, const RacerRef& ref) {
    Result<std::vector<settings::Racer>> defaults = ReadDefaultRacers(vfs);
    Result<std::vector<settings::Racer>> quickBuilds =
        defaults.and_then([&](const auto&) { return ReadGameRacers(vfs, kQuickBuildFile); });
    if (!quickBuilds) {
        return std::unexpected(std::move(quickBuilds.error()));
    }
    for (std::vector<settings::Racer>* racers : {&*defaults, &*quickBuilds}) {
        const auto found = std::ranges::find_if(
            *racers, [&](const settings::Racer& racer) { return IsRef(racer, ref); });
        if (found != racers->end()) {
            return std::move(*found);
        }
    }
    return Fail(ErrorCode::NotFound, std::format("no racer named {}", ref.name));
}

Result<settings::Racer> FindRacer(const assets::Vfs& vfs, const std::filesystem::path& racersFile,
                                  const RacerRef& ref) {
    Result<settings::RacerStore> store = settings::LoadRacers(racersFile);
    if (!store) {
        log::Warn("{}, looking only in the game's racers", store.error().message);
        return FindGameRacer(vfs, ref);
    }
    const auto own = std::ranges::find_if(
        store->racers, [&](const settings::Racer& racer) { return IsRef(racer, ref); });
    if (own != store->racers.end()) {
        return std::move(*own);
    }
    return FindGameRacer(vfs, ref);
}

Result<void> ResolvePlayers(const assets::Vfs& vfs, const std::filesystem::path& racersFile,
                            RaceSceneSetup& setup) {
    for (const auto& [ref, racer] : {std::pair{&setup.player, &setup.playerRacer},
                                     std::pair{&setup.secondPlayer, &setup.secondPlayerRacer}}) {
        if (ref->name.empty()) {
            continue;
        }
        Result<settings::Racer> found = FindRacer(vfs, racersFile, *ref);
        if (!found) {
            return std::unexpected(std::move(found.error()));
        }
        *racer = std::move(*found);
    }
    return {};
}

Result<std::vector<settings::Racer>> ReadDefaultRacers(const assets::Vfs& vfs) {
    return ReadGameRacers(vfs, kDefaultFile).transform([&](std::vector<settings::Racer> racers) {
        std::vector<std::vector<settings::Racer>> tables;
        tables.push_back(std::move(racers));
        AppendModRacers(vfs, tables);
        return assets::MergeNamed(std::move(tables));
    });
}

Result<std::optional<settings::Racer>> SelectedRacer(const std::filesystem::path& racersFile) {
    return settings::LoadRacers(racersFile).transform([](settings::RacerStore store) {
        return store.selected && *store.selected < store.racers.size()
                   ? std::optional(std::move(store.racers[*store.selected]))
                   : std::nullopt;
    });
}

std::optional<settings::Racer> SelectedRacerOrNone(const std::filesystem::path& racersFile) {
    Result<std::optional<settings::Racer>> selected = SelectedRacer(racersFile);
    if (!selected) {
        log::Warn("{}, the award screens show the stand-in racer", selected.error().message);
        return std::nullopt;
    }
    return std::move(*selected);
}

std::optional<settings::Racer> FindPlayerRacer(const assets::Vfs& vfs,
                                               const std::filesystem::path& racersFile,
                                               const RacerRef& ref) {
    if (ref.name.empty()) {
        return std::nullopt;
    }
    Result<settings::Racer> racer = FindRacer(vfs, racersFile, ref);
    return racer ? std::optional(std::move(*racer)) : std::nullopt;
}

Result<settings::Racer> FindChampionCar(const assets::Vfs& vfs, std::string_view chassis) {
    return ReadRacerFile(vfs, kQuickBuildFile)
        .and_then([&](const std::vector<assets::SavedRacer>& racers) -> Result<settings::Racer> {
            const auto found = std::ranges::find_if(racers, [&](const assets::SavedRacer& racer) {
                return IsNamed(Utf8FromUtf16(racer.name), kChampionCarName) &&
                       IsNamed(racer.chassis, chassis);
            });
            if (found == racers.end()) {
                return Fail(ErrorCode::NotFound,
                            std::format("no {} car on the {} chassis", kChampionCarName, chassis));
            }
            return RacerFromSave(*found);
        });
}

}
