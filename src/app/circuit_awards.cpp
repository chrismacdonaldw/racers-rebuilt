#include "app/circuit_awards.hpp"

#include <cstdint>
#include <format>
#include <optional>
#include <utility>

#include "assets/race_catalog.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

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
    Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Load(vfs);
    if (!catalog) {
        return std::unexpected(std::move(catalog.error()));
    }
    const std::optional<std::size_t> circuit = catalog->FindCircuit(award.circuit);
    if (!circuit) {
        return Fail(ErrorCode::NotFound, std::format("there's no circuit {}", award.circuit));
    }
    const std::filesystem::path racersFile = file.parent_path() / kRacersFileName;
    Result<settings::Progress> progress = settings::LoadProgress(file);
    Result<settings::RacerStore> racers = progress.and_then(
        [&](const settings::Progress&) { return settings::LoadRacers(racersFile); });
    if (!racers) {
        return std::unexpected(std::move(racers.error()));
    }
    const assets::CircuitEntry& entry = catalog->Circuits()[*circuit];
    const AwardUnlocks unlocks{.opensCircuit = OpensNewCircuit(*progress, entry, award.award),
                               .partSet = NewPartSet(*progress, entry, award.award)};
    if (!GrantAward(*progress, settings::TrophiesOf(*progress, *racers), entry, award.award)) {
        return AwardUnlocks{};
    }
    Result<void> saved =
        racers->selected ? settings::SaveRacers(*racers, racersFile) : Result<void>{};
    return saved.and_then([&] { return settings::SaveProgress(*progress, file); }).transform([&] {
        return unlocks;
    });
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
