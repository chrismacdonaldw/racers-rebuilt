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
    Result<assets::CircuitEntry> entry = LoadCircuit(vfs, award.circuit);
    if (!entry) {
        return std::unexpected(std::move(entry.error()));
    }
    const std::filesystem::path racersFile = file.parent_path() / kRacersFileName;
    Result<settings::Progress> progress = settings::LoadProgress(file);
    if (!progress) {
        return std::unexpected(std::move(progress.error()));
    }
    settings::RacerStore racers = RacersOrNone(racersFile);
    const AwardUnlocks unlocks{.opensCircuit = OpensNewCircuit(*progress, *entry, award.award),
                               .partSet = NewPartSet(*progress, *entry, award.award)};
    if (!GrantAward(*progress, settings::TrophiesOf(*progress, racers), *entry, award.award)) {
        return AwardUnlocks{};
    }
    Result<void> saved =
        racers.selected ? settings::SaveRacers(racers, racersFile) : Result<void>{};
    return saved.and_then([&] { return settings::SaveProgress(*progress, file); }).transform([&] {
        return unlocks;
    });
}

AwardDrivers DriversOf(const assets::CircuitEntry& circuit, std::size_t racer) {
    const auto code = [&](std::size_t slot) {
        return slot < circuit.grid.size() ? circuit.grid[slot] : std::string();
    };
    return {.player = code(racer), .champion = code(kChampionSlot)};
}

Result<AwardDrivers> LoadAwardDrivers(const assets::Vfs& vfs, const AwardSceneSetup& award) {
    return LoadCircuit(vfs, award.circuit).transform([&](const assets::CircuitEntry& circuit) {
        return DriversOf(circuit, award.racer);
    });
}

AwardClaim ClaimAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                      const AwardSceneSetup& award) {
    const Result<AwardUnlocks> unlocks = SaveAward(file, vfs, award);
    if (!unlocks) {
        log::Warn("{}, progress wasn't saved", unlocks.error().message);
    }
    const Result<AwardDrivers> drivers = LoadAwardDrivers(vfs, award);
    if (!drivers) {
        log::Warn("{}, the award shows its stand-in racer", drivers.error().message);
    }
    const AwardUnlocks granted = unlocks.value_or(AwardUnlocks{});
    return {
        .screen = AwardScreenName(award.award, granted.partSet, drivers.value_or(AwardDrivers{})),
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
