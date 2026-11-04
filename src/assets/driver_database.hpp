#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/brick_layout.hpp"
#include "assets/model_files.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// `title` and `racer` come from a mod's table: a shown name instead of `nameIndex`'s, and a
/// built racer from the game's racers instead of `model` and `champion`. `useChances`, by
/// `BrickColour` out of 255, and `startBoost` override the stats when set.
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
    std::string title;
    std::string racer;
    std::array<std::optional<std::uint8_t>, kBrickColourNames.size()> useChances{};
    std::optional<bool> startBoost;
};

[[nodiscard]] Result<std::vector<DriverEntry>> ParseDrivers(std::span<const std::byte> bytes);

}
