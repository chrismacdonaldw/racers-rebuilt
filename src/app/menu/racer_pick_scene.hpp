#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/racer_pick_look.hpp"
#include "app/menu/racer_pick_menu.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// START saves the pick as selected, unless the racers file won't load, then opens `next`.
class RacerPickScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<RacerPickScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
        std::filesystem::path racersFile, std::string next);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    struct Racers {
        std::filesystem::path file;
        bool isLoaded = false;
        settings::RacerStore store;
        std::vector<settings::Racer> pickable;
        std::vector<std::u16string> names;
    };

    RacerPickScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                   RacerPickLook look, std::string next, Racers racers);

    void Start();
    void PlaySounds(const sim::InputFrame& input, RacerPickRequest request,
                    RacerPickRow focusBefore) const;
    [[nodiscard]] const assets::SoundIds& SoundsOf(RacerPickRow row) const;
    void DrawPicker();
    void DrawButtons();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    RacerPickLook look_;
    std::string next_;
    Racers racers_;
    RacerPickMenu menu_;
    MenuText text_;
    std::optional<std::string> opened_;
    std::vector<render::SpriteDraw> sprites_;
};

}
