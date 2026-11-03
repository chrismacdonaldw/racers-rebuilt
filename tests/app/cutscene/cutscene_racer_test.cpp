#include "app/cutscene/cutscene_racer.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace rr::app {
namespace {

assets::CutsceneFrame LegFrame() {
    return {
        .models = {{.name = "a", .model = assets::EntityByName{.name = "Swap", .isJointed = true}},
                   {.name = "b", .model = assets::EntityByName{.name = "pleg", .isJointed = true}},
                   {.name = "c", .model = assets::EntityByName{.name = "guy1", .isJointed = true}},
                   {.name = "d", .model = assets::EntityByName{.name = "swap"}}}};
}

std::vector<bool> Bound(const std::vector<ModelBinding>& models) {
    std::vector<bool> bound;
    bound.reserve(models.size());
    for (const ModelBinding& model : models) {
        bound.push_back(model.jointed.has_value() || model.staticDraws.has_value());
    }
    return bound;
}

TEST_CASE("swap and pleg leg variants show only for the peg leg") {
    const assets::CutsceneFrame frame = LegFrame();
    std::vector<ModelBinding> pegged{
        {.jointed = 0}, {.jointed = 1}, {.jointed = 2}, {.staticDraws = DrawSpan{}}};
    std::vector<ModelBinding> plain = pegged;

    DropLegVariants(frame, pegged, 10);
    DropLegVariants(frame, plain, 3);

    CHECK(Bound(pegged) == std::vector<bool>{true, true, true, true});
    CHECK(Bound(plain) == std::vector<bool>{false, false, true, true});
}

TEST_CASE("wincar hides every champion's name but the champion's own") {
    CHECK(OtherChampionNames("KK") ==
          std::vector<std::string>{"textCR", "textBB", "textJT", "textGM", "textBVB"});
    CHECK(OtherChampionNames("BVB") ==
          std::vector<std::string>{"textCR", "textKK", "textBB", "textJT", "textGM"});
    CHECK(OtherChampionNames("").size() == 6);
}

TEST_CASE("a hidden text loses the links that would show it") {
    assets::CutsceneEffectsAsset effects{
        .texts = {{.name = "textCR"}, {.name = "textKK"}},
        .links = {{.transform = "t", .kind = assets::EffectKind::Text, .effect = 0},
                  {.transform = "t", .kind = assets::EffectKind::Text, .effect = 1},
                  {.transform = "t", .kind = assets::EffectKind::Sound, .effect = 0}}};

    HideTexts(effects, std::vector<std::string>{"TEXTCR"});

    REQUIRE(effects.links.size() == 2);
    CHECK(effects.links[0].effect == 1);
    CHECK(effects.links[1].kind == assets::EffectKind::Sound);
}

std::vector<std::string> Keys(const assets::MaterialAnimationAsset& animation) {
    std::vector<std::string> keys;
    keys.reserve(animation.keys.size());
    for (const assets::MaterialKey& key : animation.keys) {
        keys.push_back(key.material);
    }
    return keys;
}

TEST_CASE("guy1's face tracks show the driver's own expressions, renamed once") {
    const assets::CutsceneFrame frame{
        .models = {{.model = assets::EntityByName{.name = "Guy1", .isJointed = true},
                    .materials = {{.track = 1}, {.track = 1}}},
                   {.model = assets::EntityByName{.name = "rr1", .isJointed = true},
                    .materials = {{.track = 0}}}}};
    WorldAnimations animations{{assets::MaterialAnimationAsset{
        .keys = {{.material = "rdflt"},
                 {.material = "dflt"},
                 {.material = "Happy"},
                 {.material = "blink"}},
        .tracks = {{.firstKey = 0, .keyCount = 1}, {.firstKey = 1, .keyCount = 3}}}}};

    ShowDriverFaces(std::vector{frame, frame}, "cr_", animations);

    CHECK(Keys(animations[0][0]) ==
          std::vector<std::string>{"rdflt", "cr_dflt", "cr_happy", "cr_blink"});
}

}
}
