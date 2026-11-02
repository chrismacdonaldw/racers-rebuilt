#include "app/menu/options_scene.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <span>
#include <string_view>
#include <utility>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/option_settings.hpp"
#include "app/ui_text/text_layout.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kWarningLine = 0xBD;
constexpr std::size_t kOkLine = 0x72;
constexpr std::size_t kMainMenuLine = 0x02;
constexpr std::size_t kOptionsLine = 0x10;
constexpr std::size_t kFirstDigitLine = 0x60;
constexpr std::size_t kStereoLine = 0xA7;
constexpr std::size_t kMonoLine = 0xA8;
constexpr char16_t kLineFeed = u'\n';
constexpr render::Color kStepColor{
    .r = 128.0F / 255.0F, .g = 128.0F / 255.0F, .b = 128.0F / 255.0F};

std::size_t TitleLine(OptionsPage page) {
    switch (page) {
        case OptionsPage::Game:
            return 0x11;
        case OptionsPage::Video:
            return 0x12;
        case OptionsPage::Audio:
            return 0x13;
        case OptionsPage::Main:
            break;
    }
    return kOptionsLine;
}

std::u16string DriverName(render::Renderer& renderer) {
    const char* driver = SDL_GetGPUDeviceDriver(renderer.Device());
    std::u16string name;
    for (const char letter : std::string_view(driver != nullptr ? driver : "")) {
        name.push_back(static_cast<char16_t>(std::toupper(static_cast<unsigned char>(letter))));
    }
    return name;
}

std::optional<DialogLook> WarningLook(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the video and audio warning won't show", look.error().message);
        return std::nullopt;
    }
    return std::move(*look);
}

}

Result<std::unique_ptr<OptionsScene>> OptionsScene::Create(render::Renderer& renderer,
                                                           const assets::Vfs& vfs,
                                                           MenuSounds sounds) {
    Result<OptionsLook> look = LoadOptionsLook(renderer, vfs);
    if (!look) {
        return std::unexpected(std::move(look.error()));
    }
    return std::unique_ptr<OptionsScene>(
        new OptionsScene(renderer, vfs, sounds, std::move(*look), WarningLook(renderer, vfs)));
}

void OptionsScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    menu_.Show(ReadOptionValues(settings));
    if (warning_) {
        warning_->Measure(text_);
    }
}

std::optional<std::string> OptionsScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

std::vector<SettingChange> OptionsScene::TakeSettingChanges() {
    return std::exchange(changes_, {});
}

void OptionsScene::Tick(const sim::InputFrame& input) {
    if (warning_) {
        TickWarning(input);
        return;
    }
    const OptionValues before = menu_.Values();
    Respond(menu_.Tick(input), before);
}

void OptionsScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
}

void OptionsScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(std::span(&look_.backdrop, 1), sprites_);
    DrawTitle();
    DrawButtons();
    DrawLabels();
    for (const OptionsSelector& selector : look_.selectors) {
        if (std::ranges::contains(menu_.Rows(), selector.row)) {
            DrawSelector(selector);
        }
    }
    for (const OptionsSlider& slider : look_.sliders) {
        if (std::ranges::contains(menu_.Rows(), slider.row)) {
            DrawSlider(slider);
        }
    }
    if (warning_) {
        warning_->Draw(text_, sprites_);
    }
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

OptionsScene::OptionsScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                           OptionsLook look, std::optional<DialogLook> warningLook)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      look_(std::move(look)),
      warningLook_(std::move(warningLook)),
      driverName_(DriverName(renderer)) {}

void OptionsScene::TickWarning(const sim::InputFrame& input) {
    if (!warning_) {
        return;
    }
    static_cast<void>(warning_->Tick(input, sounds_));
    if (warning_->IsClosed()) {
        warning_.reset();
        menu_.Proceed();
    }
}

void OptionsScene::Respond(OptionsRequest request, const OptionValues& before) {
    switch (request) {
        case OptionsRequest::Warn:
            if (!warningLook_) {
                menu_.Proceed();
                return;
            }
            warning_.emplace(*warningLook_, kWarningLine,
                             std::vector<DialogButton>{{.line = kOkLine}});
            warning_->Measure(text_);
            return;
        case OptionsRequest::ChangeValue:
            std::ranges::move(OptionChanges(before, menu_.Values()), std::back_inserter(changes_));
            return;
        case OptionsRequest::Player1Controls:
            next_ = std::string(kControlsScene);
            return;
        case OptionsRequest::Player2Controls:
            next_ = std::string(kPlayer2ControlsScene);
            return;
        case OptionsRequest::Language:
            next_ = std::string(kLanguageScene);
            return;
        case OptionsRequest::Leave:
            next_ = std::string(kMainMenuScene);
            return;
        case OptionsRequest::NotBuilt:
            log::Info("options: that screen isn't built yet");
            return;
        case OptionsRequest::None:
            return;
    }
}

