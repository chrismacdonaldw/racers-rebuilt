#include "app/menu/main_menu_scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_focus.hpp"
#include "app/menu/menu_scenes.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::size_t kUnlockNoticeLine = 0x7C;
constexpr std::size_t kOkLine = 0x72;

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
    ButtonTarget{.button = "options", .scene = kOptionsScene},
    ButtonTarget{.button = "quit", .scene = kQuitScene},
};

constexpr std::array kFocusMoves{
    std::pair{sim::MenuAction::Up, FocusMove::Up},
    std::pair{sim::MenuAction::Down, FocusMove::Down},
    std::pair{sim::MenuAction::Left, FocusMove::Left},
    std::pair{sim::MenuAction::Right, FocusMove::Right},
};

std::optional<MessageDialog> UnlockNotice(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the new circuit notice won't show", look.error().message);
        return std::nullopt;
    }
    return MessageDialog(std::move(*look), kUnlockNoticeLine, kOkLine);
}

}

Result<std::unique_ptr<MainMenuScene>> MainMenuScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             bool showsUnlockNotice) {
    Result<MenuPage> page = ReadMenuFiles(vfs, kScreen).and_then([&](const MenuFiles& files) {
        return BuildMenuPage(renderer, vfs, files, kButtonLines);
    });
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }
    std::optional<MessageDialog> notice =
        showsUnlockNotice ? UnlockNotice(renderer, vfs) : std::nullopt;
    return std::unique_ptr<MainMenuScene>(
        new MainMenuScene(renderer, vfs, std::move(*page), std::move(notice)));
}

void MainMenuScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    if (notice_) {
        notice_->Measure(text_);
    }
}

std::optional<std::string> MainMenuScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void MainMenuScene::Tick(const sim::InputFrame& input) {
    if (notice_) {
        notice_->Tick(input);
        if (notice_->IsClosed()) {
            notice_.reset();
        }
        return;
    }
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
    if (notice_) {
        notice_->Draw(text_, sprites_);
    }
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

MainMenuScene::MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuPage page,
                             std::optional<MessageDialog> notice)
    : renderer_(&renderer), vfs_(&vfs), page_(std::move(page)), notice_(std::move(notice)) {
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
