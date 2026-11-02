#include "app/circuit_awards.hpp"

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
    if (award == sim::CircuitAward::Gold && circuit.partSet) {
        changed = progress.partSets.insert(*circuit.partSet).second || changed;
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

Result<bool> SaveAward(const std::filesystem::path& file, const assets::Vfs& vfs,
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
    const bool opens = OpensNewCircuit(*progress, entry, award.award);
    if (!GrantAward(*progress, settings::TrophiesOf(*progress, *racers), entry, award.award)) {
        return false;
    }
    Result<void> saved =
        racers->selected ? settings::SaveRacers(*racers, racersFile) : Result<void>{};
    return saved.and_then([&] { return settings::SaveProgress(*progress, file); }).transform([&] {
        return opens;
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
