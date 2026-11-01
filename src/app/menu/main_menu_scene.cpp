#include "app/menu/main_menu_scene.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_focus.hpp"
#include "app/menu/menu_scenes.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";

struct ButtonTarget {
    std::string_view button;
    std::string_view scene;
};

constexpr std::array kButtonLines{
    ButtonLine{.button = "garage", .line = 37}, ButtonLine{.button = "circuit", .line = 33},
    ButtonLine{.button = "single", .line = 34}, ButtonLine{.button = "vs", .line = 35},
    ButtonLine{.button = "time", .line = 36},   ButtonLine{.button = "options", .line = 38},
    ButtonLine{.button = "quit", .line = 39},
};

constexpr std::array kButtonTargets{
    ButtonTarget{.button = "circuit", .scene = kCircuitRaceScene},
    ButtonTarget{.button = "single", .scene = kSingleRaceScene},
    ButtonTarget{.button = "quit", .scene = kQuitScene},
};

constexpr std::array kFocusMoves{
    std::pair{sim::MenuAction::Up, FocusMove::Up},
    std::pair{sim::MenuAction::Down, FocusMove::Down},
    std::pair{sim::MenuAction::Left, FocusMove::Left},
    std::pair{sim::MenuAction::Right, FocusMove::Right},
};

}

Result<std::unique_ptr<MainMenuScene>> MainMenuScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs) {
    Result<MenuPage> page = ReadMenuFiles(vfs, kScreen).and_then([&](const MenuFiles& files) {
        return BuildMenuPage(renderer, vfs, files, kButtonLines);
    });
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }
    return std::unique_ptr<MainMenuScene>(new MainMenuScene(renderer, vfs, std::move(*page)));
}

void MainMenuScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
}

std::optional<std::string> MainMenuScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void MainMenuScene::Tick(const sim::InputFrame& input) {
    if (page_.buttons.empty()) {
        return;
    }
    for (const auto& [action, move] : kFocusMoves) {
        if (input.WasPressed(action)) {
            focus_ = NextFocus(buttonBounds_, focus_, move);
        }
    }
    if (input.WasPressed(sim::MenuAction::Confirm)) {
        Choose(page_.buttons[focus_]);
    }
}

void MainMenuScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = focus_;
}

void MainMenuScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(page_.pictures, sprites_);
    for (std::size_t i = 0; i < page_.buttons.size(); ++i) {
        DrawButton(page_.buttons[i], i == focus_ ? kFocusedState : kNormalState);
    }
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

MainMenuScene::MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuPage page)
    : renderer_(&renderer), vfs_(&vfs), page_(std::move(page)) {
    for (const MenuButton& button : page_.buttons) {
        buttonBounds_.push_back(button.bounds);
    }
}

void MainMenuScene::Choose(const MenuButton& button) {
    const auto target =
        std::ranges::find(kButtonTargets, std::string_view(button.name), &ButtonTarget::button);
    if (target == kButtonTargets.end()) {
        log::Info("main menu: {} isn't built yet", button.name);
        return;
    }
    next_ = std::string(target->scene);
}

void MainMenuScene::DrawButton(const MenuButton& button, std::size_t state) {
    const MenuButtonState& look = button.states.at(state);
    sprites_.push_back({.texture = look.image, .bounds = button.bounds});
    DrawLabel(text_, look, text_.Line(button.textLine), button.bounds.x, button.bounds.y, sprites_);
}

}
