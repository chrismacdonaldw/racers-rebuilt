#include "app/racer/racer_ref.hpp"

#include <array>
#include <cstddef>
#include <span>

#include "assets/game_path.hpp"
#include "sim/replay/replay_bytes.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kLookParts = 5;

}

std::uint64_t RacerHash(const settings::Racer& racer) {
    const assets::DriverLook& look = racer.look;
    const std::array<std::uint8_t, kLookParts> parts{look.face, look.hat, look.legs, look.torso,
                                                     look.expression};
    std::uint64_t hash = sim::HashBytes(std::as_bytes(std::span(parts)));
    hash = sim::HashBytes(std::as_bytes(std::span(racer.chassis)), hash);
    return sim::HashBytes(racer.car, hash);
}

RacerRef RefOf(const settings::Racer& racer) {
    return {.name = racer.name, .hash = RacerHash(racer)};
}

bool IsRef(const settings::Racer& racer, const RacerRef& ref) {
    return assets::ToUpperAscii(racer.name) == assets::ToUpperAscii(ref.name) &&
           (!ref.hash || *ref.hash == RacerHash(racer));
}

}
