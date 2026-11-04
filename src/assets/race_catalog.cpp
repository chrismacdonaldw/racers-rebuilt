#include "assets/race_catalog.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

#include "assets/circuit_table.hpp"
#include "assets/game_path.hpp"
#include "core/log.hpp"

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
        std::optional<std::size_t>& slot = slots[static_cast<std::size_t>(found - circuits.begin())]
                                                [static_cast<std::size_t>(races[race].position)];
        if (slot) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("races {} and {} share position {} in circuit {}", races[*slot].name,
                            races[race].name, races[race].position, *circuit));
        }
        slot = race;
    }
    return RaceCatalog(std::move(circuits), std::move(races), std::move(slots));
}

Result<RaceCatalog> RaceCatalog::Load(const Vfs& vfs) {
    std::vector<std::string> problems;
    Result<RaceCatalog> catalog = Load(vfs, problems);
    for (const std::string& problem : problems) {
        log::Warn("{}, leaving it out", problem);
    }
    return catalog;
}

Result<RaceCatalog> RaceCatalog::Load(const Vfs& vfs, std::vector<std::string>& problems) {
    Result<std::vector<CircuitEntry>> circuits = ReadFile(vfs, kCircuitsPath, ParseCircuits);
    Result<std::vector<RaceEntry>> races = circuits.and_then(
        [&](const std::vector<CircuitEntry>&) { return ReadFile(vfs, kRacesPath, ParseRaces); });
    Result<ModTables<CircuitPatch>> tables =
        races.and_then([&](const std::vector<RaceEntry>&) { return ReadCircuitTables(vfs); });
    if (!tables) {
        return std::unexpected(std::move(tables.error()));
    }
    std::ranges::move(tables->problems, std::back_inserter(problems));
    std::ranges::move(ApplyCircuitPatches(tables->entries, *circuits, *races),
                      std::back_inserter(problems));
    return Create(std::move(*circuits), std::move(*races));
}

std::vector<std::size_t> RaceCatalog::Story() const {
    std::vector<std::size_t> story;
    std::optional<std::size_t> circuit =
        circuits_.empty() ? std::nullopt : std::optional<std::size_t>(0);
    while (circuit && !std::ranges::contains(story, *circuit)) {
        story.push_back(*circuit);
        const std::optional<std::string>& next = circuits_[*circuit].next;
        circuit = next ? FindCircuit(*next) : std::nullopt;
    }
    return story;
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
    for (std::size_t circuit = 0; circuit < circuits_.size(); ++circuit) {
        if (!circuits_[circuit].isFromMod && std::ranges::any_of(slots_[circuit], racesTrack)) {
            return circuit;
        }
    }
    return std::nullopt;
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