void OptionsScene::DrawTitle() {
    DrawBanner(text_, look_.banner, text_.Line(TitleLine(menu_.Page())), look_.backdrop,
               look_.bannerTop, sprites_);
}

void OptionsScene::DrawButtons() {
    const bool isMain = menu_.Page() == OptionsPage::Main;
    for (const OptionsButton& button : look_.buttons) {
        const std::size_t state = menu_.Focus() == button.row ? kFocusedState : kNormalState;
        if (button.row != OptionsRow::Back) {
            if (isMain) {
                DrawLabel(text_, look_.textButton.at(state), text_.Line(button.textLine),
                          button.bounds.x, button.bounds.y, sprites_);
            }
            continue;
        }
        DrawImageButton(text_, look_.backButton, state,
                        text_.Line(isMain ? kMainMenuLine : kOptionsLine),
                        {.x = button.bounds.x, .y = button.bounds.y}, sprites_);
    }
}

void OptionsScene::DrawLabels() {
    const UiFont* font = text_.Font(look_.label.font);
    if (font == nullptr) {
        return;
    }
    for (const OptionsLabel& label : look_.labels) {
        if (label.page != menu_.Page()) {
            continue;
        }
        float y = label.y;
        std::u16string_view text = text_.Line(label.textLine);
        while (!text.empty()) {
            const std::size_t end = text.find(kLineFeed);
            LayoutText(*font, text.substr(0, end),
                       {.x = label.x, .y = y, .tint = look_.label.color}, sprites_);
            if (end == std::u16string_view::npos) {
                break;
            }
            text.remove_prefix(end + 1);
            y += static_cast<float>(font->heightPx);
        }
    }
}

void OptionsScene::DrawSelector(const OptionsSelector& selector) {
    const bool isFocused = menu_.Focus() == selector.row;
    const SelectorPlace& place = selector.place;
    DrawSelectorArrows(look_.arrows, place, isFocused, sprites_);
    const bool isDriver = selector.row == OptionsRow::Driver;
    const MenuTextStyle& look = isDriver ? look_.driver : look_.label;
    const UiFont* font = text_.Font(look.font);
    if (font == nullptr) {
        return;
    }
    const render::Color valueTint = isFocused ? kFocusedSelectorValueColor : kSelectorValueColor;
    DrawSelectorValue(*font, ValueText(selector.row), place, isDriver ? look.color : valueTint,
                      sprites_);
}

void OptionsScene::DrawSlider(const OptionsSlider& slider) {
    const bool isFocused = menu_.Focus() == slider.row;
    const OptionValues& values = menu_.Values();
    const int step = slider.row == OptionsRow::MusicVolume ? values.musicStep : values.soundStep;
    const SliderParts parts = LayOutSlider(slider.bounds, look_, step);
    const render::Color stepTint = isFocused ? render::Color{} : kStepColor;
    const render::Color barTint = isFocused ? kFocusedSelectorValueColor : kSelectorValueColor;
    sprites_.push_back({.texture = look_.rail.texture, .bounds = parts.rail, .tint = barTint});
    sprites_.push_back({.texture = look_.thumb.texture, .bounds = parts.thumb, .tint = barTint});
    sprites_.push_back(
        {.texture = look_.stepDown.texture, .bounds = parts.stepDown, .tint = stepTint});
    sprites_.push_back({.texture = look_.stepUp.texture, .bounds = parts.stepUp, .tint = stepTint});
}

std::u16string_view OptionsScene::ValueText(OptionsRow row) const {
    const OptionValues& values = menu_.Values();
    switch (row) {
        case OptionsRow::Opponents:
            return text_.Line(kFirstDigitLine + static_cast<std::size_t>(values.opponents));
        case OptionsRow::VersusLaps:
            return text_.Line(kFirstDigitLine + static_cast<std::size_t>(values.versusLaps));
        case OptionsRow::Stereo:
            return text_.Line(values.isStereo ? kStereoLine : kMonoLine);
        case OptionsRow::Driver:
            return driverName_;
        default:
            return {};
    }
}

}
