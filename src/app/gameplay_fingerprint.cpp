#include "app/gameplay_fingerprint.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>

#include "mods/mod_manifest.hpp"
#include "sim/replay/replay_bytes.hpp"

namespace rr::app {
namespace {

std::uint64_t HashRacer(const settings::Racer& racer) {
    const assets::DriverLook& look = racer.look;
    const std::array<std::uint8_t, 5> parts{look.face, look.hat, look.legs, look.torso,
                                            look.expression};
    std::uint64_t hash = sim::HashBytes(std::as_bytes(std::span(parts)));
    hash = sim::HashBytes(std::as_bytes(std::span(racer.chassis)), hash);
    return sim::HashBytes(racer.car, hash);
}

}

std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                             const mods::ModList& mods,
                                             const std::optional<settings::Racer>& player) {
    std::vector<std::string> entries;
    for (const mods::Mod* mod : mods.Enabled(settings)) {
        entries.push_back(std::format("mod:{}@{}", mod->Manifest().id, mod->Manifest().version));
    }
    if (player) {
        entries.push_back(std::format("racer:{}@{:016x}", player->name, HashRacer(*player)));
    }
    return entries;
}

}
