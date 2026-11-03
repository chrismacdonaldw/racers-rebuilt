#include "app/menu/menu_scene_factory.hpp"

#include <cstddef>

#include "app/circuit_awards.hpp"
#include "app/menu/controls_scene.hpp"
#include "app/menu/language_scene.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/options_scene.hpp"
#include "app/menu/race_setup_scene.hpp"
#include "app/menu/racer_pick_scene.hpp"

namespace rr::app {

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
    if (const std::optional<std::string_view> back = SceneAfter(scene, kControlsScene)) {
        return ControlsScene::Create(renderer, vfs, 0, std::string(*back))
            .transform(ToScene<ControlsScene>);
    }
    if (const std::optional<std::string_view> next = SceneAfter(scene, kPickRacerScene)) {
        return RacerPickScene::Create(renderer, vfs, sounds(), parts.racersFile, std::string(*next))
            .transform(ToScene<RacerPickScene>);
    }
    if (scene == kLanguageScene) {
        return LanguageScene::Create(renderer, vfs, sounds()).transform(ToScene<LanguageScene>);
    }
    if (const std::optional<RaceSetupMode> mode = RaceSetupModeOf(scene)) {
        return RaceSetupScene::Create(parts.outputs, vfs, sounds(), *mode,
                                      ReadProgress(parts.progressFile), parts.language)
            .transform(ToScene<RaceSetupScene>);
    }
    return std::nullopt;
}

}
