#include "app/racer/racer_lookup.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "core/log.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kQuickBuildFile = "MENUDATA/QBUILD.LRS";
constexpr std::string_view kDefaultFile = "MENUDATA/DEFAULT.LRS";
constexpr std::array<std::string_view, 2> kRacerFiles{kDefaultFile, kQuickBuildFile};
constexpr std::string_view kChampionCarName = "CHAMP";

bool IsNamed(std::string_view name, std::string_view wanted) {
    return assets::ToUpperAscii(name) == assets::ToUpperAscii(wanted);
}

Result<std::vector<assets::SavedRacer>> ReadRacerFile(const assets::Vfs& vfs,
                                                      std::string_view file) {
    return vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseRacerFile(data.Bytes()); });
}

}

settings::Racer RacerFromSave(const assets::SavedRacer& saved) {
    return {.name = Utf8FromUtf16(saved.name),
            .look = saved.look,
            .chassis = saved.chassis,
            .car = saved.car,
            .trophies = {}};
}

Result<settings::Racer> FindGameRacer(const assets::Vfs& vfs, const RacerRef& ref) {
    for (const std::string_view file : kRacerFiles) {
        Result<std::vector<assets::SavedRacer>> racers = ReadRacerFile(vfs, file);
        if (!racers) {
            return std::unexpected(std::move(racers.error()));
        }
        for (const assets::SavedRacer& saved : *racers) {
            settings::Racer racer = RacerFromSave(saved);
            if (IsRef(racer, ref)) {
                return racer;
            }
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
    return ReadRacerFile(vfs, kDefaultFile)
        .transform([](const std::vector<assets::SavedRacer>& saved) {
            std::vector<settings::Racer> racers;
            racers.reserve(saved.size());
            for (const assets::SavedRacer& racer : saved) {
                racers.push_back(RacerFromSave(racer));
            }
            return racers;
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
