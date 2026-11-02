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

bool GrantAward(settings::Progress& progress, const assets::CircuitEntry& circuit,
                sim::CircuitAward award) {
    if (award == sim::CircuitAward::None) {
        return false;
    }
    bool changed = settings::AwardTrophy(progress, circuit.name, TrophyOf(award));
    if (award == sim::CircuitAward::Gold && circuit.partSet) {
        changed = progress.partSets.insert(*circuit.partSet).second || changed;
    }
    if (award != sim::CircuitAward::Bronze && circuit.next) {
        changed = progress.unlockedCircuits.insert(*circuit.next).second || changed;
    }
    return changed;
}

Result<void> SaveAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                       const AwardSceneSetup& award) {
    Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Load(vfs);
    if (!catalog) {
        return std::unexpected(std::move(catalog.error()));
    }
    const std::optional<std::size_t> circuit = catalog->FindCircuit(award.circuit);
    if (!circuit) {
        return Fail(ErrorCode::NotFound, std::format("there's no circuit {}", award.circuit));
    }
    Result<settings::Progress> progress = settings::LoadProgress(file);
    if (!progress) {
        return std::unexpected(std::move(progress.error()));
    }
    if (!GrantAward(*progress, catalog->Circuits()[*circuit], award.award)) {
        return {};
    }
    return settings::SaveProgress(*progress, file);
}

std::set<std::string> UnlockedCircuits(const std::filesystem::path& file) {
    Result<settings::Progress> progress = settings::LoadProgress(file);
    if (!progress) {
        log::Warn("{}, only the first circuit is open", progress.error().message);
        return {};
    }
    return std::move(progress->unlockedCircuits);
}

}
