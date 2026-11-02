#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assets/driver_look.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::size_t kSavedRaces = 13;

struct SavedTime {
    std::uint32_t ms = 0;
    std::u16string holder;

    friend bool operator==(const SavedTime&, const SavedTime&) = default;
};

struct SavedRacer {
    std::u16string name;
    DriverLook look;
    std::uint16_t trophies = 0;

    friend bool operator==(const SavedRacer&, const SavedRacer&) = default;
};

/// A 0 ms time is no record. Trophies are 2 bits per circuit, from 1 gold to 3 bronze.
struct OriginalSave {
    std::uint8_t partSets = 0;
    std::uint8_t circuits = 0;
    std::uint16_t beatenRecords = 0;
    std::array<SavedTime, kSavedRaces> bestLaps;
    std::array<SavedTime, kSavedRaces> bestRaces;
    std::vector<SavedRacer> racers;

    friend bool operator==(const OriginalSave&, const OriginalSave&) = default;
};

/// Fails on a block whose checksum doesn't match, as the original does.
[[nodiscard]] Result<OriginalSave> ParseOriginalSave(std::span<const std::byte> bytes);

}
