#include "app/menu/language_scene.hpp"

#include <cstddef>
#include <span>
#include <utility>

#include "app/game_settings.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_widgets.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kTitleLine = 0x9C;
constexpr std::size_t kFirstLanguageLine = 0x9D;
constexpr std::size_t kOptionsLine = 0x10;

}

Result<std::unique_ptr<LanguageScene>> LanguageScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             MenuSounds sounds) {
    Result<LanguageLook> look = LoadLanguageLook(renderer, vfs);
    if (!look) {
        return std::unexpected(std::move(look.error()));
    }
    return std::unique_ptr<LanguageScene>(
        new LanguageScene(renderer, vfs, sounds, std::move(*look)));
}

void LanguageScene::ApplySettings(const settings::Registry& settings) {
    if (menu_) {
        return;
    }
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    menu_.emplace(SelectedLanguage(settings));
}

std::optional<std::string> LanguageScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

std::vector<SettingChange> LanguageScene::TakeSettingChanges() {
    return std::exchange(changes_, {});
}

void LanguageScene::Tick(const sim::InputFrame& input) {
    if (!menu_) {
        return;
    }
    const bool wasBackFocused = menu_->IsBackFocused();
    const LanguageRequest request = menu_->Tick(input);
    const bool isBackFocused = menu_->IsBackFocused();
    if (request == LanguageRequest::Change) {
        changes_.push_back(
            {.key = std::string(kGameLanguage.key), .value = std::string(menu_->Language())});
    }
    if (request == LanguageRequest::Leave) {
        next_ = std::string(kOptionsScene);
    }
    PlaySounds(input, request, wasBackFocused, isBackFocused);
}

void LanguageScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_ ? menu_->StateHash() : 0;
}

void LanguageScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(std::span(&look_.backdrop, 1), sprites_);
    DrawBanner(text_, look_.banner, text_.Line(kTitleLine), look_.backdrop, look_.bannerTop,
               sprites_);
    DrawPicker();
    const bool isBackFocused = menu_ && menu_->IsBackFocused();
    DrawImageButton(text_, look_.backButton, isBackFocused ? kFocusedState : kNormalState,
                    text_.Line(kOptionsLine), look_.back, sprites_);
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

LanguageScene::LanguageScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                             LanguageLook look)
    : renderer_(&renderer), vfs_(&vfs), sounds_(sounds), look_(std::move(look)) {}

void LanguageScene::PlaySounds(const sim::InputFrame& input, LanguageRequest request,
                               bool wasBackFocused, bool isBackFocused) {
    if (isBackFocused != wasBackFocused) {
        sounds_.MoveHighlight(wasBackFocused ? look_.backButton.sounds : look_.pickerSounds,
                              isBackFocused ? look_.backButton.sounds : look_.pickerSounds);
    }
    if (request == LanguageRequest::Change) {
        sounds_.Scroll(look_.scrollSounds, input.WasPressed(sim::MenuAction::Left) ? -1 : 1);
    }
    if (request == LanguageRequest::Leave) {
        sounds_.Confirm(look_.backButton.sounds);
    }
}

void LanguageScene::DrawPicker() {
    if (!menu_) {
        return;
    }
    const bool isFocused = !menu_->IsBackFocused();
    DrawSelectorArrows(look_.arrows, look_.picker, isFocused, sprites_);
    if (const UiFont* font = text_.Font(look_.label.font)) {
        DrawSelectorValue(*font, text_.Line(kFirstLanguageLine + menu_->Index()), look_.picker,
                          isFocused ? kFocusedSelectorValueColor : kSelectorValueColor, sprites_);
    }
}

}
