#include "app/menu/award_scene_name.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace rr::app {
namespace {

constexpr std::string_view kAwardPrefix = "menu:award:";

struct AwardName {
    sim::CircuitAward award;
    std::string_view name;
};

constexpr std::array kAwardNames{AwardName{.award = sim::CircuitAward::Gold, .name = "gold"},
                                 AwardName{.award = sim::CircuitAward::Silver, .name = "silver"},
                                 AwardName{.award = sim::CircuitAward::Bronze, .name = "bronze"},
                                 AwardName{.award = sim::CircuitAward::None, .name = "none"}};

}

std::string AwardSceneName(const AwardSceneSetup& setup) {
    return std::format("{}{}:{}", kAwardPrefix, setup.circuit,
                       std::ranges::find(kAwardNames, setup.award, &AwardName::award)->name);
}

bool IsAwardScene(std::string_view scene) {
    return scene.starts_with(kAwardPrefix);
}

Result<AwardSceneSetup> ParseAwardScene(std::string_view scene) {
    const std::string_view rest = IsAwardScene(scene) ? scene.substr(kAwardPrefix.size()) : "";
    const std::size_t colon = rest.find(':');
    const auto found =
        colon != 0 && colon != std::string_view::npos
            ? std::ranges::find(kAwardNames, rest.substr(colon + 1), &AwardName::name)
            : kAwardNames.end();
    if (found == kAwardNames.end()) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't an award scene", scene));
    }
    return AwardSceneSetup{.circuit = std::string(rest.substr(0, colon)), .award = found->award};
}

}
