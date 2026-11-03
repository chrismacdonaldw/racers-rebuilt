#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/language_look.hpp"
#include "app/menu/language_menu.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/pointer_buttons.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Saves each step at once, but keeps the text in the language the screen opened with.
class LanguageScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<LanguageScene>> Create(render::Renderer& renderer,
                                                                       const assets::Vfs& vfs,
                                                                       MenuSounds sounds);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    [[nodiscard]] std::vector<SettingChange> TakeSettingChanges() override;
    [[nodiscard]] bool ShowsPointer() const override { return true; }
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    LanguageScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                  LanguageLook look);

    void PlaySounds(const sim::InputFrame& input, LanguageRequest request, bool wasBackFocused,
                    bool isBackFocused);
    void AimPointer(bool isBackFocused);
    void DrawPicker();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    LanguageLook look_;
    std::optional<LanguageMenu> menu_;
    MenuText text_;
    std::optional<std::string> next_;
    std::vector<SettingChange> changes_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<PointerTarget> targets_;
    PointerButtons pointer_;
};

}
