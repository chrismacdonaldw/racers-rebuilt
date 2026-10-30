#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assets/model_files.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct DriverEntry {
    std::string name;
    std::int32_t nameIndex = 0;
    ModelFiles model;
    std::string champion;
    std::int32_t redStat = 0;
    std::int32_t yellowStat = 0;
    std::int32_t greenStat = 0;
    std::int32_t blueStat = 0;
    std::int32_t voiceBank = 0;
    std::int32_t hatIndex = 0;
    std::int32_t faceIndex = 0;
    std::int32_t torsoIndex = 0;
    std::int32_t legsIndex = 0;
    std::int32_t expressionIndex = 0;
    std::int32_t aiChargeColor = 0;
    std::int32_t aiWhiteBrickTarget = 0;
};

[[nodiscard]] Result<std::vector<DriverEntry>> ParseDrivers(std::span<const std::byte> bytes);

}
