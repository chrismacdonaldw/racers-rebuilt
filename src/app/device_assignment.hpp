#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class InputSet : std::uint8_t { Pad1, Pad2, Keys1, Keys2, Keys3 };

inline constexpr std::array kInputSets{InputSet::Pad1, InputSet::Pad2, InputSet::Keys1,
                                       InputSet::Keys2, InputSet::Keys3};
inline constexpr std::array kPadSets{InputSet::Pad1, InputSet::Pad2};

using PlayerSets = std::array<InputSet, sim::kMaxPlayers>;

inline constexpr PlayerSets kDefaultPlayerSets{InputSet::Pad1, InputSet::Pad2};

[[nodiscard]] std::string_view InputSetName(InputSet set);
[[nodiscard]] std::optional<InputSet> ParseInputSet(std::string_view name);

/// Connection order: the first pad is 0.
[[nodiscard]] std::optional<std::size_t> PadOf(InputSet set);

/// Each player keeps their set unless it names a pad that isn't attached or the earlier player's.
[[nodiscard]] PlayerSets AssignInputSets(const PlayerSets& chosen, std::size_t padCount);

/// Gives `player` the set, moving the other player off it as the controls screen does.
[[nodiscard]] PlayerSets ChooseInputSet(const PlayerSets& sets, std::size_t player, InputSet set,
                                        std::size_t padCount);

}
