#include "app/menu/menu_scene_factory.hpp"

#include <array>
#include <cstddef>

#include "app/circuit_awards.hpp"
#include "app/menu/controls_scene.hpp"
#include "app/menu/edit_car_scene.hpp"
#include "app/menu/edit_driver_scene.hpp"
#include "app/menu/garage_scene.hpp"
#include "app/menu/language_scene.hpp"
#include "app/menu/licence_scene.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/options_scene.hpp"
#include "app/menu/race_setup_scene.hpp"
#include "app/menu/racer_pick_scene.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kVersusPlayers = 2;
constexpr std::array kControls{kControlsScene, kPlayer2ControlsScene};
constexpr std::array kPicks{kPickRacerScene, kPickSecondRacerScene};

bool ClearsCheats(RaceSetupMode mode) {
    return mode == RaceSetupMode::Circuit || mode == RaceSetupMode::TimeTrial;
}

std::optional<Result<std::unique_ptr<Scene>>> CreateGarageScene(const MenuSceneParts& parts,
                                                                std::string_view scene) {
    render::Renderer& renderer = *parts.outputs.renderer;
    const assets::Vfs& vfs = *parts.vfs;
    const auto sounds = [&] {
        return LoadMenuSounds(*parts.outputs.mixer, *parts.outputs.banks, vfs);
    };
    if (scene == kGarageScene) {
        return GarageScene::Create(renderer, vfs, sounds(), parts.racersFile, 0)
            .transform(ToScene<GarageScene>);
    }
    if (const std::optional<std::size_t> racer = SceneRacer(scene, kGarageScene)) {
        return GarageScene::Create(renderer, vfs, sounds(), parts.racersFile, *racer)
            .transform(ToScene<GarageScene>);
    }
    if (const std::optional<std::size_t> racer = SceneRacer(scene, kEditDriverScene)) {
        return EditDriverScene::Create(renderer, vfs, sounds(),
                                       {.racers = parts.racersFile, .progress = parts.progressFile},
                                       *racer)
            .transform(ToScene<EditDriverScene>);
    }
    if (const std::optional<std::size_t> racer = SceneRacer(scene, kEditCarScene)) {
        return EditCarScene::Create(renderer, vfs, sounds(), parts.racersFile, *racer,
                                    ReadProgress(parts.progressFile).partSets)
            .transform(ToScene<EditCarScene>);
    }
    if (const std::optional<std::size_t> racer = SceneRacer(scene, kLicenceScene);
        racer && parts.cheats != nullptr) {
        return LicenceScene::Create(renderer, vfs, sounds(), parts.racersFile, *racer,
                                    *parts.cheats)
            .transform(ToScene<LicenceScene>);
    }
    return std::nullopt;
}

}

std::optional<Result<std::unique_ptr<Scene>>> CreateMenuScene(const MenuSceneParts& parts,
                                                              std::string_view scene) {
    render::Renderer& renderer = *parts.outputs.renderer;
    const assets::Vfs& vfs = *parts.vfs;
    const auto sounds = [&] {
        return LoadMenuSounds(*parts.outputs.mixer, *parts.outputs.banks, vfs);
    };
    if (scene == kOptionsScene) {
        return OptionsScene::Create(renderer, vfs, sounds()).transform(ToScene<OptionsScene>);
    }
    if (scene == kControlsScene || scene == kPlayer2ControlsScene) {
        const std::size_t player = scene == kPlayer2ControlsScene ? 1 : 0;
        return ControlsScene::Create(renderer, vfs, player, std::string(kOptionsScene))
            .transform(ToScene<ControlsScene>);
    }
    for (std::size_t player = 0; player < kVersusPlayers; ++player) {
        if (const std::optional<std::string_view> back = SceneAfter(scene, kControls.at(player))) {
            return ControlsScene::Create(renderer, vfs, player, std::string(*back))
                .transform(ToScene<ControlsScene>);
        }
        if (const std::optional<std::string_view> next = SceneAfter(scene, kPicks.at(player))) {
            return RacerPickScene::Create(renderer, vfs, sounds(), parts.racersFile,
                                          {.next = std::string(*next), .player = player})
                .transform(ToScene<RacerPickScene>);
        }
    }
    if (std::optional<Result<std::unique_ptr<Scene>>> garage = CreateGarageScene(parts, scene)) {
        return garage;
    }
    if (scene == kLanguageScene) {
        return LanguageScene::Create(renderer, vfs, sounds()).transform(ToScene<LanguageScene>);
    }
    if (const std::optional<RaceSetupMode> mode = RaceSetupModeOf(scene)) {
        if (ClearsCheats(*mode) && parts.cheats != nullptr) {
            *parts.cheats = {};
        }
        return RaceSetupScene::Create(parts.outputs, vfs, sounds(), *mode,
                                      ReadProgress(parts.progressFile), parts.language)
            .transform(ToScene<RaceSetupScene>);
    }
    return std::nullopt;
}
}
