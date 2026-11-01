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

std::string AwardSceneName(sim::CircuitAward award) {
    return std::format("{}{}", kAwardPrefix,
                       std::ranges::find(kAwardNames, award, &AwardName::award)->name);
}

bool IsAwardScene(std::string_view scene) {
    return scene.starts_with(kAwardPrefix);
}

Result<sim::CircuitAward> ParseAwardScene(std::string_view scene) {
    const auto found =
        IsAwardScene(scene)
            ? std::ranges::find(kAwardNames, scene.substr(kAwardPrefix.size()), &AwardName::name)
            : kAwardNames.end();
    if (found == kAwardNames.end()) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't an award scene", scene));
    }
    return found->award;
}

}
