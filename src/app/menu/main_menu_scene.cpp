#include "app/menu/main_menu_scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/attract_demo.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_focus.hpp"
#include "app/menu/menu_scenes.hpp"
#include "assets/race_catalog.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"
#include "sim/race/random.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::size_t kUnlockNoticeLine = 0x7C;
constexpr std::size_t kOkLine = 0x72;
constexpr std::size_t kYesLine = 0x73;
constexpr std::size_t kNoLine = 0x74;
constexpr std::size_t kQuitQuestionLine = 0x75;
constexpr std::string_view kQuitButton = "quit";
constexpr std::int32_t kGlassTapSound = 29;

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
    ButtonTarget{.button = "garage", .scene = kGarageScene},
    ButtonTarget{.button = "circuit", .scene = kCircuitRaceScene},
    ButtonTarget{.button = "single", .scene = kSingleRaceScene},
    ButtonTarget{.button = "vs", .scene = kVersusRaceScene},
    ButtonTarget{.button = "time", .scene = kTimeTrialScene},
    ButtonTarget{.button = "options", .scene = kOptionsScene},
};

constexpr std::array kFocusMoves{
    std::pair{sim::MenuAction::Up, FocusMove::Up},
    std::pair{sim::MenuAction::Down, FocusMove::Down},
    std::pair{sim::MenuAction::Left, FocusMove::Left},
    std::pair{sim::MenuAction::Right, FocusMove::Right},
};

std::optional<DialogLook> MainMenuDialogLook(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the main menu won't show dialogs", look.error().message);
        return std::nullopt;
    }
    return std::move(*look);
}

std::string PickDemo(const assets::Vfs& vfs) {
    sim::Random random(static_cast<std::uint64_t>(platform::Now().count()));
    Result<std::string> demo = assets::RaceCatalog::Load(vfs).and_then(
        [&](const assets::RaceCatalog& races) { return AttractDemoScene(races, random.Next()); });
    if (!demo) {
        log::Warn("{}, the main menu won't start a demo", demo.error().message);
        return {};
    }
    return std::move(*demo);
}

std::unique_ptr<MenuChampion> LoadChampion(render::Renderer& renderer, const assets::Vfs& vfs,
                                           const MenuFiles& files, std::string_view champion) {
    Result<std::unique_ptr<MenuChampion>> loaded =
        MenuChampion::Load(renderer, vfs, files, champion);
    if (!loaded) {
        log::Warn("{}, the main menu shows no champion", loaded.error().message);
        return nullptr;
    }
    return std::move(*loaded);
}

}

Result<std::unique_ptr<MainMenuScene>> MainMenuScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             MenuSounds sounds,
                                                             std::string_view champion,
                                                             bool showsUnlockNotice) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kScreen);
    Result<MenuPage> page = files.and_then(
        [&](const MenuFiles& read) { return BuildMenuPage(renderer, vfs, read, kButtonLines); });
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }
    auto scene = std::unique_ptr<MainMenuScene>(new MainMenuScene(
        renderer, vfs, sounds, std::move(*page), LoadChampion(renderer, vfs, *files, champion),
        MainMenuDialogLook(renderer, vfs)));
    scene->demo_ = PickDemo(vfs);
    if (showsUnlockNotice) {
        scene->OpenDialog(kUnlockNoticeLine, {{.line = kOkLine}}, {});
    }
    return scene;
}

void MainMenuScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    if (dialog_) {
        dialog_->Measure(text_);
    }
}

std::optional<std::string> MainMenuScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void MainMenuScene::Tick(const sim::InputFrame& input) {
    if (next_) {
        return;
    }
    if (champion_ && champion_->Tick(kSimTick)) {
        sounds_.Play(kGlassTapSound);
    }
    if (attract_.Tick(kSimTick, !dialog_) && !demo_.empty()) {
        next_ = demo_;
        return;
    }
    if (dialog_) {
        TickDialog(*dialog_, input);
        return;
    }
    if (page_.buttons.empty()) {
        return;
    }
    for (const auto& [action, move] : kFocusMoves) {
        if (input.WasPressed(action)) {
            MoveFocus(NextFocus(buttonBounds_, focus_, move));
        }
    }
    const PointerChoice pointed = pointer_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = pointed.Focus()) {
        MoveFocus(*focus);
    }
    if (input.WasPressed(sim::MenuAction::Confirm) || pointed.Clicks(sim::MenuAction::Confirm)) {
        sounds_.Confirm(page_.buttons[focus_].sounds);
        Choose(page_.buttons[focus_]);
    }
}

void MainMenuScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = focus_;
    out.entityClocks.clear();
    if (champion_) {
        out.entityClocks.assign(champion_->Clocks().begin(), champion_->Clocks().end());
    }
}

void MainMenuScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    render::RenderView view;
    if (champion_) {
        DrawPicturesAround(page_.pictures, champion_->Bounds(), sprites_);
        view = champion_->Render(interpolated.entityClocks, page_.pictures);
    } else {
        DrawPictures(page_.pictures, sprites_);
    }
    for (std::size_t i = 0; i < page_.buttons.size(); ++i) {
        DrawButton(page_.buttons[i], i == focus_ ? kFocusedState : kNormalState);
    }
    if (dialog_) {
        dialog_->Draw(text_, sprites_);
    }
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

MainMenuScene::MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                             MenuPage page, std::unique_ptr<MenuChampion> champion,
                             std::optional<DialogLook> dialogLook)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      page_(std::move(page)),
      champion_(std::move(champion)),
      dialogLook_(std::move(dialogLook)) {
    for (const MenuButton& button : page_.buttons) {
        targets_.push_back({.area = button.bounds, .control = buttonBounds_.size()});
        buttonBounds_.push_back(button.bounds);
    }
}

void MainMenuScene::TickDialog(MessageDialog& dialog, const sim::InputFrame& input) {
    if (dialog.Tick(input, sounds_) == 0 && !firstButtonScene_.empty()) {
        next_ = std::string(firstButtonScene_);
    }
    if (dialog.IsClosed()) {
        dialog_.reset();
    }
}

void MainMenuScene::OpenDialog(std::size_t messageLine, std::vector<DialogButton> buttons,
                               std::string_view firstButtonScene) {
    if (!dialogLook_) {
        if (!firstButtonScene.empty()) {
            next_ = std::string(firstButtonScene);
        }
        return;
    }
    dialog_.emplace(*dialogLook_, messageLine, std::move(buttons));
    dialog_->Measure(text_);
    firstButtonScene_ = firstButtonScene;
}

void MainMenuScene::MoveFocus(std::size_t focus) {
    if (focus != focus_) {
        sounds_.MoveHighlight(page_.buttons[focus_].sounds, page_.buttons[focus].sounds);
        focus_ = focus;
    }
}

void MainMenuScene::Choose(const MenuButton& button) {
    if (button.name == kQuitButton) {
        OpenDialog(kQuitQuestionLine,
                   {{.line = kYesLine}, {.line = kNoLine, .style = DialogButtonStyle::Cross}},
                   kQuitScene);
        return;
    }
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
