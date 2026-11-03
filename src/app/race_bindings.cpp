#include "app/race_bindings.hpp"

#include <optional>
#include <string>

#include "core/log.hpp"

namespace rr::app {

std::span<const RaceBinding> RaceBindingsOf(InputSet set) {
    switch (set) {
        case InputSet::Pad1:
            return kPad1Bindings;
        case InputSet::Pad2:
            return kPad2Bindings;
        case InputSet::Keys1:
            return kKeys1Bindings;
        case InputSet::Keys2:
            return kKeys2Bindings;
        case InputSet::Keys3:
            return kKeys3Bindings;
    }
    return kKeys1Bindings;
}

PlayerSets ChosenPlayerSets(const settings::Registry& registry) {
    PlayerSets chosen = kDefaultPlayerSets;
    for (std::size_t player = 0; player < chosen.size(); ++player) {
        const settings::SettingSpec<std::string_view>& spec = kPlayerDeviceSettings.at(player);
        const std::string name = registry.Get(spec);
        if (const std::optional<InputSet> set = ParseInputSet(name)) {
            chosen.at(player) = *set;
        } else {
            log::Warn("`{}` in {} is not an input device, using {}", name, spec.key,
                      spec.defaultValue);
        }
    }
    return chosen;
}

}
