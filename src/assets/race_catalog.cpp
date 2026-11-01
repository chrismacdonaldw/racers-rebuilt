#include "assets/race_catalog.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

#include "assets/game_path.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kCircuitsPath = "MENUDATA/LEGORACE.CRB";
constexpr std::string_view kRacesPath = "MENUDATA/LEGORACE.RCB";

template <typename Parse>
auto ReadFile(const Vfs& vfs, std::string_view path, Parse parse) {
    return vfs.Read(path).and_then([&](const FileData& file) { return parse(file.Bytes()); });
}

}

Result<RaceCatalog> RaceCatalog::Create(std::vector<CircuitEntry> circuits,
                                        std::vector<RaceEntry> races) {
    std::vector<CircuitRaces> slots(circuits.size());
    for (std::size_t race = 0; race < races.size(); ++race) {
        const std::optional<std::string>& circuit = races[race].circuit;
        if (!circuit) {
            continue;
        }
        const auto found = std::ranges::find(circuits, *circuit, &CircuitEntry::name);
        if (found == circuits.end()) {
            return Fail(ErrorCode::InvalidData, std::format("race {} names a missing circuit {}",
                                                            races[race].name, *circuit));
        }
        const auto index = static_cast<std::size_t>(found - circuits.begin());
        slots[index].at(static_cast<std::size_t>(races[race].position)) = race;
    }
    return RaceCatalog(std::move(circuits), std::move(races), std::move(slots));
}

Result<RaceCatalog> RaceCatalog::Load(const Vfs& vfs) {
    Result<std::vector<CircuitEntry>> circuits = ReadFile(vfs, kCircuitsPath, ParseCircuits);
    Result<std::vector<RaceEntry>> races = circuits.and_then(
        [&](const std::vector<CircuitEntry>&) { return ReadFile(vfs, kRacesPath, ParseRaces); });
    if (!races) {
        return std::unexpected(std::move(races.error()));
    }
    return Create(std::move(*circuits), std::move(*races));
}

std::optional<std::size_t> RaceCatalog::FindCircuit(std::string_view name) const {
    const auto found = std::ranges::find(circuits_, name, &CircuitEntry::name);
    return found == circuits_.end()
               ? std::nullopt
               : std::optional(static_cast<std::size_t>(found - circuits_.begin()));
}

std::optional<std::size_t> RaceCatalog::HomeCircuit(std::string_view track) const {
    const std::string folder = ToUpperAscii(track);
    const auto racesTrack = [&](const std::optional<std::size_t>& race) {
        return race && !races_[*race].isMirrored && ToUpperAscii(races_[*race].folder) == folder;
    };
    const auto home = std::ranges::find_if(
        slots_, [&](const CircuitRaces& slots) { return std::ranges::any_of(slots, racesTrack); });
    return home == slots_.end() ? std::nullopt
                                : std::optional(static_cast<std::size_t>(home - slots_.begin()));
}

Result<std::vector<std::string>> RaceCatalog::Grid(std::string_view track,
                                                   std::string_view circuit) const {
    const std::optional<std::size_t> index =
        circuit.empty() ? HomeCircuit(track) : FindCircuit(circuit);
    if (!index) {
        return Fail(ErrorCode::NotFound, circuit.empty()
                                             ? std::format("{} isn't on a circuit", track)
                                             : std::format("there's no circuit {}", circuit));
    }
    return circuits_[*index].grid;
}

RaceCatalog::RaceCatalog(std::vector<CircuitEntry> circuits, std::vector<RaceEntry> races,
                         std::vector<CircuitRaces> slots)
    : circuits_(std::move(circuits)), races_(std::move(races)), slots_(std::move(slots)) {}

Result<std::vector<std::string>> LoadGrid(const Vfs& vfs, std::string_view track,
                                          std::string_view circuit) {
    return RaceCatalog::Load(vfs).and_then(
        [&](const RaceCatalog& catalog) { return catalog.Grid(track, circuit); });
}

}
