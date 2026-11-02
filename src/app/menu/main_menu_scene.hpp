#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Only CIRCUIT RACE, SINGLE RACE, OPTIONS and QUIT lead anywhere yet; the other buttons log.
class MainMenuScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<MainMenuScene>> Create(render::Renderer& renderer,
                                                                       const assets::Vfs& vfs,
                                                                       MenuSounds sounds,
                                                                       bool showsUnlockNotice);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                  MenuPage page, std::optional<MessageDialog> notice);

    void MoveFocus(std::size_t focus);
    void Choose(const MenuButton& button);
    void DrawButton(const MenuButton& button, std::size_t state);

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    MenuPage page_;
    std::vector<render::Rect> buttonBounds_;
    MenuText text_;
    std::size_t focus_ = 0;
    std::optional<std::string> next_;
    std::optional<MessageDialog> notice_;
    std::vector<render::SpriteDraw> sprites_;
};

}
