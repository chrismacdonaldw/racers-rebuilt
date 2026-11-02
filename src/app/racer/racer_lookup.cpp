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

constexpr std::array<std::string_view, 2> kRacerFiles{"MENUDATA/DEFAULT.LRS",
                                                      "MENUDATA/QBUILD.LRS"};

bool IsNamed(std::string_view name, std::string_view wanted) {
    return assets::ToUpperAscii(name) == assets::ToUpperAscii(wanted);
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
        Result<std::vector<assets::SavedRacer>> racers = vfs.Read(file).and_then(
            [](const assets::FileData& data) { return assets::ParseRacerFile(data.Bytes()); });
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

}
