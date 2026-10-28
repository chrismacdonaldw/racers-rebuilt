#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "sim/input_frame.hpp"

namespace rr::sim {

inline constexpr std::uint16_t kReplayFormatVersion = 2;

/// `scene` names what was running: a track folder, or empty for the test scene.
struct Replay {
    std::string buildVersion;
    std::string scene;
    std::uint64_t seed = 0;
    std::vector<std::vector<InputFrame>> racers;

    friend bool operator==(const Replay&, const Replay&) = default;
};

[[nodiscard]] std::vector<std::byte> EncodeReplay(const Replay& replay);
[[nodiscard]] Result<Replay> DecodeReplay(std::span<const std::byte> bytes);

}
