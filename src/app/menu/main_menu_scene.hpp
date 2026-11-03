#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/attract_demo.hpp"
#include "app/menu/menu_champion.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

class MainMenuScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene. `champion` is a driver code.
    [[nodiscard]] static Result<std::unique_ptr<MainMenuScene>> Create(render::Renderer& renderer,
                                                                       const assets::Vfs& vfs,
                                                                       MenuSounds sounds,
                                                                       std::string_view champion,
                                                                       bool showsUnlockNotice);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] SceneMusic Music() const override {
        return {.track = kMenuTheme, .start = MusicStart::OnEntryIfSilent};
    }
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                  MenuPage page, std::unique_ptr<MenuChampion> champion,
                  std::optional<DialogLook> dialogLook);

    void TickDialog(MessageDialog& dialog, const sim::InputFrame& input);
    void OpenDialog(std::size_t messageLine, std::vector<DialogButton> buttons,
                    std::string_view firstButtonScene);
    void MoveFocus(std::size_t focus);
    void Choose(const MenuButton& button);
    void DrawButton(const MenuButton& button, std::size_t state);

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    MenuPage page_;
    std::unique_ptr<MenuChampion> champion_;
    std::vector<render::Rect> buttonBounds_;
    MenuText text_;
    std::size_t focus_ = 0;
    std::optional<std::string> next_;
    std::optional<DialogLook> dialogLook_;
    std::optional<MessageDialog> dialog_;
    std::string_view firstButtonScene_;
    std::vector<render::SpriteDraw> sprites_;
    std::string demo_;
    AttractTimer attract_;
};

}
