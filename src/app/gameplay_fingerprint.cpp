#include "app/gameplay_fingerprint.hpp"

#include <format>
#include <variant>

#include "app/racer/racer_ref.hpp"
#include "mods/mod_manifest.hpp"

namespace rr::app {
namespace {

void AppendChangedSettings(const settings::Registry& settings, std::vector<std::string>& entries) {
    for (const auto& [key, declaration] : settings.AllDeclarations()) {
        const settings::SettingValue& value = settings.AllValues().find(key)->second;
        if (declaration.changesGameplay && value != declaration.defaultValue) {
            std::visit(
                [&](const auto& current) {
                    entries.push_back(std::format("setting:{}={}", key, current));
                },
                value);
        }
    }
}

}

std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                             const mods::ModList& mods,
                                             std::optional<std::uint64_t> overridesHash,
                                             const PlayerRacers& players) {
    std::vector<std::string> entries;
    for (const mods::Mod* mod : mods.Enabled(settings)) {
        entries.push_back(std::format("mod:{}@{}", mod->Manifest().id, mod->Manifest().version));
    }
    if (overridesHash) {
        entries.push_back(std::format("overrides:{:016x}", *overridesHash));
    }
    AppendChangedSettings(settings, entries);
    for (const auto& [key, racer] :
         {std::pair{"racer", &players.first}, std::pair{"racer2", &players.second}}) {
        if (*racer) {
            entries.push_back(
                std::format("{}:{}@{:016x}", key, (*racer)->name, RacerHash(**racer)));
        }
    }
    return entries;
}

}
