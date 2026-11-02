#include "app/circuit_awards.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "assets/race_catalog.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kChampionSlot = 1;

Result<assets::CircuitEntry> LoadCircuit(const assets::Vfs& vfs, std::string_view name) {
    return assets::RaceCatalog::Load(vfs).and_then(
        [&](const assets::RaceCatalog& catalog) -> Result<assets::CircuitEntry> {
            const std::optional<std::size_t> circuit = catalog.FindCircuit(name);
            if (!circuit) {
                return Fail(ErrorCode::NotFound, std::format("there's no circuit {}", name));
            }
            return catalog.Circuits()[*circuit];
        });
}

settings::RacerStore RacersOrNone(const std::filesystem::path& file) {
    Result<settings::RacerStore> racers = settings::LoadRacers(file);
    if (!racers) {
        log::Warn("{}, so no racer takes the trophy", racers.error().message);
        return {};
    }
    return std::move(*racers);
}

settings::Trophy TrophyOf(sim::CircuitAward award) {
    switch (award) {
        case sim::CircuitAward::Gold:
            return settings::Trophy::Gold;
        case sim::CircuitAward::Silver:
            return settings::Trophy::Silver;
        case sim::CircuitAward::Bronze:
        case sim::CircuitAward::None:
            break;
    }
    return settings::Trophy::Bronze;
}

Result<AwardUnlocks> SaveCircuitAward(const std::filesystem::path& file,
                                      const assets::CircuitEntry& circuit,
                                      sim::CircuitAward award) {
    const std::filesystem::path racersFile = file.parent_path() / kRacersFileName;
    Result<settings::Progress> progress = settings::LoadProgress(file);
    if (!progress) {
        return std::unexpected(std::move(progress.error()));
    }
    settings::RacerStore racers = RacersOrNone(racersFile);
    const AwardUnlocks unlocks{.opensCircuit = OpensNewCircuit(*progress, circuit, award),
                               .partSet = NewPartSet(*progress, circuit, award)};
    if (!GrantAward(*progress, settings::TrophiesOf(*progress, racers), circuit, award)) {
        return AwardUnlocks{};
    }
    Result<void> saved =
        racers.selected ? settings::SaveRacers(racers, racersFile) : Result<void>{};
    return saved.and_then([&] { return settings::SaveProgress(*progress, file); }).transform([&] {
        return unlocks;
    });
}

}

bool GrantAward(settings::Progress& progress, settings::Trophies& trophies,
                const assets::CircuitEntry& circuit, sim::CircuitAward award) {
    if (award == sim::CircuitAward::None) {
        return false;
    }
    bool changed = settings::AwardTrophy(trophies, circuit.name, TrophyOf(award));
    if (const std::optional<std::int32_t> partSet = NewPartSet(progress, circuit, award)) {
        progress.partSets.insert(*partSet);
        changed = true;
    }
    if (OpensNewCircuit(progress, circuit, award)) {
        progress.unlockedCircuits.insert(*circuit.next);
        changed = true;
    }
    return changed;
}

bool OpensNewCircuit(const settings::Progress& progress, const assets::CircuitEntry& circuit,
                     sim::CircuitAward award) {
    return (award == sim::CircuitAward::Gold || award == sim::CircuitAward::Silver) &&
           circuit.next && !progress.unlockedCircuits.contains(*circuit.next);
}

std::optional<std::int32_t> NewPartSet(const settings::Progress& progress,
                                       const assets::CircuitEntry& circuit,
                                       sim::CircuitAward award) {
    if (award != sim::CircuitAward::Gold || !circuit.partSet ||
        progress.partSets.contains(*circuit.partSet)) {
        return std::nullopt;
    }
    return circuit.partSet;
}

Result<AwardUnlocks> SaveAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                               const AwardSceneSetup& award) {
    return LoadCircuit(vfs, award.circuit).and_then([&](const assets::CircuitEntry& circuit) {
        return SaveCircuitAward(file, circuit, award.award);
    });
}

AwardDrivers DriversOf(const assets::CircuitEntry& circuit, std::size_t racer) {
    const auto code = [&](std::size_t slot) {
        return slot < circuit.grid.size() ? circuit.grid[slot] : std::string();
    };
    return {.player = code(racer), .champion = code(kChampionSlot)};
}

AwardClaim ClaimAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                      const AwardSceneSetup& award) {
    const Result<assets::CircuitEntry> circuit = LoadCircuit(vfs, award.circuit);
    const Result<AwardUnlocks> unlocks = circuit.and_then([&](const assets::CircuitEntry& entry) {
        return SaveCircuitAward(file, entry, award.award);
    });
    if (!circuit) {
        log::Warn("{}, the award shows its stand-in racer", circuit.error().message);
    }
    if (!unlocks) {
        log::Warn("{}, progress wasn't saved", unlocks.error().message);
    }
    const AwardUnlocks granted = unlocks.value_or(AwardUnlocks{});
    const AwardDrivers drivers = circuit ? DriversOf(*circuit, award.racer) : AwardDrivers{};
    return {.screen = AwardScreenName(award.award, granted.partSet, drivers),
            .opensCircuit = granted.opensCircuit};
}

settings::Progress ReadProgress(const std::filesystem::path& file) {
    Result<settings::Progress> progress = settings::LoadProgress(file);
    if (!progress) {
        log::Warn("{}, starting from a fresh game's progress", progress.error().message);
        return {};
    }
    return std::move(*progress);
}

}
