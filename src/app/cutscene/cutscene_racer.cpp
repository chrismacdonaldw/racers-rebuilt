#include "app/cutscene/cutscene_racer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <iterator>
#include <variant>

#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr std::int32_t kPegLeg = 10;
constexpr std::array<std::string_view, 2> kLegVariants{"SWAP", "PLEG"};
constexpr std::string_view kChampionTextPrefix = "text";
constexpr std::array<std::string_view, 6> kChampionCodes{"CR", "KK", "BB", "JT", "GM", "BVB"};
constexpr std::array<std::string_view, 6> kExpressions{"dflt",  "angry", "blink",
                                                       "happy", "sad",   "suprz"};

bool IsLegVariant(const assets::CutsceneModel& event) {
    const auto* entity = std::get_if<assets::EntityByName>(&event.model);
    return entity != nullptr && entity->isJointed &&
           std::ranges::contains(kLegVariants, assets::ToUpperAscii(entity->name));
}

}

void DropLegVariants(const assets::CutsceneFrame& frame, std::span<ModelBinding> models,
                     std::int32_t legs) {
    if (legs == kPegLeg) {
        return;
    }
    for (std::size_t event = 0; event < frame.models.size() && event < models.size(); ++event) {
        if (IsLegVariant(frame.models[event])) {
            models[event] = {};
        }
    }
}

std::vector<std::string> OtherChampionNames(std::string_view champion) {
    std::vector<std::string> names;
    for (const std::string_view code : kChampionCodes) {
        if (!champion.starts_with(code)) {
            names.push_back(std::format("{}{}", kChampionTextPrefix, code));
        }
    }
    return names;
}

void HideTexts(assets::CutsceneEffectsAsset& effects, std::span<const std::string> names) {
    const auto isHidden = [&](const assets::CutsceneLink& link) {
        if (link.kind != assets::EffectKind::Text || link.effect >= effects.texts.size()) {
            return false;
        }
        const std::string text = assets::ToUpperAscii(effects.texts[link.effect].name);
        return std::ranges::any_of(
            names, [&](const std::string& name) { return assets::ToUpperAscii(name) == text; });
    };
    std::erase_if(effects.links, isHidden);
}

std::int32_t FaceExpression(std::string_view material) {
    const auto found = std::ranges::find(kExpressions, material);
    return found == kExpressions.end()
               ? 0
               : static_cast<std::int32_t>(std::distance(kExpressions.begin(), found));
}

}
