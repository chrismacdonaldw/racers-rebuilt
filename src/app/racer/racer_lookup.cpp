#include "app/racer/racer_lookup.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kQuickBuildFile = "MENUDATA/QBUILD.LRS";
constexpr std::array<std::string_view, 2> kRacerFiles{"MENUDATA/DEFAULT.LRS", kQuickBuildFile};
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

Result<settings::Racer> FindGameRacer(const assets::Vfs& vfs, std::string_view name) {
    for (const std::string_view file : kRacerFiles) {
        Result<std::vector<assets::SavedRacer>> racers = ReadRacerFile(vfs, file);
        if (!racers) {
            return std::unexpected(std::move(racers.error()));
        }
        const auto found = std::ranges::find_if(*racers, [&](const assets::SavedRacer& racer) {
            return IsNamed(Utf8FromUtf16(racer.name), name);
        });
        if (found != racers->end()) {
            return RacerFromSave(*found);
        }
    }
    return Fail(ErrorCode::NotFound, std::format("no racer named {}", name));
}

Result<settings::Racer> FindRacer(const assets::Vfs& vfs, const std::filesystem::path& racersFile,
                                  std::string_view name) {
    Result<settings::RacerStore> store = settings::LoadRacers(racersFile);
    if (!store) {
        return std::unexpected(std::move(store.error()));
    }
    const auto own = std::ranges::find_if(
        store->racers, [&](const settings::Racer& racer) { return IsNamed(racer.name, name); });
    if (own != store->racers.end()) {
        return std::move(*own);
    }
    return FindGameRacer(vfs, name);
}

Result<std::optional<settings::Racer>> SelectedRacer(const std::filesystem::path& racersFile) {
    return settings::LoadRacers(racersFile).transform([](settings::RacerStore store) {
        return store.selected && *store.selected < store.racers.size()
                   ? std::optional(std::move(store.racers[*store.selected]))
                   : std::nullopt;
    });
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
