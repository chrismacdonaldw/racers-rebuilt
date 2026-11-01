#include "app/hidden_models.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rr::app {
namespace {

constexpr std::uint32_t kIndices = 12;

std::vector<NamedDraws> TrackDraws() {
    return {{.entity = "PILLAR", .span = {.first = 0, .count = 2}},
            {.entity = "sphinx", .span = {.first = 2, .count = 1}}};
}

std::array<render::MeshDraw, 3> Draws() {
    std::array<render::MeshDraw, 3> draws{};
    for (render::MeshDraw& draw : draws) {
        draw.indexCount = kIndices;
    }
    return draws;
}

std::array<std::uint32_t, 3> Counts(const std::array<render::MeshDraw, 3>& draws) {
    return {draws[0].indexCount, draws[1].indexCount, draws[2].indexCount};
}

}

TEST_CASE("hidden models target the distances, then the hazard switches, by entity name") {
    const sim::TrackEntitySources sources;
    const TrackEntityDraws entities({}, sources);
    const std::vector<assets::ModelDistanceResource> distances{
        {.entity = assets::EntityRef{.name = "pillar"}}, {}};
    const std::vector<std::string> switches{"SPHINX"};
    const HiddenModels models(distances, switches, TrackDraws(), sources);
    const std::array<render::MeshDraw, 3> track = Draws();
    std::array<render::MeshDraw, 3> draws = Draws();

    models.Apply({false, true, true}, track, entities, track.size(), draws);
    CHECK(Counts(draws) == std::array<std::uint32_t, 3>{kIndices, kIndices, 0});

    models.Apply({true, false, false}, track, entities, track.size(), draws);
    CHECK(Counts(draws) == std::array<std::uint32_t, 3>{0, 0, kIndices});
}

}
