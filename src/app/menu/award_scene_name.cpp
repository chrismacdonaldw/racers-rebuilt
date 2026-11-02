#include "app/menu/award_scene_name.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <format>

#include "app/cutscene/cutscene_screen.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kAwardPrefix = "menu:award:";
constexpr std::string_view kRacerField = ":racer=";

struct AwardName {
    sim::CircuitAward award;
    std::string_view name;
};

constexpr std::array kAwardNames{AwardName{.award = sim::CircuitAward::Gold, .name = "gold"},
                                 AwardName{.award = sim::CircuitAward::Silver, .name = "silver"},
                                 AwardName{.award = sim::CircuitAward::Bronze, .name = "bronze"},
                                 AwardName{.award = sim::CircuitAward::None, .name = "none"}};
constexpr std::array<std::string_view, 4> kAwardScreens{"c_award1", "c_award2", "c_award3",
                                                        "c_award4"};
constexpr std::int32_t kRocketRacerPartSet = 6;

}

std::string AwardSceneName(const AwardSceneSetup& setup) {
    const std::string_view award =
        std::ranges::find(kAwardNames, setup.award, &AwardName::award)->name;
    return setup.racer == 0 ? std::format("{}{}:{}", kAwardPrefix, setup.circuit, award)
                            : std::format("{}{}:{}{}{}", kAwardPrefix, setup.circuit, award,
                                          kRacerField, setup.racer);
}

bool IsAwardScene(std::string_view scene) {
    return scene.starts_with(kAwardPrefix);
}

Result<AwardSceneSetup> ParseAwardScene(std::string_view scene) {
    const std::string_view rest = IsAwardScene(scene) ? scene.substr(kAwardPrefix.size()) : "";
    const std::size_t colon = rest.find(':');
    const std::string_view tail = colon == std::string_view::npos ? "" : rest.substr(colon + 1);
    const std::size_t field = tail.find(kRacerField);
    const auto found = colon != 0 && colon != std::string_view::npos
                           ? std::ranges::find(kAwardNames, tail.substr(0, field), &AwardName::name)
                           : kAwardNames.end();
    std::size_t racer = 0;
    const std::string_view digits =
        field == std::string_view::npos ? "0" : tail.substr(field + kRacerField.size());
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), racer);
    if (found == kAwardNames.end() || error != std::errc{} ||
        end != digits.data() + digits.size()) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't an award scene", scene));
    }
    return AwardSceneSetup{
        .circuit = std::string(rest.substr(0, colon)), .award = found->award, .racer = racer};
}

std::string AwardScreenName(sim::CircuitAward award, std::optional<std::int32_t> newPartSet,
                            const AwardDrivers& drivers) {
    CutsceneScreenSetup setup{
        .menu = std::string(kAwardScreens.at(static_cast<std::size_t>(award))),
        .driver = drivers.player};
    if (newPartSet && *newPartSet == kRocketRacerPartSet) {
        setup.next = CutsceneScreenName(
            {.menu = std::string(kWinRocketRacerCarScreen), .driver = drivers.player});
    } else if (newPartSet) {
        setup.next =
            CutsceneScreenName({.menu = std::string(kWinCarScreen), .driver = drivers.champion});
    }
    return CutsceneScreenName(setup);
}

}
