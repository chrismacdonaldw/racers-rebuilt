#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "archive_survey.hpp"
#include "assets/cutscene_effects.hpp"

namespace rr::assets {
namespace {

struct EffectCounts {
    std::string_view path;
    std::size_t sounds = 0;
    std::size_t texts = 0;
    std::size_t images = 0;
    std::size_t particles = 0;
    std::size_t tints = 0;
    std::size_t fades = 0;
    std::size_t startLinks = 0;
    std::size_t stopLinks = 0;

    friend bool operator==(const EffectCounts&, const EffectCounts&) = default;
};

constexpr std::array kEffects{
    EffectCounts{.path = "LEGAL/SPLASH1",
                 .sounds = 0,
                 .texts = 0,
                 .images = 1,
                 .particles = 0,
                 .tints = 0,
                 .fades = 2,
                 .startLinks = 3,
                 .stopLinks = 3},
    EffectCounts{.path = "CIRCUIT1/CIR1-01",
                 .sounds = 5,
                 .texts = 5,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 14,
                 .stopLinks = 6},
    EffectCounts{.path = "CIRCUIT2/TEST",
                 .sounds = 4,
                 .texts = 4,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 9,
                 .stopLinks = 6},
    EffectCounts{.path = "CIRCUIT3/CIR3-01",
                 .sounds = 6,
                 .texts = 4,
                 .images = 0,
                 .particles = 3,
                 .tints = 4,
                 .fades = 1,
                 .startLinks = 18,
                 .stopLinks = 13},
    EffectCounts{.path = "CIRCUIT4/CIR4-01",
                 .sounds = 2,
                 .texts = 5,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 8,
                 .stopLinks = 6},
    EffectCounts{.path = "CIRCUIT5/CIR5-01",
                 .sounds = 12,
                 .texts = 4,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 17,
                 .stopLinks = 5},
    EffectCounts{.path = "CIRCUIT6/TEST",
                 .sounds = 5,
                 .texts = 4,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 10,
                 .stopLinks = 8},
    EffectCounts{.path = "CIRCUIT7/FR",
                 .sounds = 4,
                 .texts = 11,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 16,
                 .stopLinks = 14},
    EffectCounts{.path = "C_AWARD1/1STPLACE",
                 .sounds = 5,
                 .texts = 0,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 9,
                 .stopLinks = 0},
    EffectCounts{.path = "C_AWARD2/TWO",
                 .sounds = 4,
                 .texts = 0,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 2,
                 .startLinks = 6,
                 .stopLinks = 1},
    EffectCounts{.path = "C_AWARD3/3-01",
                 .sounds = 19,
                 .texts = 0,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 21,
                 .stopLinks = 0},
    EffectCounts{.path = "C_AWARD4/LOSE",
                 .sounds = 1,
                 .texts = 2,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 4,
                 .stopLinks = 2},
    EffectCounts{.path = "WINCAR/WINCAR",
                 .sounds = 1,
                 .texts = 7,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 1,
                 .startLinks = 9,
                 .stopLinks = 7},
    EffectCounts{.path = "WINRRCAR/END",
                 .sounds = 2,
                 .texts = 7,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 2,
                 .startLinks = 11,
                 .stopLinks = 9},
    EffectCounts{.path = "WINVVCAR/VVWIN",
                 .sounds = 1,
                 .texts = 5,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 2,
                 .startLinks = 8,
                 .stopLinks = 7},
    EffectCounts{.path = "CREDITS/CREDITS",
                 .sounds = 0,
                 .texts = 81,
                 .images = 0,
                 .particles = 0,
                 .tints = 0,
                 .fades = 2,
                 .startLinks = 87,
                 .stopLinks = 87},
    EffectCounts{.path = "SINGRACE/PST",
                 .sounds = 0,
                 .texts = 0,
                 .images = 0,
                 .particles = 2,
                 .tints = 1,
                 .fades = 0,
                 .startLinks = 3,
                 .stopLinks = 3},
};

constexpr std::size_t kCutsceneCount = 17;

EffectCounts CountsOf(const CutsceneEffectsAsset& effects, std::string_view path) {
    const auto stopLinks = static_cast<std::size_t>(
        std::ranges::count(effects.links, true, &CutsceneLink::onTransformEnd));
    return {.path = path,
            .sounds = effects.sounds.size(),
            .texts = effects.texts.size(),
            .images = effects.images.size(),
            .particles = effects.particles.size(),
            .tints = effects.tints.size(),
            .fades = effects.fades.size(),
            .startLinks = effects.links.size() - stopLinks,
            .stopLinks = stopLinks};
}

TEST_CASE("every retail cutscene effects file parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    const test::Survey effects = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "CEB"; },
        [](std::span<const std::byte> bytes) {
            return ParseCutsceneEffects(bytes).transform([](const CutsceneEffectsAsset&) {});
        });
    test::CheckSurvey(effects, kCutsceneCount);
}

TEST_CASE("retail effects hold the effects and links the original loads") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    for (const EffectCounts& expected : kEffects) {
        const Result<CutsceneEffectsAsset> effects =
            test::ReadRetailFile(*archive, std::format("MENUDATA/{}.CEB", expected.path))
                .and_then([](const FileData& data) { return ParseCutsceneEffects(data.Bytes()); });
        REQUIRE_MESSAGE(effects.has_value(), expected.path);
        CHECK(CountsOf(*effects, expected.path) == expected);
    }
}

}
}
