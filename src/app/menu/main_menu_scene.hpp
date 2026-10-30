#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "app/menu/menu_parts.hpp"
#include "app/scene.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The main menu's backdrop and buttons, moved through with the menu actions. Choosing a button
/// only logs it for now.
class MainMenuScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<MainMenuScene>> Create(render::Renderer& renderer,
                                                                       const assets::Vfs& vfs);

    void ApplySettings(const settings::Registry& settings) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs,
                  std::vector<MenuPicture> pictures, std::vector<MenuButton> buttons);

    void DrawButton(const MenuButton& button, std::size_t state);

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    std::vector<MenuPicture> pictures_;
    std::vector<MenuButton> buttons_;
    std::vector<render::Rect> buttonBounds_;
    std::string language_;
    UiFonts fonts_;
    std::vector<std::u16string> text_;
    std::size_t focus_ = 0;
    std::vector<render::SpriteDraw> sprites_;
};

}
