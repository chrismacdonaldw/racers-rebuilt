#include "app/device_assignment.hpp"

#include <algorithm>

namespace rr::app {
namespace {

static_assert(sim::kMaxPlayers == 2, "the fallback rule moves the one other player");

constexpr std::array<std::string_view, kInputSets.size()> kInputSetNames{"pad1", "pad2", "keys1",
                                                                         "keys2", "keys3"};

std::size_t OtherPlayer(std::size_t player) {
    return player == 0 ? 1 : 0;
}

bool IsAttached(InputSet set, std::size_t padCount) {
    const std::optional<std::size_t> pad = PadOf(set);
    return !pad || *pad < padCount;
}

InputSet FallbackSet(InputSet otherSet, std::size_t padCount) {
    for (const InputSet pad : kPadSets) {
        if (pad != otherSet && IsAttached(pad, padCount)) {
            return pad;
        }
    }
    return otherSet == InputSet::Keys1 ? InputSet::Keys2 : InputSet::Keys1;
}

}

std::string_view InputSetName(InputSet set) {
    return kInputSetNames.at(static_cast<std::size_t>(set));
}

std::optional<InputSet> ParseInputSet(std::string_view name) {
    const auto found = std::ranges::find(kInputSetNames, name);
    if (found == kInputSetNames.end()) {
        return std::nullopt;
    }
    return kInputSets.at(static_cast<std::size_t>(found - kInputSetNames.begin()));
}

std::optional<std::size_t> PadOf(InputSet set) {
    const auto found = std::ranges::find(kPadSets, set);
    if (found == kPadSets.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - kPadSets.begin());
}

PlayerSets AssignInputSets(const PlayerSets& chosen, std::size_t padCount) {
    PlayerSets sets = chosen;
    for (std::size_t player = 0; player < sets.size(); ++player) {
        const bool isTaken = player > 0 && sets.at(player) == sets.front();
        if (isTaken || !IsAttached(sets.at(player), padCount)) {
            sets.at(player) = FallbackSet(sets.at(OtherPlayer(player)), padCount);
        }
    }
    return sets;
}

PlayerSets ChooseInputSet(const PlayerSets& sets, std::size_t player, InputSet set,
                          std::size_t padCount) {
    PlayerSets chosen = sets;
    chosen.at(player) = set;
    InputSet& other = chosen.at(OtherPlayer(player));
    if (other == set) {
        other = FallbackSet(set, padCount);
    }
    return chosen;
}

}
