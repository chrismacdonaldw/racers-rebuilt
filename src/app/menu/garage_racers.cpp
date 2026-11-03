#include "app/menu/garage_racers.hpp"

#include <format>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/part_sets.hpp"
#include "assets/piece_catalog.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::string_view kPartSetList = "CRSTMGR.LEB";

Result<assets::PartSet> FirstPartSet(const assets::Vfs& vfs) {
    return vfs.Read(assets::JoinGamePath(kPieceFolder, kPartSetList))
        .and_then(
            [](const assets::FileData& data) { return assets::ParsePartSetList(data.Bytes()); })
        .and_then([&](const std::vector<std::string>& files) -> Result<assets::FileData> {
            if (files.empty()) {
                return Fail(ErrorCode::NotFound,
                            std::format("{} lists no part sets", kPartSetList));
            }
            return vfs.Read(
                assets::JoinGamePath(kPieceFolder, assets::BinaryPartSetFile(files.front())));
        })
        .and_then([](const assets::FileData& data) { return assets::ParsePartSet(data.Bytes()); });
}

}

Result<settings::Racer> NewRacer(const assets::Vfs& vfs, std::string name) {
    return FirstPartSet(vfs).transform([&](assets::PartSet set) {
        return settings::Racer{.name = std::move(name), .chassis = std::move(set.chassisName)};
    });
}

Result<settings::RacerStore> LoadRacerStore(const std::filesystem::path& file, std::size_t racer) {
    return settings::LoadRacers(file).and_then(
        [&](settings::RacerStore store) -> Result<settings::RacerStore> {
            if (racer >= store.racers.size()) {
                return Fail(ErrorCode::NotFound, std::format("there's no racer {}", racer));
            }
            return store;
        });
}

std::size_t CopyRacer(settings::RacerStore& store, std::size_t racer) {
    settings::Racer copy = store.racers.at(racer);
    store.racers.push_back(std::move(copy));
    return store.racers.size() - 1;
}

void DeleteRacer(settings::RacerStore& store, std::size_t racer) {
    store.racers.erase(store.racers.begin() + static_cast<std::ptrdiff_t>(racer));
    if (!store.selected || *store.selected < racer) {
        return;
    }
    if (*store.selected == racer) {
        store.selected.reset();
        return;
    }
    --*store.selected;
}

}
