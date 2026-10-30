#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "assets/model_files.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

struct ChampionEntry {
    std::string name;
    ModelFiles body;
    std::string chassis;
    float mass = 0.0F;
    Vector3 centreOfMass;
};

[[nodiscard]] Result<std::vector<ChampionEntry>> ParseChampions(std::span<const std::byte> bytes);

}
